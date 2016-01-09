// Part of SimCoupe - A SAM Coupe emulator
//
// Tape.cpp: Tape handling
//
//  Copyright (c) 1999-2015 Simon Owen
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "SimCoupe.h"
#include "Tape.h"

#include "CPU.h"
#include "IO.h"
#include "Memory.h"
#include "Sound.h"
#include "Stream.h"
#include "Options.h"

namespace Tape
{

#ifdef USE_LIBSPECTRUM

static bool g_fPlaying;
static std::string strFilePath;
static std::string strFileName;

const DWORD SPECTRUM_TSTATES_PER_SECOND = 3500000;

static libspectrum_tape *pTape;
static libspectrum_byte* pbTape;
static bool fEar;
static libspectrum_dword tremain = 0;

// Return whether the supplied filename appears to be a tape image
bool IsRecognised (const char *pcsz_)
{
    libspectrum_id_t type = LIBSPECTRUM_ID_UNKNOWN;

    if (libspectrum_identify_file(&type, pcsz_, nullptr, 0) == LIBSPECTRUM_ERROR_NONE)
    {
        switch (type)
        {
            case LIBSPECTRUM_ID_TAPE_TAP:
            case LIBSPECTRUM_ID_TAPE_TZX:
            case LIBSPECTRUM_ID_TAPE_WAV:
            case LIBSPECTRUM_ID_TAPE_CSW:
                return true;
			default:
				return false;
        }
    }

    return false;
}

bool IsPlaying ()
{
    return g_fPlaying;
}

bool IsInserted ()
{
    return pTape != nullptr;
}

// Return the full path of the inserted tape image
const char* GetPath ()
{
    return strFilePath.c_str();;
}

// Return just the filename of the inserted tape image
const char* GetFile ()
{
    return strFileName.c_str();
}

libspectrum_tape *GetTape ()
{
    return pTape;
}

bool Insert (const char* pcsz_)
{
    Eject();

    CStream *pStream = CStream::Open(pcsz_, true);
    if (!pStream)
        return false;

    pTape = libspectrum_tape_alloc();
    if (!pTape)
    {
        delete pStream;
        return false;
    }

    strFileName = pStream->GetFile();
    size_t uSize = pStream->GetSize();
    pbTape = new libspectrum_byte[uSize];
    if (pbTape) pStream->Read(pbTape, uSize);
    delete pStream;

    if (!pbTape || libspectrum_tape_read(pTape, pbTape, uSize, LIBSPECTRUM_ID_UNKNOWN, pcsz_) != LIBSPECTRUM_ERROR_NONE)
    {
        Eject();
        return false;
    }

    // Store tape path on successful insert
    strFilePath = pcsz_;

    IO::AutoLoad(AUTOLOAD_TAPE);
    return true;
}

void Eject ()
{
    Stop();

    if (pTape) libspectrum_tape_free(pTape), pTape = nullptr;
    delete[] pbTape, pbTape = nullptr;

    strFileName = strFilePath = "";
}

void NextEdge (DWORD dwTime_)
{
    libspectrum_error error;

    if (fEar)
        keyboard |= BORD_EAR_MASK;
    else
        keyboard &= ~BORD_EAR_MASK;

    if (!g_nTurbo)
        pDAC->Output(fEar ? 0xa0 : 0x80);

    libspectrum_dword tstates;
    int nFlags;

    // Fetch details of the next edge, and the time until it's due
    error = libspectrum_tape_get_next_edge(&tstates, &nFlags, pTape);
    if (error)
    {
        Stop();
        return;
    }

    if (nFlags & LIBSPECTRUM_TAPE_FLAGS_LEVEL_LOW)
        fEar = false;
    else if (nFlags & LIBSPECTRUM_TAPE_FLAGS_LEVEL_HIGH)
        fEar = true;
    else if (!(nFlags & LIBSPECTRUM_TAPE_FLAGS_NO_EDGE))
        fEar = !fEar;

    // End of tape block?
    if ((nFlags & LIBSPECTRUM_TAPE_FLAGS_BLOCK))
    {
        // Stop the tape as it'll restart when required
        Stop();
    }
    else
    {
        // Timings are in 3.5MHz t-states, so convert to SAM t-states
        tstates = tstates * (REAL_TSTATES_PER_SECOND / 1000) + tremain;
        libspectrum_dword tadd = tstates / (SPECTRUM_TSTATES_PER_SECOND / 1000);
        tremain = tstates % (SPECTRUM_TSTATES_PER_SECOND / 1000);

        // Schedule an event to activate the edge
        AddCpuEvent(evtTapeEdge, dwTime_+tadd);
    }
}

void Play ()
{
    if (IsInserted() && !IsPlaying())
    {
        g_fPlaying = true;

        // Schedule next edge
        NextEdge(g_dwCycleCounter);

        // Trigger turbo mode if fast loading is enabled
        if (IsPlaying() && GetOption(turbotape))
            g_nTurbo |= TURBO_TAPE;
    }
}

void Stop ()
{
    if (IsPlaying())
    {
        // Cancel any pending edge event
        CancelCpuEvent(evtTapeEdge);

        g_fPlaying = false;
        fEar = false;

        // Clear both tape and key turbo modes, due to some overlap
        g_nTurbo &= ~(TURBO_TAPE|TURBO_KEYIN);
    }
}


bool LoadTrap ()
{
    if (!IsInserted())
        return false;

    // If traps are disabled, try normal loading
    if (!GetOption(tapetraps))
    {
        Play();
        return false;
    }

    // Skip over any metadata blocks
    libspectrum_tape_block *block = libspectrum_tape_current_block(pTape);
    while (block && libspectrum_tape_block_metadata(block))
        block = libspectrum_tape_select_next_block(pTape);

    // Nothing else to process?
    if (!block)
        return false;

    libspectrum_tape_type type = libspectrum_tape_block_type(block);
    libspectrum_tape_state_type state = libspectrum_tape_state(pTape);

    // Consider both ROM blocks (normal speed) and turbo blocks, as used by custom SAM tape speeds (DEVICE tX)
    if ((type != LIBSPECTRUM_TAPE_BLOCK_ROM && type != LIBSPECTRUM_TAPE_BLOCK_TURBO) || state != LIBSPECTRUM_TAPE_STATE_PILOT)
    {
        // Fall back on non-trap loading for anything else
        Play();
        return false;
    }


    libspectrum_byte *pbData = libspectrum_tape_block_data(block);
    size_t nData = libspectrum_tape_block_data_length(block);

    // Base load address and load request size
    WORD wDest = HL;
    int nWanted = (read_byte(0x5ac8) << 16) | DE;

    // Fetch block type
    H = *pbData++;
    nData--;

    // Spectrum header?
    if (H == 0)
    {
        // Override request length 
        nWanted = (nWanted & ~0xff) | 17;
    }
    // Otherwise the type byte must match the request
    else if (H != A_)
    {
        // Advance to next block
        libspectrum_tape_select_next_block(pTape);

        // Failed, exit via: RET NZ
        F &= ~(FLAG_C|FLAG_Z);
        PC = 0xe6f6;

        return true;
    }

    // Parity byte initialised to type byte
    L = H;

    // More still to load?
    while (nWanted >= 0)
    {
        // Are we out of source data?  (ToDo: support continuation blocks?)
        if (!nData)
        {
            // Advance to next block
            libspectrum_tape_select_next_block(pTape);

            // Failed, exit via: RET NZ
            F &= ~(FLAG_C|FLAG_Z);
            PC = 0xe6f6;

            return true;
        }

        // Read next byte and update parity
        L ^= (H = *pbData++);
        nData--;

        // Request complete?
        if (!nWanted)
            break;

        // Write new byte
        write_byte(wDest, H);
        wDest++;
        nWanted--;

        // Destination now in the top 16K?
        if (wDest >= 0xc000)
        {
            // Slide paging up and move pointer back
            IO::OutHmpr(hmpr+1);
            wDest -= 0x4000;
        }
    }

    // Advance to next block
    libspectrum_tape_select_next_block(pTape);

    // Exit via: LD A,L ; CP 1 ; RET
    PC = 0xe739;

    return true;
}


// Return a string describing a give tape block
const char *GetBlockDetails (libspectrum_tape_block *block)
{
    static char sz[128];
    sz[0] = '\0';

    char szExtra[64] = "";
    char szName[11] = "";
    const char *psz = nullptr;

    libspectrum_byte *data = libspectrum_tape_block_data(block);
    long length = static_cast<long>(libspectrum_tape_block_data_length(block));


    // Is there enough data to include a possible filename?
    if (length >= 12)
    {
        for (int i = 0 ; i < 10 ; i++)
        {
            char ch = data[i+2];
            szName[i] = (ch >= ' ' && ch <= 0x7f) ? ch : '?';
        }
        szName[10] = '\0';
    }

    // Spectrum header length and type byte?
    if (length == 17+2 && data[0] == 0x00)
    {
        // Examine Spectrum file type
        switch (data[1])
        {
            case 0:
            {
                psz = "ZX BASIC";

                UINT uLine = (data[15] << 8) | data[14];
                if (uLine != 0xffff)
                    sprintf(szExtra, " LINE %u", uLine);

                break;
            }

            case 1: psz = "ZX DATA()"; break;
            case 2: psz = "ZX DATA$()"; break;

            case 3:
            {
                psz = "ZX CODE";

                UINT uAddr = (data[15] << 8) | data[14];
                UINT uLen = (data[13] << 8) | data[12];
                sprintf(szExtra, " %u,%u", uAddr, uLen);

                break;
            }
        }
    }
    // SAM header length and type byte?
    // Real length is 82, but TZX spec suggests there could be up to 7-8 trailing bits, so accept 83
    else if ((length == 80+2 || length == 80+1+2) && data[0] == 0x01)
    {
        // Examine SAM file type
        switch (data[1])
        {
            case 16:
            {
                psz = "BASIC";

                UINT uLine = (data[40] << 8) | data[39];
                if (data[38] == 0)
                    sprintf(szExtra, " LINE %u", uLine);

                break;
            }

            case 17: psz = "DATA()"; break;
            case 18: psz = "DATA$"; break;
            case 19:
            {
                psz = "CODE";

                UINT uAddr = TPeek(data+32) + 16384;
                UINT uLen = TPeek(data+35);

                sprintf(szExtra, " %u,%u", uAddr, uLen);
                if (data[38] == 0)
                    sprintf(szExtra+strlen(szExtra), ",%u", TPeek(data+38));

                break;
            }

            case 20:
            {
                psz = "SCREEN$";
                UINT uMode = data[17]+1;
                sprintf(szExtra, " MODE %u", uMode);
                break;
            }
        }
    }

    // Do we have a type string?
    if (psz)
    {
        // Start with type and append filename
        strcpy(sz, psz);
        strcat(sz, ": '");
        strcat(sz, szName);
        strcat(sz, "'");

        // Append any additional type-specific details
        if (szExtra[0])
        {
            strcat(sz, " ");
            strcat(sz, szExtra);
        }
    }

    // No details yet?
    if (!sz[0])
    {
        libspectrum_tape_type type = libspectrum_tape_block_type(block);

        switch (type)
        {
            case LIBSPECTRUM_TAPE_BLOCK_ROM:
            case LIBSPECTRUM_TAPE_BLOCK_TURBO:
            {
                // Raw tape block data length
                size_t length = libspectrum_tape_block_data_length(block);

                // If possible, exclude the type, sync, and checksum bytes from the length
                if (length >= 3)
                    length -= 3;

                snprintf(sz, sizeof(sz), "%lu bytes", length);
                break;
            }

            case LIBSPECTRUM_TAPE_BLOCK_PURE_DATA:
            case LIBSPECTRUM_TAPE_BLOCK_RAW_DATA:
                snprintf(sz, sizeof(sz), "%lu bytes", libspectrum_tape_block_data_length(block));
                break;

            case LIBSPECTRUM_TAPE_BLOCK_PURE_TONE:
                snprintf(sz, sizeof(sz), "%u tstates", libspectrum_tape_block_pulse_length(block));
                break;

            case LIBSPECTRUM_TAPE_BLOCK_PULSES:
                snprintf(sz, sizeof(sz), "%lu pulses", libspectrum_tape_block_count(block));
                break;

            case LIBSPECTRUM_TAPE_BLOCK_PAUSE:
                snprintf(sz, sizeof(sz), "%ums", libspectrum_tape_block_pause(block));
                break;

            case LIBSPECTRUM_TAPE_BLOCK_GROUP_START:
            case LIBSPECTRUM_TAPE_BLOCK_COMMENT:
            case LIBSPECTRUM_TAPE_BLOCK_MESSAGE:
            case LIBSPECTRUM_TAPE_BLOCK_CUSTOM:
                snprintf(sz, sizeof(sz), "%s", libspectrum_tape_block_text(block));
                break;

            case LIBSPECTRUM_TAPE_BLOCK_JUMP:
            {
                int offset = libspectrum_tape_block_offset(block);
                if (offset >= 0)
                    snprintf(sz, sizeof(sz), "Forward %d blocks", offset);
                else
                    snprintf(sz, sizeof(sz), "Backward %d blocks", -offset);
                break;
            }

            case LIBSPECTRUM_TAPE_BLOCK_LOOP_START:
                snprintf(sz, sizeof(sz), "%lu iterations", libspectrum_tape_block_count(block));
                break;

            case LIBSPECTRUM_TAPE_BLOCK_SELECT:
                snprintf(sz, sizeof(sz), "%lu options", libspectrum_tape_block_count(block));
                break;

            case LIBSPECTRUM_TAPE_BLOCK_GENERALISED_DATA:
                snprintf(sz, sizeof(sz), "%u data symbols",
                    libspectrum_tape_generalised_data_symbol_table_symbols_in_block(libspectrum_tape_block_data_table(block)));
                break;

            case LIBSPECTRUM_TAPE_BLOCK_ARCHIVE_INFO:
            {
                size_t count = libspectrum_tape_block_count(block);

                for (size_t i = 0 ; i < count ; i++)
                {
                    int id = libspectrum_tape_block_ids(block, i);
                    const char *value = libspectrum_tape_block_texts(block, i);

                    // Full title TZX id?
                    if (id == 0x00)
                        strncpy(sz, value, sizeof(sz)-1);
                }
                break;
            }

            case LIBSPECTRUM_TAPE_BLOCK_HARDWARE:
            {
                size_t count = libspectrum_tape_block_count(block);

                for (size_t i = 0 ; i < count ; i++)
                {
                    int type = libspectrum_tape_block_types(block, i);
                    int id = libspectrum_tape_block_ids(block, i);

                    // Skip anything but the TZX "Computers" type
                    if (type != 0)
                        continue;

                    // Check for relevant computer ids
                    if (id == 9)
                        strcpy(sz, "SAM Coupe");
                    else if ((id >= 0x00 && id <= 0x05) || id == 0x0e)
                        strcpy(sz, "ZX Spectrum");
                    else if (id == 0x08)
                        strcpy(sz, "Pentagon");
                    else if (id == 0x06 || id == 0x07)
                        strcpy(sz, "Timex Sinclair");
                    else
                        snprintf(sz, sizeof(sz), "Unknown (%02X)", id);
                }

                break;
            }

            default:
                break;
        }
    }

    return sz;
}


bool EiHook ()
{
    // If we're leaving the ROM tape loader, consider stopping the tape
    if (PC == 0xe612 /*&& GetOption(tapeauto)*/)
        Stop();

    // Continue normal processing
    return false;
}

bool RetZHook ()
{
    // If we're at LDSTRT in ROM1, consider using the loading trap
    if (PC == 0xe679 && GetSectionPage(SECTION_D) == ROM1 && GetOption(tapetraps))
        return LoadTrap();

    // Continue normal processing
    return false;
}

bool InFEHook ()
{
    // Are we at the port read in the ROM tape edge routine?
    if (PC == 0x2053)
    {
        // Ensure the tape is playing
        Play();

        // Traps enabled and accelerating loading speed?
        if (GetOption(tapetraps) && GetOption(turbotape))
        {
            // Fetch the time until the next tape edge
            DWORD dwTime = GetEventTime(evtTapeEdge);

            // Simulate the edge code to advance to the next edge
            // Return to normal processing if C hits 255 (no edge found) or the ear bit has changed
            while (dwTime > 48 && C < 0xff && !((keyboard ^ B) & BORD_EAR_MASK))
            {
                C += 1;
                R7 += 7;
                g_dwCycleCounter += 48;
                dwTime -= 48;
                PC = 0x2051;
            }
        }
    }

    return false;
}


#else // USE_LIBSPECTRUM

// Dummy implementations, rather than peppering the above with conditional code

bool IsRecognised (const char * /*pcsz_*/) { return false; }
bool IsPlaying () { return false; }
bool IsInserted () { return false; }
const char* GetPath () { return ""; }
const char* GetFile () { return ""; }

bool Insert (const char * /*pcsz_*/) { return false; }
void Eject () { }
void Play () { }
void Stop () { }

void NextEdge (DWORD /*dwTime_*/) { }
bool LoadTrap () { return false; }

bool EiHook () { return false; }
bool RetZHook () { return false; }
bool InFEHook () { return false; }

#endif // USE_LIBSPECTRUM

} // namespace Tape
