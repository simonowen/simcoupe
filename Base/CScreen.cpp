// Part of SimCoupe - A SAM Coupé emulator
//
// CScreen.cpp: SAM screen handling, including on-screen display text
//
//  Copyright (c) 1999-2001  Simon Owen
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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

// Notes:
//  The SAM screen is stored with 1 byte holding the palette colour used
//  for each screen pixel, regardless of the screen mode.
//
//  On-screen text and graphics are always drawn in high resolution mode
//  (double with width of low), and any existing line data is simply
//  converted first.

#include "SimCoupe.h"

#include "CScreen.h"
#include "Font.h"


const unsigned int CHAR_WIDTH = 8, CHAR_HEIGHT = 8;     // Character cell dimensions


CScreen::CScreen (int nWidth_, int nHeight_)
{
    m_nPitch = nWidth_ & ~15;   // Round down to the nearest hi-res screen block chunk
    m_nHeight = nHeight_;

    m_pbFrame = new BYTE [m_nPitch * m_nHeight];
    m_pfHiRes = new bool [m_nHeight];

    // Create the look-up table from line number to start of
    m_ppbLines = new BYTE* [m_nHeight];
    for (int i = 0 ; i < m_nHeight ; i++)
        m_ppbLines[i] = m_pbFrame + (m_nPitch * i);

    Clear();
}

CScreen::~CScreen ()
{
    if (m_pbFrame) { delete m_pbFrame; m_pbFrame = NULL; }
    if (m_pfHiRes) { delete m_pfHiRes; m_pfHiRes = NULL; }
    if (m_ppbLines) { delete m_ppbLines; m_ppbLines = NULL; }
}


////////////////////////////////////////////////////////////////////////////////


// Return the address of a high-res version of a line, converting from lo-res if necessary
BYTE* CScreen::GetHiResLine (int nLine_, int nWidth_/*=WIDTH_BLOCKS*/)
{
    bool fHiRes;
    BYTE* pbLine = GetLine(nLine_, fHiRes);

    // If the line is already hi-res, return the pointer to it
    if (fHiRes)
        return pbLine;

    // Limit the amount converted to the maximum visible width
    nWidth_ = min(nWidth_, (m_nPitch >> 4)) << 3;

    // Double up each pixel on the line
    for (int i = nWidth_-1 ; i >= 0 ; i -= 8)
    {
        reinterpret_cast<WORD*>(pbLine)[i-0] = static_cast<WORD>(pbLine[i-0]) * 0x0101;
        reinterpret_cast<WORD*>(pbLine)[i-1] = static_cast<WORD>(pbLine[i-1]) * 0x0101;
        reinterpret_cast<WORD*>(pbLine)[i-2] = static_cast<WORD>(pbLine[i-2]) * 0x0101;
        reinterpret_cast<WORD*>(pbLine)[i-3] = static_cast<WORD>(pbLine[i-3]) * 0x0101;
        reinterpret_cast<WORD*>(pbLine)[i-4] = static_cast<WORD>(pbLine[i-4]) * 0x0101;
        reinterpret_cast<WORD*>(pbLine)[i-5] = static_cast<WORD>(pbLine[i-5]) * 0x0101;
        reinterpret_cast<WORD*>(pbLine)[i-6] = static_cast<WORD>(pbLine[i-6]) * 0x0101;
        reinterpret_cast<WORD*>(pbLine)[i-7] = static_cast<WORD>(pbLine[i-7]) * 0x0101;
    }

    // Mark the line as hi-res
    SetHiRes(nLine_, true);

    return pbLine;
}


void CScreen::Clear ()
{
    memset(m_pbFrame, 0, m_nPitch * m_nHeight);
    memset(m_pfHiRes, 0, m_nHeight * sizeof(bool));
}



// Draw a solid rectangle on the display
void CScreen::FillRect (int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_)
{
    // Negative coordinates are taken to be in from the right and/or bottom of the display
    if (nX_ < 0) nX_ += m_nPitch - nWidth_;
    if (nY_ < 0) nY_ += m_nHeight - nHeight_;

    // Iterate through each line in the block
    while (nHeight_--)
    {
        BYTE* pb = GetHiResLine(nY_++) + nX_;

        // For small block this is probably faster than memset!
        for (int nWidth = nWidth_ ; nWidth-- ; *pb++ = bColour_);
    }
}


// Draw an opaque character at a given location using the specified foregound and background colours
void CScreen::DrawOpaqueChar (BYTE bChar_, BYTE bInk_/*=0x7f*/, BYTE bPaper_/*=0x00*/)
{
    // Out-of-range characters will be shown as a space
    if (bChar_ < ' ' || bChar_ > 128)
        bChar_ = ' ';

    for (int nLine = m_nY+CHAR_HEIGHT-1 ; nLine >= m_nY ; nLine--)
        GetHiResLine(nLine);

    const BYTE* pbData = &abFont[bChar_ - ' '][1];

    for (int i = 0 ; i < (int)CHAR_HEIGHT ; i++)
    {
        BYTE* pLine = GetLine(m_nY+i) + m_nX;

        BYTE bData = *pbData++;
        pLine[0] = (bData & 0x80) ? bInk_ : bPaper_;
        pLine[1] = (bData & 0x40) ? bInk_ : bPaper_;
        pLine[2] = (bData & 0x20) ? bInk_ : bPaper_;
        pLine[3] = (bData & 0x10) ? bInk_ : bPaper_;
        pLine[4] = (bData & 0x08) ? bInk_ : bPaper_;
        pLine[5] = (bData & 0x04) ? bInk_ : bPaper_;
        pLine[6] = (bData & 0x02) ? bInk_ : bPaper_;
        pLine[7] = (bData & 0x01) ? bInk_ : bPaper_;
    }

    // Advance a character, wrapping if necessary
    if ((m_nX += 8) >= (m_nPitch - 8))
    {
        m_nY += 8;
        m_nX = 0;
    }
}


// Draw an opaque string of characters at the specified character position
void CScreen::DrawOpaqueString (int nX_, int nY_, const char* pcsz_, BYTE bInk_/*=0x7f*/, BYTE bPaper_/*=0x00*/)
{
    MoveTo(nX_, nY_);

    while (*pcsz_)
        DrawOpaqueChar(*pcsz_++, bInk_, bPaper_);
}


// Draw a proportionally spaced string of characters at a specified pixel position
void CScreen::DrawString (int nX_, int nY_, const char* pcsz_, BYTE bInk_)
{
    if (nX_ < 0) nX_ += m_nPitch - GetStringWidth(pcsz_);
    if (nY_ < 0) nY_ += m_nHeight - CHAR_HEIGHT;

    MoveTo(nX_, nY_);

    // Ensure the lines containing the character is in hi-res
    for (int i = CHAR_HEIGHT-1 ; i >= 0 ; i--)
        GetHiResLine(m_nY+i);

    // Iterate thru the characters in the string
    for (BYTE bChar ; (bChar = *pcsz_++) ; )
    {
        // Out-of-range characters will be shown as a space
        if (bChar < ' ' || bChar > 128)
            bChar = ' ';

        // Look up the font data for the character
        const BYTE* pbData = &abFont[bChar - ' '][0];

        // Pick out the proportional spacing details, and step back by the space preceding the character
        BYTE bSpace = *pbData++;
        m_nX -= (bSpace >> 4);

        for (int i = 0 ; i < (int)CHAR_HEIGHT ; i++)
        {
            BYTE* pLine = GetLine(m_nY+i) + m_nX;

            BYTE bData = *pbData++;
            if (bData & 0x80) pLine[0] = bInk_;
            if (bData & 0x40) pLine[1] = bInk_;
            if (bData & 0x20) pLine[2] = bInk_;
            if (bData & 0x10) pLine[3] = bInk_;
            if (bData & 0x08) pLine[4] = bInk_;
            if (bData & 0x04) pLine[5] = bInk_;
            if (bData & 0x02) pLine[6] = bInk_;
            if (bData & 0x01) pLine[7] = bInk_;
        }

        // Step back by the amount of space after the character
        m_nX += CHAR_WIDTH - (bSpace & 0xf) + 1;
    }
}


// Get the on-screen width required for a specified string if drawn proportionally
int CScreen::GetStringWidth (const char* pcsz_)
{
    int nWidth = 0;

    for (BYTE bChar ; (bChar = *pcsz_++) ; )
    {
        // Out-of-range characters will be drawn as a space
        if (bChar < ' ' || bChar > 128)
            bChar = ' ';

        BYTE bExtents = abFont[bChar - ' '][0];
        nWidth += (CHAR_WIDTH - (bExtents & 0xf) + 1 - (bExtents >> 4));
    }

    return nWidth;
}
