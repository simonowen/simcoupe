// Part of SimCoupe - A SAM Coupe emulator
//
// Memory.cpp: Memory configuration and management
//
//  Copyright (c) 1999-2015 Simon Owen
//  Copyright (c) 1996-2001 Allan Skillman
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
#include "Options.h"
#include "OSD.h"
#include "Stream.h"
#include "Util.h"

////////////////////////////////////////////////////////////////////////////////

// Single block holding all memory needed
BYTE *pMemory;

// Master read and write lists that are static for a given memory configuration
int anReadPages[TOTAL_PAGES];
int anWritePages[TOTAL_PAGES];

// Page numbers present in each of the 4 sections in the 64K address range
int anSectionPages[4];
bool afSectionContended[4];

// Array of pointers for memory to use when reading from or writing to each each section
BYTE *apbSectionReadPtrs[4];
BYTE *apbSectionWritePtrs[4];

// Look-up tables for fast mapping between mode 1 display addresses and line numbers
WORD g_awMode1LineToByte[SCREEN_LINES];
BYTE g_abMode1ByteToLine[SCREEN_LINES];

////////////////////////////////////////////////////////////////////////////////

namespace Memory
{
static bool fUpdateRom;

static void SetConfig ();
static bool LoadRoms ();

// Allocate and initialise memory
bool Init (bool fFirstInit_/*=false*/)
{
    if (fFirstInit_)
    {
        // Build the tables for fast mapping between mode 1 display addresses and line numbers
        for (UINT uOffset = 0 ; uOffset < SCREEN_LINES ; uOffset++)
        {
            g_abMode1ByteToLine[uOffset] = (uOffset & 0xc0) + ((uOffset << 3) & 0x38) + ((uOffset >> 3) & 0x07);
            g_awMode1LineToByte[g_abMode1ByteToLine[uOffset]] = uOffset << 5;
        }

        // Allocate a single block for our memory requirements
        if (!(pMemory = new BYTE[TOTAL_PAGES*MEM_PAGE_SIZE]))
            Message(msgFatal, "Out of memory!");

        // Initialise memory to 0xff
        memset(pMemory, 0xff, TOTAL_PAGES*MEM_PAGE_SIZE);

        // Stripe RAM in blocks of 0x00 every 128 bytes
        for (int i = 0 ; i < ROM0*MEM_PAGE_SIZE ; i += 0x100)
            memset(pMemory+i, 0x00, 0x80);
    }

    // Set the active memory configuration
    SetConfig();

    // Load the ROM on first boot, or if asked to refresh it
    if (fFirstInit_ || fUpdateRom)
    {
        LoadRoms();
        fUpdateRom = false;
    }

    return true;
}

void Exit (bool fReInit_/*=false*/)
{
    if (!fReInit_) delete[] pMemory, pMemory = nullptr;
}


// Update the active memory configuration
void UpdateConfig ()
{
    SetConfig();
}

// Request the ROM image be reloaded on the next reset
void UpdateRom ()
{
    fUpdateRom = true;
}


// Memory page description, for the debugger
const char *PageDesc (int nPage_, bool fCompact_/*=false*/)
{
    static char sz[32];
    const char *pcszSep = fCompact_ ? "" : " ";

    if (nPage_ >= INTMEM && nPage_ < EXTMEM)
        snprintf(sz, sizeof(sz)-1, "RAM%s%02X", pcszSep, nPage_-INTMEM);
	else if (nPage_ >= EXTMEM && nPage_ < ROM0)
		snprintf(sz, sizeof(sz) - 1, "EXT%s%02X", pcszSep, nPage_ - EXTMEM);
	else if (nPage_ == ROM0 || nPage_ == ROM1)
		snprintf(sz, sizeof(sz) - 1, "ROM%s%X", pcszSep, nPage_ - ROM0);
	else
		snprintf(sz, sizeof(sz) - 1, "UNK%s%02X", pcszSep, nPage_);

	return sz;
}


// Set the current memory configuration
static void SetConfig ()
{
	// Start with no memory accessible
	for (int nPage = 0; nPage < TOTAL_PAGES; nPage++)
	{
		anReadPages[nPage] = SCRATCH_READ;
		anWritePages[nPage] = SCRATCH_WRITE;
	}

	// Add internal RAM as read/write
	int nIntPages = (GetOption(mainmem) == 256) ? N_PAGES_MAIN / 2 : N_PAGES_MAIN;
	for (int nInt = 0; nInt < nIntPages; nInt++)
		anReadPages[INTMEM + nInt] = anWritePages[INTMEM + nInt] = INTMEM + nInt;

	// Add external RAM as read/write
	int nExtPages = std::min(GetOption(externalmem), MAX_EXTERNAL_MB) * N_PAGES_1MB;
	for (int nExt = 0; nExt < nExtPages; nExt++)
		anReadPages[EXTMEM + nExt] = anWritePages[EXTMEM + nExt] = EXTMEM + nExt;

	// Add the ROMs as read-only
	anReadPages[ROM0] = ROM0;
	anReadPages[ROM1] = ROM1;

	// If enabled, allow ROM writes
	if (GetOption(romwrite))
	{
		anWritePages[ROM0] = anReadPages[ROM0];
		anWritePages[ROM1] = anReadPages[ROM1];
	}
}

// Set the ROM from our internal 3.0 image or external custom file
static bool LoadRoms ()
{
	BYTE *pb0 = PageReadPtr(ROM0);
	BYTE *pb1 = PageReadPtr(ROM1);

	// Default to the standard ROM image
	std::string rom_file = OSD::MakeFilePath(MFP_RESOURCE, "samcoupe.rom");

	// Allow a custom ROM override
	if (*GetOption(rom))
		rom_file = GetOption(rom);

	// Allow an Atom / Atom Lite ROM override
	else if (GetOption(atombootrom))
	{
		// Atom Lite ROM used if active on either drive
		if (GetOption(drive2) == drvAtomLite || GetOption(drive2) == drvAtomLite)
			rom_file = OSD::MakeFilePath(MFP_RESOURCE, "atomlite.rom");

		// Atom ROM used if active as drive 2 only
		else if (GetOption(drive2) == drvAtom)
			rom_file = OSD::MakeFilePath(MFP_RESOURCE, "atom.rom");
	}

	auto rom = CStream::Open(rom_file.c_str());
	if (!rom)
	{
		// Fall back on the default if a specific ROM image failed to load
		rom_file = OSD::MakeFilePath(MFP_RESOURCE, "samcoupe.rom");
		rom = CStream::Open(rom_file.c_str());
	}

	if (rom)
	{
		size_t uRead = 0;

		// Read the header+bootstrap code from what could be a ZX82 file (for Andy Wright's ROM images)
		BYTE abHeader[140];
		rom->Read(abHeader, sizeof(abHeader));

		// If we don't find the ZX82 signature, rewind to read as a plain ROM file
		if (memcmp(abHeader, "ZX82", 4))
			rom->Rewind();

		// Read both 16K ROM images
		uRead += rom->Read(pb0, MEM_PAGE_SIZE);
		uRead += rom->Read(pb1, MEM_PAGE_SIZE);

		// Clean up the ROM file stream
		delete rom;

		// Return if the full 32K was read
		if (uRead == MEM_PAGE_SIZE * 2)
			return true;
	}

	// Invalidate the ROM image
	memset(pb0, 0xff, MEM_PAGE_SIZE);
	memset(pb1, 0xff, MEM_PAGE_SIZE);

	Message(msgWarning, "Error loading ROM:\n\n%s", rom_file.c_str());
	return false;
}

} // namespace Memory
