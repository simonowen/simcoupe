// Part of SimCoupe - A SAM Coupé emulator
//
// OSD.cpp: Win32 common OS-dependant functions
//
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

#include "SimCoupe.h"

#include <mmsystem.h>

#include "OSD.h"
#include "CPU.h"
#include "Main.h"
#include "Options.h"
#include "UI.h"


HANDLE g_hEvent;
MMRESULT g_hTimer;

int OSD::s_nTicks;


// Timer handler, called every 20ms - seemed more reliable than having it set the event directly, for some weird reason
void CALLBACK TimeCallback (UINT uTimerID_, UINT uMsg_, DWORD dwUser_, DWORD dw1_, DWORD dw2_)
{
    OSD::s_nTicks++;

    // Signal that the next frame is due
    SetEvent(g_hEvent);
}


bool OSD::Init (bool fFirstInit_/*=false*/)
{
    UI::Exit(true);
    TRACE("-> OSD::Init(%s)\n", fFirstInit_ ? "first" : "");

    bool fRet = false;

    // Create an event that will be set every 20ms for the 50Hz sync
    if (!(g_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL)))
        Message(msgWarning, "Failed to create sync event object (%#08lx)", GetLastError());

    // Set a timer to fire every every 20ms for our 50Hz frame synchronisation
    else if (!(g_hTimer = timeSetEvent(1000/EMULATED_FRAMES_PER_SECOND, 0, TimeCallback, 0, TIME_PERIODIC|TIME_CALLBACK_FUNCTION)))
        Message(msgWarning, "Failed to start sync timer (%#08lx)", GetLastError());

    else
        fRet = UI::Init(fFirstInit_);

    TRACE("<- OSD::Init() returning %s\n", fRet ? "true" : "false");
    return fRet;
}

void OSD::Exit (bool fReInit_/*=false*/)
{
    if (g_hEvent)   { CloseHandle(g_hEvent); g_hEvent = NULL; }
    if (g_hTimer)   { timeKillEvent(g_hTimer); g_hTimer = NULL; }

    UI::Exit();
}


// Return an accurate time stamp of type PROFILE_T (__int64 for Win32)
PROFILE_T OSD::GetProfileTime ()
{
    __int64 llNow;

    // Read the current 64-bit time value
    if (QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&llNow)))
        return llNow;

    // If that failed, our only choice is to make it up using the multimedia version
    return 1000i64 * timeGetTime();
}


// Return a time-stamp in milliseconds
// Note: calling could should allow for the value wrapping by only comparing differences
DWORD OSD::GetTime ()
{
    static __int64 llFreq = 0;

    // Need to read the frequency?
    if (!llFreq)
    {
        // If the best high-resolution timer is not available, fall back to the less accurate multimedia timer
        if (!QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&llFreq)))
            return timeGetTime();

        // Convert frequency to millisecond units
        llFreq /= 1000i64;
    }

    // Return the profile time as milliseconds
    return static_cast<DWORD>(GetProfileTime() / llFreq);
}


// Do whatever is necessary to locate an additional SimCoupe file - The Win32 version looks in the
// same directory as the EXE, but other platforms could use an environment variable, etc.
// If the path is already fully qualified (an OS-specific decision), return the same string
const char* OSD::GetFilePath (const char* pcszFile_)
{
    static char szPath[MAX_PATH];

    // If the supplied file path looks absolute, use it as-is
    if (*pcszFile_ == '\\' || strchr(pcszFile_, ':'))
        lstrcpyn(szPath, pcszFile_, sizeof szPath);

    // Form the full path relative to the current EXE file
    else
    {
        // Get the full path of the running module
        GetModuleFileName(__hinstance, szPath, sizeof szPath);

        // Strip the module file and append the supplied file/path
        strrchr(szPath, '\\')[1] = '\0';
        strcat(szPath, pcszFile_);
    }

    // Return a pointer to the new path
    return szPath;
}

void OSD::DebugTrace (const char* pcsz_)
{
    OutputDebugString(pcsz_);
}

int OSD::FrameSync (bool fWait_/*=true*/)
{
    extern LPDIRECTDRAW pdd;

    if (fWait_)
    {
        switch (GetOption(sync))
        {
            case 1:
                ResetEvent(g_hEvent);
                WaitForSingleObject(g_hEvent, INFINITE);
                break;

            case 3:
                pdd->WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, NULL);
                // Fall through...

            case 2:
                pdd->WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, NULL);
                break;
        }
    }

    return s_nTicks;
}

////////////////////////////////////////////////////////////////////////////////

// Win32 lacks a few of the required POSIX functions, so we'll implement them ourselves...

WIN32_FIND_DATA s_fd;
struct dirent s_dir;

DIR* opendir (const char* pcszDir_)
{
    static char szPath[MAX_PATH];

    memset(&s_dir, 0, sizeof s_dir);

    // Append a wildcard to match all files
    lstrcpy(szPath, pcszDir_);
    if (szPath[lstrlen(szPath)-1] != '\\')
        lstrcat(szPath, "\\");
    lstrcat(szPath, "*");

    // Find the first file, saving the details for later
    HANDLE h = FindFirstFile(szPath, &s_fd);

    // Return the handle if successful, otherwise NULL
    return (h == INVALID_HANDLE_VALUE) ? NULL : reinterpret_cast<DIR*>(h);
}

struct dirent* readdir (DIR* hDir_)
{
    // All done?
    if (!s_fd.cFileName[0])
        return NULL;

    // Copy the filename and set the length
    s_dir.d_reclen = lstrlen(lstrcpyn(s_dir.d_name, s_fd.cFileName, sizeof s_dir.d_name));

    // If we'd already reached the end
    if (!FindNextFile(reinterpret_cast<HANDLE>(hDir_), &s_fd))
        s_fd.cFileName[0] = '\0';

    // Return the current entry
    return &s_dir;
}

int closedir (DIR* hDir_)
{
    return FindClose(reinterpret_cast<HANDLE>(hDir_)) ? 0 : -1;
}
