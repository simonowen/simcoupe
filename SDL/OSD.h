// Part of SimCoupe - A SAM Coupe emulator
//
// OSD.h: SDL common "OS-dependant" functions
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

#ifndef OSD_H
#define OSD_H

// There's no SDL method to get a time stamp better than 1 millisecond yet
#define GetProfileTime      GetTime


class OSD
{
public:
    static bool Init (bool fFirstInit_=false);
    static void Exit (bool fReInit_=false);

    static DWORD GetTime ();
    static const char* GetFilePath (const char* pcszFile_="");
    static const char* GetFloppyDevice (int nDrive_);
    static bool IsHidden (const char* pcszPath_);

    static void DebugTrace (const char* pcsz_);
    static int FrameSync (bool fWait_=true);

    static int s_nTicks;
};


////////////////////////////////////////////////////////////////////////////////

#include <sys/types.h>      // for _off_t definition
#include <fcntl.h>
#include "SDL.h"
#define SDL

#ifndef SDL_DISABLE
#define SDL_DISABLE  0
#define SDL_ENABLE   1
#endif

#ifndef SDL_VIDEOEXPOSE
#define SDL_VIDEOEXPOSE  17
#endif


#ifndef _WINDOWS
#include <sys/ioctl.h>
#include <dirent.h>
#include <unistd.h>
#define PATH_SEPARATOR      '/'
#endif

#ifdef __QNX__
#include <strings.h>        // for strcasecmp
#endif

#ifdef _WINDOWS

#include <windows.h>
#include <direct.h>

#pragma include_alias(<io.h>, <..\Include\IO.h>)
#include <io.h>

#pragma comment(lib, "sdl")
#pragma comment(lib, "sdlmain")

#ifdef USE_ZLIB
#define ZLIB_DLL
#pragma comment(lib, "zlib")
#endif

#ifdef USE_SAASOUND
#pragma comment(lib, "SAASound")
#endif

#ifdef USE_OPENGL
#pragma comment(lib, "OpenGL32.lib")
#endif

#pragma warning(disable:4786)   // Disable the stupid warning about debug symbols being truncated

#define PATH_SEPARATOR      '\\'

#define strcasecmp  _strcmpi
#define mkdir(p,m)  _mkdir(p)
#define lstat       stat
#define ioctl(f,c,x)
#define readlink(p,b,n) -1

#define access      _access
#define R_OK        4
#define W_OK        2
#define X_OK        1
#define F_OK        0

#define O_NONBLOCK  0           // Normally 04000, but not needed

#define _S_ISTYPE(mode,mask)    (((mode) & _S_IFMT) == (mask))
#define S_ISDIR(mode)           _S_ISTYPE((mode), _S_IFDIR)
#define S_ISREG(mode)           _S_ISTYPE((mode), _S_IFREG)
#define S_ISBLK(mode)           0
#define S_ISLNK(mode)           0


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
