// Part of SimCoupe - A SAM Coupe emulator
//
// Memory.h: Memory configuration and management
//
//  Copyright (c) 1999-2010  Simon Owen
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#pragma once

#include "Frame.h"

namespace Memory
{
bool Init(bool fFirstInit_ = false);
void Exit(bool fReInit_ = false);

void UpdateConfig();
void UpdateRom();

const char* PageDesc(int nPage_, bool fCompact_ = false);
}

constexpr auto NUM_SCRATCH_PAGES = 2;

constexpr auto TOTAL_PAGES =
    NUM_INTERNAL_PAGES +
    NUM_EXTERNAL_PAGES_1MB * MAX_EXTERNAL_MB +
    NUM_ROM_PAGES +
    NUM_SCRATCH_PAGES;

enum { INTMEM, EXTMEM = NUM_INTERNAL_PAGES, ROM0 = EXTMEM + (NUM_EXTERNAL_PAGES_1MB * MAX_EXTERNAL_MB), ROM1, SCRATCH_READ, SCRATCH_WRITE };
enum class Section { A, B, C, D };

extern uint8_t pMemory[];

extern int anReadPages[];
extern int anWritePages[];

extern int anSectionPages[4];
extern bool afSectionContended[4];

extern uint8_t* apbSectionReadPtrs[4];
extern uint8_t* apbSectionWritePtrs[4];

extern uint8_t g_abMode1ByteToLine[GFX_SCREEN_LINES];
extern uint16_t g_awMode1LineToByte[GFX_SCREEN_LINES];


// Map a 16-bit address through the memory indirection - allows fast paging
inline int AddrSection(uint16_t wAddr_) { return wAddr_ >> 14; }
inline int AddrPage(uint16_t wAddr_) { return anSectionPages[AddrSection(wAddr_)]; }
inline int AddrOffset(uint16_t wAddr_) { return wAddr_ & (MEM_PAGE_SIZE - 1); }

inline int GetSectionPage(Section section) { return anSectionPages[static_cast<int>(section)]; }

inline int PageReadOffset(int nPage_) { return anReadPages[nPage_] * MEM_PAGE_SIZE; }
inline int PageWriteOffset(int nPage_) { return anWritePages[nPage_] * MEM_PAGE_SIZE; }

inline uint8_t* PageReadPtr(int nPage_) { return pMemory + PageReadOffset(nPage_); }
inline uint8_t* PageWritePtr(int nPage_) { return pMemory + PageWriteOffset(nPage_); }
inline uint8_t* AddrReadPtr(uint16_t wAddr_) { return apbSectionReadPtrs[AddrSection(wAddr_)] + (wAddr_ & (MEM_PAGE_SIZE - 1)); }
inline uint8_t* AddrWritePtr(uint16_t wAddr_) { return apbSectionWritePtrs[AddrSection(wAddr_)] + (wAddr_ & (MEM_PAGE_SIZE - 1)); }
inline bool ReadOnlyAddr(uint16_t wAddr_) { return apbSectionWritePtrs[AddrSection(wAddr_)] == PageWritePtr(SCRATCH_WRITE); }

inline int PtrPage(const void* pv_) { return int((reinterpret_cast<const uint8_t*>(pv_) - pMemory) / MEM_PAGE_SIZE); }
inline int PtrOffset(const void* pv_) { return int((reinterpret_cast<const uint8_t*>(pv_) - pMemory)& (MEM_PAGE_SIZE - 1)); }

void write_to_screen_vmpr0(uint16_t wAddr_);
void write_to_screen_vmpr1(uint16_t wAddr_);
void write_word(uint16_t wAddr_, uint16_t wVal_);

inline void check_video_write(uint16_t wAddr_)
{
    // Look up the page containing the specified address
    int nPage = AddrPage(wAddr_);

    // Does the write fall within the first display page?
    if (nPage == vmpr_page1)
        write_to_screen_vmpr0(wAddr_);

    // Does the write fall within the second display page? (modes 3 and 4 only)
    else if (nPage == vmpr_page2)
        write_to_screen_vmpr1(wAddr_);
}


inline uint8_t read_byte(uint16_t wAddr_)
{
    return *AddrReadPtr(wAddr_);
}

inline uint16_t read_word(uint16_t wAddr_)
{
    return read_byte(wAddr_) | (read_byte(wAddr_ + 1) << 8);
}

inline void write_byte(uint16_t wAddr_, uint8_t bVal_)
{
    *AddrWritePtr(wAddr_) = bVal_;
}

inline void write_word(uint16_t wAddr_, uint16_t wVal_)
{
    // Write the low byte then the high byte
    write_byte(wAddr_, wVal_ & 0xff);
    write_byte(wAddr_ + 1, wVal_ >> 8);
}

// Page in real memory page at <nSection_>, where <nSection_> is in range 0..3
inline void PageIn(Section section, int page)
{
    auto index = static_cast<int>(section);
    anSectionPages[index] = page;
    afSectionContended[index] = (page < NUM_INTERNAL_PAGES);

    apbSectionReadPtrs[index] = PageReadPtr(page);
    apbSectionWritePtrs[index] = PageWritePtr(page);

    if ((section == Section::A) && (lmpr & LMPR_WPROT))
        apbSectionWritePtrs[index] = PageWritePtr(SCRATCH_WRITE);
}


inline void write_to_screen_vmpr0(uint16_t wAddr_)
{
    // Limit the address to the 16K page we're considering
    wAddr_ &= (MEM_PAGE_SIZE - 1);

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
            int nLine = (((wAddr_ - 6144) & 0xffe0) >> 2) + TOP_BORDER_LINES;
            Frame::TouchLines(nLine, nLine + 7);
        }

        break;

    case MODE_2:
        // If the write falls within the screen data or attributes, invalidate the line
        if (wAddr_ < 6144 || (wAddr_ >= 8192 && wAddr_ < (8192 + 6144)))
            Frame::TouchLine(((wAddr_ & 0x1fff) >> 5) + TOP_BORDER_LINES);
        break;

        // Modes 3 and 4
    default:
        // The write is to the first 16K of a mode 3 or 4 screen
        Frame::TouchLine((wAddr_ >> 7) + TOP_BORDER_LINES);
        break;
    }
}

inline void write_to_screen_vmpr1(uint16_t wAddr_)
{
    // Limit the address to the 16K page we're considering
    wAddr_ &= (MEM_PAGE_SIZE - 1);

    if (wAddr_ < 8192)
        Frame::TouchLine(((wAddr_ + MEM_PAGE_SIZE) >> 7) + TOP_BORDER_LINES);
}
