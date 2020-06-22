// Part of SimCoupe - A SAM Coupe emulator
//
// Font.h: Font data used for on-screen text
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

struct Font
{
    static constexpr auto CHAR_HEIGHT = 11;
    static constexpr auto CHAR_SPACING = 1;
    static constexpr auto LINE_SPACING = 4;
    static constexpr auto DEFAULT_CHR = '_';

    Font(int width, int height, int bytes_per_chr, uint8_t first_chr, uint8_t last_chr, bool fixed_width, const std::vector<uint8_t>& data) :
        width(width), height(height), bytes_per_chr(bytes_per_chr), first_chr(first_chr), last_chr(last_chr), fixed_width(fixed_width), data(data) { }

    int width;          // zero for variable width
    int height;
    int bytes_per_chr;
    uint8_t first_chr;
    uint8_t last_chr;
    bool fixed_width;
    const std::vector<uint8_t>& data;

    int StringWidth(const char* str, int max_chars=-1) const;
};

extern std::shared_ptr<Font> sFixedFont, sPropFont, sGUIFont, sSpacedGUIFont;
