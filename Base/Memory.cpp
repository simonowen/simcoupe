// Part of SimCoupe - A SAM Coupe emulator
//
// Memory.cpp: Memory configuration and management
//
//  Copyright (c) 1999-2004  Simon Owen
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
//
// Includes Edwin Blink's HDBOOT ROM modifications:
//  http://home.wanadoo.nl/edwin.blink/samcoupe/software/bdos/hdboot.htm

#include "SimCoupe.h"
#include "Memory.h"

#include "CPU.h"
#include "CStream.h"
#include "Options.h"
#include "OSD.h"
#include "SAMROM.h"
#include "Util.h"

////////////////////////////////////////////////////////////////////////////////

// Single block holding all memory needed
BYTE* pMemory;

// Master read and write lists that are static for a given memory configuration
BYTE* apbPageReadPtrs[TOTAL_PAGES];
BYTE* apbPageWritePtrs[TOTAL_PAGES];

// Page numbers present in each of the 4 sections in the 64K address range
int anSectionPages[4];
bool afContendedPages[4];

// Array of pointers for memory to use when reading from or writing to each each section
BYTE* apbSectionReadPtrs[4];
BYTE* apbSectionWritePtrs[4];

// Look-up tables for fast mapping between mode 1 display addresses and line numbers
WORD g_awMode1LineToByte[SCREEN_LINES];
BYTE g_abMode1ByteToLine[SCREEN_LINES];

////////////////////////////////////////////////////////////////////////////////

static void LoadRoms ();


// Allocate and initialise memory
bool Memory::Init (bool fFirstInit_/*=false*/)
{
    // Memory only requires one-off initialisation
    if (fFirstInit_)
    {
        // Allocate a single block for all we're likely to need (512K + 4MB external + 2 ROMs + 2 scratch)
        if (!pMemory && !(pMemory = new BYTE[TOTAL_PAGES * MEM_PAGE_SIZE]))
        {
            Message(msgFatal, "Out of memory!");
            return false;
        }

        // Stripe-initialise the RAM banks as seems to be the situation on the real unit
        for (int i = 0 ; i < (ROM0 * MEM_PAGE_SIZE) ; i += 256)
        {
            memset(pMemory+i,     0x00, 128);
            memset(pMemory+i+128, 0xff, 128);
        }

        // Fill the ROMs and scratch memory with 0xff for now
        memset(pMemory+ROM0 * MEM_PAGE_SIZE, 0xff, (TOTAL_PAGES-ROM0) * MEM_PAGE_SIZE);
    }


    // Start with all memory available, and fully readable and writable
    for (int nPage = 0 ; nPage < TOTAL_PAGES ; nPage++)
        apbPageReadPtrs[nPage] = apbPageWritePtrs[nPage] = pMemory + (nPage * MEM_PAGE_SIZE);

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
    for (UINT uOffset = 0 ; uOffset < SCREEN_LINES ; uOffset++)
    {
        g_abMode1ByteToLine[uOffset] = (uOffset & 0xc0) + ((uOffset << 3) & 0x38) + ((uOffset >> 3) & 0x07);
        g_awMode1LineToByte[g_abMode1ByteToLine[uOffset]] = uOffset << 5;
    }

    // Load the ROMs, using the second file if the first doesn't contain both
    LoadRoms();

    // Writes to the ROM are discarded
    apbPageWritePtrs[ROM0] = apbPageWritePtrs[ROM1] = apbPageWritePtrs[SCRATCH_WRITE];

    return true;
}

void Memory::Exit (bool fReInit_/*=false*/)
{
    if (!fReInit_) { delete[] pMemory; pMemory = NULL; }
}


// Read the ROM image into the ROM area of our paged memory block
static void LoadRoms ()
{
    CStream* pROM;

    const char* pcszROM = GetOption(rom);

    // Use a custom ROM if supplied
    if (*pcszROM && ((pROM = CStream::Open(OSD::GetFilePath(pcszROM))) || (pROM = CStream::Open(pcszROM))))
    {
        // Read the header+bootstrap code from what could be a ZX82 file (for Andy Wright's ROM images)
        BYTE abHeader[140];
        pROM->Read(abHeader, sizeof(abHeader));

        // If we don't find the ZX82 signature, rewind to read as a plain ROM file
        if (memcmp(abHeader, "ZX82", 4))
            pROM->Rewind();

        // Clear out any existing ROM data, and load the new ROM
        memset(apbPageReadPtrs[ROM0], 0, MEM_PAGE_SIZE*2);
        pROM->Read(apbPageReadPtrs[ROM0], MEM_PAGE_SIZE*2);

        delete pROM;
        return;
    }

    // Report the failure if there
    if (*pcszROM)
    {
        Message(msgWarning, "Error loading custom ROM:\n%s\n\nReverting to built-in ROM image.", pcszROM);
        SetOption(rom,"");
    }

    // Use the built-in 3.0 ROM image
    memcpy(apbPageReadPtrs[ROM0], abSAMROM, sizeof(abSAMROM));

    if (GetOption(hdbootrom))
    {
        // Apply Edwin Blink's HDBOOT ROM modifications if required
        // See: http://home.wanadoo.nl/edwin.blink/samcoupe/software/bdos/hdboot.htm

        static BYTE abHDBoot1[] = {
            0xd4, 0x39, 0xd1, 0x00, 0x00, 0xfa, 0x5f, 0x00, 0x52, 0x02, 0x00, 0x5f, 0xb9
        };

        static BYTE abHDBoot2[] = {
            0x3a, 0xc2, 0x5b, 0xa7, 0x20, 0x15, 0x21, 0x3b, 0x5c, 0xcb, 0xee, 0x2e, 0x08, 0x36, 0xc9, 0x18,
            0x1c, 0xcd, 0x6c, 0xd9, 0x47, 0x79, 0xd3, 0xe0, 0x10, 0xfe, 0xc9, 0x11, 0x09, 0x00, 0xcd, 0x1b,
            0x08, 0xaf, 0xcd, 0xb0, 0x3d, 0xcd, 0xb1, 0x1c, 0x28, 0xfb, 0xcd, 0xb5, 0x06, 0x21, 0x00, 0x56,
            0x35, 0x18, 0x7d
        };

        static BYTE abHDBoot3[] = {
            0xf3, 0xcd, 0xae, 0xeb, 0x21, 0x00, 0xbe, 0x01, 0xf7, 0x21, 0xcd, 0xdd, 0xf5, 0x3e, 0xff, 0xd3,
            0xf5, 0x0e, 0xd0, 0xcd, 0x93, 0x0f, 0x26, 0xfe, 0x5c, 0x06, 0x03, 0x2b, 0x7c, 0xb5, 0x20, 0x05,
            0x1c, 0x20, 0x02, 0xcf, 0x37, 0xdb, 0xe0, 0x57, 0xa9, 0xe6, 0x02, 0x28, 0xee, 0x4a, 0x10, 0xeb,
            0x5f, 0x16, 0x05, 0xcd, 0x67, 0xd9, 0x3e, 0x01, 0xd3, 0xe2, 0x3e, 0x04, 0xd3, 0xe3, 0x0e, 0x1b,
            0xcd, 0x69, 0xd9, 0x0e, 0x80, 0xcd, 0x90, 0x0f, 0x61, 0x68, 0x0e, 0xe3, 0xfe, 0xed, 0xa2, 0xdb,
            0xe0, 0xcb, 0x4f, 0x20, 0xf8, 0x0f, 0x38, 0xf7, 0xe6, 0x0e, 0x28, 0x08, 0x15, 0x20, 0xd7, 0x1d,
            0x20, 0xcf, 0xcf, 0x13, 0x26, 0x81, 0xcd, 0x75, 0xd9, 0xca, 0x09, 0x80, 0xcf, 0x35, 0x0e, 0x0b,
            0xcd, 0x90, 0x0f, 0xdb, 0xe0, 0x0f, 0xd0, 0xcd, 0x5d, 0x0e, 0x18, 0xf7, 0x11, 0x94, 0xfb, 0x06,
            0x04, 0x1a, 0xae, 0xe6, 0x5f, 0xc0, 0x13, 0x23, 0x10, 0xf7, 0xc9, 0x3e, 0x7f, 0xdb, 0xfe, 0x1f,
            0x30, 0x09, 0x3e, 0xf7, 0xd3, 0xf5, 0xdb, 0xf6, 0xdb, 0xf7, 0xc9, 0xf1, 0xc9, 0xff
        };

        static BYTE abHDBoot4[] = {
            0x3e, 0xee, 0xd3, 0xf5, 0xd3, 0xf7, 0x3e, 0xfa, 0xd3, 0xf7, 0xc9, 0x3e, 0xff, 0xd3, 0xf5, 0xaf,
            0xed, 0x47, 0xed, 0x56, 0x01, 0xf8, 0x00, 0x57, 0x87, 0x87, 0x87, 0xed, 0x79, 0x7a, 0xd3, 0xfb,
            0x21, 0x00, 0x80, 0x11, 0x01, 0x80, 0x01, 0xff, 0x3f, 0x75, 0xed, 0xb0, 0x75, 0x34, 0x20, 0x17,
            0x34, 0x35, 0x20, 0x13, 0x25, 0xfa, 0xda, 0xeb, 0x3c, 0xfe, 0x20, 0x38, 0xd7, 0x3e, 0xfe, 0xdb,
            0xfe, 0x1f, 0x3e, 0x10, 0x30, 0x01, 0x87, 0x47, 0x3d, 0x5f, 0x32, 0xb4, 0x5c, 0xd3, 0xfc, 0x31,
            0x00, 0x4f, 0x21, 0x00, 0x51, 0x71, 0x23, 0x10, 0xfc, 0xd6, 0x21, 0x2f, 0x47, 0x36, 0xff, 0x23,
            0x10, 0xfb, 0x69, 0x06, 0x04, 0x36, 0x40, 0x7d, 0x23, 0x10, 0xfa, 0x32, 0xb0, 0x5c, 0x32, 0xb1,
            0x5c, 0x6b, 0x7d, 0xc6, 0x5f, 0x32, 0x9f, 0x5c, 0x3e, 0xc0, 0x77, 0x2b
        };

        static BYTE abHDBoot5[] = {
            0xcd, 0x84, 0xd9, 0xfe, 0x50, 0x20, 0xf9, 0xed, 0x41, 0xcd, 0x84, 0xd9, 0x87, 0x38, 0xfa, 0xe6,
            0x10, 0x28, 0xf4, 0xe5, 0x3e, 0xf0, 0xd3, 0xf5, 0xaf, 0x0d, 0xed, 0xa2, 0x0c, 0xed, 0xa2, 0x3d,
            0x20, 0xf7, 0xe1, 0xcd, 0x75, 0xd9, 0xc0, 0xe9, 0x48, 0x44, 0x20, 0x42, 0x4f, 0x4f, 0x54, 0x52,
            0x4f, 0x4d, 0x20, 0x56, 0xb2
        };

        static BYTE abHDBoot6[] = {
            0x01, 0x00, 0x90, 0xc8, 0x07, 0x00, 0x90, 0xff, 0x61, 0xff, 0x64, 0x30, 0x37
        };

        // Patch the 6 code blocks from above
        memcpy(&apbPageReadPtrs[ROM0][0x00b0], abHDBoot1, sizeof(abHDBoot1));
        memcpy(&apbPageReadPtrs[ROM0][0x0f7f], abHDBoot2, sizeof(abHDBoot2));
        memcpy(&apbPageReadPtrs[ROM1][0x18f9], abHDBoot3, sizeof(abHDBoot3));
        memcpy(&apbPageReadPtrs[ROM1][0x2bae], abHDBoot4, sizeof(abHDBoot4));
        memcpy(&apbPageReadPtrs[ROM1][0x35dd], abHDBoot5, sizeof(abHDBoot5));
        memcpy(&apbPageReadPtrs[ROM1][0x3bff], abHDBoot6, sizeof(abHDBoot6));

        // A few additional small tweaks
        apbPageReadPtrs[ROM0][0x0001] = 0x31;
        apbPageReadPtrs[ROM1][0x3c44] = 0x05;
        apbPageReadPtrs[ROM1][0x3c45] = 0xf6;
    }
}
