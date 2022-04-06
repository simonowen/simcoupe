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

    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
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

std::string OSD::MakeFilePath(PathType type, const std::string& filename)
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
        path = fs::path(RESOURCE_DIR).make_preferred();
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

    path /= filename;

    // Use the EXE location in portable mode, or if we can't find the resource.
    if (portable_mode || (type == PathType::Resource && !fs::exists(path)))
        path = exe_dir / filename;

    return path.string();
}

// Return whether a file/directory is normally hidden from a directory listing
bool OSD::IsHidden(const std::string& path)
{
    // Hide entries with the hidden or system attribute bits set
    uint32_t dwAttrs = GetFileAttributes(path.c_str());
    return (dwAttrs != INVALID_FILE_ATTRIBUTES) && (dwAttrs & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
}

static std::wstring TranslitCyrillic(std::wstring str)
{
    std::wstring str_out;

    // Count the number or Cyrillic characters in a block, and the number of those capitalised
    int num_chars = 0, num_caps = 0;

    // Process character by character
    for (WCHAR wch, *pwsz = str.data(); (wch = *pwsz); pwsz++)
    {
        // Cyrillic character?
        bool cyrillic = wch == 0x0401 || (wch >= 0x0410 && wch <= 0x044f) || wch == 0x0451;
        num_chars = cyrillic ? num_chars + 1 : 0;

        // Map GBP and (c) directly to SAM codes
        if (wch == 0x00a3)  // GBP
            str_out.push_back(0x60);
        else if (wch == 0x00a9) // (c)
            str_out.push_back(0x7f);

        // Cyrillic?
        else if (cyrillic)
        {
            // Determine XOR value to preserve case of the source character
            char chCase = ~(wch - 0x03d0) & 0x20;
            num_caps = chCase ? num_caps + 1 : 0;

            // Is the next character Cyrillic too?
            cyrillic = (pwsz[1] == 0x0401 || (pwsz[1] >= 0x0410 && pwsz[1] <= 0x044f) || pwsz[1] == 0x0451);

            // If the next character is Cyrillic, match the case for any extra translit letters
            // Otherwise if >1 character and all capitals so far, continue as capitals
            char chCase1 = cyrillic ? (~(pwsz[1] - 0x03d0) & 0x20) :
                (num_chars > 1 && num_chars == num_caps) ? chCase : 0;

            // Special-case Cyrillic characters not in the main range
            if (wch == 0x0401)
            {
                str_out.push_back('Y');
                str_out.push_back('o' ^ chCase1);
            }
            else if (wch == 0x0451)
            {
                str_out.push_back('y');
                str_out.push_back('o' ^ chCase1);
            }
            else
            {
                // Unicode to transliterated Latin, starting from 0x410
                static const char* aszConv[] =
                {
                    "a", "b", "v", "g", "d", "e", "zh", "z",
                    "i", "j", "k", "l", "m", "n", "o", "p",
                    "r", "s", "t", "u", "f", "h", "c", "ch",
                    "sh", "shh", "\"", "y", "'", "e", "yu", "ya"
                };

                // Look up the transliterated string
                const char* psz = aszConv[(wch - 0x0410) & 0x1f];

                for (char ch; (ch = *psz); psz++)
                {
                    // Toggle case of alphabetic characters only
                    if (ch >= 'a' && ch <= 'z')
                        str_out.push_back(ch ^ chCase);
                    else
                        str_out.push_back(ch);

                    // For the remaining characters, use the case of the next character
                    chCase = chCase1;
                }
            }
        }
        else
        {
            // Copy anything else as-is
            str_out.push_back(wch);
        }
    }

    return str_out;
}

std::string OSD::GetClipboardText()
{
    std::string text;

    if (OpenClipboard(g_hwnd))
    {
        if (auto hClip = GetClipboardData(CF_UNICODETEXT))
        {
            if (auto pwsz = reinterpret_cast<LPCWSTR>(GlobalLock(hClip)))
            {
                auto wstr = TranslitCyrillic(pwsz);

                // Convert to US-ASCII, stripping diacritic marks as we go
                constexpr UINT CP_USASCII = 20127;
                auto len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
                std::vector<char> ascii(len);
                len = WideCharToMultiByte(CP_USASCII, 0, wstr.c_str(), -1, ascii.data(), len, nullptr, nullptr);
                text = ascii.data();

                GlobalUnlock(hClip);
            }
        }

        CloseClipboard();
    }

    return text;
}

void OSD::SetClipboardText(const std::string& str)
{
    if (OpenClipboard(g_hwnd))
    {
        EmptyClipboard();

        if (auto ptr = GlobalAllocPtr(GMEM_ZEROINIT, str.length() + 1))
        {
            std::memcpy(ptr, str.c_str(), str.length() + 1);
            GlobalUnlockPtr(ptr);
            SetClipboardData(CF_TEXT, GlobalHandle(ptr));
        }

        CloseClipboard();
    }
}

void OSD::DebugTrace(const std::string& str)
{
    OutputDebugString(str.c_str());
}
