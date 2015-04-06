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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef OSD_H
#define OSD_H

// disable stupid 'debug symbols being truncated' warning
#pragma warning(disable:4786)

// Enable a few helper warnings in debug mode, without using level 4
#ifdef _DEBUG
#pragma warning(3:4189) // 'identifier' : local variable is initialized but not referenced
#pragma warning(3:4701) // local variable 'name' may be used without having been initialized
#include <crtdbg.h>     // used for tracking heap allocations
#define new new(_NORMAL_BLOCK,__FILE__, __LINE__)   // track allocation locations
#endif

#define NOMINMAX        // no min/max macros from windef.h
#include <windows.h>
#include <windowsx.h>   // for GET_X_LPARAM and GET_Y_LPARAM
#include <winioctl.h>   // for DISK_GEOMETRY and IOCTL_DISK_GET_DRIVE_GEOMETRY
#include <mmsystem.h>   // for timeSetEvent
#include <sys/types.h>  // for _off_t etc.
#include <direct.h>     // for _mkdir
#include <stdio.h>      // for FILE structure
#include <winspool.h>   // for print spooling
#include <commctrl.h>   // for Windows common controls
#include <commdlg.h>    // for Windows common dialogs
#include <cderr.h>      // for common dialog errors
#include <shellapi.h>   // for shell functions (ShellExecute, etc.)
#include <Shlobj.h>     // for shell COM definitions
#include <process.h>    // for _beginthreadex/_endthreadex

#pragma include_alias(<io.h>, <..\Include\IO.h>)
#include <io.h>

#ifdef USE_ZLIB
#ifndef ZLIB_WINAPI
#error ZLIB_WINAPI must be defined for the new WINAPI exports!
#endif
#ifdef USE_ZLIBSTAT
#pragma comment(lib, "zlibstat")	// Use static library
#else
#pragma comment(lib, "zlibwapi")    // Use DLL
#endif // USE_ZLIBSTAT
#endif // USE_ZLIB

#ifdef USE_RESID
#pragma comment(lib, "resid.lib")   // SID chip emulation
#define RESID_NAMESPACE reSID       // use reSID namespace, due to SID symbol clash with winnt.h
#endif

#ifdef USE_CAPSIMAGE
#pragma comment(lib,"capsimg.lib")	// IPF support
#endif

#ifdef USE_LIBSPECTRUM
#pragma comment(lib, "spectrum")    // Tape and snapshot functions
#endif

// For NT4 compatability we only use DX3 features, except for input which requires DX5
#define DIRECTSOUND_VERSION     0x0300
#define DIRECTINPUT_VERSION     0x0500  // we'll do a run-time check for DX5 before using it

#include <dsound.h>
#include <dinput.h>

extern HINSTANCE g_hinstDInput, g_hinstDSound;

typedef HRESULT (WINAPI *PFNDIRECTINPUTCREATE) (HINSTANCE, DWORD, LPDIRECTINPUTA*, LPUNKNOWN);
typedef HRESULT (WINAPI *PFNDIRECTSOUNDCREATE) (LPGUID, LPDIRECTSOUND*, LPUNKNOWN);

extern PFNDIRECTINPUTCREATE pfnDirectInputCreate;
extern PFNDIRECTSOUNDCREATE pfnDirectSoundCreate;

#define PATH_SEPARATOR          '\\'

#define strcasecmp  _strcmpi
#define strncasecmp _strnicmp
#define mkdir(p,m)  _mkdir(p)
#define snprintf	_snprintf

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

DIR* opendir (const char* pcszDir_);
struct dirent* readdir (DIR* hDir_);
int closedir (DIR* hDir_);

////////////////////////////////////////////////////////////////////////////////

enum { MFP_SETTINGS, MFP_INPUT, MFP_OUTPUT, MFP_RESOURCE };

class OSD
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

        static DWORD GetTime ();
        static const char* MakeFilePath (int nDir_, const char* pcszFile_="");
        static const char* GetFloppyDevice (int nDrive_);
        static bool CheckPathAccess (const char* pcszPath_);
        static bool IsHidden (const char* pcszFile_);

        static void DebugTrace (const char* pcsz_);
};

#endif
