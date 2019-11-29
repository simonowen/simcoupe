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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#pragma once

#include <sys/types.h>      // for _off_t definition
#include <fcntl.h>

#ifdef HAVE_LIBSDL2
#include "SDL2/SDL.h"
#endif

#ifdef HAVE_LIBSDL
#include "SDL/SDL.h"
#endif

#ifdef __APPLE__
#include <sys/disk.h>       // for DKIOCGETBLOCKCOUNT
#define main SimCoupe_main  // rename main() so Cocoa can use it
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

#define NOMINMAX        // no min/max macros from windef.h
#define SID WIN32_SID   // TODO: limit scope of windows.h avoid SID symbol clash
#include <windows.h>
#undef SID

#include <direct.h>
#include <io.h>

#pragma warning(disable:4786)   // Disable the stupid warning about debug symbols being truncated

#define PATH_SEPARATOR      '\\'

#define strcasecmp  _strcmpi
#define strncasecmp _strnicmp
#define mkdir(p,m)  _mkdir(p)
#define snprintf    _snprintf
#define lstat       stat
#define ioctl(f,c,x)    -1
#define readlink(p,b,n) -1

#define access      _access
#define R_OK        4
#define W_OK        2
#define X_OK        0           // Should be 1, but the VC runtime asserts if we use it!
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

DIR* opendir(const char* pcszDir_);
struct dirent* readdir(DIR* hDir_);
int closedir(DIR* hDir_);

#endif  // _WINDOWS

////////////////////////////////////////////////////////////////////////////////

enum { MFP_SETTINGS, MFP_INPUT, MFP_OUTPUT, MFP_RESOURCE };

class OSD
{
public:
    static bool Init(bool fFirstInit_ = false);
    static void Exit(bool fReInit_ = false);

    static DWORD GetTime();
    static const char* MakeFilePath(int nDir_, const char* pcszFile_ = "");
    static const char* GetFloppyDevice(int nDrive_);
    static bool CheckPathAccess(const char* pcszPath_);
    static bool IsHidden(const char* pcszPath_);

    static void DebugTrace(const char* pcsz_);
};
