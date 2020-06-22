// Part of SimCoupe - A SAM Coupe emulator
//
// FrameBuffer.h: SAM screen handling, including on-screen display text
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

#include "Font.h"

enum : uint8_t
{
    BLUE_1 = 1, BLUE_2 = 9, BLUE_3 = 16, BLUE_4 = 24, BLUE_5 = 17, BLUE_6 = 25, BLUE_7 = 113, BLUE_8 = 121,
    RED_1 = 2, RED_2 = 10, RED_3 = 32, RED_4 = 40, RED_5 = 34, RED_6 = 42, RED_7 = 114, RED_8 = 122,
    MAGENTA_1 = 3, MAGENTA_2 = 11, MAGENTA_3 = 48, MAGENTA_4 = 56, MAGENTA_5 = 51, MAGENTA_6 = 59, MAGENTA_7 = 115, MAGENTA_8 = 123,
    GREEN_1 = 4, GREEN_2 = 12, GREEN_3 = 64, GREEN_4 = 72, GREEN_5 = 68, GREEN_6 = 76, GREEN_7 = 116, GREEN_8 = 124,
    CYAN_1 = 5, CYAN_2 = 13, CYAN_3 = 80, CYAN_4 = 88, CYAN_5 = 85, CYAN_6 = 93, CYAN_7 = 117, CYAN_8 = 125,
    YELLOW_1 = 6, YELLOW_2 = 14, YELLOW_3 = 96, YELLOW_4 = 104, YELLOW_5 = 102, YELLOW_6 = 110, YELLOW_7 = 118, YELLOW_8 = 126,
    GREY_1 = 0, GREY_2 = 8, GREY_3 = 7, GREY_4 = 15, GREY_5 = 112, GREY_6 = 120, GREY_7 = 119, GREY_8 = 127,

    BLACK = GREY_1, WHITE = GREY_8
};

class FrameBuffer
{
public:
    FrameBuffer(int nWidth_, int nHeight_);

    const uint8_t* GetLine(int line) const { return &m_framebuffer[line * m_width]; }
    uint8_t* GetLine(int line) { return &m_framebuffer[line * m_width]; }

    int Width() const { return m_width; }
    int GetWidth() const { return m_width; }
    int Height() const { return m_height; }

    void ClipTo(int x, int y, int width, int height);
    void ClipNone();
    bool Clip(int& x, int& y, int& width, int& height);

    void Plot(int nX_, int y, uint8_t bColour_);
    void DrawLine(int x, int y, int nWidth_, int nHeight_, uint8_t colour);
    void FillRect(int x, int y, int nWidth_, int nHeight_, uint8_t colour);
    void FrameRect(int x, int y, int nWidth_, int nHeight_, uint8_t colour, bool round_ = false);
    void Poke(int x, int y, const uint8_t* data, int len);
    void DrawImage(int x, int y, int width, int height, const uint8_t* img_data, const uint8_t* img_palette);
    void DrawString(int x, int y, const char* str, uint8_t colour = WHITE);
    void Printf(int x, int y, const char* format, ...);

    void SetFont(std::shared_ptr<Font> font);
    int StringWidth(const char* pcsz_, int max_chars=-1) const;

protected:
    int m_width{};
    int m_height{};

    int m_clip_x{};
    int m_clip_y{};
    int m_clip_width{};
    int m_clip_height{};

    std::shared_ptr<Font> m_pFont;
    std::vector<uint8_t> m_framebuffer;
};
