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

#include <fcntl.h>

#ifdef HAVE_LIBSDL2
#include "SDL2/SDL.h"
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

#endif


#ifdef __QNX__
#include <strings.h>        // for strcasecmp
#endif


#ifdef _WINDOWS

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

#endif  // _WINDOWS

////////////////////////////////////////////////////////////////////////////////

class OSD
{
public:
    static bool Init();
    static void Exit();

    static fs::path MakeFilePath(PathType type, const std::string& filename="");
    static bool IsHidden(const std::string& path);

    static void DebugTrace(const std::string& str);
};
