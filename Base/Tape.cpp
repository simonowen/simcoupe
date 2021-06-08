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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "SimCoupe.h"
#include "Tape.h"

#include "CPU.h"
#include "Events.h"
#include "Frame.h"
#include "SAMIO.h"
#include "Sound.h"
#include "Stream.h"
#include "Options.h"

namespace Tape
{

#ifdef HAVE_LIBSPECTRUM

static bool g_fPlaying;
static fs::path tape_path;

const uint32_t SPECTRUM_TSTATES_PER_SECOND = 3500000;

static libspectrum_tape* pTape;
static libspectrum_byte* pbTape;
static bool fEar;
static libspectrum_dword tremain = 0;

// Return whether the supplied filename appears to be a tape image
bool IsRecognised(const std::string& filepath)
{
    libspectrum_id_t type = LIBSPECTRUM_ID_UNKNOWN;

    if (libspectrum_identify_file(&type, filepath.c_str(), nullptr, 0) == LIBSPECTRUM_ERROR_NONE)
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

bool IsPlaying()
{
    return g_fPlaying;
}

bool IsInserted()
{
    return pTape != nullptr;
}

// Return the full path of the inserted tape image
std::string GetPath()
{
    return tape_path.string();
}

// Return just the filename of the inserted tape image
std::string GetFile()
{
    return tape_path.filename().string();
}

libspectrum_tape* GetTape()
{
    return pTape;
}

bool Insert(const std::string& filepath)
{
    Eject();

    auto stream = Stream::Open(filepath, true);
    if (!stream)
        return false;

    auto uSize = stream->GetSize();
    if (!uSize)
        return false;

    pTape = libspectrum_tape_alloc();
    if (!pTape)
        return false;

    pbTape = new libspectrum_byte[uSize];
    if (pbTape) stream->Read(pbTape, uSize);

    if (!pbTape || libspectrum_tape_read(pTape, pbTape, uSize, LIBSPECTRUM_ID_UNKNOWN, filepath.c_str()) != LIBSPECTRUM_ERROR_NONE)
    {
        Eject();
        return false;
    }

    // Store tape path on successful insert
    tape_path = filepath;

    IO::AutoLoad(AutoLoadType::Tape);
    return true;
}

void Eject()
{
    Stop();

    if (pTape) libspectrum_tape_free(pTape), pTape = nullptr;
    delete[] pbTape, pbTape = nullptr;

    tape_path.clear();
}

void NextEdge(uint32_t dwTime_)
{
    libspectrum_error error;

    if (fEar)
        IO::State().keyboard |= KEYBOARD_EAR_MASK;
    else
        IO::State().keyboard &= ~KEYBOARD_EAR_MASK;

    if (!Frame::TurboMode())
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
        tstates = tstates * (CPU_CLOCK_HZ / 1000) + tremain;
        libspectrum_dword tadd = tstates / (SPECTRUM_TSTATES_PER_SECOND / 1000);
        tremain = tstates % (SPECTRUM_TSTATES_PER_SECOND / 1000);

        // Schedule an event to activate the edge
        AddEvent(EventType::TapeEdge, dwTime_ + tadd);
    }
}

void Play()
{
    if (IsInserted() && !IsPlaying())
    {
        g_fPlaying = true;

        // Schedule next edge
        NextEdge(CPU::frame_cycles);
    }
}

void Stop()
{
    if (IsPlaying())
    {
        // Cancel any pending edge event
        CancelEvent(EventType::TapeEdge);

        g_fPlaying = false;
        fEar = false;
    }
}


bool LoadTrap()
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
    libspectrum_tape_block* block = libspectrum_tape_current_block(pTape);
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


    libspectrum_byte* pbData = libspectrum_tape_block_data(block);
    size_t nData = libspectrum_tape_block_data_length(block);

    // Base load address and load request size
    auto wDest = cpu.get_hl();
    int nWanted = (read_byte(0x5ac8) << 16) | cpu.get_de();

    // Fetch block type
    cpu.set_h(*pbData++);
    nData--;

    // Spectrum header?
    if (cpu.get_h() == 0)
    {
        // Override request length 
        nWanted = (nWanted & ~0xff) | 17;
    }
    // Otherwise the type byte must match the request
    else if (cpu.get_h() != z80::get_high8(cpu.get_alt_af()))
    {
        // Advance to next block
        libspectrum_tape_select_next_block(pTape);

        // Failed, exit via: RET NZ
        cpu.set_f(cpu.get_f() & ~(cpu.zf_mask | cpu.cf_mask));
        cpu.set_pc(0xe6f6);

        return true;
    }

    // Parity byte initialised to type byte
    cpu.set_l(cpu.get_h());

    // More still to load?
    while (nWanted >= 0)
    {
        // Are we out of source data?  (ToDo: support continuation blocks?)
        if (!nData)
        {
            // Advance to next block
            libspectrum_tape_select_next_block(pTape);

            // Failed, exit via: RET NZ
            cpu.set_f(cpu.get_f() & ~(cpu.zf_mask | cpu.cf_mask));
            cpu.set_pc(0xe6f6);

            return true;
        }

        // Read next byte and update parity
        cpu.set_h(*pbData++);
        cpu.set_l(cpu.get_l() ^ cpu.get_h());
        nData--;

        // Request complete?
        if (!nWanted)
            break;

        // Write new byte
        write_byte(wDest, cpu.get_h());
        wDest++;
        nWanted--;

        // Destination now in the top 16K?
        if (wDest >= 0xc000)
        {
            // Slide paging up and move pointer back
            IO::out_hmpr(IO::State().hmpr + 1);
            wDest -= 0x4000;
        }
    }

    // Advance to next block
    libspectrum_tape_select_next_block(pTape);

    // Exit via: LD A,L ; CP 1 ; RET
    cpu.set_pc(0xe739);

    return true;
}


// Return a string describing a give tape block
std::string GetBlockDetails(libspectrum_tape_block* block)
{
    std::string type;
    std::string filename;
    std::string extra;

    libspectrum_byte* data = libspectrum_tape_block_data(block);
    long length = static_cast<long>(libspectrum_tape_block_data_length(block));

    // Is there enough data to include a possible filename?
    if (length >= 12)
    {
        for (int i = 0; i < 10; i++)
        {
            char ch = data[i + 2];
            filename += (ch >= ' ' && ch <= 0x7f) ? ch : '?';
        }
    }

    // Spectrum header length and type byte?
    if (length == 17 + 2 && data[0] == 0x00)
    {
        // Examine Spectrum file type
        switch (data[1])
        {
        case 0:
        {
            type = "ZX BASIC";

            unsigned int line = (data[15] << 8) | data[14];
            if (line != 0xffff)
                extra = fmt::format(" LINE {}", line);

            break;
        }

        case 1: type = "ZX DATA()"; break;
        case 2: type = "ZX DATA$()"; break;

        case 3:
        {
            type = "ZX CODE";

            unsigned int addr = (data[15] << 8) | data[14];
            unsigned int len = (data[13] << 8) | data[12];
            extra = fmt::format(" {},{}", addr, len);

            break;
        }
        }
    }
    // SAM header length and type byte?
    // Real length is 82, but TZX spec suggests there could be up to 7-8 trailing bits, so accept 83
    else if ((length == 80 + 2 || length == 80 + 1 + 2) && data[0] == 0x01)
    {
        // Examine SAM file type
        switch (data[1])
        {
        case 16:
        {
            type = "BASIC";

            unsigned int line = (data[40] << 8) | data[39];
            if (data[38] == 0)
                extra = fmt::format(" LINE {}", line);

            break;
        }

        case 17: type = "DATA()"; break;
        case 18: type = "DATA$"; break;
        case 19:
        {
            type = "CODE";

            unsigned int addr = TPeek(data + 32) + 16384;
            unsigned int len = TPeek(data + 35);
            extra = fmt::format(" {},{}", addr, len);

            if (data[38] == 0)
                extra += fmt::format(",{}", TPeek(data + 38));

            break;
        }

        case 20:
        {
            type = "SCREEN$";

            unsigned int mode = data[17] + 1;
            extra = fmt::format(" MODE {}", mode);
            break;
        }
        }
    }

    std::stringstream ss;
    if (!type.empty())
    {
        ss << fmt::format("{}: '{}'", type, filename);

        if (!extra.empty())
        {
            ss << " " << extra;
        }
    }
    else
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

            ss << fmt::format("{} bytes", length);
            break;
        }

        case LIBSPECTRUM_TAPE_BLOCK_PURE_DATA:
        case LIBSPECTRUM_TAPE_BLOCK_RAW_DATA:
            ss << fmt::format("{} bytes", libspectrum_tape_block_data_length(block));
            break;

        case LIBSPECTRUM_TAPE_BLOCK_PURE_TONE:
            ss << fmt::format("{} tstates", libspectrum_tape_block_pulse_length(block));
            break;

        case LIBSPECTRUM_TAPE_BLOCK_PULSES:
            ss << fmt::format("{} pulses", libspectrum_tape_block_count(block));
            break;

        case LIBSPECTRUM_TAPE_BLOCK_PAUSE:
            ss << fmt::format("{}ms", libspectrum_tape_block_pause(block));
            break;

        case LIBSPECTRUM_TAPE_BLOCK_GROUP_START:
        case LIBSPECTRUM_TAPE_BLOCK_COMMENT:
        case LIBSPECTRUM_TAPE_BLOCK_MESSAGE:
        case LIBSPECTRUM_TAPE_BLOCK_CUSTOM:
            ss << libspectrum_tape_block_text(block);
            break;

        case LIBSPECTRUM_TAPE_BLOCK_JUMP:
        {
            int offset = libspectrum_tape_block_offset(block);
            if (offset >= 0)
                ss << fmt::format("Forward {} blocks", offset);
            else
                ss << fmt::format("Backward {} blocks", -offset);
            break;
        }

        case LIBSPECTRUM_TAPE_BLOCK_LOOP_START:
            ss << fmt::format("{} iterations", libspectrum_tape_block_count(block));
            break;

        case LIBSPECTRUM_TAPE_BLOCK_SELECT:
            ss << fmt::format("{} options", libspectrum_tape_block_count(block));
            break;

        case LIBSPECTRUM_TAPE_BLOCK_GENERALISED_DATA:
            ss << fmt::format("{} data symbols",
                libspectrum_tape_generalised_data_symbol_table_symbols_in_block(libspectrum_tape_block_data_table(block)));
            break;

        case LIBSPECTRUM_TAPE_BLOCK_ARCHIVE_INFO:
        {
            size_t count = libspectrum_tape_block_count(block);

            for (size_t i = 0; i < count; i++)
            {
                int id = libspectrum_tape_block_ids(block, i);
                auto value = libspectrum_tape_block_texts(block, i);

                // Full title TZX id?
                if (id == 0x00)
                    ss << value;
            }
            break;
        }

        case LIBSPECTRUM_TAPE_BLOCK_HARDWARE:
        {
            size_t count = libspectrum_tape_block_count(block);

            for (size_t i = 0; i < count; i++)
            {
                int type = libspectrum_tape_block_types(block, i);
                int id = libspectrum_tape_block_ids(block, i);

                // Skip anything but the TZX "Computers" type
                if (type != 0)
                    continue;

                // Check for relevant computer ids
                if (id == 9)
                    ss << "SAM Coupe";
                else if ((id >= 0x00 && id <= 0x05) || id == 0x0e)
                    ss << "ZX Spectrum";
                else if (id == 0x08)
                    ss << "Pentagon";
                else if (id == 0x06 || id == 0x07)
                    ss << "Timex Sinclair";
                else
                    ss << fmt::format("Unknown hardware ({:02x})", id);
            }

            break;
        }

        default:
            break;
        }
    }

    return ss.str();
}


void EiHook()
{
    // If we're leaving the ROM tape loader, consider stopping the tape
    if (cpu.get_pc() == 0xe612 /*&& GetOption(tapeauto)*/)
        Stop();
}

bool RetZHook()
{
    // If we're at LDSTRT in ROM1, consider using the loading trap
    if (cpu.get_pc() == 0xe679 && GetSectionPage(Section::D) == ROM1 && GetOption(tapetraps))
        return LoadTrap();

    return false;
}

void InFEHook()
{
    // Are we at the port read in the ROM tape edge routine?
    if (cpu.get_pc() == 0x2053)
    {
        // Ensure the tape is playing
        Play();

        // Traps enabled and accelerating loading speed?
        if (GetOption(tapetraps) && GetOption(turbotape))
        {
            // Fetch the time until the next tape edge
            auto dwTime = GetEventTime(EventType::TapeEdge);

            // Simulate the edge code to advance to the next edge
            // Return to normal processing if C hits 255 (no edge found) or the ear bit has changed
            while (dwTime > 48 && cpu.get_c() < 0xff && !((IO::State().keyboard ^ cpu.get_b()) & KEYBOARD_EAR_MASK))
            {
                cpu.set_c(cpu.get_c() + 1);
                cpu.set_r((cpu.get_r() & 0x80) | ((cpu.get_r() + 7) & 0x7f));
                CPU::frame_cycles += 48;
                dwTime -= 48;
                cpu.set_pc(0x2051);
            }
        }
    }
}


#else // HAVE_LIBSPECTRUM

// Dummy implementations, rather than peppering the above with conditional code

bool IsRecognised(const std::string&) { return false; }
bool IsPlaying() { return false; }
bool IsInserted() { return false; }
std::string GetPath() { return ""; }
std::string GetFile() { return ""; }

bool Insert(const std::string&) { return false; }
void Eject() { }
void Play() { }
void Stop() { }

void NextEdge(uint32_t /*dwTime_*/) { }
bool LoadTrap() { return false; }

void EiHook() { }
bool RetZHook() { return false; }
void InFEHook() { }

#endif // HAVE_LIBSPECTRUM

} // namespace Tape
