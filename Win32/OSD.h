// Part of SimCoupe - A SAM Coupe emulator
//
// OSD.h: Win32 common OS-dependant functions
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

#pragma once

// disable stupid 'debug symbols being truncated' warning
#pragma warning(disable:4786)

// Enable a few helper warnings in debug mode, without using level 4
#ifdef _DEBUG
#pragma warning(3:4189) // 'identifier' : local variable is initialized but not referenced
#pragma warning(3:4701) // local variable 'name' may be used without having been initialized
#include <crtdbg.h>     // used for tracking heap allocations
#define new new(_NORMAL_BLOCK,__FILE__, __LINE__)   // track allocation locations
#endif

#include <windows.h>    // TODO: remove to limit type pollution
#include <io.h>         // for _access

#define PATH_SEPARATOR          '\\'

#define strcasecmp  _strcmpi
#define strncasecmp _strnicmp
#define mkdir(p,m)  _mkdir(p)
#define snprintf    _snprintf

#if _MSC_VER > 1200
#define off_t   __int64
#define fseek   _fseeki64
#define fstat   _fstat64
#define stat    _stat64
#endif

#define R_OK        4
#define W_OK        2
#define X_OK        0   // Should be 1, but the VC runtime asserts if we use it!
#define F_OK        0

#define access(p,m)      _access((p),(m)&(R_OK|W_OK))

#define _S_ISTYPE(mode,mask)    (((mode) & _S_IFMT) == (mask))
#define S_ISDIR(mode)           _S_ISTYPE((mode), _S_IFDIR)
#define S_ISREG(mode)           _S_ISTYPE((mode), _S_IFREG)
#define S_ISBLK(mode)           0
#define S_ISLNK(mode)           0

// Windows lacks direct.h, so we'll supply our own
struct dirent
{
    long            d_ino;
    _off_t          d_off;
    unsigned short  d_reclen;
    char            d_name[256];
};

typedef void* DIR;

DIR* opendir(const char* pcszDir_);
struct dirent* readdir(DIR* hDir_);
int closedir(DIR* hDir_);

////////////////////////////////////////////////////////////////////////////////

enum { MFP_SETTINGS, MFP_INPUT, MFP_OUTPUT, MFP_RESOURCE };

class OSD
{
public:
    static bool Init(bool fFirstInit_ = false);
    static void Exit(bool fReInit_ = false);

    static uint32_t GetTime();
    static const char* MakeFilePath(int nDir_, const char* pcszFile_ = "");
    static const char* GetFloppyDevice(int nDrive_);
    static bool CheckPathAccess(const char* pcszPath_);
    static bool IsHidden(const char* pcszFile_);

    static void DebugTrace(const char* pcsz_);
};
