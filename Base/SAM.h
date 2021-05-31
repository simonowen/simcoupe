// Part of SimCoupe - A SAM Coupe emulator
//
// SAM.h: SAM-specific constants
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#pragma once

constexpr auto PAL_VTOTAL_PROGRESSIVE = 624;
constexpr auto PAL_FIELDS_PER_SECOND = 50;
constexpr auto PAL_FIELDS_PER_FRAME = 2;
constexpr auto PAL_BT601_ACTVE_TIME_uS = 52;
constexpr auto PAL_BT601_WIDTH_PIXELS = 702;
constexpr auto PAL_BT601_PIXEL_ASPECT_RATIO_4_3 = 59.0f / 54;

constexpr auto CRYSTAL_CLOCK_HZ = 24'000'000;
constexpr auto GFX_PIXEL_CLOCK_HZ = CRYSTAL_CLOCK_HZ / 2;
constexpr auto GFX_ACTIVE_WIDTH_PIXELS = PAL_BT601_ACTVE_TIME_uS * GFX_PIXEL_CLOCK_HZ / 1'000'000;
constexpr auto GFX_DISPLAY_ASPECT_RATIO = PAL_BT601_WIDTH_PIXELS * PAL_BT601_PIXEL_ASPECT_RATIO_4_3 / GFX_ACTIVE_WIDTH_PIXELS; // 59:48
constexpr auto GFX_PIXELS_PER_LINE = GFX_PIXEL_CLOCK_HZ / PAL_FIELDS_PER_SECOND / PAL_VTOTAL_PROGRESSIVE * PAL_FIELDS_PER_FRAME;
constexpr auto GFX_PIXELS_PER_CELL = 16;
constexpr auto GFX_LORES_PIXELS_PER_CELL = GFX_PIXELS_PER_CELL / 2;
constexpr auto GFX_DATA_BYTES_PER_CELL = 4;

constexpr auto GFX_WIDTH_CELLS = GFX_PIXELS_PER_LINE / GFX_PIXELS_PER_CELL;
constexpr auto GFX_HEIGHT_LINES = PAL_VTOTAL_PROGRESSIVE / PAL_FIELDS_PER_FRAME;
constexpr auto GFX_SCREEN_LINES = 192;
constexpr auto GFX_SCREEN_CELLS = 32;
constexpr auto GFX_SCREEN_PIXELS = GFX_SCREEN_CELLS * GFX_PIXELS_PER_CELL;

constexpr auto TOP_BORDER_LINES = 68;
constexpr auto BOTTOM_BORDER_LINES = GFX_HEIGHT_LINES - GFX_SCREEN_LINES - TOP_BORDER_LINES;
constexpr auto FIRST_SCREEN_LINE = TOP_BORDER_LINES;
constexpr auto LAST_SCREEN_LINE = TOP_BORDER_LINES + GFX_SCREEN_LINES - 1;
constexpr auto SIDE_BORDER_CELLS = (GFX_WIDTH_CELLS - GFX_SCREEN_CELLS) / 2;

constexpr auto CPU_CLOCK_HZ = 6'000'000;
constexpr auto CPU_CYCLES_PER_CELL = CPU_CLOCK_HZ * GFX_PIXELS_PER_CELL / GFX_PIXEL_CLOCK_HZ;
constexpr auto CPU_CYCLES_PER_LINE = CPU_CYCLES_PER_CELL * GFX_WIDTH_CELLS;
constexpr auto CPU_CYCLES_PER_FRAME = CPU_CYCLES_PER_LINE * GFX_HEIGHT_LINES;
constexpr auto CPU_CYCLES_PER_SIDE_BORDER = CPU_CYCLES_PER_CELL * SIDE_BORDER_CELLS;
constexpr auto CPU_CYCLES_ASIC_TO_FRAME_OFFSET = CPU_CYCLES_PER_SIDE_BORDER;

constexpr auto MODE1_FRAMES_PER_FLASH = 16;
constexpr auto MODE1_LINES_PER_ATTR = 8;
constexpr auto MODE12_BYTES_PER_LINE = GFX_SCREEN_CELLS;
constexpr auto MODE12_DATA_BYTES = MODE12_BYTES_PER_LINE * GFX_SCREEN_LINES;
constexpr auto MODE1_ATTR_BYTES = GFX_SCREEN_CELLS * (GFX_SCREEN_LINES / MODE1_LINES_PER_ATTR);
constexpr auto MODE12_FLASH_FRAMES = 16;
constexpr auto MODE1_DISPLAY_BYTES = MODE12_DATA_BYTES + MODE1_ATTR_BYTES;
constexpr auto MODE2_ATTR_BYTES = MODE12_DATA_BYTES;
constexpr auto MODE2_ATTR_OFFSET = 0x2000;
constexpr auto MODE34_BYTES_PER_LINE = GFX_DATA_BYTES_PER_CELL * GFX_SCREEN_CELLS;
constexpr auto MODE34_DISPLAY_BYTES = MODE34_BYTES_PER_LINE * GFX_SCREEN_LINES;

constexpr auto EMULATED_FRAMES_PER_SECOND = PAL_FIELDS_PER_SECOND;
constexpr auto ACTUAL_FRAMES_PER_SECOND = static_cast<float>(CPU_CLOCK_HZ) / CPU_CYCLES_PER_FRAME; // ~50.08

constexpr auto CPU_CYCLES_INTERRUPT_ACTIVE = 128;

// CPU cycles after power-on before the ASIC responds to I/O (~49ms)
constexpr auto CPU_CYCLES_ASIC_STARTUP = 291675;

constexpr auto MEM_PAGE_SIZE = 0x4000;
constexpr auto MEM_PAGE_MASK = MEM_PAGE_SIZE - 1;
constexpr auto NUM_INTERNAL_PAGES = (0x80000 / MEM_PAGE_SIZE);
constexpr auto NUM_EXTERNAL_PAGES_1MB = (0x100000 / MEM_PAGE_SIZE);
constexpr auto MAX_EXTERNAL_MB = 4;
constexpr auto NUM_ROM_PAGES = 2;

constexpr auto NUM_PALETTE_COLOURS = 128;
constexpr auto NUM_CLUT_REGS = 16;

constexpr auto usecs_to_tstates(int cycles)
{
    return cycles * CPU_CLOCK_HZ / 1'000'000;
}

constexpr uint16_t SYSVAR_LAST_K = 0x5c08;
constexpr uint16_t SYSVAR_FLAGS = 0x5c3b;
