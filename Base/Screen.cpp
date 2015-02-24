// Part of SimCoupe - A SAM Coupe emulator
//
// Screen.cpp: SAM screen handling, including on-screen display text
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


static int nClipX, nClipY, nClipWidth, nClipHeight;    // Clip box for any screen drawing

static const GUIFONT* pFont = &sGUIFont;


CScreen::CScreen (int nWidth_, int nHeight_)
{
    m_nPitch = nWidth_ & ~15;   // Round down to the nearest mode 3 screen block chunk
    m_nHeight = nHeight_;

    m_pbFrame = new BYTE [m_nPitch * m_nHeight];

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
    delete[] m_ppbLines;
}

////////////////////////////////////////////////////////////////////////////////

void CScreen::Clear ()
{
    memset(m_pbFrame, 0, m_nPitch * m_nHeight);
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
        GetLine(nY_++)[nX_] = bColour_;
}

// Draw a line from horizontal or vertical a given point (no diagonal lines yet)
void CScreen::DrawLine (int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_)
{
    // Horizontal line?
    if (nWidth_ > 0)
    {
        nHeight_ = 1;
        if (Clip(nX_, nY_, nWidth_, nHeight_))
            memset(GetLine(nY_++) + nX_, bColour_, nWidth_);
    }

    // Vertical line
    else if (nHeight_ > 0)
    {
        nWidth_ = 1;
        if (Clip(nX_, nY_, nWidth_, nHeight_))
            while (nHeight_--)
                GetLine(nY_++)[nX_] = bColour_;
    }
}

// Draw a solid rectangle on the display
void CScreen::FillRect (int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_)
{
    if (Clip(nX_, nY_, nWidth_, nHeight_))
    {
        // Iterate through each line in the block
        while (nHeight_--)
            memset(GetLine(nY_++) + nX_, bColour_, nWidth_);
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
        BYTE *pb = GetLine(y);

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
        memcpy(GetLine(nY_++) + nX_, pcbData_+nX_-nX, nWidth);
}


// Draw a proportionally spaced string of characters at a specified pixel position
int CScreen::DrawString (int nX_, int nY_, const char* pcsz_, BYTE bInk_/*=WHITE*/)
{
    bool fColour = true;
    BYTE bDefaultInk = bInk_;
    int nLeft = nX_;

    // Iterate through characters in the string, stopping if we hit the character limit
    for (BYTE bChar ; (bChar = *pcsz_++) ; )
    {
        // Newline?
        if (bChar == '\n')
        {
            nX_ = nLeft;
            nY_ += pFont->wHeight + LINE_SPACING;
            continue;
        }
        // Embedded colour code?
        else if (bChar == '\a')
        {
            BYTE bColour = *pcsz_;
            if (bColour) pcsz_++;

            // Ignore colours if disabled
            if (!fColour)
                continue;

            switch (bColour)
            {
                // Subtle colours
                case 'k': bInk_ = BLACK;        break;
                case 'b': bInk_ = BLUE_8;       break;
                case 'r': bInk_ = RED_8;        break;
                case 'm': bInk_ = MAGENTA_8;    break;
                case 'g': bInk_ = GREEN_8;      break;
                case 'c': bInk_ = CYAN_8;       break;
                case 'y': bInk_ = YELLOW_8;     break;
                case 'w': bInk_ = GREY_6;       break;

                // Bright colours
                case 'K': bInk_ = GREY_5;       break;
                case 'B': bInk_ = BLUE_5;       break;
                case 'R': bInk_ = RED_5;        break;
                case 'M': bInk_ = MAGENTA_5;    break;
                case 'G': bInk_ = GREEN_5;      break;
                case 'C': bInk_ = CYAN_5;       break;
                case 'Y': bInk_ = YELLOW_5;     break;
                case 'W': bInk_ = WHITE;        break;

                // Disable or enable colour code processing
                case '0': fColour = false; bDefaultInk = bInk_; break;
                case '1': fColour = true;       break;

                // End colour block, return to default
                case 'X': bInk_ = bDefaultInk; break;
            }

            continue;
        }

        // Out-of-range characters will be shown as an underscore
        if (bChar < pFont->bFirst || bChar > pFont->bLast)
            bChar = CHAR_UNKNOWN;

        // Look up the font data for the character
        const BYTE* pbData = pFont->pcbData + (bChar - pFont->bFirst) * pFont->wCharSize;

        // Retrieve the character width
        int nWidth = (*pbData++ & 0x0f) + pFont->wWidth;

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

        // Only draw the character if it's not a space, and the entire width fits inside the clipping area
        if (bChar != ' ' && (nX_ >= nClipX) && (nX_+nWidth <= nClipX+nClipWidth))
        {
            BYTE* pLine = GetLine(nFrom) + nX_;
            pbData += (nFrom - nY_);

            for (int i = nFrom ; i < nTo ; pLine += m_nPitch, i += 1)
            {
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
        }

        // Move to the next character position
        nX_ += nWidth + CHAR_SPACING;
    }

	return 0;
}

// Formatted string drawing, in white by default
int CScreen::Printf (int nX_, int nY_, const char* pcszFormat_, ...)
{
    va_list args;
    va_start(args, pcszFormat_);

    char sz[512];
    vsnprintf(sz, sizeof(sz)-1, pcszFormat_, args);
    sz[sizeof(sz)-1] = '\0';

    va_end(args);

    return DrawString(nX_, nY_, sz, WHITE);
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
        // Colour codes are ignored
        else if (bChar == '\a')
        {
            // Skip the colour unless at the end of string
            if (*pcsz_)
            {
                pcsz_++;
                nMaxChars_++;
            }
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
            nWidth += (nWidth ? CHAR_SPACING : 0) + (*pChar & 0xf) + pFont->wWidth;
        }

        // Update the maximum segment width
        nMaxWidth = max(nWidth,nMaxWidth);
    }

    return nMaxWidth;
}

/*static*/ void CScreen::SetFont (const GUIFONT* pFont_)
{
    pFont = pFont_;
}
