// Part of SimCoupe - A SAM Coupé emulator
//
// OSD.h: Win32 common OS-dependant functions
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

// We can do more accurate profiling than the default
#define PROFILE_T   __int64
#define AddTime(x)  sprintf(sz + strlen(sz), "  %s:%I64dus", #x, (s_sProfile.prof##x + 5) / 10i64)


class OSD
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

        static PROFILE_T GetProfileTime ();
        static DWORD GetTime ();
        static const char* GetFilePath (const char* pcszFile_);
        static void DebugTrace (const char* pcsz_);
        static int FrameSync (bool fWait_=true);

        static int s_nTicks;
};

////////////////////////////////////////////////////////////////////////////////


#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmsystem.h>
#include <sys\types.h>  // for _off_t etc.
#include <direct.h>     // for _mkdir

#pragma warning(disable:4786)   // disable stupid 'debug symbols being truncated' warning

#ifndef NO_ZLIB
#define ZLIB_DLL
#pragma comment(lib, "Zlib")
#endif


// For NT4 compatability we need to ensure than only DX3 features are used
#define DIRECTDRAW_VERSION      0x0300
#define DIRECT3D_VERSION        0x0300
#define DIRECTSOUND_VERSION     0x0300
#define DIRECTINPUT_VERSION     0x0500  // DX5 is needed joystick support (since NT4 SP3 only has DX3, we'll do a run-time check)

#include <dsound.h>
#include <ddraw.h>

#define strcasecmp      _strcmpi
#define mkdir(p,m)  _mkdir(p)

#define PATH_SEPARATOR          "\\"

// Windows doesn't support dirent.h, so we'll provide our own version
struct dirent
{
    long            d_ino;
    _off_t          d_off;
    unsigned short  d_reclen;
    char            d_name[256];
};

typedef void* DIR;

DIR* opendir (const char* pcszDir_);
struct dirent* readdir (DIR* hDir_);
int closedir (DIR* hDir_);

#endif
