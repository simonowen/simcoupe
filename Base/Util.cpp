// Part of SimCoupe - A SAM Coupe emulator
//
// Util.cpp: Debug tracing, and other utility tasks
//
//  Copyright (c) 1999-2010  Simon Owen
//  Copyright (c) 1996-2002  Allan Skillman
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


bool Util::Init ()
{
    if (!(s_pszTrace = new char[TRACE_BUFFER_SIZE]))
    {
        Message(msgError, "Out of memory!\n");
        return false;
    }

    return true;
}

void Util::Exit ()
{
    if (s_pszTrace) { delete[] s_pszTrace; s_pszTrace = NULL; }
}


int Util::GetUniqueFile (const char* pcszTemplate_, int nNext_, char* psz_, int cb_)
{
    char sz[MAX_PATH];
    struct stat st;

    do {
        sprintf(sz, pcszTemplate_, nNext_++);
    } while (!::stat(sz, &st));

    strncpy(psz_, sz, cb_);
    return nNext_;
}


UINT Util::HCF (UINT x_, UINT y_)
{
    UINT uHCF = 1, uMin = min(x_, y_) >> 1;

    for (UINT uFactor = 2 ; uFactor <= uMin ; uFactor++)
    {
        while (!(x_ % uFactor) && !(y_ % uFactor))
        {
            uHCF *= uFactor;
            x_ /= uFactor;
            y_ /= uFactor;
        }
    }

    return uHCF;
}

// Report an info, warning, error or fatal message.  Exit if a fatal message has been reported
void Message (eMsgType eType_, const char* pcszFormat_, ...)
{
    va_list args;
    va_start(args, pcszFormat_);

    char szMessage[512];
    vsprintf(szMessage, pcszFormat_, args);

    TRACE("%s\n", szMessage);
    UI::ShowMessage(eType_, szMessage);

    // Fatal error?
    if (eType_ == msgFatal)
    {
        // Flush any changed disk data first, then close everything else down and exit
        IO::InitDrives(false, false);
        Main::Exit();
        exit(1);
    }
}


BYTE GetSizeCode (UINT uSize_)
{
    BYTE bCode;
    for (bCode = 0 ; uSize_ > 128 ; bCode++, uSize_ >>= 1);
    return bCode;
}


void AdjustBrightness (BYTE &r_, BYTE &g_, BYTE &b_, int nAdjust_)
{
    int nOffset = (nAdjust_ <= 0) ? 0 : nAdjust_;
    int nMult = 100 - ((nAdjust_ <= 0) ? -nAdjust_ : nAdjust_);

    r_ = nOffset + (r_ * nMult / 100);
    g_ = nOffset + (g_ * nMult / 100);
    b_ = nOffset + (b_ * nMult / 100);
}

void RGB2YUV (BYTE r_, BYTE g_, BYTE b_, BYTE *py_, BYTE *pu_, BYTE *pv_)
{
    *py_ = (unsigned char)( ( (  66 * r_ + 129 * g_ +  25 * b_ + 128) >> 8)  +  16 );
    *pu_ = (unsigned char)( ( ( -38 * r_ -  74 * g_ + 112 * b_ + 128) >> 8)  + 128 );
    *pv_ = (unsigned char)( ( ( 112 * r_ -  94 * g_ -  18 * b_ + 128) >> 8)  + 128 );
}

DWORD RGB2Native (BYTE r_, BYTE g_, BYTE b_, DWORD dwRMask_, DWORD dwGMask_, DWORD dwBMask_)
{
    return RGB2Native(r_,g_,b_,0, dwRMask_,dwGMask_,dwBMask_,0);
}

DWORD RGB2Native (BYTE r_, BYTE g_, BYTE b_, BYTE a_, DWORD dwRMask_, DWORD dwGMask_, DWORD dwBMask_, DWORD dwAMask_)
{
    DWORD dwRed   = static_cast<DWORD>(((static_cast<ULONGLONG>(dwRMask_) * (r_+1)) >> 8) & dwRMask_);
    DWORD dwGreen = static_cast<DWORD>(((static_cast<ULONGLONG>(dwGMask_) * (g_+1)) >> 8) & dwGMask_);
    DWORD dwBlue  = static_cast<DWORD>(((static_cast<ULONGLONG>(dwBMask_) * (b_+1)) >> 8) & dwBMask_);
    DWORD dwAlpha = static_cast<DWORD>(((static_cast<ULONGLONG>(dwAMask_) * (a_+1)) >> 8) & dwAMask_);

    return dwRed | dwGreen | dwBlue | dwAlpha;
}

////////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG

void TraceOutputString (const char *pcszFormat_, ...)
{
}

void TraceOutputString (const BYTE *pcb, UINT uLen/*=0*/)
{
}

#else

DWORD g_dwStart;

static void TraceOutputString (const char *pcszFormat_, va_list pcvArgs);
static void WriteTimeString (char* psz_);


// Output a formatted debug message
void TraceOutputString (const char *pcszFormat_, ...)
{
    if (!s_pszTrace)
        return;

    va_list pcvArgs;
    va_start (pcvArgs, pcszFormat_);

    TraceOutputString(pcszFormat_, pcvArgs);

    va_end(pcvArgs);
}


// Output a formatted debug message
static void TraceOutputString (const char *pcszFormat_, va_list pcvArgs_)
{
    // Write the time value to the start of the output
    WriteTimeString(s_pszTrace);

    // If the format string doesn't require any parameters, don't bother formatting it
    if (!strchr(pcszFormat_, '%'))
        strcat(s_pszTrace, pcszFormat_);

    // Format the string to a buffer, verify that it doesn't overflow and corrupt memory
    else
        vsprintf(s_pszTrace+strlen(s_pszTrace), pcszFormat_, pcvArgs_);

    // Output the debug message
    OSD::DebugTrace(s_pszTrace);
}


// Output a formatted debug message in hex blocks with ASCII
void TraceOutputString (const BYTE *pcb, size_t uLen/*=0*/)
{
    if (!s_pszTrace)
        return;

    // Write the time value to the start of the output
    WriteTimeString(s_pszTrace);

    // If the length wasn't given or was zero, assume it's a string and use strlen to get the length
    if (!uLen)
        uLen = strlen(reinterpret_cast<const char *>(pcb));

    // Loop while there is still data to process
    while (uLen > 0)
    {
        BYTE *pabASCIIPos = (BYTE*)s_pszTrace + 16*3+4;
        BYTE *pabLinePos = reinterpret_cast<BYTE*>(s_pszTrace);
        memset(pabLinePos, ' ', 80);

        // Append each hex byte until no more bytes or this line is full.
        for (int i = 0 ; uLen && i < 16 ; i++, pcb++, uLen--)
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
        *pabLinePos   = '\0';

        // Output the debug message
        OSD::DebugTrace(s_pszTrace);
    }
}


// Output the time string to the debug device
void WriteTimeString (char* psz_)
{
    // Fetch the current system time in milliseconds
    DWORD dwNow = OSD::GetTime(), dwElapsed = g_dwStart ? dwNow-g_dwStart : dwNow-(g_dwStart=dwNow);

    // Break the elapsed time into seconds, minutes and milliseconds
    DWORD dwMillisecs = dwElapsed % 1000, dwSecs = (dwElapsed /= 1000) % 60, dwMins = (dwElapsed /= 60) % 100;

    // Form the time string and send to the debugger
    sprintf(psz_, "%02u:%02u.%03u  %03d:%03d  ", dwMins, dwSecs, dwMillisecs, g_nLine, g_nLineCycle);
}

#endif  // _DEBUG
