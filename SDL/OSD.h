// Part of SimCoupe - A SAM Coupe emulator
//
// OSD.h: SDL common "OS-dependant" functions
//
//  Copyright (c) 1999-2014 Simon Owen
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

#define CUSTOM_MAIN

#include <sys/types.h>      // for _off_t definition
#include <fcntl.h>

#ifdef USE_SDL2
#include "SDL2/SDL.h"
#else
#ifdef __AMIGAOS4__
#include <SDL/SDL.h>
#else
#include "SDL.h"
#endif
#endif
#define SDL

#ifdef __APPLE__
#include <sys/disk.h>       // for DKIOCGETBLOCKCOUNT
#endif

#ifndef _WINDOWS

#include <sys/ioctl.h>
#include <dirent.h>
#include <unistd.h>

#define PATH_SEPARATOR      '/'

typedef unsigned int        DWORD;  // must be 32-bit
#ifndef __AMIGAOS4__
typedef unsigned short      WORD;   // must be 16-bit
typedef unsigned char       BYTE;   // must be 8-bit
#endif

#endif


#ifdef __QNX__
#include <strings.h>        // for strcasecmp
#endif


#ifdef _WINDOWS

#include <windows.h>
#include <direct.h>

#pragma include_alias(<io.h>, <..\Include\IO.h>)
#include <io.h>

#ifdef USE_SDL2
#pragma comment(lib, "sdl2")
#pragma comment(lib, "sdl2main")
#else
#pragma comment(lib, "sdl")
#pragma comment(lib, "sdlmain")
#endif

#ifdef USE_ZLIB
#ifdef _WIN64
#pragma comment(lib, "zlibwapi")   // zlibwapi.lib is the official import library for the x64 version
#else
#pragma comment(lib, "zdll")   // zdll.lib is the official import library for 1.2.x versions
#endif
#endif

#ifdef USE_RESID
#pragma comment(lib, "resid.lib")
#define RESID_NAMESPACE reSID       // use reSID namespace, due to SID symbol clash with winnt.h
#endif

#ifdef USE_LIBSPECTRUM
#pragma comment(lib, "spectrum")    // Tape and snapshot functions
#endif

#pragma warning(disable:4786)   // Disable the stupid warning about debug symbols being truncated

#define PATH_SEPARATOR      '\\'

#define strcasecmp  _strcmpi
#define strncasecmp _strnicmp
#define mkdir(p,m)  _mkdir(p)
#define snprintf    _snprintf
#define lstat       stat
#define ioctl(f,c,x)	-1
#define readlink(p,b,n) -1

#define access      _access
#define R_OK        4
#define W_OK        2
#define X_OK        0			// Should be 1, but the VC runtime asserts if we use it!
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

////////////////////////////////////////////////////////////////////////////////

enum { MFP_SETTINGS, MFP_INPUT, MFP_OUTPUT, MFP_EXE };

class OSD
{
public:
    static bool Init (bool fFirstInit_=false);
    static void Exit (bool fReInit_=false);

    static DWORD GetTime ();
    static const char* MakeFilePath (int nDir_, const char* pcszFile_="");
    static const char* GetFloppyDevice (int nDrive_);
    static bool CheckPathAccess (const char* pcszPath_);
    static bool IsHidden (const char* pcszPath_);

    static void DebugTrace (const char* pcsz_);
};

#endif  // OSD_H
