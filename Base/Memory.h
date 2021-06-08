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

#include "SAMIO.h"

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
inline int AddrSection(uint16_t addr) { return addr >> 14; }
inline int AddrPage(uint16_t addr) { return anSectionPages[AddrSection(addr)]; }
inline int AddrOffset(uint16_t addr) { return addr & (MEM_PAGE_SIZE - 1); }

inline int GetSectionPage(Section section) { return anSectionPages[static_cast<int>(section)]; }

inline int PageReadOffset(int page) { return anReadPages[page] * MEM_PAGE_SIZE; }
inline int PageWriteOffset(int page) { return anWritePages[page] * MEM_PAGE_SIZE; }

inline uint8_t* PageReadPtr(int page) { return pMemory + PageReadOffset(page); }
inline uint8_t* PageWritePtr(int page) { return pMemory + PageWriteOffset(page); }
inline uint8_t* AddrReadPtr(uint16_t addr) { return apbSectionReadPtrs[AddrSection(addr)] + (addr & (MEM_PAGE_SIZE - 1)); }
inline uint8_t* AddrWritePtr(uint16_t addr) { return apbSectionWritePtrs[AddrSection(addr)] + (addr & (MEM_PAGE_SIZE - 1)); }
inline bool ReadOnlyAddr(uint16_t addr) { return apbSectionWritePtrs[AddrSection(addr)] == PageWritePtr(SCRATCH_WRITE); }

inline int PtrPage(const void* pv_) { return int((reinterpret_cast<const uint8_t*>(pv_) - pMemory) / MEM_PAGE_SIZE); }
inline int PtrOffset(const void* pv_) { return int((reinterpret_cast<const uint8_t*>(pv_) - pMemory)& (MEM_PAGE_SIZE - 1)); }

void write_to_screen_vmpr0(uint16_t addr);
void write_to_screen_vmpr1(uint16_t addr);
void write_word(uint16_t addr, uint16_t val);

inline void check_video_write(uint16_t addr)
{
    auto page = AddrPage(addr);
    if (page == (IO::State().vmpr & VMPR_PAGE_MASK))
        write_to_screen_vmpr0(addr);
    else if (page == ((IO::State().vmpr + 1) & VMPR_PAGE_MASK))
        write_to_screen_vmpr1(addr);
}


inline uint8_t read_byte(uint16_t addr)
{
    return *AddrReadPtr(addr);
}

inline uint16_t read_word(uint16_t addr)
{
    return read_byte(addr) | (read_byte(addr + 1) << 8);
}

inline void write_byte(uint16_t addr, uint8_t bVal_)
{
    *AddrWritePtr(addr) = bVal_;
}

inline void write_word(uint16_t addr, uint16_t wVal_)
{
    write_byte(addr, wVal_ & 0xff);
    write_byte(addr + 1, wVal_ >> 8);
}

inline void PageIn(Section section, int page)
{
    auto index = static_cast<int>(section);
    anSectionPages[index] = page;
    afSectionContended[index] = (page < NUM_INTERNAL_PAGES);

    apbSectionReadPtrs[index] = PageReadPtr(page);
    apbSectionWritePtrs[index] = PageWritePtr(page);

    if ((section == Section::A) && (IO::State().lmpr & LMPR_WPROT))
        apbSectionWritePtrs[index] = PageWritePtr(SCRATCH_WRITE);
}

namespace Memory
{
    extern bool full_contention;
    extern uint8_t* last_phys_read1, * last_phys_read2, * last_phys_write1, * last_phys_write2;

    bool Init(bool fFirstInit_ = false);
    void Exit(bool fReInit_ = false);

    void UpdateConfig();
    void UpdateRom();

    inline uint8_t Read(uint16_t addr)
    {
        last_phys_read2 = last_phys_read1;
        return *(last_phys_read1 = AddrReadPtr(addr));
    }

    inline void Write(uint16_t addr, uint8_t val)
    {
        last_phys_write2 = last_phys_write1;
        *(last_phys_write1 = AddrWritePtr(addr)) = val;
    }

    inline int WaitStates(uint32_t frame_cycles, uint16_t addr)
    {
        if (!afSectionContended[AddrSection(addr)])
            return 0;

        int line = frame_cycles / CPU_CYCLES_PER_LINE;
        auto line_cycle = (frame_cycles + CPU_CYCLES_SCREEN_CONTENTION_OFFSET) % CPU_CYCLES_PER_LINE;
        bool main_screen =
            line >= TOP_BORDER_LINES &&
            line < TOP_BORDER_LINES + GFX_SCREEN_LINES &&
            line_cycle >= CPU_CYCLES_PER_SIDE_BORDER + CPU_CYCLES_PER_SIDE_BORDER;
        bool mode1 = (!(line_cycle & 0x40)) && ((IO::State().vmpr & VMPR_MODE_MASK) == VMPR_MODE_1);

        auto mask = (full_contention && !IO::ScreenDisabled() && (main_screen || mode1)) ? 7 : 3;
        auto delay = (mask - ((frame_cycles + 2) & mask));
        return delay;
    }

    std::string PageDesc(int nPage_, bool fCompact_ = false);
}
