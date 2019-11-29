// Part of SimCoupe - A SAM Coupe emulator
//
// Util.cpp: Debug tracing, and other utility tasks
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

// Changed 1999-2001 by Simon Owen
//  - Added more comprehensive tracing function with variable arguments

// ToDo:
//  - make better use of writing to a log for error conditions to help
//    troubleshoot any problems in release versions.  We currently rely
//    too much on debug tracing in debug-only versions.

#include "SimCoupe.h"
#include "Util.h"

#include "CPU.h"
#include "Memory.h"
#include "Main.h"
#include "Options.h"
#include "OSD.h"
#include "UI.h"

static const int TRACE_BUFFER_SIZE = 2048;
static char* s_pszTrace;

namespace Util
{

bool Init()
{
    if (!(s_pszTrace = new char[TRACE_BUFFER_SIZE]))
    {
        Message(msgError, "Out of memory!\n");
        return false;
    }

    return true;
}

void Exit()
{
    if (s_pszTrace) { delete[] s_pszTrace; s_pszTrace = nullptr; }
}


char* GetUniqueFile(const char* pcszExt_, char* psz_, int cb_)
{
    char szPath[MAX_PATH];
    struct stat st;

    const char* pcszPath = OSD::MakeFilePath(MFP_OUTPUT);
    int nNext = GetOption(nextfile);

    do {
        snprintf(szPath, MAX_PATH, "%ssimc%04d.%s", pcszPath, nNext++, pcszExt_);
    } while (!::stat(szPath, &st));

    SetOption(nextfile, nNext);

    // Copy the completed string, returning the filename pointer
    strncpy(psz_, szPath, cb_);
    return psz_ + strlen(pcszPath);
}

} // namespace Util

//////////////////////////////////////////////////////////////////////////////

// Report an info, warning, error or fatal message.  Exit if a fatal message has been reported
void Message(eMsgType eType_, const char* pcszFormat_, ...)
{
    va_list args;
    va_start(args, pcszFormat_);

    char sz[512];
    vsnprintf(sz, sizeof(sz) - 1, pcszFormat_, args);
    sz[sizeof(sz) - 1] = '\0';

    va_end(args);

    TRACE("%s\n", sz);
    UI::ShowMessage(eType_, sz);

    // Fatal error?
    if (eType_ == msgFatal)
    {
        Main::Exit();
        exit(1);
    }
}


BYTE GetSizeCode(UINT uSize_)
{
    BYTE bCode = 0;
    for (bCode = 0; uSize_ > 128; bCode++, uSize_ >>= 1);
    return bCode;
}


const char* AbbreviateSize(uint64_t ullSize_)
{
    static const char* pcszUnits = "KMGTPE";

    // Work up from Kilobytes
    auto nUnits = 0;
    ullSize_ /= 1000;

    // Loop while there are more than 1000 and we have another unit to move up to
    while (ullSize_ >= 1000 && pcszUnits[nUnits + 1])
    {
        // Determine the percentage error/loss in the next scaling
        UINT uClipPercent = static_cast<UINT>((ullSize_ % 1000) * 100 / (ullSize_ - (ullSize_ % 1000)));

        // Stop if it's at least 20%
        if (uClipPercent >= 20)
            break;

        // Next unit, rounding to nearest
        nUnits++;
        ullSize_ = (ullSize_ + 500) / 1000;
    }

    static char sz[32] = {};
    snprintf(sz, sizeof(sz) - 1, "%u%cB", static_cast<UINT>(ullSize_), pcszUnits[nUnits]);
    return sz;
}


// CRC-CCITT for id/data checksums, with bit and byte order swapped
WORD CrcBlock(const void* pcv_, size_t uLen_, WORD wCRC_/*=0xffff*/)
{
    static WORD awCRC[256];

    // Build the table if not already built
    if (!awCRC[1])
    {
        for (int i = 0; i < 256; i++)
        {
            WORD w = i << 8;

            // 8 shifts, for each bit in the update byte
            for (int j = 0; j < 8; j++)
                w = (w << 1) ^ ((w & 0x8000) ? 0x1021 : 0);

            awCRC[i] = w;
        }
    }

    // Update the CRC with each byte in the block
    const BYTE* pb = reinterpret_cast<const BYTE*>(pcv_);
    while (uLen_--)
        wCRC_ = (wCRC_ << 8) ^ awCRC[((wCRC_ >> 8) ^ *pb++) & 0xff];

    // Return the updated CRC
    return wCRC_;
}


void PatchBlock(BYTE* pb_, BYTE* pbPatch_)
{
    for (;;)
    {
        // Flag+length in big-endian format
        WORD wLen = (pbPatch_[0] << 8) | pbPatch_[1];
        pbPatch_ += 2;

        // End marker is zero
        if (!wLen)
            break;

        // Top bit clear for skip
        else if (!(wLen & 0x8000))
            pb_ += wLen;

        // Remaining 15 bits for copy length
        else
        {
            wLen &= 0x7fff;
            memcpy(pb_, pbPatch_, wLen);
            pb_ += wLen;
            pbPatch_ += wLen;
        }
    }
}


// SAM ROM triple-peek used for stored addresses
UINT TPeek(const BYTE* pb_)
{
    UINT u = ((pb_[0] & 0x1f) << 14) | ((pb_[2] & 0x3f) << 8) | pb_[1];

    // Clip to 512K
    return (u & ((1U << 19) - 1));
}


void AdjustBrightness(BYTE& r_, BYTE& g_, BYTE& b_, int nAdjust_)
{
    int nOffset = (nAdjust_ <= 0) ? 0 : nAdjust_;
    int nMult = 100 - ((nAdjust_ <= 0) ? -nAdjust_ : nAdjust_);

    r_ = nOffset + (r_ * nMult / 100);
    g_ = nOffset + (g_ * nMult / 100);
    b_ = nOffset + (b_ * nMult / 100);
}

DWORD RGB2Native(BYTE r_, BYTE g_, BYTE b_, DWORD dwRMask_, DWORD dwGMask_, DWORD dwBMask_)
{
    return RGB2Native(r_, g_, b_, 0, dwRMask_, dwGMask_, dwBMask_, 0);
}

DWORD RGB2Native(BYTE r_, BYTE g_, BYTE b_, BYTE a_, DWORD dwRMask_, DWORD dwGMask_, DWORD dwBMask_, DWORD dwAMask_)
{
    DWORD dwRed = static_cast<DWORD>(((static_cast<uint64_t>(dwRMask_)* (r_ + 1)) >> 8)& dwRMask_);
    DWORD dwGreen = static_cast<DWORD>(((static_cast<uint64_t>(dwGMask_)* (g_ + 1)) >> 8)& dwGMask_);
    DWORD dwBlue = static_cast<DWORD>(((static_cast<uint64_t>(dwBMask_)* (b_ + 1)) >> 8)& dwBMask_);
    DWORD dwAlpha = static_cast<DWORD>(((static_cast<uint64_t>(dwAMask_)* (a_ + 1)) >> 8)& dwAMask_);

    return dwRed | dwGreen | dwBlue | dwAlpha;
}

////////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG

void TraceOutputString(const char* /*pcszFormat_*/, ...)
{
}

void TraceOutputString(const BYTE* /*pcb*/, UINT /*uLen=0*/)
{
}

#else

DWORD g_dwStart;

static void TraceOutputString(const char* pcszFormat_, va_list pcvArgs);
static void WriteTimeString(char* psz_);


// Output a formatted debug message
void TraceOutputString(const char* pcszFormat_, ...)
{
    if (!s_pszTrace)
        return;

    va_list pcvArgs;
    va_start(pcvArgs, pcszFormat_);

    TraceOutputString(pcszFormat_, pcvArgs);

    va_end(pcvArgs);
}


// Output a formatted debug message
static void TraceOutputString(const char* pcszFormat_, va_list pcvArgs_)
{
    // Write the time value to the start of the output
    WriteTimeString(s_pszTrace);

    // If the format string doesn't require any parameters, don't bother formatting it
    if (!strchr(pcszFormat_, '%'))
        strcat(s_pszTrace, pcszFormat_);

    // Format the string to a buffer, verify that it doesn't overflow and corrupt memory
    else
        vsprintf(s_pszTrace + strlen(s_pszTrace), pcszFormat_, pcvArgs_);

    // Output the debug message
    OSD::DebugTrace(s_pszTrace);
}


// Output a formatted debug message in hex blocks with ASCII
void TraceOutputString(const BYTE* pcb, size_t uLen/*=0*/)
{
    if (!s_pszTrace)
        return;

    // Write the time value to the start of the output
    WriteTimeString(s_pszTrace);

    // If the length wasn't given or was zero, assume it's a string and use strlen to get the length
    if (!uLen)
        uLen = strlen(reinterpret_cast<const char*>(pcb));

    // Loop while there is still data to process
    while (uLen > 0)
    {
        BYTE* pabASCIIPos = (BYTE*)s_pszTrace + 16 * 3 + 4;
        BYTE* pabLinePos = reinterpret_cast<BYTE*>(s_pszTrace);
        memset(pabLinePos, ' ', 80);

        // Append each hex byte until no more bytes or this line is full.
        for (int i = 0; uLen && i < 16; i++, pcb++, uLen--)
        {
            // Make the data more readable by adding an extra space every 4 bytes
            if (i && !(i % 4))
                *pabLinePos++ = ' ';

            // Store the hex byte
            static BYTE abHexBytes[] = "0123456789ABCDEF";
            *pabLinePos++ = abHexBytes[*pcb >> 4];
            *pabLinePos++ = abHexBytes[*pcb & 0xf];
            *pabLinePos++ = ' ';

            // Store the ASCII character or '.' for out-of-range characters
            *pabASCIIPos++ = (*pcb > 0x1f && *pcb < 0x7f) ? *pcb : '.';

            // Double up '%' character so they're displayed correctly
            if (*pcb == '%')
                *pabASCIIPos++ = '%';
        }

        // Terminate with a newline and a null
        pabLinePos = pabASCIIPos;

        *pabLinePos++ = '\n';
        *pabLinePos = '\0';

        // Output the debug message
        OSD::DebugTrace(s_pszTrace);
    }
}


// Output the time string to the debug device
void WriteTimeString(char* psz_)
{
    // Fetch the current system time in milliseconds
    DWORD dwNow = OSD::GetTime(), dwElapsed = g_dwStart ? dwNow - g_dwStart : dwNow - (g_dwStart = dwNow);

    // Break the elapsed time into seconds, minutes and milliseconds
    DWORD dwMillisecs = dwElapsed % 1000, dwSecs = (dwElapsed /= 1000) % 60, dwMins = (dwElapsed /= 60) % 100;

    DWORD dwScreenCycles = g_dwCycleCounter - BORDER_PIXELS;
    int nLine = dwScreenCycles / TSTATES_PER_LINE, nLineCycle = dwScreenCycles % TSTATES_PER_LINE;

    // Form the time string and send to the debugger
    sprintf(psz_, "%02u:%02u.%03u  %03d:%03d  ", dwMins, dwSecs, dwMillisecs, nLine, nLineCycle);
}

#endif  // _DEBUG
