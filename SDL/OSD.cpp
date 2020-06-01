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
#include "OSD.h"

#include "CPU.h"
#include "Frame.h"
#include "Main.h"
#include "Options.h"
#include "Parallel.h"


bool OSD::Init(bool /*fFirstInit_=false*/)
{
#ifdef _WINDOWS
    // We'll do our own error handling, so suppress any windows error dialogs
    SetErrorMode(SEM_FAILCRITICALERRORS);
#endif

    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        Message(msgError, "SDL init failed: %s", SDL_GetError());
        return false;
    }

    return true;
}

void OSD::Exit(bool /*fReInit_=false*/)
{
    SDL_Quit();
}


// Return a millisecond accurate time stamp.
// Note: calling could should allow for the value wrapping by only comparing differences
uint32_t OSD::GetTime()
{
    return SDL_GetTicks();
}


const char* OSD::MakeFilePath(int nDir_, const char* pcszFile_/*=""*/)
{
    static char szPath[MAX_PATH * 2];
    szPath[0] = '\0';

    // Set an appropriate base location
#if defined(_WINDOWS)
    GetModuleFileName(nullptr, szPath, MAX_PATH);
    strrchr(szPath, '\\')[1] = '\0';
#elif defined(__AMIGAOS4__)
    // Amiga uses the magic PROGDIR device for EXE location
    strcpy(szPath, "PROGDIR:");
#else
    // $HOME is a fairly safe default
    strncpy(szPath, getenv("HOME"), MAX_PATH - 2);
    szPath[MAX_PATH - 2] = '\0';

    if (szPath[0])
        strcat(szPath, "/");
#endif

    switch (nDir_)
    {
    case MFP_SETTINGS:
#if defined(__APPLE__)
        strcat(szPath, "Library/Preferences/SimCoupe/");
#elif !defined(_WINDOWS) && !defined(__AMIGAOS4__)
        strcat(szPath, ".simcoupe/");
#endif
        break;

    case MFP_INPUT:
        // Input override
        if (GetOption(inpath)[0])
        {
            strncpy(szPath, GetOption(inpath), MAX_PATH);
            break;
        }

        // Default should be good enough
        break;

    case MFP_OUTPUT:
    {
        // Output override
        if (GetOption(outpath)[0])
        {
            strncpy(szPath, GetOption(outpath), MAX_PATH - 1);
            szPath[MAX_PATH - 1] = '\0';
            break;
        }

#if defined(__APPLE__)
        strncat(szPath, "Documents/SimCoupe/", MAX_PATH - strlen(szPath) - 1);
        szPath[MAX_PATH - 1] = '\0';
#elif !defined(_WINDOWS) && !defined(__AMIGAOS4__)
        struct stat st;
        size_t u = strlen(szPath);

        // If there's a Desktop folder, it'll be more visible there
        strncat(szPath, "Desktop/", MAX_PATH - strlen(szPath) - 1);
        szPath[MAX_PATH - 1] = '\0';

        // Ignore Desktop folder if it doesn't exist
        if (stat(szPath, &st))
            szPath[u] = '\0';

        // Keep it tidy though
        strncat(szPath, "SimCoupe/", MAX_PATH - strlen(szPath) - 1);
        szPath[MAX_PATH - 1] = '\0';
#endif
        break;
    }

    case MFP_RESOURCE:
    {
#if defined(__APPLE__) && defined(HAVE_LIBSDL2)
        // Resources path in the app bundle (requires SDL 2.0.1)
        char* pszBasePath = SDL_GetBasePath();
        strncpy(szPath, pszBasePath, MAX_PATH - 1);
        szPath[MAX_PATH - 1] = '\0';
        SDL_free(pszBasePath);
#elif !defined(__AMIGAOS4__) && defined(RESOURCE_DIR)
        // If available, use the resource directory from the build process
        strncpy(szPath, RESOURCE_DIR, MAX_PATH - 1);
        strncat(szPath, "/", MAX_PATH - strlen(szPath) - 1);
        szPath[MAX_PATH - 1] = '\0';
#else
        // Fall back on an empty path as a safe default
        szPath[0] = '\0';
#endif
        break;
    }
    }

    // Create the directory if it doesn't already exist
    // This assumes only the last component could be missing
    if (mkdir(szPath, 0755) != 0 && errno != EEXIST)
        TRACE("!!! Failed to create directory: %s\n", szPath);

    // Append any supplied filename (backslash separator already added)
    strncat(szPath, pcszFile_, sizeof(szPath) - strlen(szPath) - 1);
    szPath[sizeof(szPath) - 1] = '\0';

    // Return a pointer to the new path
    return szPath;
}


// Check whether the specified path is accessible
bool OSD::CheckPathAccess(const char* pcszPath_)
{
    return !access(pcszPath_, X_OK);
}


// Return whether a file/directory is normally hidden from a directory listing
bool OSD::IsHidden(const char* pcszPath_)
{
#ifdef _WINDOWS
    // Hide entries with the hidden or system attribute bits set
    auto dwAttrs = GetFileAttributes(pcszPath_);
    return (dwAttrs != 0xffffffff) && (dwAttrs & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
#else
    // Hide entries beginning with a dot
    pcszPath_ = strrchr(pcszPath_, PATH_SEPARATOR);
    return pcszPath_ && pcszPath_[1] == '.';
#endif
}


// Return the path to use for a given drive with direct floppy access
const char* OSD::GetFloppyDevice(int nDrive_)
{
#if defined (__AMIGAOS4__)
    static char szDevice[] = "DF0:";
#else
    static char szDevice[] = "/dev/fd_";
#endif

    szDevice[7] = '0' + nDrive_ - 1;
    return szDevice;
}


void OSD::DebugTrace(const char* pcsz_)
{
#ifdef _WINDOWS
    OutputDebugString(pcsz_);
#elif defined (__AMIGAOS4__)
    printf("%s", pcsz_);
#else
    fprintf(stderr, "%s", pcsz_);
#endif
}


////////////////////////////////////////////////////////////////////////////////

// Dummy printer device implementation
PrinterDevice::PrinterDevice() { }
PrinterDevice::~PrinterDevice() { }
bool PrinterDevice::Open() { return false; }
void PrinterDevice::Close() { }
void PrinterDevice::Write(uint8_t* /*pb_*/, size_t /*uLen_*/) { }

////////////////////////////////////////////////////////////////////////////////

#ifdef _WINDOWS

WIN32_FIND_DATA s_fd;
struct dirent s_dir;

DIR* opendir(const char* pcszDir_)
{
    static char szPath[_MAX_PATH];

    memset(&s_dir, 0, sizeof s_dir);

    // Append a wildcard to match all files
    lstrcpy(szPath, pcszDir_);
    if (szPath[lstrlen(szPath) - 1] != '\\')
        lstrcat(szPath, "\\");
    lstrcat(szPath, "*");

    // Find the first file, saving the details for later
    HANDLE h = FindFirstFile(szPath, &s_fd);

    // Return the handle if successful, otherwise nullptr
    return (h == INVALID_HANDLE_VALUE) ? nullptr : reinterpret_cast<DIR*>(h);
}

struct dirent* readdir(DIR* hDir_)
{
    // All done?
    if (!s_fd.cFileName[0])
        return nullptr;

    // Copy the filename and set the length
    s_dir.d_reclen = lstrlen(lstrcpyn(s_dir.d_name, s_fd.cFileName, sizeof s_dir.d_name));

    // If we'd already reached the end
    if (!FindNextFile(reinterpret_cast<HANDLE>(hDir_), &s_fd))
        s_fd.cFileName[0] = '\0';

    // Return the current entry
    return &s_dir;
}

int closedir(DIR* hDir_)
{
    return FindClose(reinterpret_cast<HANDLE>(hDir_)) ? 0 : -1;
}

#endif
