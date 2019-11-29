// Part of SimCoupe - A SAM Coupe emulator
//
// Screen.h: SAM screen handling, including on-screen display text
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

#ifndef SCREEN_H
#define SCREEN_H

#include "Font.h"

const int CHAR_HEIGHT = 11;     // Character cell dimensions
const int CHAR_SPACING = 1;     // 1 pixel between each character
const int LINE_SPACING = 4;     // 4 pixels between each row
const char CHAR_UNKNOWN = '_';  // Character to display when not available in charset

// Colours, shared with the SAM palette
enum : uint8_t
{
    BLUE_1 = 1, BLUE_2 = 9, BLUE_3 = 16, BLUE_4 = 24, BLUE_5 = 17, BLUE_6 = 25, BLUE_7 = 113, BLUE_8 = 121,
    RED_1 = 2, RED_2 = 10, RED_3 = 32, RED_4 = 40, RED_5 = 34, RED_6 = 42, RED_7 = 114, RED_8 = 122,
    MAGENTA_1 = 3, MAGENTA_2 = 11, MAGENTA_3 = 48, MAGENTA_4 = 56, MAGENTA_5 = 51, MAGENTA_6 = 59, MAGENTA_7 = 115, MAGENTA_8 = 123,
    GREEN_1 = 4, GREEN_2 = 12, GREEN_3 = 64, GREEN_4 = 72, GREEN_5 = 68, GREEN_6 = 76, GREEN_7 = 116, GREEN_8 = 124,
    CYAN_1 = 5, CYAN_2 = 13, CYAN_3 = 80, CYAN_4 = 88, CYAN_5 = 85, CYAN_6 = 93, CYAN_7 = 117, CYAN_8 = 125,
    YELLOW_1 = 6, YELLOW_2 = 14, YELLOW_3 = 96, YELLOW_4 = 104, YELLOW_5 = 102, YELLOW_6 = 110, YELLOW_7 = 118, YELLOW_8 = 126,
    GREY_1 = 0, GREY_2 = 8, GREY_3 = 7, GREY_4 = 15, GREY_5 = 112, GREY_6 = 120, GREY_7 = 119, GREY_8 = 127,

    BLACK = GREY_1, WHITE = GREY_8      // Useful aliases
};


class CScreen final
{
public:
    CScreen(int nWidth_, int nHeight_);
    CScreen(const CScreen&) = delete;
    void operator= (const CScreen&) = delete;
    ~CScreen();

public:
    BYTE* GetLine(int nLine_) { return m_ppbLines[nLine_]; }

    int GetPitch() const { return m_nPitch; }
    int GetWidth() const { return m_nPitch; }
    int GetHeight() const { return m_nHeight; }

public:
    void Clear();

    void SetClip(int nX_ = 0, int nY_ = 0, int nWidth_ = 0, int nHeight_ = 0);
    bool Clip(int& rnX_, int& rnY_, int& rnWidth_, int& rnHeight_);

    void Plot(int nX_, int nY_, BYTE bColour_);
    void DrawLine(int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_);
    void FillRect(int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_);
    void FrameRect(int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_, bool fRound_ = false);
    void Poke(int nX_, int nY_, const BYTE* pcbData_, UINT uLen_);
    void DrawImage(int nX_, int nY_, int nWidth_, int nHeight_, const BYTE* pbData_, const BYTE* pbPalette_);
    int DrawString(int nX_, int nY_, const char* pcsz_, BYTE bInk_ = WHITE);
    int Printf(int nX_, int nY_, const char* pcszFormat_, ...);

    static int GetStringWidth(const char* pcsz_, size_t nMaxChars_ = -1, const GUIFONT* pFont_ = nullptr);
    static void SetFont(const GUIFONT* pFont_);

protected:
    int m_nPitch = 0, m_nHeight = 0;    // Pitch and height of the screen

    BYTE* m_pbFrame = nullptr;          // Screen data block
    BYTE** m_ppbLines = nullptr;        // Look-up table from line number to pointer to start of the line
};

#endif  // SCREEN_H
