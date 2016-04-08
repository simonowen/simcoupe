// Part of SimCoupe - A SAM Coupe emulator
//
// OSD.h: Allegro common "OS-dependant" functions
//
//  Copyright (c) 1999-2011  Simon Owen
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

#ifndef OSD_H
#define OSD_H

// On Windows, ensure Allegro finds the system <io.h> and not our "IO.h"
#ifdef _WINDOWS
#pragma include_alias(<io.h>, <..\Include\IO.h>)
#endif

#ifdef MSDOS
#include "dos.h"
#endif

#include "allegro.h"

#ifdef DEBUGMODE
 #define AL_ASSERT(condition)     { if (!(condition)) al_assert(__FILE__, __LINE__); }
 #define AL_TRACE                 al_trace
#else
 #define AL_ASSERT(condition)
 #define AL_TRACE                 1 ? (void) 0 : al_trace
#endif

#undef TRACE
#undef ASSERT


#include <sys/types.h>      // for _off_t definition

#define PATH_SEPARATOR      OTHER_PATH_SEPARATOR

#ifndef _WINDOWS

#include <dirent.h>

typedef unsigned int        DWORD;  // must be 32-bit
typedef unsigned short      WORD;   // must be 16-bit
typedef unsigned char       BYTE;   // must be 8-bit

#endif


#ifdef _WINDOWS

#include "winalleg.h"
#undef TRACE

#ifdef _DEBUG
#pragma comment(lib, "alleg")
#else
#pragma comment(lib, "alleg")
#endif

#ifdef USE_ZLIB
#pragma comment(lib, "zlib1")   // new 1.2.x version, required to avoid zlib binary mismatch problems
#endif

#pragma warning(disable:4786)   // Disable the stupid warning about debug symbols being truncated

#define strcasecmp  _strcmpi
#define mkdir(p,m)  _mkdir(p)

#define access      _access
#define R_OK        4
#define W_OK        2
#define X_OK        0			// Should be 1, but the VC runtime asserts if we use it!
#define F_OK        0

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

class OSD
{
public:
    static bool Init (bool fFirstInit_=false);
    static void Exit (bool fReInit_=false);

    static DWORD GetTime ();
    static const char* GetFilePath (const char* pcszFile_="");
    static const char* GetDirPath (const char* pcszDir_="");
    static const char* GetFloppyDevice (int nDrive_);
    static bool CheckPathAccess (const char* pcszPath_);
    static bool IsHidden (const char* pcszPath_);

    static void DebugTrace (const char* pcsz_);
    static int FrameSync (bool fWait_=true);

    volatile static int s_nTicks;
};

#endif  // OSD_H
