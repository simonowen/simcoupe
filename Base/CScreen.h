// Part of SimCoupe - A SAM Coupé emulator
//
// CScreen.h: SAM screen handling, including on-screen display text
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

#ifndef CSCREEN_H
#define CSCREEN_H

class CScreen
{
    public:
        CScreen (int nWidth_, int nHeight_);
        ~CScreen ();

    public:
        BYTE* GetLine (int nLine_) { return m_ppbLines[nLine_]; }
        BYTE* GetLine (int nLine_, bool &rfHiRes_) { rfHiRes_ = IsHiRes(nLine_); return m_ppbLines[nLine_]; }
        BYTE* GetHiResLine (int nLine_, int nWidth_=WIDTH_BLOCKS);

        bool IsHiRes (int nLine_) const { return m_pfHiRes[nLine_]; }
        void SetHiRes (int nLine_, bool fHiRes_) { m_pfHiRes[nLine_] = fHiRes_; }

        int GetPitch () const { return m_nPitch; }
        int GetWidth (int nLine_) const { return IsHiRes(nLine_) ? m_nPitch : (m_nPitch >> 1); }
        int GetHeight () const { return m_nHeight; }

    public:
        void Clear ();

        void MoveTo(int nX_, int nY_) { m_nX = nX_; m_nY = nY_; }
        void FillRect (int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_);

        int GetStringWidth (const char* pcsz_);
        void DrawString (int nX_, int nY_, const char* pcsz_, BYTE bInk_);
        void DrawOpaqueChar (BYTE bChar_, BYTE bInk_=0x7f, BYTE bPaper_=0x00);
        void DrawOpaqueString (int nX_, int nY_, const char* pcsz_, BYTE bInk_=0x7f, BYTE bPaper_=0x00);

    public:
        int m_nPitch, m_nHeight;    // Pitch (width of low-res lines is half the pitch) and height of the screen

        BYTE *m_pbFrame;            // Screen data block
        bool* m_pfHiRes;            // Array of bools for whether each line is hi-res or not
        BYTE **m_ppbLines;          // Look-up table from line number to pointer to start of the line

        int m_nX, m_nY;             // Current drawing position
};

#endif  // CSCREEN_H
