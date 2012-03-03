// Part of SimCoupe - A SAM Coupe emulator
//
// Memory.cpp: Memory configuration and management
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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "SimCoupe.h"
#include "Memory.h"

#include "CPU.h"
#include "CStream.h"
#include "HDBOOT.h"
#include "Options.h"
#include "OSD.h"
#include "SAMROM.h"
#include "Util.h"

////////////////////////////////////////////////////////////////////////////////

// Single block holding all memory needed
BYTE* pMemory;
int nAllocatedPages;

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

static void LoadRoms (BYTE* pb0_, BYTE* pb1_);


// Allocate and initialise memory
bool Memory::Init (bool fFirstInit_/*=false*/)
{
    if (fFirstInit_)
    {
        // Build the tables for fast mapping between mode 1 display addresses and line numbers
        for (UINT uOffset = 0 ; uOffset < SCREEN_LINES ; uOffset++)
        {
            g_abMode1ByteToLine[uOffset] = (uOffset & 0xc0) + ((uOffset << 3) & 0x38) + ((uOffset >> 3) & 0x07);
            g_awMode1LineToByte[g_abMode1ByteToLine[uOffset]] = uOffset << 5;
        }
    }

    int nIntPages = N_PAGES_MAIN / (1+(GetOption(mainmem) == 256));
    int nExtBanks = min(GetOption(externalmem), MAX_EXTERNAL_MB), nExtPages =  nExtBanks*N_PAGES_1MB;
    int nRamPages = nIntPages+nExtPages, nTotalPages = nRamPages + 2 + 2;
    BYTE *pb, *apbRead[TOTAL_PAGES], *apbWrite[TOTAL_PAGES];

    // Only consider changes if the memory requirements have changes
    if (nTotalPages != nAllocatedPages)
    {
        // Error/fail depending on whether we've got an existing allocation to fall back on
        if (!(pb = new BYTE[nTotalPages*MEM_PAGE_SIZE]))
        {
            Message(pMemory ? msgError : msgFatal, "Out of memory!");
            return false;
        }
        else
        {
            // Initialise memory to 0xff, and stripe the RAM banks between 0xff and 0x00 every 128 bytes
            memset(pb, 0xff, nTotalPages*MEM_PAGE_SIZE);
            for (int i = 0 ; i < nRamPages*MEM_PAGE_SIZE ; i += 0x100)
                memset(pb+i, 0x00, 0x080);
        }


        // Set up the scratch banks after the ROMs, used for reads from invalid memory and writes to read-only memory
        apbRead[SCRATCH_READ]  = apbWrite[SCRATCH_READ]  = pb + (nIntPages+nExtPages+2)*MEM_PAGE_SIZE;
        apbRead[SCRATCH_WRITE] = apbWrite[SCRATCH_WRITE] = pb + (nIntPages+nExtPages+3)*MEM_PAGE_SIZE;

        // Invalidate all of memory
        for (int nPage = 0 ; nPage < TOTAL_PAGES ; nPage++)
        {
            apbRead[nPage]  = apbRead[SCRATCH_READ];
            apbWrite[nPage] = apbWrite[SCRATCH_WRITE];
        }

        // Add internal RAM as read/write
        for (int nInt = 0 ; nInt < nIntPages ; nInt++)
            apbRead[INTMEM+nInt] = apbWrite[nInt] = pb + nInt*MEM_PAGE_SIZE;

        // Add external RAM as read/write
        for (int nExt = 0 ; nExt < nExtPages ; nExt++)
            apbRead[EXTMEM+nExt] = apbWrite[EXTMEM+nExt] = pb + (nIntPages+nExt)*MEM_PAGE_SIZE;

        // Add the ROMs as read-only
        for (int nRom = 0 ; nRom < 2 ; nRom++)
            apbRead[ROM0+nRom] = pb + (nIntPages+nExtPages+nRom)*MEM_PAGE_SIZE;


        // If there's an existing memory image, copy it to the new configuration
        if (pMemory)
        {
            for (int nPage = 0 ; nPage < ROM0 ; nPage++)
            {
                // Copy only present pages
                if (apbRead[nPage] != apbRead[SCRATCH_READ])
                    memcpy(apbRead[nPage], apbPageReadPtrs[nPage], MEM_PAGE_SIZE);
            }

            delete pMemory;
            pMemory = NULL;
        }

        // Set the new configuration to be live
        memcpy(apbPageReadPtrs, apbRead, sizeof(apbPageReadPtrs));
        memcpy(apbPageWritePtrs, apbWrite, sizeof(apbPageWritePtrs));
        pMemory = pb;
        nAllocatedPages = nTotalPages;

        // Finally, refresh the paging to update any physical memory references
        IO::OutLmpr(lmpr);
        IO::OutHmpr(hmpr);
        IO::OutVmpr(vmpr);
    }

    // Load/update the ROM images
    LoadRoms(apbPageReadPtrs[ROM0], apbPageReadPtrs[ROM1]);

    return true;
}

void Memory::Exit (bool fReInit_/*=false*/)
{
    if (!fReInit_) { delete[] pMemory; pMemory = NULL; }
}


// Read the ROM image into the ROM area of our paged memory block
static void LoadRoms (BYTE* pb0_, BYTE* pb1_)
{
    CStream* pROM;

    const char* pcszROM = GetOption(rom);

    // Use a custom ROM if supplied
    if (*pcszROM && (pROM = CStream::Open(pcszROM)))
    {
        // Read the header+bootstrap code from what could be a ZX82 file (for Andy Wright's ROM images)
        BYTE abHeader[140];
        pROM->Read(abHeader, sizeof(abHeader));

        // If we don't find the ZX82 signature, rewind to read as a plain ROM file
        if (memcmp(abHeader, "ZX82", 4))
            pROM->Rewind();

        // Read both 16K ROM images
        pROM->Read(pb0_, MEM_PAGE_SIZE);
        pROM->Read(pb1_, MEM_PAGE_SIZE);

        delete pROM;
        return;
    }

    // Report the failure if there
    if (*pcszROM)
    {
        Message(msgWarning, "Error loading custom ROM:\n%s\n\nReverting to built-in ROM image.", pcszROM);
        SetOption(rom,"");
    }

    // Start with the built-in 3.0 ROM image
    memcpy(pb0_, abSAMROM, MEM_PAGE_SIZE);
    memcpy(pb1_, &abSAMROM[MEM_PAGE_SIZE], MEM_PAGE_SIZE);

    // Edwin Blink's HDBOOT ROM enabled?
    if (GetOption(hdbootrom))
    {
        // What we patch depends on the HDD interface attached
        switch (GetOption(drive2))
        {
            // Original Atom
            case dskAtom:
                // Patch from ROM30 to Atom
                PatchBlock(pb0_, abAtomPatch0);
                PatchBlock(pb1_, abAtomPatch1);
                break;

            // Atom Lite
            case dskAtomLite:
                PatchBlock(pb0_, abAtomLitePatch0);
                PatchBlock(pb1_, abAtomLitePatch1);
                break;
        }
    }
}
