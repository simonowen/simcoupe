// Part of SimCoupe - A SAM Coupe emulator
//
// Memory.h: Memory configuration and management
//
//  Copyright (c) 1999-2005  Simon Owen
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

#ifndef MEMORY_H
#define MEMORY_H

#include "Frame.h"

class Memory
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);
};


enum { INTMEM, EXTMEM=N_PAGES_MAIN, ROM0=EXTMEM+(N_PAGES_1MB*MAX_EXTERNAL_MB), ROM1, SCRATCH_READ, SCRATCH_WRITE, TOTAL_PAGES };
enum eSection { SECTION_A, SECTION_B, SECTION_C, SECTION_D };

extern int anSectionPages[4];
extern BYTE* apbSectionReadPtrs[4];
extern BYTE* apbSectionWritePtrs[4];
extern BYTE g_abMode1ByteToLine[SCREEN_LINES];
extern WORD g_awMode1LineToByte[SCREEN_LINES];
extern BYTE *apbPageReadPtrs[],  *apbPageWritePtrs[];
extern bool afContendedPages[4];


// Map a 16-bit address through the memory indirection - allows fast paging
inline int VPAGE (WORD wAddr_) { return wAddr_ >> 14; }
inline int RPAGE (WORD wAddr_) { return anSectionPages[VPAGE(wAddr_)]; }

void write_to_screen_vmpr0 (WORD wAddr_);
void write_to_screen_vmpr1 (WORD wAddr_);
void write_word (WORD wAddr_, WORD wVal_);


inline BYTE* phys_read_addr (UINT uPage_, UINT uOffset_)
{
    return &apbSectionReadPtrs[uPage_ & (N_PAGES_MAIN-1)][uOffset_ & (MEM_PAGE_SIZE-1)];
}

inline BYTE* phys_read_addr (WORD wAddr_)
{
    return &apbSectionReadPtrs[VPAGE(wAddr_)][wAddr_ & (MEM_PAGE_SIZE-1)];
}

inline BYTE* phys_write_addr (WORD wAddr_)
{
    return &apbSectionWritePtrs[VPAGE(wAddr_)][wAddr_ & (MEM_PAGE_SIZE-1)];
}


inline void check_video_write (WORD wAddr_)
{
    // Does the write fall within the first display page?
    if (RPAGE(wAddr_) == vmpr_page1)
        write_to_screen_vmpr0(wAddr_);

    // Does the write fall within the second display page? (modes 3 and 4 only)
    else if ((RPAGE(wAddr_) == vmpr_page2) && (vmpr_mode > MODE_2))
        write_to_screen_vmpr1(wAddr_);
}


inline BYTE read_byte (WORD wAddr_)
{
    return *phys_read_addr(wAddr_);
}

inline WORD read_word (WORD wAddr_)
{
    return read_byte(wAddr_) | (read_byte(wAddr_+1) << 8);
}

inline void write_byte (WORD wAddr_, BYTE bVal_)
{
    *phys_write_addr(wAddr_) = bVal_;
}

inline void write_word (WORD wAddr_, WORD wVal_)
{
    // Write the low byte then the high byte
    write_byte(wAddr_, wVal_ & 0xff);
    write_byte(wAddr_+1, wVal_ >> 8);
}


inline int GetSectionPage (eSection nSection_)
{
    return anSectionPages[nSection_];
}

// Page in real memory page at <nSection_>, where <nSection_> is in range 0..3
inline void PageIn (eSection nSection_, int nPage_)
{
    // Remember the page that's now occupying the section
    anSectionPages[nSection_] = nPage_;
    afContendedPages[nSection_] = (nPage_ < N_PAGES_MAIN);

    // Look up the relevant read and write pointers for the section
    apbSectionReadPtrs[nSection_] = apbPageReadPtrs[nPage_];
    apbSectionWritePtrs[nSection_] = apbPageWritePtrs[nPage_];

    // Check for write protected RAM in section A
    if ((nSection_ == SECTION_A) && (lmpr & LMPR_WPROT))
        apbSectionWritePtrs[nSection_] = apbPageWritePtrs[SCRATCH_WRITE];
}


inline void write_to_screen_vmpr0 (WORD wAddr_)
{
    // Limit the address to the 16K page we're considering
    wAddr_ &= (MEM_PAGE_SIZE-1);

    // The display area affected by the write depends on the current screen mode
    switch (vmpr_mode)
    {
        case MODE_1:
            // If writing to the main screen data, invalidate the screen line we're writing to
            if (wAddr_ < 6144)
                Frame::TouchLine(g_abMode1ByteToLine[wAddr_ >> 5] + TOP_BORDER_LINES);

            // If writing to the attribute area, invalidate the 8 lines containing the attribute
            else if (wAddr_ < 6912)
            {
                int nLine = (((wAddr_-6144) & 0xffe0) >> 2) + TOP_BORDER_LINES;
                Frame::TouchLines(nLine, nLine + 7);
            }

            break;

        case MODE_2:
            // If the write falls within the screen data or attributes, invalidate the line
            if (wAddr_ < 6144 || (wAddr_ >= 8192 && wAddr_ < (8192+6144)))
                Frame::TouchLine(((wAddr_ & 0x1fff) >> 5) + TOP_BORDER_LINES);
            break;

        // Modes 3 and 4
        default:
            // The write is to the first 16K of a mode 3 or 4 screen
            Frame::TouchLine((wAddr_ >> 7) + TOP_BORDER_LINES);
            break;
    }
}

inline void write_to_screen_vmpr1 (WORD wAddr_)
{
    // Limit the address to the 16K page we're considering
    wAddr_ &= (MEM_PAGE_SIZE-1);

    if (wAddr_ < 8192)
        Frame::TouchLine(((wAddr_ + MEM_PAGE_SIZE) >> 7) + TOP_BORDER_LINES);
}


#endif  // MEMORY_H
