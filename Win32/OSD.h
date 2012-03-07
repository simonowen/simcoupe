// Part of SimCoupe - A SAM Coupe emulator
//
// OSD.h: Win32 common OS-dependant functions
//
//  Copyright (c) 1999-2012 Simon Owen
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

#include <windows.h>
#include <windowsx.h>   // for GET_X_LPARAM and GET_Y_LPARAM
#include <mmsystem.h>   // for timeSetEvent
#include <sys/types.h>  // for _off_t etc.
#include <direct.h>     // for _mkdir
#include <stdio.h>      // for FILE structure
#include <winioctl.h>   // for DISK_GEOMETRY and IOCTL_DISK_GET_DRIVE_GEOMETRY
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
#pragma comment(lib, "zlibwapi")    // Note: ZLIB_WINAPI must be defined for the new WINAPI exports
#endif

#ifdef USE_RESID
#pragma comment(lib, "resid.lib")   // SID chip emulation
#endif

#ifdef USE_CAPSIMAGE
#pragma comment(lib,"capsimg.lib")	// IPF support
#endif

// Check delay-load DLLs for optional features
#define CheckLibFunction(lib,func)  (LoadLibrary(#lib) != NULL)

// For NT4 compatability we only use DX3 features, except for input which requires DX5
#define DIRECTDRAW_VERSION      0x0300
#define DIRECT3D_VERSION        0x0300
#define DIRECTSOUND_VERSION     0x0300
#define DIRECTINPUT_VERSION     0x0500  // we'll do a run-time check for DX5 before using it

#include <dsound.h>
#include <ddraw.h>
#include <dinput.h>

extern HINSTANCE g_hinstDDraw, g_hinstDInput, g_hinstDSound;

typedef HRESULT (WINAPI *PFNDIRECTDRAWCREATE)(GUID*,LPDIRECTDRAW*,IUnknown*);
typedef HRESULT (WINAPI *PFNDIRECTINPUTCREATE) (HINSTANCE, DWORD, LPDIRECTINPUTA*, LPUNKNOWN);
typedef HRESULT (WINAPI *PFNDIRECTSOUNDCREATE) (LPGUID, LPDIRECTSOUND*, LPUNKNOWN);

extern PFNDIRECTDRAWCREATE pfnDirectDrawCreate;
extern PFNDIRECTINPUTCREATE pfnDirectInputCreate;
extern PFNDIRECTSOUNDCREATE pfnDirectSoundCreate;

// Make sure the DX5 (or higher) SDK was found
#ifndef DSBLOCK_ENTIREBUFFER
#error DX5 (or higher) SDK is required to build SimCoupe
#endif

//#define NODEFAULT     __assume(0)     // ToDo: find out why this doesn't work

#define PATH_SEPARATOR          '\\'

#define strcasecmp  _strcmpi
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

// VC6 or an old SDK will mean some symbols are missing or different, so define them
#if _MSC_VER <= 1200
typedef long LONG_PTR, *PLONG_PTR;
typedef unsigned long ULONG_PTR, *PULONG_PTR;
typedef DWORD DWORD_PTR, *PDWORD_PTR;
#define INT_PTR int
#define SetWindowLongPtr SetWindowLong
#define DWLP_MSGRESULT DWL_MSGRESULT
#define GWLP_WNDPROC GWL_WNDPROC
#endif

#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC         0
#endif

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL               0x020a
#define GET_WHEEL_DELTA_WPARAM(w)   ((short)HIWORD(w))
#endif

// From winioctl.h, since they're W2K+ only
#ifndef IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS

#define IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS    CTL_CODE(IOCTL_VOLUME_BASE, 0, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _DISK_EXTENT {
    DWORD           DiskNumber;
    LARGE_INTEGER   StartingOffset;
    LARGE_INTEGER   ExtentLength;
} DISK_EXTENT, *PDISK_EXTENT;

typedef struct _VOLUME_DISK_EXTENTS {
    DWORD       NumberOfDiskExtents;
    DISK_EXTENT Extents[1];
} VOLUME_DISK_EXTENTS, *PVOLUME_DISK_EXTENTS;

#endif


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
        static bool IsHidden (const char* pcszFile_);

        static void DebugTrace (const char* pcsz_);
        static void FrameSync ();
};

#endif
