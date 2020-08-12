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

#include <winsdkver.h>
#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WIN7
#endif
#include <SDKDDKVer.h>
#include <VersionHelpers.h>

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
#include <windowsx.h>
#include <io.h>         // for _access

#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#define PATH_SEPARATOR          '\\'

#define strcasecmp  _strcmpi
#define strncasecmp _strnicmp
#define mkdir(p,m)  _mkdir(p)

#if _MSC_VER > 1200
#define off_t   __int64
#define fseek   _fseeki64
#define fstat   _fstat64
#define stat    _stat64
#endif

////////////////////////////////////////////////////////////////////////////////

class OSD
{
public:
    static bool Init();
    static void Exit();

    static fs::path MakeFilePath(PathType type, const std::string& filename = "");
    static bool IsHidden(const std::string& path);

    static void DebugTrace(const std::string& str);
};
