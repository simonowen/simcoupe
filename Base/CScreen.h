// Part of SimCoupe - A SAM Coupe emulator
//
// CScreen.h: SAM screen handling, including on-screen display text
//
//  Copyright (c) 1999-2002  Simon Owen
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

#ifndef CSCREEN_H
#define CSCREEN_H

#include "Font.h"


const int CHAR_HEIGHT = 11;     // Character cell dimensions

const int CHAR_SPACING = 1;         // 1 pixel between each character
const char CHAR_UNKNOWN = '_';      // Character to display when not available in charset


class CScreen
{
    public:
        CScreen (int nWidth_, int nHeight_);
        ~CScreen ();

    public:
        BYTE* GetLine (int nLine_) { return m_ppbLines[nLine_]; }
        BYTE* GetLine (int nLine_, bool &rfHiRes_) { rfHiRes_ = IsHiRes(nLine_); return m_ppbLines[nLine_]; }
        BYTE* GetHiResLine (int nLine_, int nWidth_=WIDTH_BLOCKS);

        int GetPitch () const { return m_nPitch; }
        int GetWidth (int nLine_) const { return IsHiRes(nLine_) ? m_nPitch : (m_nPitch >> 1); }
        int GetHeight () const { return m_nHeight; }

        bool IsHiRes (int nLine_) const { return m_pfHiRes[nLine_]; }
        void SetHiRes (int nLine_, bool fHiRes_) { m_pfHiRes[nLine_] = fHiRes_; }
        bool* GetHiRes () { return m_pfHiRes; }

    public:
        void Clear ();

        void SetClip (int nX_=0, int nY_=0, int nWidth_=0, int nHeight_=0);
        bool Clip (int& rnX_, int& rnY_, int& rnWidth_, int& rnHeight_);

        void Plot (int nX_, int nY_, BYTE bColour_);
        void DrawLine (int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_);
        void FillRect (int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_);
        void FrameRect (int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_, bool fRound_=false);
        void DrawImage (int nX_, int nY_, int nWidth_, int nHeight_, const BYTE* pbData_, const BYTE* pbPalette_);
        void DrawString (int nX_, int nY_, const char* pcsz_, BYTE bInk_, bool fBold_=false);

        static int GetStringWidth (const char* pcsz_, bool fBold_=false);
        static void SetFont (const GUIFONT* pFont_, bool fFixedWidth_=false);

    protected:
        int m_nPitch, m_nHeight;    // Pitch (width of low-res lines is half the pitch) and height of the screen

        BYTE *m_pbFrame;            // Screen data block
        bool* m_pfHiRes;            // Array of bools for whether each line is hi-res or not
        BYTE **m_ppbLines;          // Look-up table from line number to pointer to start of the line
};

#endif  // CSCREEN_H
