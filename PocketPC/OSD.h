// Part of SimCoupe - A SAM Coupe emulator
//
// OSD.h: WinCE OS-dependant routines
//
//  Copyright (c) 1999-2006  Simon Owen
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

inline void _ASSERTE(bool) { }

#define USE_LOWRES
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <windowsx.h>
#include <atlconv.h>
#include <aygshell.h>
#include <sipapi.h>
#include <mmsystem.h>
#include <commdlg.h>
#include <shellapi.h>
#include <Prsht.h>      // needed for PROPSHEETPAGE under MIPS/SH3
#include <commctrl.h>   // needed for COMCTL32_VERSION and TCS_BOTTOM under MIPS/SH3

#ifndef GX_H
#include <gx.h>         // GAPI header file
#define GX_H
#endif

#pragma warning(disable:4244)

extern HINSTANCE __hinstance;


#define PATH_SEPARATOR          '\\'

#define strcasecmp(a,b) _memicmp(a,b,strlen(a))
#define strdup          _strdup

#define access(a,b) 0       // Permit anything
#define R_OK        4
#define W_OK        2
#define X_OK        1
#define F_OK        0

#define _S_IFMT     0170000         // file type mask
#define _S_IFDIR    0040000         // directory
#define _S_IFREG    0100000         // regular

#define _S_ISTYPE(mode,mask)    (((mode) & _S_IFMT) == (mask))
#define S_ISDIR(mode)           _S_ISTYPE((mode), _S_IFDIR)
#define S_ISREG(mode)           _S_ISTYPE((mode), _S_IFREG)
#define S_ISBLK(mode)           0
#define S_ISLNK(mode)           0


// Windows doesn't seem to support dirent.h, so we'll provide our own version
struct dirent
{
    LONG    d_ino;
    LONG    d_off;
    WORD    d_reclen;
    char    d_name[256];
};

typedef HANDLE  DIR;

struct stat
{
    WORD st_mode;
    DWORD st_size;
};

struct tm
{
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};


DIR* opendir (const char* pcszDir_);
struct dirent* readdir (DIR* hDir_);
int closedir (DIR* hDir_);

void unlink (const char* pcsz_);

int stat (const char* psz_, struct stat* pst_);

time_t time(time_t* pt_);
struct tm * localtime (const time_t *);
time_t mktime (struct tm*);

// We need true 64-bit values to calculate C-time values
#define ULONGLONG   __int64

////////////////////////////////////////////////////////////////////////////////

// PockerPC can do more accurate profiling than the default
#define PROFILE_T   __int64
#define AddTime(x)  sprintf(sz + strlen(sz), "  %s:%I64dus", #x, (s_sProfile.prof##x + 5) / 10i64)

class OSD
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

        static PROFILE_T GetProfileTime ();
        static DWORD GetTime ();
        static const char* GetFilePath (const char* pcszFile_="");
        static const char* GetDirPath (const char* pcszDir_="");
        static const char* GetFloppyDevice (int nDrive_);
        static bool CheckPathAccess (const char* pcszPath_);
        static bool IsHidden (const char* pcszFile_);

        static void DebugTrace (const char* pcsz_);
        static int FrameSync (bool fWait_=true);

        static void OnTimer ();
        static int s_nTicks;
};

#endif
