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
//  for each screen pixel, regardless of the screen mode. (0,0) is top-left.
//
//  On-screen text and graphics are always drawn in high resolution mode
//  (double with width of low), and any existing line data is simply
//  converted first.

#include "SimCoupe.h"

#include "CScreen.h"
#include "Font.h"


int nClipX, nClipY, nClipWidth, nClipHeight;    // Clip box for any screen drawing
FONT* pFont = &sNewFont;


CScreen::CScreen (int nWidth_, int nHeight_)
{
    m_nPitch = nWidth_ & ~15;   // Round down to the nearest hi-res screen block chunk
    m_nHeight = nHeight_;

    m_pbFrame = new BYTE [m_nPitch * m_nHeight];
    m_pfHiRes = new bool [m_nHeight];

    // Create the look-up table from line number to start of screen line
    m_ppbLines = new BYTE* [m_nHeight];
    for (int i = 0 ; i < m_nHeight ; i++)
        m_ppbLines[i] = m_pbFrame + (m_nPitch * i);

    // Set default clipping (full screen) and clear the screen
    SetClip();
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

////////////////////////////////////////////////////////////////////////////////

void CScreen::SetClip (int nX_/*=0*/, int nY_/*=0*/, int nWidth_/*=0*/, int nHeight_/*=0*/)
{
    if (!nWidth_) nWidth_= m_nPitch;
    if (!nHeight_) nHeight_= m_nHeight;

    if ((nClipX = nX_) < 0) { nWidth_  += nX_; nClipX = 0; }
    if ((nClipY = nY_) < 0) { nHeight_ += nY_; nClipY = 0; }

    nClipWidth = (nClipX+nWidth_ > m_nPitch) ? m_nPitch - nClipX : nWidth_;
    nClipHeight = (nClipY+nHeight_ > m_nHeight) ? m_nHeight - nClipY : nHeight_;
}

bool CScreen::Clip (int& rnX_, int& rnY_, int& rnWidth_, int& rnHeight_)
{
    // Limit the supplied region to the current clipping region
    if (rnX_ < nClipX) { rnWidth_ -= nClipX-rnX_; rnX_ = nClipX; }
    if (rnY_ < nClipY) { rnHeight_ -= nClipY-rnY_; rnY_ = nClipY; }

    int r = nClipX+nClipWidth, b = nClipY+nClipHeight;
    if (rnX_+rnWidth_ > r) rnWidth_ = r - rnX_;
    if (rnY_+rnHeight_ > b) rnHeight_ = b - rnY_;

    // Return if there's anything left to draw
    return rnWidth_ > 0 && rnHeight_ > 0;
}

////////////////////////////////////////////////////////////////////////////////

void CScreen::Plot (int nX_, int nY_, BYTE bColour_)
{
    int nWidth = 1, nHeight = 1;

    if (Clip(nX_, nY_, nWidth, nHeight))
        GetHiResLine(nY_++)[nX_] = bColour_;
}

// Draw a line from horizontal or vertical a given point (no diagonal lines yet)
void CScreen::DrawLine (int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_)
{
    // Horizontal line?
    if (nWidth_)
    {
        nHeight_ = 1;
        if (Clip(nX_, nY_, nWidth_, nHeight_))
            memset(GetHiResLine(nY_++) + nX_, bColour_, nWidth_);
    }

    // Vertical line
    else if (nHeight_)
    {
        nWidth_ = 1;
        if (Clip(nX_, nY_, nWidth_, nHeight_))
            while (nHeight_--)
                GetHiResLine(nY_++)[nX_] = bColour_;
    }
}

// Draw a solid rectangle on the display
void CScreen::FillRect (int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_)
{
    if (Clip(nX_, nY_, nWidth_, nHeight_))
    {
        // Iterate through each line in the block
        while (nHeight_--)
            memset(GetHiResLine(nY_++) + nX_, bColour_, nWidth_);
    }
}

// Draw a rectangle outline
void CScreen::FrameRect (int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_)
{
    // Draw lines for top, left, right and bottom
    DrawLine(nX_, nY_, nWidth_, 0, bColour_);
    DrawLine(nX_, nY_, 0, nHeight_, bColour_);
    DrawLine(nX_+nWidth_-1, nY_, 0, nHeight_, bColour_);
    DrawLine(nX_, nY_+nHeight_-1, nWidth_, 0, bColour_);
}

// Draw an image from a matrix of palette colours
void CScreen::DrawImage (int nX_, int nY_, int nWidth_, int nHeight_, const BYTE* pcbData_, const BYTE* pcbPalette_)
{
    // Return if the image is entirely clipped
    int nX = nX_, nY = nY_, nWidth = nWidth_, nHeight = nHeight_;
    if (!Clip(nX, nY, nWidth, nHeight))
        return;

    // Draw the region within the clipping area
    for (int y = nY ; y < (nY+nHeight) ; y++)
    {
        const BYTE* pcbImage = pcbData_ + (y-nY_)*nWidth_;
        BYTE *pb = GetHiResLine(y);

        for (int x = nX ; x < (nX+nWidth) ; x++)
        {
            BYTE b1 = pcbPalette_[pcbImage[x-nX_]];

            if (b1)
                pb[x] = b1;
        }
    }
}

// Draw a proportionally spaced string of characters at a specified pixel position
void CScreen::DrawString (int nX_, int nY_, const char* pcsz_, BYTE bInk_, bool fBold_)
{
    int nFrom = max(nClipY,nY_);
    int nTo = nY_ + pFont->wHeight;
    nTo = min(nClipY+nClipHeight-1, nTo);

    if (nFrom > nTo)
        return;

    // Ensure the lines containing the character are hi-res
    for (int i = nFrom ; i < nTo ; i++)
        GetHiResLine(i);

    // Iterate thru the characters in the string
    for (BYTE bChar ; (bChar = *pcsz_++) ; )
    {
        // Out-of-range characters will be shown as an underscore
        if (bChar < pFont->bFirst || bChar > pFont->bLast)
            bChar = CHAR_UNKNOWN;

        // Look up the font data for the character
        const BYTE* pbData = pFont->pcbData + (bChar - pFont->bFirst) * pFont->wCharSize;

        // Pick up the spacing details, calculate the width and step back by the space preceding the character
        int nWidth = *pbData++ & 0x0f;

        // Only draw the character if the entire width fits inside the clipping area
        // Smarter clipping will only slow it down further, and can be added later if necessary
        if ((nX_ >= nClipX) && (nX_ < (nClipX+nClipWidth-nWidth)))
        {
            BYTE* pLine = GetLine(nFrom) + nX_;
            pbData += (nFrom - nY_);

            for (int i = nFrom ; i < nTo ; pLine += m_nPitch, i += 1)
            {
                BYTE bData = *pbData++;

                if (!fBold_)
                {
                    if (bData & 0x80) pLine[0] = bInk_;
                    if (bData & 0x40) pLine[1] = bInk_;
                    if (bData & 0x20) pLine[2] = bInk_;
                    if (bData & 0x10) pLine[3] = bInk_;
                    if (bData & 0x08) pLine[4] = bInk_;
                    if (bData & 0x04) pLine[5] = bInk_;
                    if (bData & 0x02) pLine[6] = bInk_;
                    if (bData & 0x01) pLine[7] = bInk_;
                }
                else
                {
                    if (bData & 0x80) pLine[0] = pLine[1] = bInk_;
                    if (bData & 0x40) pLine[1] = pLine[2] = bInk_;
                    if (bData & 0x20) pLine[2] = pLine[3] = bInk_;
                    if (bData & 0x10) pLine[3] = pLine[4] = bInk_;
                    if (bData & 0x08) pLine[4] = pLine[5] = bInk_;
                    if (bData & 0x04) pLine[5] = pLine[6] = bInk_;
                    if (bData & 0x02) pLine[6] = pLine[7] = bInk_;
                    if (bData & 0x01) pLine[7] = pLine[8] = bInk_;
                }
            }
        }

        // Move to the next character position
        nX_ += nWidth + 1 + fBold_;
    }
}

// Get the on-screen width required for a specified string if drawn proportionally
/*static*/ int CScreen::GetStringWidth (const char* pcsz_)
{
    int nWidth = 0;

    for (BYTE bChar ; bChar = *pcsz_++ ; )
    {
        // Out-of-range characters will be drawn as an underscore
        if (bChar < pFont->bFirst || bChar > pFont->bLast)
            bChar = CHAR_UNKNOWN;

        const BYTE* pChar = pFont->pcbData + (bChar - pFont->bFirst) * pFont->wCharSize;
        nWidth += (*pChar & 0xf) + CHAR_SPACING;
    }

    // Return the width, not including the final space
    return nWidth - CHAR_SPACING;
}

/*static*/ void CScreen::SetFont (FONT* pFont_)
{
    pFont = pFont_;
}
