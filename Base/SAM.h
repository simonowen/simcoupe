// Part of SimCoupe - A SAM Coupé emulator
//
// SAM.h: SAM-specific constants
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
#ifndef SAM_H
#define SAM_H

// Constants defining screen height and the visible portion of it
#define HEIGHT_LINES            312                                         // Total generated screen lines
#define SCREEN_LINES            192                                         // Lines in main screen area
#define TOP_BORDER_LINES        68                                          // Lines in top border (8 above central position)
#define BOTTOM_BORDER_LINES     (HEIGHT_LINES-SCREEN_LINES-TOP_BORDER_LINES)// Lines in bottom border

// Constants defining screen width and the visible portion of it (in 8 'pixel' blocks)
#define WIDTH_BLOCKS            48                                      // Total generated screen width
#define SCREEN_BLOCKS           32                                      // Blocks in main screen area
#define BORDER_BLOCKS           ((WIDTH_BLOCKS-SCREEN_BLOCKS) >> 1)     // Blocks for each of left and right border
#define WIDTH_PIXELS            (WIDTH_BLOCKS << 3)
#define BORDER_PIXELS           (BORDER_BLOCKS << 3)
#define SCREEN_PIXELS           (SCREEN_BLOCKS << 3)


// Constants for video/frame timing
#define TSTATES_PER_LINE            (WIDTH_BLOCKS << 3)
#define TSTATES_PER_FRAME           (TSTATES_PER_LINE * HEIGHT_LINES)
#define REAL_TSTATES_PER_SECOND     6000000UL                                       // SAM's Z80b CPU runs at 6MHz
#define REAL_FRAMES_PER_SECOND      (REAL_TSTATES_PER_SECOND / TSTATES_PER_FRAME)   // Actually 50.08
#define EMULATED_FRAMES_PER_SECOND  50
#define EMULATED_TSTATES_PER_SECOND (EMULATED_FRAMES_PER_SECOND * TSTATES_PER_FRAME)

#define ASIC_STARTUP_DELAY          291675  // approx. t-states after power-on before the ASIC responds to I/O (~49ms)

#define USECONDS_TO_TSTATES(x)      (static_cast<long>(x) * (REAL_TSTATES_PER_SECOND / 1000000))

#define VIDEO_DELAY                 8       // t-states by which contention precedes the actual screen as seen


#define MEM_PAGE_SIZE               0x4000                      // Memory pages are 16K
#define N_PAGES_MAIN                (0x80000/MEM_PAGE_SIZE)     // Number of pages in the SAM's 512K main memory (32)
#define N_PAGES_1MB                 (0x100000/MEM_PAGE_SIZE)    // Number of pages in 1MB of memory (64)
#define MAX_EXTERNAL_MB             4                           // SAM supports 0 to 4 MB of external memory


#define N_PALETTE_COLOURS           128     // 128 colours in the SAM palette
#define N_CLUT_REGS                 16      // 16 CLUT entries
#define N_SAA_REGS                  32      // 32 registers in the Philips SAA1099 sound chip

#define BASE_ASIC_PORT              0xf8    // Ports from this value require ASIC attention, and can cause contention delays

#endif  // SAM_H
