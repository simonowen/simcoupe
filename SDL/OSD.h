// Part of SimCoupe - A SAM Coupé emulator
//
// OSD.h: SDL common "OS-dependant" functions
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

#ifndef OSD_H
#define OSD_H

// There's no SDL method to get a time stamp better than 1 millisecond yet
#define GetProfileTime      GetTime


namespace OSD
{
    bool Init ();
    void Exit ();

    DWORD GetTime ();
    const char* GetFilePath (const char* pcszFile_);
    void DebugTrace (const char* pcsz_);
    bool FrameSync (bool fWait_=true);
};

////////////////////////////////////////////////////////////////////////////////

#include <sys/types.h>      // for _off_t definition
#include "SDL.h"
#define SDL

#ifndef _WINDOWS
#include <dirent.h>
#endif


#ifdef _WINDOWS

#include <windows.h>
#include <direct.h>

#pragma comment(lib, "sdl")
#pragma comment(lib, "sdlmain")

#ifndef NO_ZLIB
#define ZLIB_DLL
#pragma comment(lib, "zlib")
#endif

#pragma warning(disable:4786)   // Disable the stupid warning about debug symbols being truncated

#define strcasecmp  _strcmpi
#define mkdir(p,m)  _mkdir(p)



// Windows lacks direct.h, so we'll supply our own
struct dirent
{
    long    d_ino;
    _off_t  d_off;
    unsigned short d_reclen;
    char    d_name[256];
};

typedef HANDLE  DIR;

DIR* opendir (const char* pcszDir_);
struct dirent* readdir (DIR* hDir_);
int closedir (DIR* hDir_);

#endif  // _WINDOWS

#endif  // OSD_H
