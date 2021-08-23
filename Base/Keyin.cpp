// Part of SimCoupe - A SAM Coupe emulator
//
// Keyin.cpp: Automatic keyboard input
//
//  Copyright (c) 2012 Simon Owen
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
#include "Keyin.h"
#include "Memory.h"

namespace Keyin
{
constexpr uint8_t FLAGS_NEW_KEY = 0x20;
constexpr auto MAX_STUCK_FRAMES = 500;

static std::string s_input_text;
static bool s_map_chars = true;
static int s_skipped_frames;


void String(const std::string& text, bool map_chars)
{
    s_input_text = text;
    s_map_chars = map_chars;
    s_skipped_frames = 0;
}

void Stop()
{
    s_input_text.clear();

    if (CanType())
    {
        auto pPage0 = PageReadPtr(0);
        pPage0[SYSVAR_FLAGS & MEM_PAGE_MASK] &= ~FLAGS_NEW_KEY;
    }
}

bool CanType()
{
    return GetSectionPage(Section::A) == ROM0 && GetSectionPage(Section::B) == 0;
}

bool IsTyping()
{
    if (s_input_text.empty())
        return false;

    if (++s_skipped_frames == MAX_STUCK_FRAMES)
        s_input_text.clear();

    return !s_input_text.empty();
}

void Next()
{
    if (s_input_text.empty() || !CanType())
        return;

    auto pPage0 = PageReadPtr(0);
    if (pPage0[SYSVAR_FLAGS & MEM_PAGE_MASK] & FLAGS_NEW_KEY)
        return;

    s_skipped_frames = 0;

    auto b = static_cast<uint8_t>(s_input_text.front());
    s_input_text.erase(s_input_text.begin());

    if (s_map_chars)
    {
        b = (b == '\n') ? '\r' : (b < ' ' && b >= 0x80 && b != '\t') ? 0 : b;
    }

    if (b)
    {
        pPage0[SYSVAR_LAST_K & MEM_PAGE_MASK] = b;
        pPage0[SYSVAR_FLAGS & MEM_PAGE_MASK] |= FLAGS_NEW_KEY;
    }
}

} // namespace Keyin
