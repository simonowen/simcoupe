// Part of SimCoupe - A SAM Coupe emulator
//
// OSD.cpp: WinCE OS-dependant routines
//
//  Copyright (c) 1999-2006 Simon Owen
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

#include "SimCoupe.h"
#include <atlconv.h>

#include "OSD.h"

#include "CPU.h"
#include "Frame.h"
#include "Main.h"
#include "Options.h"
#include "Parallel.h"
#include "UI.h"

int OSD::s_nTicks;
HINSTANCE hinstGAPI;

GXOPENDISPLAYPROC GXOpenDisplay;
GXCLOSEDISPLAYPROC GXCloseDisplay;
GXBEGINDRAWPROC GXBeginDraw;
GXENDDRAWPROC GXEndDraw;
GXOPENINPUTPROC GXOpenInput;
GXCLOSEINPUTPROC GXCloseInput;
GXGETDISPLAYPROPERTIESPROC GXGetDisplayProperties;
GXGETDEFAULTKEYSPROC GXGetDefaultKeys;
GXSUSPENDPROC GXSuspend;
GXRESUMEPROC GXResume;
GXSETVIEWPORTPROC GXSetViewport;
GXISDISPLAYDRAMBUFFERPROC GXIsDisplayDRAMBuffer;

#define GAPI_BIND(pfn,type,func)	pfn = reinterpret_cast<type>(GetProcAddress(hinstGAPI, _T(func)))


bool OSD::Init (bool fFirstInit_/*=false*/)
{
    UI::Exit(true);
    TRACE("-> OSD::Init(%s)\n", fFirstInit_ ? "first" : "");

    if (fFirstInit_)
    {
        // Load official GAPI, then local GAPI, falling back on the GAPI emulator as a last resort
        if (!(hinstGAPI = LoadLibrary(_T("\\Windows\\gx.dll")))
         && !(hinstGAPI = LoadLibrary(_T("gx.dll")))
         && !(hinstGAPI = LoadLibrary(_T("gapi_emu.dll"))))
        {
            UI::ShowMessage(msgError, "GAPI (gx.dll) not installed!\n\nSee FAQ for details.");
            return false;
        }

        GAPI_BIND(GXOpenDisplay, GXOPENDISPLAYPROC, "?GXOpenDisplay@@YAHPAUHWND__@@K@Z");
        GAPI_BIND(GXCloseDisplay, GXCLOSEDISPLAYPROC, "?GXCloseDisplay@@YAHXZ");
        GAPI_BIND(GXBeginDraw, GXBEGINDRAWPROC, "?GXBeginDraw@@YAPAXXZ");
        GAPI_BIND(GXEndDraw, GXENDDRAWPROC, "?GXEndDraw@@YAHXZ");
        GAPI_BIND(GXOpenInput, GXOPENINPUTPROC, "?GXOpenInput@@YAHXZ");
        GAPI_BIND(GXCloseInput, GXCLOSEINPUTPROC, "?GXCloseInput@@YAHXZ");
        GAPI_BIND(GXGetDisplayProperties, GXGETDISPLAYPROPERTIESPROC, "?GXGetDisplayProperties@@YA?AUGXDisplayProperties@@XZ");
        GAPI_BIND(GXGetDefaultKeys, GXGETDEFAULTKEYSPROC, "?GXGetDefaultKeys@@YA?AUGXKeyList@@H@Z");
        GAPI_BIND(GXSuspend, GXSUSPENDPROC, "?GXSuspend@@YAHXZ");
        GAPI_BIND(GXResume, GXRESUMEPROC, "?GXResume@@YAHXZ");
        GAPI_BIND(GXSetViewport, GXSETVIEWPORTPROC, "?GXSetViewport@@YAHKKKK@Z");
        GAPI_BIND(GXIsDisplayDRAMBuffer, GXISDISPLAYDRAMBUFFERPROC, "?GXIsDisplayDRAMBuffer@@YAHXZ");

        // Reject the DLL if the 2 main entry/exit functions don't exist
        if (!GXOpenDisplay || !GXCloseDisplay)
        {
            UI::ShowMessage(msgError, "Invalid GAPI (gx.dll) found!\n\nPlease reinstall.");
            return false;
        }

        // On startup we need to set the correct view
        if (GetOption(fullscreen))
            Frame::SetView(SCREEN_BLOCKS+4, SCREEN_LINES+48);   // landscape
        else
            Frame::SetView(SCREEN_BLOCKS, SCREEN_LINES+66);     // portrait
    }

    bool fRet = UI::Init(fFirstInit_);

    TRACE("<- OSD::Init() returning %s\n", fRet ? "true" : "false");
    return fRet;
}

void OSD::Exit (bool fReInit_/*=false*/)
{
    UI::Exit();

    if (!fReInit_ && hinstGAPI)
    {
        FreeLibrary(hinstGAPI);
        hinstGAPI = NULL;
    }
}


// Return an accurate time stamp of type PROFILE_T (__int64 for Win32)
PROFILE_T OSD::GetProfileTime ()
{
    __int64 llNow;

    // Read the current 64-bit time value
    if (QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&llNow)))
        return llNow;

    // Resolution of this is OEM dependant, but it's better than nothing
    return GetTickCount();
}


// Return a time-stamp in milliseconds
// Note: calling could should allow for the value wrapping by only comparing differences
DWORD OSD::GetTime ()
{
    static __int64 llFreq = 0;

    // Need to read the frequency?
    if (!llFreq)
    {
        // If the best high-resolution timer is not available, fall back to the less accurate multimedia timer
        if (QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&llFreq)))
            llFreq /= 1000i64;
        else
            llFreq = 1;
    }

    // Return the profile time as milliseconds
    return static_cast<DWORD>(GetProfileTime() / llFreq);
}

// Do whatever is necessary to locate an additional SimCoupe file - The Win32 version looks in the
// same directory as the EXE, but other platforms could use an environment variable, etc.
// If the path is already fully qualified (an OS-specific decision), return the same string
const char* OSD::GetFilePath (const char* pcszFile_/*=""*/)
{
    USES_CONVERSION;
    static char szPath[_MAX_PATH];

    // If the supplied path looks absolute, use it as-is
    if (*pcszFile_ == '\\' || strchr(pcszFile_, ':'))
        strncpy(szPath, pcszFile_, sizeof(szPath));

    // Form the full path relative to the current EXE file
    else
    {
        // Get the full path of the running module
        WCHAR wszPath[_MAX_PATH];
        GetModuleFileName(__hinstance, wszPath, sizeof(wszPath)/sizeof(wszPath[0]));

        // Strip the module file and append the supplied file/path
        strcpy(szPath, W2A(wszPath));
        strrchr(szPath, '\\')[1] = '\0';
        strcat(szPath, pcszFile_);
    }

    // Return a pointer to the new path
    return szPath;
}

// Same as GetFilePath but ensures a trailing backslash
const char* OSD::GetDirPath (const char* pcszDir_/*=""*/)
{
    char* psz = const_cast<char*>(GetFilePath(pcszDir_));

    // Append a backslash to non-empty strings that don't already have one
    if (*psz && psz[strlen(psz)-1] != '\\')
        strcat(psz, "\\");

    return psz;
}


// Check whether the specified path is accessible
bool OSD::CheckPathAccess (const char* pcszPath_)
{
    return true;
}

// Return whether a file/directory is normally hidden from a directory listing
bool OSD::IsHidden (const char* pcszFile_)
{
    USES_CONVERSION;

    // Hide entries with the hidden or system attribute bits set
    DWORD dwAttrs = GetFileAttributes(A2W(pcszFile_));
    return (dwAttrs != 0xffffffff) && (dwAttrs & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM));
}

// Return the path to use for a given drive with direct floppy access
const char* OSD::GetFloppyDevice (int nDrive_)
{
    return "";
}


void OSD::DebugTrace (const char* pcsz_)
{
    USES_CONVERSION;
    OutputDebugString(A2W(pcsz_));
}

int OSD::FrameSync (bool fWait_/*=true*/)
{
    static DWORD dwLast;
    DWORD dwNow = GetTime();

    // Set the 'last' time, if not already set
    if (!dwLast)
        dwLast = dwNow;

    // Determine how many ticks have gone by since last time
    int nElapsed = (dwNow - dwLast) / 20;
    dwLast += (nElapsed * 20);
    s_nTicks += nElapsed;

    // Wait for next frame?
    if (fWait_)
    {
        // Sit in a tight loop waiting, yielding our timeslice until it's time
        while ((GetTime() - dwLast) < 20)
            Sleep(0);

        // Adjust the time and tick count for the new frame
        dwLast += 20;
        s_nTicks++;
    }

    // Return the current tick count
    return s_nTicks;
}

////////////////////////////////////////////////////////////////////////////////

// Dummy printer device implementation
CPrinterDevice::CPrinterDevice () { }
CPrinterDevice::~CPrinterDevice () { }
bool CPrinterDevice::Open () { return false; }
void CPrinterDevice::Close () { }
void CPrinterDevice::Write (BYTE *pb_, size_t uLen_) { }

////////////////////////////////////////////////////////////////////////////////

// WinCE lacks a few of the required POSIX functions, so we'll implement them ourselves...


WIN32_FIND_DATA s_fd;
struct dirent s_dir;

DIR* opendir (const char* pcszDir_)
{
    USES_CONVERSION;

    static char szPath[_MAX_PATH];
    memset(&s_dir, 0, sizeof s_dir);

    // Append a wildcard to match all files
    strcpy(szPath, pcszDir_);
    if (szPath[strlen(szPath)-1] != '\\')
        strcat(szPath, "\\");
    strcat(szPath, "*");

    // Find the first file, saving the details for later
    HANDLE h = FindFirstFile(A2CW(pcszDir_), &s_fd);

    // Return the handle if successful, otherwise NULL
    return (h == INVALID_HANDLE_VALUE) ? NULL : reinterpret_cast<DIR*>(h);
}

struct dirent* readdir (DIR* hDir_)
{
    USES_CONVERSION;

    // All done?
    if (!s_fd.cFileName[0])
        return NULL;

    // Copy the filename and set the length
    s_dir.d_reclen = strlen(strncpy(s_dir.d_name, W2A(s_fd.cFileName), sizeof s_dir.d_name));

    // If we'd already reached the end
    if (!FindNextFile(reinterpret_cast<HANDLE>(hDir_), &s_fd))
        s_fd.cFileName[0] = '\0';

    // Return the current entry
    return &s_dir;
}

int closedir (DIR* hDir_)
{
    return FindClose(reinterpret_cast<HANDLE>(hDir_)) ? 0 : -1;
}

void unlink (const char* pcsz_)
{
    USES_CONVERSION;
    DeleteFile(A2W(pcsz_));
}

int stat (const char* psz_, struct stat* pst_)
{
    USES_CONVERSION;

    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile(A2W(psz_), &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
        return -1;

    // We only support size and type at the moment
    memset(pst_, 0, sizeof(struct stat));
    pst_->st_size = ffd.nFileSizeLow;
    pst_->st_mode = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? _S_IFDIR : _S_IFREG;

    FindClose(hFind);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

// WinCE also lacks the CRT time functions!

static const SYSTEMTIME st0 = { 1970, 1, 0, 1, 0, 0, 0 };

time_t time (time_t* pt_)
{
    SYSTEMTIME st;
    GetSystemTime(&st);

    ULONGLONG ft0, ftNow;
    SystemTimeToFileTime(&st0, reinterpret_cast<FILETIME*>(&ft0));
    SystemTimeToFileTime(&st, reinterpret_cast<FILETIME*>(&ftNow));

    ULONGLONG diff = (ftNow-ft0) / 10000000I64;
    time_t t = static_cast<time_t>(diff);

    if (pt_)
        *pt_ = t;

    return t;
}

time_t mktime(struct tm* ptm_)
{
    SYSTEMTIME st;
    st.wYear = 1900+ptm_->tm_year;
    st.wMonth = ptm_->tm_mon+1;
    st.wDayOfWeek = ptm_->tm_wday;
    st.wDay = ptm_->tm_mday;
    st.wHour = ptm_->tm_hour;
    st.wMinute = ptm_->tm_min;
    st.wSecond = ptm_->tm_sec;
    st.wMilliseconds = 0;

    ULONGLONG ft0, ft;
    SystemTimeToFileTime(&st0, reinterpret_cast<FILETIME*>(&ft0));
    SystemTimeToFileTime(&st,  reinterpret_cast<FILETIME*>(&ft));

    ULONGLONG diff = (ft-ft0) / 10000000I64;
    time_t t = static_cast<time_t>(diff);

    if (ptm_->tm_isdst)
        t -= 3600;

    return t;
}

struct tm* localtime (const time_t *pt_)
{
    time_t t = pt_ ? *pt_ : time(NULL);

    // Adjust for daylight savings, adding an hour if required
    TIME_ZONE_INFORMATION tzi;
    bool fDST = (GetTimeZoneInformation(&tzi) == TIME_ZONE_ID_DAYLIGHT);
    if (fDST)
        t += 3600;

    ULONGLONG ft0;
    SystemTimeToFileTime(&st0, reinterpret_cast<FILETIME*>(&ft0));

    ULONGLONG ft = ft0 + (t * 10000000I64);

    SYSTEMTIME st;
    FileTimeToSystemTime(reinterpret_cast<FILETIME*>(&ft), &st);

    if (st.wYear < 1970)
        return NULL;

    static struct tm tm;
    tm.tm_year = st.wYear - 1900;
    tm.tm_mon = st.wMonth - 1;
    tm.tm_wday = st.wDayOfWeek;
    tm.tm_mday = st.wDay;
    tm.tm_hour = st.wHour;
    tm.tm_min = st.wMinute;
    tm.tm_sec = st.wSecond;
    tm.tm_isdst = fDST;
    tm.tm_yday = 0;         // day in year not supported

    return &tm;
}
