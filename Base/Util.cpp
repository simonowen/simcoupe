// Part of SimCoupe - A SAM Coupé emulator
//
// Util.cpp: Logging, tracing, and other utility tasks
//
//  Copyright (c) 1996-2001  Allan Skillman
//  Copyright (c) 1999-2001  Simon Owen
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
//  - tidy this mess up!
//  - make better use of writing to the log for error conditions to help
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

static FILE* hLogFile = NULL;
static char* s_pszTrace;


bool Util::Init ()
{
    if (!(s_pszTrace = new char[TRACE_BUFFER_SIZE]))
    {
        Message(msgError, "Out of memory!\n");
        return false;
    }

    OpenLog();

    return true;
}


void Util::Exit ()
{
    CloseLog();

    if (s_pszTrace) { delete s_pszTrace; s_pszTrace = NULL; }
}


////////////////////////////////////////////////////////////////////////////////


// Report an info, warning, error or fatal message.  Exit if a fatal message has been reported
void Message (eMsgType eType_, const char* pcszFormat_, ...)
{
    va_list args;
    va_start(args, pcszFormat_);

    const char* pcszType_ = "";
    switch (eType_)
    {
        case msgWarning:    pcszType_ = "warning: ";    break;
        case msgError:      pcszType_ = "error: ";      break;
        case msgInfo:       pcszType_ = "info: ";       break;
        case msgFatal:      pcszType_ = "fatal: ";      break;
    }

    static char szMessage[512];
    strcpy(szMessage, pcszType_);
    char* pszMessage = szMessage + strlen(szMessage);
    vsprintf(pszMessage, pcszFormat_, args);
    strcat(szMessage, "\n");

    // Write to the debugger
    TRACE(szMessage);

    // Write to file, if open
    if (hLogFile)
        fputs(szMessage, hLogFile);

    UI::ShowMessage(eType_, pszMessage);

    // Fatal error?
    if (eType_ == msgFatal)
    {
        // Flush any changed disk data first, then close everything else down and exit
        IO::InitDrives(false, false);
        Main::Exit();
        exit(1);
    }
}

// Log file utilities
bool OpenLog ()
{
    if (!*GetOption(logfile) || (hLogFile = fopen(GetOption(logfile), "w")))
        return true;

    Message(msgError, "can't open log file");
    return false;
}

void CloseLog ()
{
    if (hLogFile)
        fclose(hLogFile);
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

static void TraceOutputString (const char *pcszFormat_, const char *pcvArgs);
static void WriteTimeString (char* psz_);


// Output a formatted debug message
void TraceOutputString (const char *pcszFormat_, ...)
{
    // Get a pointer to the arguments
    va_list pcvArgs;
    va_start (pcvArgs, pcszFormat_);

    TraceOutputString(pcszFormat_, pcvArgs);

    va_end(pcvArgs);
}


// Output a formatted debug message
static void TraceOutputString (const char *pcszFormat_, const char *pcvArgs)
{
    // Prevent a crash if we're called after Exit()
    if (!s_pszTrace)
        return;

    // Write the time value to the start of the output
    WriteTimeString(s_pszTrace);

    // If the format string doesn't require any parameters, don't bother formatting it
    if (!strchr(pcszFormat_, '%'))
        strcat(s_pszTrace, pcszFormat_);

    // Format the string to a buffer, verify that it doesn't overflow and corrupt memory
    else
        vsprintf(s_pszTrace+strlen(s_pszTrace), pcszFormat_, (va_list)pcvArgs);

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
    sprintf(psz_, "%02lu:%02lu.%03lu  %03d:%03d  ", dwMins, dwSecs, dwMillisecs, g_nLine, g_nLineCycle);
}

#endif  // _DEBUG
