// Part of SimCoupe - A SAM Coupé emulator
//
// Memory.cpp: Memory configuration and management
//
//  Copyright (c) 1996-2001  Allan Skillman
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

// Changes 1999-2001 by Simon Owen
//  - added support for real 256K memory configuration
//  - added fast boot option by temporarily patching the SAM ROMs
//  - display memory writes now now catch-up/update the frame image
//  - mode 1 look-up tables built during initialisation

// ToDo:
//  - finish the ATOM auto-boot code that was started

#include "SimCoupe.h"
#include "Memory.h"

#include "CPU.h"
#include "Options.h"
#include "OSD.h"
#include "Util.h"

////////////////////////////////////////////////////////////////////////////////

// Single block holding all memory needed
BYTE* pMemory;

// Master read and write lists that are static for a given memory configuration
BYTE* apbPageReadPtrs[TOTAL_PAGES];
BYTE* apbPageWritePtrs[TOTAL_PAGES];

// Page numbers present in each of the 4 sections in the 64K address range
int anSectionPages[4];

// Array of pointers for memory to use when reading from or writing to each each section
BYTE* apbSectionReadPtrs[4];
BYTE* apbSectionWritePtrs[4];

// Look-up tables for fast mapping between mode 1 display addresses and line numbers
WORD g_awMode1LineToByte[SCREEN_LINES];
BYTE g_abMode1ByteToLine[SCREEN_LINES*SCREEN_BLOCKS];

////////////////////////////////////////////////////////////////////////////////

namespace Memory
{
static int ReadRoms ();

// Allocate and initialise memory
bool Init ()
{
    Exit(true);

    // Allocate and clear all we're likely to need in a single block (512K + 4MB external + 2 ROMs + 2 scratch)
    if (!pMemory && !(pMemory = new BYTE[TOTAL_PAGES * MEM_PAGE_SIZE]))
    {
        Message(msgError, "Out of memory!");
        return false;
    }

    // Clear out memory - allows use of the fast-boot option to skip SAM's memory check and initialisation
    memset(pMemory, 0, TOTAL_PAGES * MEM_PAGE_SIZE);

    // Start with all memory available, and fully readable and writable
    for (int i = 0 ; i < TOTAL_PAGES ; i++)
        apbPageReadPtrs[i] = apbPageWritePtrs[i] = pMemory + (i * MEM_PAGE_SIZE);

    // Read the ROMs and set the memory as read-only
    ReadRoms();
    apbPageWritePtrs[ROM0] = apbPageWritePtrs[ROM1] = apbPageWritePtrs[SCRATCH_WRITE];

    // Are we in 256K mode?
    if (GetOption(mainmem) == 256)
    {
        // Invalidate the top 256K of main memory
        for (int i = N_PAGES_MAIN/2 ; i < N_PAGES_MAIN ; i++)
            apbPageReadPtrs[i] = (apbPageWritePtrs[i] = apbPageWritePtrs[SCRATCH_WRITE]) - MEM_PAGE_SIZE;
    }

    // Make any absent portions of external memory read-only (note: external memory is added top-down!)
    int nExternal = min(GetOption(externalmem), MAX_EXTERNAL_MB);
    for (int nExt = 0 ; nExt < (MAX_EXTERNAL_MB - nExternal) * N_PAGES_1MB ; nExt++)
        apbPageWritePtrs[N_PAGES_MAIN + nExt] = apbPageWritePtrs[SCRATCH_WRITE];


    // Build the tables for fast mapping between mode 1 display addresses and line numbers
    for (int nOffset = 0 ; nOffset < (int)sizeof g_abMode1ByteToLine ; nOffset++)
    {
        g_abMode1ByteToLine[nOffset] = ((nOffset >> 5) & 0xc0) + ((nOffset >> 2) & 0x38) + ((nOffset >> 8) & 0x07);
        g_awMode1LineToByte[g_abMode1ByteToLine[nOffset]] = (nOffset & ~0x1f);
    }

    return true;
}

void Exit (bool fReInit_/*=false*/)
{
    if (!fReInit_ && pMemory) { delete pMemory; pMemory = NULL; }
}

// Patch ROM0 and ROM1 to speed up the initial boot 
void FastStartPatch (bool fPatch_/*=true*/)
{
    static bool fPatched = false;

    // Check the option is enabled, and for ROM version 3.x before changing anything, just in case
    if ((fPatched != fPatch_) && apbPageReadPtrs[ROM0][0x000f] / 10 == 3)
    {
        static BYTE abHDBoot1[] = { 0xd4, 0x39, 0xd1, 0x00, 0x00, 0xfa, 0x5f, 0x00, 0x52, 0x02, 0x00, 0x5f, 0xb9 };
        static BYTE abHDBoot2[] = { 0x3A, 0xC2, 0x5B, 0xA7, 0x20, 0x15, 0x21, 0x3B, 0x5C, 0xCB, 0xEE, 0x2E, 0x08,
                                    0x36, 0xC9, 0x18, 0x1C, 0xCD, 0x6C, 0xD9, 0x47, 0x79, 0xD3, 0xE0, 0x10, 0xFE,
                                    0xC9, 0x11, 0x09, 0x00, 0xCD, 0x1B, 0x08, 0xAF, 0xCD, 0xB0, 0x3D, 0xCD, 0xB1,
                                    0x1C, 0x28, 0xFB, 0xCD, 0xB5, 0x06, 0x21, 0x35, 0x18, 0x7d };

        static BYTE abNormal1[sizeof abHDBoot1], abNormal2[sizeof abHDBoot2];

        if (fPatched = fPatch_)
        {
            // Save the original contents
            memcpy(abNormal1, &apbPageReadPtrs[ROM0][0x00b0], sizeof abHDBoot1);
            memcpy(abNormal2, &apbPageReadPtrs[ROM0][0x0f7f], sizeof abHDBoot2);
#if 0
            // Make the Atom-boot modifications
            memcpy(&apbPageReadPtrs[ROM0][0x00b0], abHDBoot1, sizeof abHDBoot1);
            memcpy(&apbPageReadPtrs[ROM0][0x0f7f], abHDBoot2, sizeof abHDBoot2);
            apbPageReadPtrs[ROM0][0x0001] = 0x31;
            apbPageReadPtrs[ROM0][0x000f] = 0x1e;
#else

            // Make the fast-boot modifications
            apbPageReadPtrs[ROM0][0x00b5] = 0x06;   // JR NZ,e -> LD B,e    to short-circuit BC delay loop
            apbPageReadPtrs[ROM0][0x0796] = 0x06;   // JR NZ,e -> LD B,e    to short-circuit BC delay loop
            apbPageReadPtrs[ROM1][0x2bc9] = 0x06;   // LDIR    -> LD B,176  to avoid page clear slowdown
            apbPageReadPtrs[ROM1][0x2bd0] = 0x01;   // LD E,64 -> LD E,1    only one read/write test per page
#endif
        }
        else
        {
            // Reverse the Atom boot patches
            memcpy(&apbPageReadPtrs[ROM0][0x00b0], abNormal1, sizeof abHDBoot1);
            memcpy(&apbPageReadPtrs[ROM0][0x0f7f], abNormal2, sizeof abHDBoot2);
            apbPageReadPtrs[ROM0][0x0001] = 0xc3;
            apbPageReadPtrs[ROM0][0x000f] = 0x1f;

            // Reverse the fast-boot patches
            apbPageReadPtrs[ROM0][0x00b5] = 0x20;
            apbPageReadPtrs[ROM0][0x0796] = 0x10;
            apbPageReadPtrs[ROM1][0x2bc9] = 0xed;
            apbPageReadPtrs[ROM1][0x2bd0] = 0x40;
        }
    }
}

// Read the ROM image into the ROM area of our paged memory
static bool LoadRomImage (const char* pcszImage_, int nPage_)
{
    FILE* file;
    if (!(file = fopen(OSD::GetFilePath(pcszImage_), "rb")) && !(file = fopen(pcszImage_, "rb")))
        Message(msgError, "Failed to open ROM image: %s", pcszImage_);
    else
    {
        // Read the image, warning if the image is not exactly 16K
        if (!fread(apbPageReadPtrs[nPage_], MEM_PAGE_SIZE, 1, file))
            Message(msgWarning, "ROM image (%s) is too small (under 16K)!", pcszImage_);

        fclose(file);
        return true;
    }

    return false;
}

int ReadRoms ()
{
    // Read in the two SAM ROM images
    bool f = LoadRomImage(GetOption(rom0), ROM0) && LoadRomImage(GetOption(rom1), ROM1);

    // The fast reset option requires a few temporary ROM patches that are reversed later
    if (GetOption(fastreset))
        FastStartPatch();

    return f;
}

};  // namespace Memory
