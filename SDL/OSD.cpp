// Part of SimCoupe - A SAM Coupe emulator
//
// OSD.cpp: SDL common "OS-dependant" functions
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

// Notes:
//  This "OS-dependant" module for the SDL version required some yucky
//  conditional blocks, but it's probably forgiveable in here!

#include "SimCoupe.h"

#include "CPU.h"
#include "Frame.h"
#include "Main.h"
#include "Options.h"
#include "Parallel.h"
#include "whereami.h"


bool OSD::Init()
{
#ifdef _WINDOWS
    SetErrorMode(SEM_FAILCRITICALERRORS);
#endif

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        Message(MsgType::Error, "SDL init failed: {}", SDL_GetError());
        return false;
    }

    return true;
}

void OSD::Exit()
{
    SDL_Quit();
}


std::string OSD::MakeFilePath(PathType type, const std::string& filename)
{
    fs::path base_path;
    fs::path path;

    std::string exe;
    exe.resize(wai_getExecutablePath(nullptr, 0, nullptr));
    wai_getExecutablePath(exe.data(), exe.length(), nullptr);
    auto exe_path = fs::path(exe).remove_filename();

#if defined(_WINDOWS)
    base_path = exe_path;
#elif defined(__AMIGAOS4__)
    base_path = "PROGDIR:";
#else
    base_path = getenv("HOME");
#endif

    switch (type)
    {
    case PathType::Settings:
#if defined(_WINDOWS)
        path = fs::path(getenv("APPDATA")) / "SimCoupe";
#elif defined(__APPLE__)
        path = base_path / "Library/Preferences/SimCoupe";
#elif defined(__AMIGAOS4__)
        path = base_path;
#else
        path = base_path / ".simcoupe";
#endif
        break;

    case PathType::Input:
        path = GetOption(inpath);
        break;

    case PathType::Output:
        if (!GetOption(outpath).empty())
        {
            path = GetOption(outpath);
            break;
        }

#if defined(__APPLE__)
        path = base_path / "Documents/SimCoupe";
#elif !defined(_WINDOWS) && !defined(__AMIGAOS4__)
        path = base_path / "Desktop";
        if (!fs::exists(path))
        {
            path = base_path;
        }
        path /= "SimCoupe";
#endif
        break;

    case PathType::Resource:
#if defined(__APPLE__) && defined(HAVE_LIBSDL2)
        // Resources path in the app bundle
        if (auto pBasePath = SDL_GetBasePath())
        {
            path = pBasePath;
            SDL_free(pBasePath);
        }
#elif defined(RESOURCE_DIR) && !defined(__AMIGAOS4__)
        path = RESOURCE_DIR;

        if (!fs::exists(path / filename) && fs::exists(exe_path / filename))
        {
            path = exe_path;
        }
#else
        path.clear();
#endif
        break;
    }

    if (path.empty())
        path = base_path;

    if (!fs::exists(path))
    {
        std::error_code error;
        fs::create_directories(path, error);
    }

    path /= filename;

    return path.string();
}


// Return whether a file/directory is normally hidden from a directory listing
bool OSD::IsHidden(const std::string& path)
{
#ifdef _WINDOWS
    // Hide entries with the hidden or system attribute bits set
    auto dwAttrs = GetFileAttributes(path.c_str());
    return (dwAttrs != 0xffffffff) && (dwAttrs & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
#else
    auto filepath = fs::path(path);
    return filepath.has_filename() && filepath.filename().string().front() == '.';
#endif
}

std::string OSD::GetClipboardText()
{
    std::string text;

    if (SDL_HasClipboardText() == SDL_TRUE)
    {
        auto ptr = SDL_GetClipboardText();
        text = ptr;
        SDL_free(ptr);
    }

    return text;
}

void OSD::SetClipboardText(const std::string& str)
{
    SDL_SetClipboardText(str.c_str());
}

void OSD::DebugTrace(const std::string& str)
{
#ifdef _WINDOWS
    OutputDebugString(str.c_str());
#elif defined (__AMIGAOS4__)
    puts(str.c_str());
#else
    fprintf(stderr, "%s", str.c_str());
#endif
}
