// Part of SimCoupe - A SAM Coupe emulator
//
// Frame.h: Display frame generation
//
//  Copyright (c) 1999-2002  Simon Owen
//  Copyright (c) 1996-2001  Allan Skillman
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
#include "Profile.h"
#include "CScreen.h"
#include "Util.h"


class Frame
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

        static void Start ();

        static void Update ();
        static void Complete ();
        static void TouchLines (int nFrom_, int nTo_);
        static inline void TouchLine (int nLine_) { TouchLines(nLine_, nLine_); }
        static void ChangeMode (BYTE bVal_);

        static void Sync ();
        static void Clear ();
        static void Redraw ();
        static void SaveFrame (const char* pcszPath_=NULL);

        static CScreen* GetScreen ();
        static int GetWidth ();
        static int GetHeight ();

        static void SetStatus (const char *pcszFormat_, ...);
};


inline bool IsScreenLine (int nLine_) { return nLine_ >= (TOP_BORDER_LINES) && nLine_ < (TOP_BORDER_LINES+SCREEN_LINES); }
inline BYTE AttrBg (BYTE bAttr_) { return (((bAttr_) >> 3) & 0xf); }
inline BYTE AttrFg (BYTE bAttr_) { return ((((bAttr_) >> 3) & 8) | ((bAttr_) & 7)); }


extern bool fDrawFrame;
extern int g_nFrame;

extern int s_nWidth, s_nHeight;         // hi-res pixels
extern int s_nViewTop, s_nViewBottom;   // in lines
extern int s_nViewLeft, s_nViewRight;   // in screen blocks

extern BYTE *apbPageReadPtrs[],  *apbPageWritePtrs[];
extern WORD g_awMode1LineToByte[SCREEN_LINES];

// Generic base for all screen classes
class CFrame
{
    typedef void (CFrame::* FNLINEUPDATE)(int nLine_, int nFrom_, int nTo_);

    public:
        CFrame ()
        {
            m_pLineUpdate = &CFrame::Mode1Line;
        }

    public:
        void SetMode (BYTE bVal_)
        {
            static FNLINEUPDATE apfnLineUpdates[] =
                { &CFrame::Mode1Line, &CFrame::Mode2Line, &CFrame::Mode3Line, &CFrame::Mode4Line };

            m_pLineUpdate = apfnLineUpdates[(bVal_ & VMPR_MODE_MASK) >> 5];

            // Bit 0 of the VMPR page is always taken as zero for modes 3 and 4
            int nPage = (bVal_ & 0x40) ? (bVal_ & VMPR_PAGE_MASK & ~1) : bVal_ & VMPR_PAGE_MASK;
            m_pbScreenData = apbPageReadPtrs[nPage];
        }

        void UpdateLine (int nLine_, int nFrom_, int nTo_)
        {
            // Is the line within the view port?
            if (nLine_ >= s_nViewTop && nLine_ < s_nViewBottom)
            {
                // Screen off in mode 3 or 4?
                if (BORD_SOFF && VMPR_MODE_3_OR_4)
                    BlackLine(nLine_, nFrom_, nTo_);

                // Line on the main screen?
                else if (nLine_ >= TOP_BORDER_LINES && nLine_ < (TOP_BORDER_LINES+SCREEN_LINES))
                    (this->*m_pLineUpdate)(nLine_, nFrom_, nTo_);

                // Top or bottom border
                else// if (nLine_ < TOP_BORDER_LINES || nLine_ >= (TOP_BORDER_LINES+SCREEN_LINES))
                    BorderLine(nLine_, nFrom_, nTo_);
            }
        }

        virtual void ModeChange (BYTE bNewVal_, int nLine_, int nBlock_) = 0;

    protected:
        virtual void BorderLine (int nLine_, int nFrom_, int nTo_) = 0;
        virtual void BlackLine (int nLine_, int nFrom_, int nTo_) = 0;

        virtual void Mode1Line (int nLine_, int nFrom_, int nTo_) = 0;
        virtual void Mode2Line (int nLine_, int nFrom_, int nTo_) = 0;
        virtual void Mode3Line (int nLine_, int nFrom_, int nTo_) = 0;
        virtual void Mode4Line (int nLine_, int nFrom_, int nTo_) = 0;

    protected:
        FNLINEUPDATE m_pLineUpdate;     // Function used to draw current mode
        BYTE*   m_pbScreenData;         // Cached pointer to start of RAM page containing video memory
};


////////////////////////////////////////////////////////////////////////////////

// Template class for lo-res and hi-res drawing
template <bool fHiRes_>
class CFrameXx1 : public CFrame
{
    protected:
        void Mode1Line (int nLine_, int nFrom_, int nTo_);
        void Mode2Line (int nLine_, int nFrom_, int nTo_);
        void Mode3Line (int nLine_, int nFrom_, int nTo_);
        void Mode4Line (int nLine_, int nFrom_, int nTo_);
        void ModeChange (BYTE bNewVal_, int nLine_, int nBlock_);

    protected:
        void LeftBorder (BYTE* pbLine_, int nFrom_, int nTo_);
        void RightBorder (BYTE* pbLine_, int nFrom_, int nTo_);
        void BorderLine (int nLine_, int nFrom_, int nTo_);
        void BlackLine (int nLine_, int nFrom_, int nTo_);

        void memset_ (void* pv_, BYTE b_, int nSize_)
        {
            // Take advantage of the size always being divisible by sizeof DWORD
            DWORD dw = static_cast<DWORD>(b_) * 0x01010101, *pdw = reinterpret_cast<DWORD*>(pv_);

            // In fact it'll always be a multiple of 8 bytes (8 pixels in a block), so fill in DWORD pairs at a time
            for (nSize_ >>= 3 ; nSize_-- ; pdw += 2)
                pdw[0] = pdw[1] = dw;
        }
};


template <bool fHiRes_>
inline void CFrameXx1<fHiRes_>::LeftBorder (BYTE* pbLine_, int nFrom_, int nTo_)
{
    int nFrom = max(s_nViewLeft, nFrom_), nTo = min(nTo_, BORDER_BLOCKS);

    // Draw the required section of the left border, if any
    if (nFrom < nTo)
        memset_(pbLine_ + ((nFrom-s_nViewLeft) << (fHiRes_ ? 4 : 3)), clutval[border_col], (nTo - nFrom) << (fHiRes_ ? 4 : 3));
}

template <bool fHiRes_>
inline void CFrameXx1<fHiRes_>::RightBorder (BYTE* pbLine_, int nFrom_, int nTo_)
{
    int nFrom = max((WIDTH_BLOCKS-BORDER_BLOCKS), nFrom_), nTo = min(nTo_, s_nViewRight);

    // Draw the required section of the right border, if any
    if (nFrom < nTo)
        memset_(pbLine_ + ((nFrom-s_nViewLeft) << (fHiRes_ ? 4 : 3)), clutval[border_col], (nTo - nFrom) << (fHiRes_ ? 4 : 3));
}

template <bool fHiRes_>
inline void CFrameXx1<fHiRes_>::BorderLine (int nLine_, int nFrom_, int nTo_)
{
    BYTE* pbLine = Frame::GetScreen()->GetLine(nLine_-s_nViewTop);

    // Work out the range that within the visible area
    int nFrom = max(s_nViewLeft, nFrom_), nTo = min(nTo_, s_nViewRight);

    // Draw the required section of the border, if any
    if (nFrom < nTo)
        memset_(pbLine + ((nFrom-s_nViewLeft) << (fHiRes_ ? 4 : 3)), clutval[border_col], (nTo - nFrom) << (fHiRes_ ? 4 : 3));
}

template <bool fHiRes_>
inline void CFrameXx1<fHiRes_>::BlackLine (int nLine_, int nFrom_, int nTo_)
{
    BYTE* pbLine = Frame::GetScreen()->GetLine(nLine_-s_nViewTop);

    // Work out the range that within the visible area
    int nFrom = max(s_nViewLeft, nFrom_), nTo = min(nTo_, s_nViewRight);

    // Draw the required section of the left border, if any
    if (nFrom < nTo)
        memset_(pbLine + ((nFrom-s_nViewLeft) << (fHiRes_ ? 4 : 3)), 0, (nTo - nFrom) << (fHiRes_ ? 4 : 3));
}


template <bool fHiRes_>
void CFrameXx1<fHiRes_>::Mode1Line (int nLine_, int nFrom_, int nTo_)
{
    BYTE* pbLine = Frame::GetScreen()->GetLine(nLine_-s_nViewTop);
    nLine_ -= TOP_BORDER_LINES;

    // Draw the required section of the left border, if any
    LeftBorder(pbLine, nFrom_, nTo_);

    // Work out the range that within the visible area
    int nFrom = max(BORDER_BLOCKS, nFrom_), nTo = min(nTo_, BORDER_BLOCKS+SCREEN_BLOCKS);

    // Draw the required section of the main screen, if any
    if (nFrom < nTo)
    {
        BYTE* pFrame = pbLine + ((nFrom - s_nViewLeft) << (fHiRes_ ? 4 : 3));
        BYTE* pbDataMem = m_pbScreenData + g_awMode1LineToByte[nLine_] + (nFrom - BORDER_BLOCKS);
        BYTE* pbAttrMem = m_pbScreenData + 6144 + ((nLine_ & 0xf8) << 2) + (nFrom - BORDER_BLOCKS);

        // The actual screen line
        for (int i = nFrom; i < nTo; i++)
        {
            BYTE bData = *pbDataMem++, bAttr = *pbAttrMem++, bInk = AttrFg(bAttr), bPaper = AttrBg(bAttr);

            // toggle the colours if we're in the inverse part of the FLASH cycle
            if (g_fFlashPhase && (bAttr & 0x80))
                swap(bInk, bPaper);

            BYTE ink = clutval[bInk], paper = clutval[bPaper];

            if (!fHiRes_)
            {
                pFrame[0] = (bData & 0x80) ? ink : paper;
                pFrame[1] = (bData & 0x40) ? ink : paper;
                pFrame[2] = (bData & 0x20) ? ink : paper;
                pFrame[3] = (bData & 0x10) ? ink : paper;
                pFrame[4] = (bData & 0x08) ? ink : paper;
                pFrame[5] = (bData & 0x04) ? ink : paper;
                pFrame[6] = (bData & 0x02) ? ink : paper;
                pFrame[7] = (bData & 0x01) ? ink : paper;
            }
            else
            {
                pFrame[0]  = pFrame[1]  = (bData & 0x80) ? ink : paper;
                pFrame[2]  = pFrame[3]  = (bData & 0x40) ? ink : paper;
                pFrame[4]  = pFrame[5]  = (bData & 0x20) ? ink : paper;
                pFrame[6]  = pFrame[7]  = (bData & 0x10) ? ink : paper;
                pFrame[8]  = pFrame[9]  = (bData & 0x08) ? ink : paper;
                pFrame[10] = pFrame[11] = (bData & 0x04) ? ink : paper;
                pFrame[12] = pFrame[13] = (bData & 0x02) ? ink : paper;
                pFrame[14] = pFrame[15] = (bData & 0x01) ? ink : paper;
            }

            pFrame += fHiRes_ ? 16 : 8;
        }
    }

    // Draw the required section of the right border, if any
    RightBorder(pbLine, nFrom_, nTo_);
}

template <bool fHiRes_>
void CFrameXx1<fHiRes_>::Mode2Line (int nLine_, int nFrom_, int nTo_)
{
    BYTE* pbLine = Frame::GetScreen()->GetLine(nLine_-s_nViewTop);
    nLine_ -= TOP_BORDER_LINES;

    // Draw the required section of the left border, if any
    LeftBorder(pbLine, nFrom_, nTo_);

    // Work out the range that within the visible area
    int nFrom = max(BORDER_BLOCKS, nFrom_), nTo = min(nTo_, BORDER_BLOCKS+SCREEN_BLOCKS);

    // Draw the required section of the main screen, if any
    if (nFrom < nTo)
    {
        BYTE* pFrame = pbLine + ((nFrom - s_nViewLeft) << (fHiRes_ ? 4 : 3));
        BYTE* pbDataMem = m_pbScreenData + (nLine_ << 5) + (nFrom - BORDER_BLOCKS);
        BYTE* pbAttrMem = pbDataMem + 0x2000;

        // The actual screen line
        for (int i = nFrom; i < nTo; i++)
        {
            BYTE bData = *pbDataMem++, bAttr = *pbAttrMem++, bInk = AttrFg(bAttr), bPaper = AttrBg(bAttr);

            // toggle the colours if we're in the inverse part of the FLASH cycle
            if (g_fFlashPhase && (bAttr & 0x80))
                swap(bInk, bPaper);

            BYTE ink = clutval[bInk], paper = clutval[bPaper];

            if (!fHiRes_)
            {
                pFrame[0] = (bData & 0x80) ? ink : paper;
                pFrame[1] = (bData & 0x40) ? ink : paper;
                pFrame[2] = (bData & 0x20) ? ink : paper;
                pFrame[3] = (bData & 0x10) ? ink : paper;
                pFrame[4] = (bData & 0x08) ? ink : paper;
                pFrame[5] = (bData & 0x04) ? ink : paper;
                pFrame[6] = (bData & 0x02) ? ink : paper;
                pFrame[7] = (bData & 0x01) ? ink : paper;
            }
            else
            {
                pFrame[0]  = pFrame[1]  = (bData & 0x80) ? ink : paper;
                pFrame[2]  = pFrame[3]  = (bData & 0x40) ? ink : paper;
                pFrame[4]  = pFrame[5]  = (bData & 0x20) ? ink : paper;
                pFrame[6]  = pFrame[7]  = (bData & 0x10) ? ink : paper;
                pFrame[8]  = pFrame[9]  = (bData & 0x08) ? ink : paper;
                pFrame[10] = pFrame[11] = (bData & 0x04) ? ink : paper;
                pFrame[12] = pFrame[13] = (bData & 0x02) ? ink : paper;
                pFrame[14] = pFrame[15] = (bData & 0x01) ? ink : paper;
            }

            pFrame += fHiRes_ ? 16 : 8;
        }
    }

    // Draw the required section of the right border, if any
    RightBorder(pbLine, nFrom_, nTo_);
}

template <bool fHiRes_>
void CFrameXx1<fHiRes_>::Mode3Line (int nLine_, int nFrom_, int nTo_)
{
    BYTE* pbLine = Frame::GetScreen()->GetLine(nLine_-s_nViewTop);
    nLine_ -= TOP_BORDER_LINES;

    // Draw the required section of the left border, if any
    LeftBorder(pbLine, nFrom_, nTo_);

    // Work out the range that within the visible area
    int nFrom = max(BORDER_BLOCKS, nFrom_), nTo = min(nTo_, BORDER_BLOCKS+SCREEN_BLOCKS);

    // Draw the required hi-res section of the main screen, if any
    if (fHiRes_ && nFrom < nTo)
    {
        BYTE* pFrame = pbLine + ((nFrom - s_nViewLeft) << 4);
        BYTE* pbDataMem = m_pbScreenData + (nLine_ << 7) + ((nFrom - BORDER_BLOCKS) << 2);

        // The actual screen line
        for (int i = nFrom; i < nTo; i++)
        {
            BYTE bData;

            bData = pbDataMem[0];
            pFrame[0] = mode3clutval[ bData         >> 6];
            pFrame[1] = mode3clutval[(bData & 0x30) >> 4];
            pFrame[2] = mode3clutval[(bData & 0x0c) >> 2];
            pFrame[3] = mode3clutval[(bData & 0x03)     ];

            bData = pbDataMem[1];
            pFrame[4] = mode3clutval[ bData         >> 6];
            pFrame[5] = mode3clutval[(bData & 0x30) >> 4];
            pFrame[6] = mode3clutval[(bData & 0x0c) >> 2];
            pFrame[7] = mode3clutval[(bData & 0x03)     ];

            bData = pbDataMem[2];
            pFrame[8]  = mode3clutval[ bData         >> 6];
            pFrame[9]  = mode3clutval[(bData & 0x30) >> 4];
            pFrame[10] = mode3clutval[(bData & 0x0c) >> 2];
            pFrame[11] = mode3clutval[(bData & 0x03)     ];

            bData = pbDataMem[3];
            pFrame[12] = mode3clutval[ bData         >> 6];
            pFrame[13] = mode3clutval[(bData & 0x30) >> 4];
            pFrame[14] = mode3clutval[(bData & 0x0c) >> 2];
            pFrame[15] = mode3clutval[(bData & 0x03)     ];

            pFrame += 16;
            pbDataMem += 4;
        }
    }

    // Draw the required section of the right border, if any
    RightBorder(pbLine, nFrom_, nTo_);
}

template <bool fHiRes_>
void CFrameXx1<fHiRes_>::Mode4Line (int nLine_, int nFrom_, int nTo_)
{
    BYTE* pbLine = Frame::GetScreen()->GetLine(nLine_-s_nViewTop);
    nLine_ -= TOP_BORDER_LINES;

    // Draw the required section of the left border, if any
    LeftBorder(pbLine, nFrom_, nTo_);

    // Work out the range that within the visible area
    int nFrom = max(BORDER_BLOCKS, nFrom_), nTo = min(nTo_, BORDER_BLOCKS+SCREEN_BLOCKS);

    // Draw the required section of the main screen, if any
    if (nFrom < nTo)
    {
        BYTE* pFrame = pbLine + ((nFrom - s_nViewLeft) << (fHiRes_ ? 4 : 3));
        BYTE* pbDataMem = ((nFrom - BORDER_BLOCKS) << 2) + m_pbScreenData + (nLine_ << 7);

        // The actual screen line
        for (int i = nFrom; i < nTo; i++)
        {
            BYTE bData;

            if (!fHiRes_)
            {
                bData = pbDataMem[0];
                pFrame[0] = clutval[bData >> 4];
                pFrame[1] = clutval[bData & 0x0f];

                bData = pbDataMem[1];
                pFrame[2] = clutval[bData >> 4];
                pFrame[3] = clutval[bData & 0x0f];

                bData = pbDataMem[2];
                pFrame[4] = clutval[bData >> 4];
                pFrame[5] = clutval[bData & 0x0f];

                bData = pbDataMem[3];
                pFrame[6] = clutval[bData >> 4];
                pFrame[7] = clutval[bData & 0x0f];
            }
            else
            {
                bData = pbDataMem[0];
                pFrame[0]  = pFrame[1]  = clutval[bData >> 4];
                pFrame[2]  = pFrame[3]  = clutval[bData & 0x0f];

                bData = pbDataMem[1];
                pFrame[4]  = pFrame[5]  = clutval[bData >> 4];
                pFrame[6]  = pFrame[7]  = clutval[bData & 0x0f];

                bData = pbDataMem[2];
                pFrame[8]  = pFrame[9]  = clutval[bData >> 4];
                pFrame[10] = pFrame[11] = clutval[bData & 0x0f];

                bData = pbDataMem[3];
                pFrame[12] = pFrame[13] = clutval[bData >> 4];
                pFrame[14] = pFrame[15] = clutval[bData & 0x0f];
            }

            pFrame += fHiRes_ ? 16 : 8;
            pbDataMem += 4;
        }
    }

    // Draw the required section of the right border, if any
    RightBorder(pbLine, nFrom_, nTo_);
}


template <bool fHiRes_>
void CFrameXx1<fHiRes_>::ModeChange (BYTE bNewVal_, int nLine_, int nBlock_)
{
    int nScreenLine = nLine_ - TOP_BORDER_LINES;

    // Make sure the mode change is on the main screen
    if (nBlock_ < BORDER_BLOCKS || nBlock_ >= (BORDER_BLOCKS+SCREEN_BLOCKS))
        return;


    BYTE ab[4];

    // Source mode 3 or 4?
    if (VMPR_MODE_3_OR_4)
    {
        BYTE* pb = m_pbScreenData + (nScreenLine << 7) + ((nBlock_ - BORDER_BLOCKS) << 2);

        // Extract the 4 ASIC bytes used to display a block
        ab[0] = ab[1] = ( pb[0]       & 0x80) | ((pb[0] << 3) & 0x40) |
                        ((pb[1] >> 2) & 0x20) | ((pb[1] << 1) & 0x10) |
                        ((pb[2] >> 4) & 0x08) | ((pb[2] >> 1) & 0x04) |
                        ((pb[3] >> 6) & 0x02) | ( pb[3]       & 0x01);
        ab[2] = ab[3] = pb[2];
    }

    // Source mode 1 or 2
    else
    {
        BYTE* pData = m_pbScreenData + ((VMPR_MODE == MODE_1) ? g_awMode1LineToByte[nScreenLine] + (nBlock_ - BORDER_BLOCKS)
                                                                     : (nScreenLine << 5) + (nBlock_ - BORDER_BLOCKS));
        BYTE* pAttr = (VMPR_MODE == MODE_1) ? m_pbScreenData + 6144 + ((nScreenLine & 0xf8) << 2) + (nBlock_ - BORDER_BLOCKS)
                                                   : pData + 0x2000;

        // Extract the 4 ASIC bytes used to display a block
        BYTE bData = *pData, bAttr = *pAttr;
        ab[0] = (bData & 0x77) | ( bData       & 0x80) | ((bData >> 3) & 0x08);
        ab[1] = (bData & 0x77) | ((bData << 2) & 0x80) | ((bData >> 1) & 0x08);
        ab[2] = (bAttr & 0x77) | ((bData << 4) & 0x80) | ((bData << 1) & 0x08);
        ab[3] = (bAttr & 0x77) | ((bData << 6) & 0x80) | ((bData << 3) & 0x08);
    }

    // The target mode decides how the data actually appears in the transition block
    switch (bNewVal_ & VMPR_MODE_MASK)
    {
        case MODE_1:
        {
            BYTE* pData = m_pbScreenData + g_awMode1LineToByte[nScreenLine] + (nBlock_ - BORDER_BLOCKS), bData = *pData;
            BYTE* pAttr = m_pbScreenData + 6144 + ((nScreenLine & 0xf8) << 2) + (nBlock_ - BORDER_BLOCKS), bAttr = *pAttr;

            *pData = ab[0];
            *pAttr = ab[2];
            Mode1Line(nLine_, nBlock_, nBlock_+1);

            *pData = bData;
            *pAttr = bAttr;
            break;
        }

        case MODE_2:
        {
            BYTE* pData = m_pbScreenData + (nScreenLine << 5) + (nBlock_ - BORDER_BLOCKS), bData = *pData;
            BYTE* pAttr = pData + 0x2000, bAttr = *pAttr;

            *pData = ab[0];
            *pAttr = ab[2];
            Mode2Line(nLine_, nBlock_, nBlock_+1);

            *pData = bData;
            *pAttr = bAttr;
            break;
        }

        default:
        {
            BYTE* pb = m_pbScreenData + (nScreenLine << 7) + ((nBlock_ - BORDER_BLOCKS) << 2);
            DWORD* pdw = reinterpret_cast<DWORD*>(pb), dw = *pdw;

            pb[0] = ab[0];
            pb[1] = ab[1];
            pb[2] = ab[2];
            pb[3] = ab[3];

            if ((bNewVal_ & VMPR_MODE_MASK) == MODE_3)
                Mode3Line(nLine_, nBlock_, nBlock_+1);
            else
                Mode4Line(nLine_, nBlock_, nBlock_+1);

            *pdw = dw;
            break;
        }
    }
}

#endif  // FRAME_H
