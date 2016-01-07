// Part of SimCoupe - A SAM Coupe emulator
//
// Frame.h: Display frame generation
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
//  Contains portions of the drawing code are from the original SAMGRX.C
//  ASIC artefact during mode change identified by Dave Laundon

#ifndef FRAME_H
#define FRAME_H

#include "CPU.h"
#include "IO.h"
#include "Screen.h"
#include "Util.h"


namespace Frame
{
    bool Init (bool fFirstInit_=false);
    void Exit (bool fReInit_=false);

    void Flyback ();
    void Begin ();
    void End ();

    void Update ();

    void TouchLines (int nFrom_, int nTo_);
    inline void TouchLine (int nLine_) { TouchLines(nLine_, nLine_); }

    void GetAsicData (BYTE *pb0_, BYTE *pb1_, BYTE *pb2_, BYTE *pb3_);
    void ChangeMode (BYTE bNewVmpr_);
    void ChangeScreen (BYTE bNewBorder_);

    void Sync ();
    void Redraw ();
    void SaveScreenshot ();

    int GetWidth ();
    int GetHeight ();
    int GetRasterPos (int *pnLine_);
    void SetView (UINT uBlocks_, UINT uLines_);

    void SetStatus (const char *pcszFormat_, ...);
}


inline bool IsScreenLine (int nLine_) { return nLine_ >= (TOP_BORDER_LINES) && nLine_ < (TOP_BORDER_LINES+SCREEN_LINES); }
inline BYTE AttrBg (BYTE bAttr_) { return (((bAttr_) >> 3) & 0xf); }
inline BYTE AttrFg (BYTE bAttr_) { return ((((bAttr_) >> 3) & 8) | ((bAttr_) & 7)); }


extern bool fDrawFrame, g_fFlashPhase;
extern int nFrame;

extern int s_nWidth, s_nHeight;         // in mode 3 pixels
extern int s_nViewTop, s_nViewBottom;   // in lines
extern int s_nViewLeft, s_nViewRight;   // in screen blocks

extern WORD g_awMode1LineToByte[SCREEN_LINES];

////////////////////////////////////////////////////////////////////////////////

// Generic base for all screen classes
class CFrame
{
    typedef void (CFrame::* FNLINEUPDATE)(BYTE *pbLine_, int nLine_, int nFrom_, int nTo_);

    public:
        CFrame () : m_pLineUpdate(&CFrame::Mode1Line), m_pbScreenData(nullptr) { }
        CFrame (const CFrame &) = delete;
        void operator= (const CFrame &) = delete;
        virtual ~CFrame () = default;

    public:
        void SetMode (BYTE bVal_);
        void UpdateLine (CScreen *pScreen_, int nLine_, int nFrom_, int nTo_);
        void GetAsicData (BYTE *pb0_, BYTE *pb1_, BYTE *pb2_, BYTE *pb3_);

        void ModeChange (BYTE *pbLine_, int nLine_, int nBlock_, BYTE bNewVmpr_);
        void ScreenChange (BYTE *pbLine_, int nLine_, int nBlock_, BYTE bNewBorder_);

    protected:
        void BorderLine (BYTE *pbLine_, int nFrom_, int nTo_);
        void BlackLine (BYTE *pbLine_, int nFrom_, int nTo_);
        void LeftBorder (BYTE *pbLine_, int nFrom_, int nTo_);
        void RightBorder (BYTE *pbLine_, int nFrom_, int nTo_);

        void Mode1Line (BYTE *pbLine_, int nLine_, int nFrom_, int nTo_);
        void Mode2Line (BYTE *pbLine_, int nLine_, int nFrom_, int nTo_);
        void Mode3Line (BYTE *pbLine_, int nLine_, int nFrom_, int nTo_);
        void Mode4Line (BYTE *pbLine_, int nLine_, int nFrom_, int nTo_);

    protected:
        FNLINEUPDATE m_pLineUpdate;     // Function used to draw current mode
        BYTE *m_pbScreenData;           // Cached pointer to start of RAM page containing video memory
};

////////////////////////////////////////////////////////////////////////////////

inline void CFrame::LeftBorder (BYTE *pbLine_, int nFrom_, int nTo_)
{
    int nFrom = std::max(s_nViewLeft, nFrom_), nTo = std::min(nTo_, BORDER_BLOCKS);

    // Draw the required section of the left border, if any
    if (nFrom < nTo)
        memset(pbLine_ + ((nFrom-s_nViewLeft) << 4), clut[border_col], (nTo - nFrom) << 4);
}

inline void CFrame::RightBorder (BYTE *pbLine_, int nFrom_, int nTo_)
{
    int nFrom = std::max((WIDTH_BLOCKS-BORDER_BLOCKS), nFrom_), nTo = std::min(nTo_, s_nViewRight);

    // Draw the required section of the right border, if any
    if (nFrom < nTo)
        memset(pbLine_ + ((nFrom-s_nViewLeft) << 4), clut[border_col], (nTo - nFrom) << 4);
}

inline void CFrame::BorderLine (BYTE *pbLine_, int nFrom_, int nTo_)
{
    // Work out the range that within the visible area
    int nFrom = std::max(s_nViewLeft, nFrom_), nTo = std::min(nTo_, s_nViewRight);

    // Draw the required section of the border, if any
    if (nFrom < nTo)
        memset(pbLine_ + ((nFrom-s_nViewLeft) << 4), clut[border_col], (nTo - nFrom) << 4);
}

inline void CFrame::BlackLine (BYTE *pbLine_, int nFrom_, int nTo_)
{
    // Work out the range that within the visible area
    int nFrom = std::max(s_nViewLeft, nFrom_), nTo = std::min(nTo_, s_nViewRight);

    // Draw the required section of the left border, if any
    if (nFrom < nTo)
        memset(pbLine_ + ((nFrom-s_nViewLeft) << 4), 0, (nTo - nFrom) << 4);
}


inline void CFrame::Mode1Line (BYTE *pbLine_, int nLine_, int nFrom_, int nTo_)
{
    nLine_ -= TOP_BORDER_LINES;

    // Draw the required section of the left border, if any
    LeftBorder(pbLine_, nFrom_, nTo_);

    // Work out the range that within the visible area
    int nFrom = std::max(BORDER_BLOCKS, nFrom_), nTo = std::min(nTo_, BORDER_BLOCKS+SCREEN_BLOCKS);

    // Draw the required section of the main screen, if any
    if (nFrom < nTo)
    {
        BYTE *pFrame = pbLine_ + ((nFrom - s_nViewLeft) << 4);
        BYTE *pbDataMem = m_pbScreenData + g_awMode1LineToByte[nLine_] + (nFrom - BORDER_BLOCKS);
        BYTE *pbAttrMem = m_pbScreenData + 6144 + ((nLine_ & 0xf8) << 2) + (nFrom - BORDER_BLOCKS);

        // The actual screen line
        for (int i = nFrom; i < nTo; i++)
        {
            BYTE bData = *pbDataMem++, bAttr = *pbAttrMem++, bInk = AttrFg(bAttr), bPaper = AttrBg(bAttr);

            // toggle the colours if we're in the inverse part of the FLASH cycle
            if (g_fFlashPhase && (bAttr & 0x80))
                std::swap(bInk, bPaper);

            BYTE ink = clut[bInk], paper = clut[bPaper];

            pFrame[0]  = pFrame[1]  = (bData & 0x80) ? ink : paper;
            pFrame[2]  = pFrame[3]  = (bData & 0x40) ? ink : paper;
            pFrame[4]  = pFrame[5]  = (bData & 0x20) ? ink : paper;
            pFrame[6]  = pFrame[7]  = (bData & 0x10) ? ink : paper;
            pFrame[8]  = pFrame[9]  = (bData & 0x08) ? ink : paper;
            pFrame[10] = pFrame[11] = (bData & 0x04) ? ink : paper;
            pFrame[12] = pFrame[13] = (bData & 0x02) ? ink : paper;
            pFrame[14] = pFrame[15] = (bData & 0x01) ? ink : paper;

            pFrame += 16;
        }
    }

    // Draw the required section of the right border, if any
    RightBorder(pbLine_, nFrom_, nTo_);
}

inline void CFrame::Mode2Line (BYTE *pbLine_, int nLine_, int nFrom_, int nTo_)
{
    nLine_ -= TOP_BORDER_LINES;

    // Draw the required section of the left border, if any
    LeftBorder(pbLine_, nFrom_, nTo_);

    // Work out the range that within the visible area
    int nFrom = std::max(BORDER_BLOCKS, nFrom_), nTo = std::min(nTo_, BORDER_BLOCKS+SCREEN_BLOCKS);

    // Draw the required section of the main screen, if any
    if (nFrom < nTo)
    {
        BYTE *pFrame = pbLine_ + ((nFrom - s_nViewLeft) << 4);
        BYTE *pbDataMem = m_pbScreenData + (nLine_ << 5) + (nFrom - BORDER_BLOCKS);
        BYTE *pbAttrMem = pbDataMem + 0x2000;

        // The actual screen line
        for (int i = nFrom; i < nTo; i++)
        {
            BYTE bData = *pbDataMem++, bAttr = *pbAttrMem++, bInk = AttrFg(bAttr), bPaper = AttrBg(bAttr);

            // toggle the colours if we're in the inverse part of the FLASH cycle
            if (g_fFlashPhase && (bAttr & 0x80))
                std::swap(bInk, bPaper);

            BYTE ink = clut[bInk], paper = clut[bPaper];

            pFrame[0]  = pFrame[1]  = (bData & 0x80) ? ink : paper;
            pFrame[2]  = pFrame[3]  = (bData & 0x40) ? ink : paper;
            pFrame[4]  = pFrame[5]  = (bData & 0x20) ? ink : paper;
            pFrame[6]  = pFrame[7]  = (bData & 0x10) ? ink : paper;
            pFrame[8]  = pFrame[9]  = (bData & 0x08) ? ink : paper;
            pFrame[10] = pFrame[11] = (bData & 0x04) ? ink : paper;
            pFrame[12] = pFrame[13] = (bData & 0x02) ? ink : paper;
            pFrame[14] = pFrame[15] = (bData & 0x01) ? ink : paper;

            pFrame += 16;
        }
    }

    // Draw the required section of the right border, if any
    RightBorder(pbLine_, nFrom_, nTo_);
}

inline void CFrame::Mode3Line (BYTE *pbLine_, int nLine_, int nFrom_, int nTo_)
{
    nLine_ -= TOP_BORDER_LINES;

    // Draw the required section of the left border, if any
    LeftBorder(pbLine_, nFrom_, nTo_);

    // Work out the range that within the visible area
    int nFrom = std::max(BORDER_BLOCKS, nFrom_), nTo = std::min(nTo_, BORDER_BLOCKS+SCREEN_BLOCKS);

    // Draw the required section of the main screen, if any
    if (nFrom < nTo)
    {
        BYTE *pFrame = pbLine_ + ((nFrom - s_nViewLeft) << 4);
        BYTE *pbDataMem = m_pbScreenData + (nLine_ << 7) + ((nFrom - BORDER_BLOCKS) << 2);

        // The actual screen line
        for (int i = nFrom; i < nTo; i++)
        {
            BYTE bData;

            bData = pbDataMem[0];
            pFrame[0] = mode3clut[ bData         >> 6];
            pFrame[1] = mode3clut[(bData & 0x30) >> 4];
            pFrame[2] = mode3clut[(bData & 0x0c) >> 2];
            pFrame[3] = mode3clut[(bData & 0x03)     ];

            bData = pbDataMem[1];
            pFrame[4] = mode3clut[ bData         >> 6];
            pFrame[5] = mode3clut[(bData & 0x30) >> 4];
            pFrame[6] = mode3clut[(bData & 0x0c) >> 2];
            pFrame[7] = mode3clut[(bData & 0x03)     ];

            bData = pbDataMem[2];
            pFrame[8]  = mode3clut[ bData         >> 6];
            pFrame[9]  = mode3clut[(bData & 0x30) >> 4];
            pFrame[10] = mode3clut[(bData & 0x0c) >> 2];
            pFrame[11] = mode3clut[(bData & 0x03)     ];

            bData = pbDataMem[3];
            pFrame[12] = mode3clut[ bData         >> 6];
            pFrame[13] = mode3clut[(bData & 0x30) >> 4];
            pFrame[14] = mode3clut[(bData & 0x0c) >> 2];
            pFrame[15] = mode3clut[(bData & 0x03)     ];

            pFrame += 16;

            pbDataMem += 4;
        }
    }

    // Draw the required section of the right border, if any
    RightBorder(pbLine_, nFrom_, nTo_);
}

inline void CFrame::Mode4Line (BYTE *pbLine_, int nLine_, int nFrom_, int nTo_)
{
    nLine_ -= TOP_BORDER_LINES;

    // Draw the required section of the left border, if any
    LeftBorder(pbLine_, nFrom_, nTo_);

    // Work out the range that within the visible area
    int nFrom = std::max(BORDER_BLOCKS, nFrom_), nTo = std::min(nTo_, BORDER_BLOCKS+SCREEN_BLOCKS);

    // Draw the required section of the main screen, if any
    if (nFrom < nTo)
    {
        BYTE *pFrame = pbLine_ + ((nFrom - s_nViewLeft) << 4);
        BYTE *pbDataMem = ((nFrom - BORDER_BLOCKS) << 2) + m_pbScreenData + (nLine_ << 7);

        // The actual screen line
        for (int i = nFrom; i < nTo; i++)
        {
            BYTE bData;

            bData = pbDataMem[0];
            pFrame[0]  = pFrame[1]  = clut[bData >> 4];
            pFrame[2]  = pFrame[3]  = clut[bData & 0x0f];

            bData = pbDataMem[1];
            pFrame[4]  = pFrame[5]  = clut[bData >> 4];
            pFrame[6]  = pFrame[7]  = clut[bData & 0x0f];

            bData = pbDataMem[2];
            pFrame[8]  = pFrame[9]  = clut[bData >> 4];
            pFrame[10] = pFrame[11] = clut[bData & 0x0f];

            bData = pbDataMem[3];
            pFrame[12] = pFrame[13] = clut[bData >> 4];
            pFrame[14] = pFrame[15] = clut[bData & 0x0f];

            pFrame += 16;
            pbDataMem += 4;
        }
    }

    // Draw the required section of the right border, if any
    RightBorder(pbLine_, nFrom_, nTo_);
}

inline void CFrame::ModeChange (BYTE *pbLine_, int nLine_, int nBlock_, BYTE bNewVmpr_)
{
    int nScreenLine = nLine_ - TOP_BORDER_LINES;
    BYTE ab[4];

    // Fetch the 4 display data bytes for the original mode
    BYTE b0, b1, b2, b3;
    GetAsicData(&b0, &b1, &b2, &b3);

    // Perform the necessary massaging the ASIC does to prepare for display
    if (VMPR_MODE_3_OR_4)
    {
        // Mode 3+4
        ab[0] = ab[1] = ( b0       & 0x80) | ((b0 << 3) & 0x40) |
                        ((b1 >> 2) & 0x20) | ((b1 << 1) & 0x10) |
                        ((b2 >> 4) & 0x08) | ((b2 >> 1) & 0x04) |
                        ((b3 >> 6) & 0x02) | ( b3       & 0x01);
        ab[2] = ab[3] = b2;
    }
    else
    {
        // Mode 1+2
        ab[0] = (b0 & 0x77) | ( b0       & 0x80) | ((b0 >> 3) & 0x08);
        ab[1] = (b1 & 0x77) | ((b1 << 2) & 0x80) | ((b1 >> 1) & 0x08);
        ab[2] = (b2 & 0x77) | ((b0 << 4) & 0x80) | ((b0 << 1) & 0x08);
        ab[3] = (b3 & 0x77) | ((b1 << 6) & 0x80) | ((b1 << 3) & 0x08);
    }

    // The target mode decides how the data actually appears in the transition block
    switch (bNewVmpr_ & VMPR_MODE_MASK)
    {
        case MODE_1:
        {
            // Determine data+attr location for current cell, and preserve the values at each location
            BYTE *pData = m_pbScreenData + g_awMode1LineToByte[nScreenLine] + (nBlock_ - BORDER_BLOCKS), bData = *pData;
            BYTE *pAttr = m_pbScreenData + 6144 + ((nScreenLine & 0xf8) << 2) + (nBlock_ - BORDER_BLOCKS), bAttr = *pAttr;

            // Write the artefact bytes from the old mode, and draw the cell
            *pData = ab[0];
            *pAttr = ab[2];
            Mode1Line(pbLine_, nLine_, nBlock_, nBlock_+1);

            // Restore the original data+attr bytes
            *pData = bData;
            *pAttr = bAttr;
            break;
        }

        case MODE_2:
        {
            BYTE *pData = m_pbScreenData + (nScreenLine << 5) + (nBlock_ - BORDER_BLOCKS), bData = *pData;
            BYTE *pAttr = pData + 0x2000, bAttr = *pAttr;

            *pData = ab[0];
            *pAttr = ab[2];
            Mode2Line(pbLine_, nLine_, nBlock_, nBlock_+1);

            *pData = bData;
            *pAttr = bAttr;
            break;
        }

        default:
        {
            BYTE *pb = m_pbScreenData + (nScreenLine << 7) + ((nBlock_ - BORDER_BLOCKS) << 2);
            DWORD *pdw = reinterpret_cast<DWORD*>(pb), dw = *pdw;

            pb[0] = ab[0];
            pb[1] = ab[1];
            pb[2] = ab[2];
            pb[3] = ab[3];

            if ((bNewVmpr_ & VMPR_MODE_MASK) == MODE_3)
                Mode3Line(pbLine_, nLine_, nBlock_, nBlock_+1);
            else
                Mode4Line(pbLine_, nLine_, nBlock_, nBlock_+1);

            *pdw = dw;
            break;
        }
    }
}

inline void CFrame::ScreenChange (BYTE *pbLine_, int /*nLine_*/, int nBlock_, BYTE bNewBorder_)
{
    BYTE *pFrame = pbLine_ + ((nBlock_ - s_nViewLeft) << 4);

    // Part of the first pixel is the previous border colour, from when the screen was disabled.
    // We don't have the resolution to show only part, but using the most significant colour bits
    // in the least significant position will reduce the intensity enough to be close
    pFrame[0] = clut[border_col] >> 4;

    // The rest of the cell is the new border colour, even on the main screen since the ASIC has no data!
                 pFrame[1]  = pFrame[2]  = pFrame[3]  =
    pFrame[4]  = pFrame[5]  = pFrame[6]  = pFrame[7]  =
    pFrame[8]  = pFrame[9]  = pFrame[10] = pFrame[11] =
    pFrame[12] = pFrame[13] = pFrame[14] = pFrame[15] = clut[BORD_COL(bNewBorder_)];
}

#endif  // FRAME_H
