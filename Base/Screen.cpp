// Part of SimCoupe - A SAM Coupe emulator
//
// Screen.cpp: SAM screen handling, including on-screen display text
//
//  Copyright (c) 1999-2012 Simon Owen
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
#include "Screen.h"

#include "Font.h"


int nClipX, nClipY, nClipWidth, nClipHeight;    // Clip box for any screen drawing

const GUIFONT* pFont = &sGUIFont;


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
    delete[] m_pbFrame;
    delete[] m_pfHiRes;
    delete[] m_ppbLines;
}

////////////////////////////////////////////////////////////////////////////////

// Return the address of a high-res version of a line, converting from lo-res if necessary
BYTE* CScreen::GetHiResLine (int nLine_, int nWidth_/*=WIDTH_BLOCKS*/)
{
    bool fHiRes;
    BYTE* pbLine = GetLine(nLine_, fHiRes);
    WORD *pwLine = reinterpret_cast<WORD*>(pbLine);

    // If the line is already hi-res, return the pointer to it
    if (fHiRes)
        return pbLine;

    // Limit the amount converted to the maximum visible width
    nWidth_ = min(nWidth_, (m_nPitch >> 4)) << 3;

    // Double up each pixel on the line
    for (int i = nWidth_-1 ; i >= 0 ; i -= 8)
    {
        pwLine[i-0] = pbLine[i-0] * 0x0101;
        pwLine[i-1] = pbLine[i-1] * 0x0101;
        pwLine[i-2] = pbLine[i-2] * 0x0101;
        pwLine[i-3] = pbLine[i-3] * 0x0101;
        pwLine[i-4] = pbLine[i-4] * 0x0101;
        pwLine[i-5] = pbLine[i-5] * 0x0101;
        pwLine[i-6] = pbLine[i-6] * 0x0101;
        pwLine[i-7] = pbLine[i-7] * 0x0101;
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
    if (nWidth_ > 0)
    {
        nHeight_ = 1;
        if (Clip(nX_, nY_, nWidth_, nHeight_))
            memset(GetHiResLine(nY_++) + nX_, bColour_, nWidth_);
    }

    // Vertical line
    else if (nHeight_ > 0)
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
void CScreen::FrameRect (int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_, bool fRound_/*=false*/)
{
    // Single pixel width or height boxes can be drawn more efficiently
    if (nWidth_ == 1)
        DrawLine(nX_, nY_, 0, nHeight_, bColour_);
    else if (nHeight_ == 1)
        DrawLine(nX_, nY_, nWidth_, 0, bColour_);
    else
    {
        // Rounding offsets, if required
        int nR = fRound_ ? 1 : 0, nR2 = nR+nR;

        // Draw lines for top, left, right and bottom
        DrawLine(nX_+nR, nY_, nWidth_-nR2, 0, bColour_);
        DrawLine(nX_, nY_+nR, 0, nHeight_-nR2, bColour_);
        DrawLine(nX_+nWidth_-1, nY_+nR, 0, nHeight_-nR2, bColour_);
        DrawLine(nX_+nR, nY_+nHeight_-1, nWidth_-nR2, 0, bColour_);
    }
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
            BYTE i = pcbImage[x-nX_];

            if (i)
                pb[x] = pcbPalette_[i];
        }
    }
}

// Copy a line of raw data to a specified point on the screen
void CScreen::Poke (int nX_, int nY_, const BYTE* pcbData_, UINT uLen_)
{
    int nWidth = static_cast<int>(uLen_), nHeight_ = 1, nX = nX_;

    if (Clip(nX_, nY_, nWidth, nHeight_))
        memcpy(GetHiResLine(nY_++) + nX_, pcbData_+nX_-nX, nWidth);
}


// Draw a proportionally spaced string of characters at a specified pixel position
void CScreen::DrawString (int nX_, int nY_, const char* pcsz_, BYTE bInk_, bool fBold_/*=false*/, size_t nMaxChars_/*=-1*/)
{
    int nLeft = nX_;

    // Iterate through characters in the string, stopping if we hit the character limit
    for (BYTE bChar ; (bChar = *pcsz_++) && nMaxChars_-- ; )
    {
        // Newline?
        if (bChar == '\n')
        {
            nX_ = nLeft;
            nY_ += pFont->wHeight + LINE_SPACING;
            continue;
        }

        // Out-of-range characters will be shown as an underscore
        if (bChar < pFont->bFirst || bChar > pFont->bLast)
            bChar = CHAR_UNKNOWN;

        // Look up the font data for the character
        const BYTE* pbData = pFont->pcbData + (bChar - pFont->bFirst) * pFont->wCharSize;

        // Retrieve the character width
        int nWidth = *pbData++ & 0x0f;

        // Draw as fixed-width?
        if (pFont->fFixedWidth)
        {
            // Fetch required shift to centralise character
            int nShift = pbData[-1] >> 4;

            // Offset to start, and subtract that from the total width
            nX_ += nShift;
            nWidth = pFont->wWidth - nShift;
        }

        // Determine the vertical extent we're drawing
        int nFrom = max(nClipY,nY_);
        int nTo = nY_ + pFont->wHeight;
        nTo = min(nClipY+nClipHeight-1, nTo);

        // Ensure the lines containing the character are hi-res
        for (int i = nFrom ; i < nTo ; i++)
            GetHiResLine(i);

#ifdef USE_LOWRES
        // Double the width, to account for skipped pixels, and force an odd pixel position
        nWidth <<= 1;
        nX_ |= 1;
#endif
        // Only draw the character if it's not a space, and the entire width fits inside the clipping area
        if (bChar != ' ' && (nX_ >= nClipX) && (nX_+nWidth <= nClipX+nClipWidth))
        {
            BYTE* pLine = GetLine(nFrom) + nX_;
            pbData += (nFrom - nY_);

            for (int i = nFrom ; i < nTo ; pLine += m_nPitch, i += 1)
            {
                BYTE bData = *pbData++;

                if (!fBold_)
                {
#ifdef USE_LOWRES
                    // Draw ever other pixel, since they're the only visible ones in low-res mode
                    if (bData & 0x80) pLine[0]  = bInk_;
                    if (bData & 0x40) pLine[2]  = bInk_;
                    if (bData & 0x20) pLine[4]  = bInk_;
                    if (bData & 0x10) pLine[6]  = bInk_;
                    if (bData & 0x08) pLine[8]  = bInk_;
                    if (bData & 0x04) pLine[10] = bInk_;
                    if (bData & 0x02) pLine[12] = bInk_;
                    if (bData & 0x01) pLine[14] = bInk_;
#else
                    if (bData & 0x80) pLine[0] = bInk_;
                    if (bData & 0x40) pLine[1] = bInk_;
                    if (bData & 0x20) pLine[2] = bInk_;
                    if (bData & 0x10) pLine[3] = bInk_;
                    if (bData & 0x08) pLine[4] = bInk_;
                    if (bData & 0x04) pLine[5] = bInk_;
                    if (bData & 0x02) pLine[6] = bInk_;
                    if (bData & 0x01) pLine[7] = bInk_;
#endif
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
        nX_ += nWidth + CHAR_SPACING + fBold_;
    }
}

// Get the on-screen width required for a specified string if drawn proportionally
/*static*/ int CScreen::GetStringWidth (const char* pcsz_, size_t nMaxChars_/*=-1*/, const GUIFONT *pFont_/*=NULL*/)
{
    int nMaxWidth = 0;
    int nWidth = 0;

    // Use the current font if one isn't supplied
    if (!pFont_)
        pFont_ = pFont;

    for (BYTE bChar ; (bChar = *pcsz_) && nMaxChars_-- ; pcsz_++)
    {
        // Newlines reset the segment width
        if (bChar == '\n')
        {
            nWidth = 0;
            continue;
        }

        // Out-of-range characters will be drawn as an underscore
        if (bChar < pFont_->bFirst || bChar > pFont_->bLast)
            bChar = CHAR_UNKNOWN;

        // Add the new width, width a separator space if needed
        if (pFont_->fFixedWidth)
        {
            nWidth += (nWidth ? CHAR_SPACING : 0) + pFont_->wWidth;
        }
        else
        {
            const BYTE* pChar = pFont_->pcbData + (bChar - pFont_->bFirst) * pFont_->wCharSize;
            nWidth += (nWidth ? CHAR_SPACING : 0) + (*pChar & 0xf);
        }

        // Update the maximum segment width
        nMaxWidth = max(nWidth,nMaxWidth);
    }

#ifdef USE_LOWRES
    return nMaxWidth << 1; // Double-spaced pixels need twice the room
#else
    return nMaxWidth;
#endif
}

/*static*/ void CScreen::SetFont (const GUIFONT* pFont_)
{
    pFont = pFont_;
}
