// Part of SimCoupe - A SAM Coupe emulator
//
// OSD.cpp: Allegro common "OS-dependant" functions
//
//  Copyright (c) 1999-2002  Simon Owen
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
#include "OSD.h"

#include "CPU.h"
#include "Main.h"
#include "Options.h"

#ifndef _DEBUG
#include <signal.h>
#endif

volatile int OSD::s_nTicks;
volatile DWORD dwTime;

void TimerCallback ()
{
    OSD::s_nTicks++;
    dwTime += (1000/EMULATED_FRAMES_PER_SECOND);
}

END_OF_FUNCTION(TimerCallback);


bool OSD::Init (bool fFirstInit_/*=false*/)
{
    bool fRet = false;

#ifndef _DEBUG
    // Ignore Ctrl-C and Ctrl-Break in release modes
    signal(SIGINT, SIG_IGN);
#endif

    LOCK_VARIABLE(OSD::s_nTicks);
    LOCK_FUNCTION((void*)TimerCallback);

    if (fFirstInit_)
        allegro_init();

    install_int_ex(TimerCallback, BPS_TO_TIMER(EMULATED_FRAMES_PER_SECOND));

    fRet = true;
    return fRet;
}

void OSD::Exit (bool fReInit_/*=false*/)
{
    remove_int(TimerCallback);

    if (!fReInit_)
        allegro_exit();
}


// This is supposed to return a milli-second accurate value, but Allegro doesn't support that
// Instead we'll return one accurate to 20ms, which is good enough for basic timing
DWORD OSD::GetTime ()
{
    return dwTime;
}

// We certainly don't support higher resolution timing!
PROFILE_T OSD::GetProfileTime ()
{
    // Returning zero will limit the profiler to displaying speed and FPS only
    return 0;
}


// Do whatever is necessary to locate an additional SimCoupe file - The Win32 version looks in the
// same directory as the EXE, but other platforms could use an environment variable, etc.
// If the path is already fully qualified (an OS-specific decision), return the same string
const char* OSD::GetFilePath (const char* pcszFile_/*=""*/)
{
    static char szPath[512];

    // If the supplied file path looks absolute, use it as-is
    if (*pcszFile_ == '/'
#ifdef _WINDOWS
        || strchr(pcszFile_, ':')
#endif
        )
        strncpy(szPath, pcszFile_, sizeof szPath);

    // Form the full path relative to the current EXE file
    else
    {
        // Get the full path of the running module

#if defined(_WINDOWS)
        // Strip the module file and append the supplied file/path
        GetModuleFileName(NULL, szPath, sizeof szPath);
        strrchr(szPath, '\\')[1] = '\0';
#else
        szPath[0] = '\0';
#endif

        // Append the supplied file/path
        strcat(szPath, pcszFile_);
    }

    // Return a pointer to the new path
    return szPath;
}

// Return whether a file/directory is normally hidden from a directory listing
bool OSD::IsHidden (const char* pcszPath_)
{
#if defined(ALLEGRO_DOS)
    // Hide entries with the hidden or system attribute bits set
    UINT uAttrs = 0;
    return !_dos_getfileattr(pcszPath_, &uAttrs) && (uAttrs & (_A_HIDDEN|_A_SYSTEM));
#elif defined(ALLEGRO_WINDOWS)
    // Hide entries with the hidden or system attribute bits set
    DWORD dwAttrs = GetFileAttributes(pcszPath_);
    return (dwAttrs != 0xffffffff) && (dwAttrs & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM));
#else
    // Hide entries beginning with a dot
    pcszPath_ = strrchr(pcszPath_, PATH_SEPARATOR);
    return pcszPath_ && pcszPath_[1] == '.';
#endif
}

// Return the path to use for a given drive with direct floppy access
const char* OSD::GetFloppyDevice (int nDrive_)
{
#if defined(ALLEGRO_DOS) || defined(ALLEGRO_WINDOWS)
    static char szDevice[] = "_:";
    szDevice[0] = 'A' + nDrive_-1;
    return szDevice;
#else
    static char szDevice[] = "/dev/fd_";
    szDevice[7] = '0' + nDrive_-1;
    return szDevice;
#endif
}

void OSD::DebugTrace (const char* pcsz_)
{
#ifdef _WINDOWS
    OutputDebugString(pcsz_);
#endif
}

int OSD::FrameSync (bool fWait_/*=true*/)
{
    if (fWait_)
        for (int nCurrent = s_nTicks ; s_nTicks == nCurrent ; );

    return s_nTicks;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef _WINDOWS

WIN32_FIND_DATA s_fd;
struct dirent s_dir;

DIR* opendir (const char* pcszDir_)
{
    static char szPath[_MAX_PATH];

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

#endif  // _WINDOWS


// Allegro may need to do some magic of its own regarding main()
extern int main (int argc_, char* argv_[]);
END_OF_MAIN();
