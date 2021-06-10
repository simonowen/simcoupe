// Part of SimCoupe - A SAM Coupe emulator
//
// OSD.cpp: Win32 common OS-dependant functions
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

#include "SimCoupe.h"

#include <winspool.h>
#include <mmsystem.h>
#include <commctrl.h>
#include <shlobj.h>

#include "CPU.h"
#include "Frame.h"
#include "Main.h"
#include "Options.h"
#include "Parallel.h"
#include "UI.h"
#include "Video.h"


constexpr auto TIMER_RESOLUTION_MS = 1;

bool portable_mode = false;


bool OSD::Init()
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
#endif

    char szModule[MAX_PATH]{};
    GetModuleFileName(nullptr, szModule, MAX_PATH);
    if (GetFileAttributes(szModule) & FILE_ATTRIBUTE_READONLY)
    {
        // Quit after 42 seconds if the main EXE is read-only, to discourage
        // eBay sellers bundling us on CD/DVD with unauthorised SAM software.
        SetTimer(nullptr, 0, 42 * 1000, [](HWND, UINT, UINT_PTR, DWORD) {
            PostMessage(g_hwnd, WM_CLOSE, 0, 0L);
            });
    }

    // Enable portable mode if the options file is local
    portable_mode = fs::exists(fs::path(szModule).remove_filename() / OPTIONS_FILE);
    if (portable_mode)
        Options::Load(__argc, __argv);

    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
        return false;

    InitCommonControls();
    SetErrorMode(SEM_FAILCRITICALERRORS);

    timeBeginPeriod(TIMER_RESOLUTION_MS);

    return true;
}

void OSD::Exit()
{
    timeEndPeriod(TIMER_RESOLUTION_MS);
    CoUninitialize();
}


static fs::path GetSpecialFolderPath(int csidl_)
{
    fs::path path;

    LPITEMIDLIST pidl{};
    if (SUCCEEDED(SHGetSpecialFolderLocation(nullptr, csidl_, &pidl)))
    {
        char szPath[MAX_PATH + 1]{};
        if (SHGetPathFromIDList(pidl, szPath))
        {
            path = szPath;
        }

        CoTaskMemFree(pidl);
    }

    return path;
}

fs::path OSD::MakeFilePath(PathType type, const std::string& filename)
{
    fs::path path;

    char exe_path[MAX_PATH]{};
    GetModuleFileName(GetModuleHandle(NULL), exe_path, _countof(exe_path) - 1);
    auto exe_dir = fs::path(exe_path).remove_filename();

    switch (type)
    {
    case PathType::Settings:
        path = GetSpecialFolderPath(CSIDL_APPDATA) / "SimCoupe";
        break;

    case PathType::Input:
        path = GetOption(inpath);
        if (path.empty())
        {
            path = GetSpecialFolderPath(CSIDL_MYDOCUMENTS);
        }
        break;

    case PathType::Output:
        path = GetOption(outpath);
        if (path.empty())
        {
            path = GetSpecialFolderPath(CSIDL_MYDOCUMENTS) / "SimCoupe";
        }
        break;

    case PathType::Resource:
#ifdef RESOURCE_DIR
        path = RESOURCE_DIR;
#else
        path = exe_dir;
#endif
        break;
    }

    if (!path.empty() && !fs::exists(path))
    {
        std::error_code error;
        fs::create_directories(path, error);
    }

    if (!filename.empty())
        path /= filename;

    // Use the EXE location in portable mode, or if we can't find the resource.
    if (portable_mode || (type == PathType::Resource && !fs::exists(path)))
        path = exe_dir / filename;

    return path;
}

// Return whether a file/directory is normally hidden from a directory listing
bool OSD::IsHidden(const std::string& path)
{
    // Hide entries with the hidden or system attribute bits set
    uint32_t dwAttrs = GetFileAttributes(path.c_str());
    return (dwAttrs != INVALID_FILE_ATTRIBUTES) && (dwAttrs & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
}

void OSD::DebugTrace(const std::string& str)
{
    OutputDebugString(str.c_str());
}
