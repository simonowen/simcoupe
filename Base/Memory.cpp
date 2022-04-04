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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "SimCoupe.h"
#include "Memory.h"

#include "CPU.h"
#include "Frame.h"
#include "Options.h"
#include "Stream.h"

////////////////////////////////////////////////////////////////////////////////

// Single block holding all memory needed
uint8_t pMemory[TOTAL_PAGES * MEM_PAGE_SIZE];

// Primary read and write lists that are static for a given memory configuration
int anReadPages[TOTAL_PAGES];
int anWritePages[TOTAL_PAGES];

// Page numbers present in each of the 4 sections in the 64K address range
int anSectionPages[4];
bool afSectionContended[4];

// Array of pointers for memory to use when reading from or writing to each each section
uint8_t* apbSectionReadPtrs[4];
uint8_t* apbSectionWritePtrs[4];

// Look-up tables for fast mapping between mode 1 display addresses and line numbers
uint16_t g_awMode1LineToByte[GFX_SCREEN_LINES];
uint8_t g_abMode1ByteToLine[GFX_SCREEN_LINES];

static void update_rom_hooks();

////////////////////////////////////////////////////////////////////////////////

namespace Memory
{
bool full_contention = true;
uint8_t *last_phys_read1, *last_phys_read2, *last_phys_write1, *last_phys_write2;

uint8_t contention_mode1[CPU_CYCLES_PER_FRAME + 64];
uint8_t contention_mode234[CPU_CYCLES_PER_FRAME + 64];
uint8_t contention_4T[CPU_CYCLES_PER_FRAME + 64];
const uint8_t* contention_ptr = contention_mode1;

static bool fUpdateRom;

static bool LoadRoms();

// Allocate and initialise memory
bool Init(bool fFirstInit_/*=false*/)
{
    if (fFirstInit_)
    {
        // Build the tables for fast mapping between mode 1 display addresses and line numbers
        for (unsigned int uOffset = 0; uOffset < GFX_SCREEN_LINES; uOffset++)
        {
            g_abMode1ByteToLine[uOffset] = (uOffset & 0xc0) + ((uOffset << 3) & 0x38) + ((uOffset >> 3) & 0x07);
            g_awMode1LineToByte[g_abMode1ByteToLine[uOffset]] = uOffset << 5;
        }

        // Initialise memory to 0xff
        memset(pMemory, 0xff, TOTAL_PAGES * MEM_PAGE_SIZE);

        // Stripe RAM in blocks of 0x00 every 128 bytes
        for (int i = 0; i < ROM0 * MEM_PAGE_SIZE; i += 0x100)
            memset(pMemory + i, 0x00, 0x80);

        // Build memory contention tables.
        for (unsigned t = 0; t < std::size(contention_mode1); ++t)
        {
            int line = t / CPU_CYCLES_PER_LINE;
            auto line_cycle = (t + CPU_CYCLES_SCREEN_CONTENTION_OFFSET) % CPU_CYCLES_PER_LINE;
            bool main_screen =
                line >= TOP_BORDER_LINES &&
                line < TOP_BORDER_LINES + GFX_SCREEN_LINES &&
                line_cycle >= CPU_CYCLES_PER_SIDE_BORDER + CPU_CYCLES_PER_SIDE_BORDER;
            bool mode1_band = !(line_cycle & 0x40);

            auto mask = (main_screen || mode1_band) ? 7 : 3;
            contention_mode1[t] = mask - ((t + 2) & mask);

            mask = main_screen ? 7 : 3;
            contention_mode234[t] = mask - ((t + 2) & mask);

            mask = 3;
            contention_4T[t] = mask - ((t + 2) & mask);
        }
    }

    UpdateConfig();

    // Load the ROM on first boot, or if asked to refresh it
    if (fFirstInit_ || fUpdateRom)
    {
        LoadRoms();
        update_rom_hooks();
        fUpdateRom = false;
    }

    return true;
}

void Exit(bool fReInit_/*=false*/)
{
}

void UpdateContention()
{
    contention_ptr = !full_contention ? contention_4T :
        ((IO::State().vmpr & VMPR_MODE_MASK) == VMPR_MODE_1) ? contention_mode1 :
        IO::ScreenDisabled() ? contention_4T :
        contention_mode234;
}

void UpdateRom()
{
    fUpdateRom = true;
}

void UpdateConfig()
{
    for (int page = 0; page < TOTAL_PAGES; page++)
    {
        anReadPages[page] = SCRATCH_READ;
        anWritePages[page] = SCRATCH_WRITE;
    }

    int nIntPages = (GetOption(mainmem) == 256) ? NUM_INTERNAL_PAGES / 2 : NUM_INTERNAL_PAGES;
    for (int nInt = 0; nInt < nIntPages; nInt++)
        anReadPages[INTMEM + nInt] = anWritePages[INTMEM + nInt] = INTMEM + nInt;

    int nExtPages = std::min(GetOption(externalmem), MAX_EXTERNAL_MB) * NUM_EXTERNAL_PAGES_1MB;
    for (int nExt = 0; nExt < nExtPages; nExt++)
        anReadPages[EXTMEM + nExt] = anWritePages[EXTMEM + nExt] = EXTMEM + nExt;

    anReadPages[ROM0] = ROM0;
    anReadPages[ROM1] = ROM1;

    if (GetOption(romwrite))
    {
        anWritePages[ROM0] = anReadPages[ROM0];
        anWritePages[ROM1] = anReadPages[ROM1];
    }
}

// Set the ROM from our internal 3.0 image or external custom file
static bool LoadRoms()
{
    auto pb0 = PageReadPtr(ROM0);
    auto pb1 = PageReadPtr(ROM1);

    // Default to the standard ROM image
    auto rom_file = OSD::MakeFilePath(PathType::Resource, "samcoupe.rom");

    // Allow a custom ROM override
    if (!GetOption(rom).empty())
        rom_file = GetOption(rom);

    // Allow an Atom / Atom Lite ROM override
    else if (GetOption(atombootrom))
    {
        // Atom Lite ROM used if active on either drive
        if (GetOption(drive1) == drvAtomLite || GetOption(drive2) == drvAtomLite)
            rom_file = OSD::MakeFilePath(PathType::Resource, "atomlite.rom");

        // Atom ROM used if active as drive 2 only
        else if (GetOption(drive2) == drvAtom)
            rom_file = OSD::MakeFilePath(PathType::Resource, "atom.rom");
    }

    auto rom = Stream::Open(rom_file.c_str());
    if (!rom)
    {
        // Fall back on the default if a specific ROM image failed to load
        rom_file = OSD::MakeFilePath(PathType::Resource, "samcoupe.rom");
        rom = Stream::Open(rom_file.c_str());
    }

    if (rom)
    {
        size_t bytes_read = 0;

        // Read the header+bootstrap code from what could be a ZX82 file (for Andy Wright's ROM images)
        constexpr auto zx82_header_size = 140;
        std::array<uint8_t, zx82_header_size> header;
        rom->Read(header.data(), header.size());

        // If we don't find the ZX82 signature, rewind to read as a plain ROM file
        constexpr std::string_view zx82_sig{ "ZX82" };
        if (std::memcmp(header.data(), zx82_sig.data(), zx82_sig.size()))
            rom->Rewind();

        bytes_read += rom->Read(pb0, MEM_PAGE_SIZE);
        bytes_read += rom->Read(pb1, MEM_PAGE_SIZE);

        // Accept raw images or ZX82 files starting with a DI instruction.
        constexpr size_t rom_image_size = MEM_PAGE_SIZE * 2;
        if (bytes_read == rom_image_size &&
            (rom->GetSize() == rom_image_size || *pb0 == OP_DI))
        {
            return true;
        }
    }

    memset(pb0, 0xff, MEM_PAGE_SIZE);
    memset(pb1, 0xff, MEM_PAGE_SIZE);

    Message(MsgType::Warning, "Error loading ROM:\n\n{}", rom_file);
    return false;
}

std::string PageDesc(int page, bool compact)
{
    auto separator = compact ? "" : " ";

    if (page >= INTMEM && page < EXTMEM)
        return fmt::format("RAM{}{:02X}", separator, page - INTMEM);
    else if (page >= EXTMEM && page < ROM0)
        return fmt::format("EXT{}{:02X}", separator, page - EXTMEM);
    else if (page == ROM0 || page == ROM1)
        return fmt::format("ROM{}{:X}", separator, page - ROM0);
    else
        return fmt::format("UNK{}{:02X}", separator, page);
}

} // namespace Memory


void write_to_screen_vmpr0(uint16_t addr)
{
    addr &= (MEM_PAGE_SIZE - 1);

    switch (IO::State().vmpr & VMPR_MODE_MASK)
    {
    case VMPR_MODE_1:
        if (addr < MODE12_DATA_BYTES)
        {
            Frame::TouchLine(g_abMode1ByteToLine[addr >> 5] + TOP_BORDER_LINES);
        }
        else if (addr < MODE1_DISPLAY_BYTES)
        {
            auto line = (((addr - MODE12_DATA_BYTES) & 0xffe0) >> 2) + TOP_BORDER_LINES;
            Frame::TouchLines(line, line + 7);
        }

        break;

    case VMPR_MODE_2:
        if (addr < MODE12_DATA_BYTES || (addr >= MODE2_ATTR_OFFSET && addr < (MODE2_ATTR_OFFSET + MODE12_DATA_BYTES)))
            Frame::TouchLine(((addr & 0x1fff) >> 5) + TOP_BORDER_LINES);
        break;

    default:
        Frame::TouchLine((addr >> 7) + TOP_BORDER_LINES);
        break;
    }
}

void write_to_screen_vmpr1(uint16_t addr)
{
    addr &= (MEM_PAGE_SIZE - 1);

    if (addr < (MODE34_DISPLAY_BYTES - MEM_PAGE_SIZE))
        Frame::TouchLine(((addr + MEM_PAGE_SIZE) >> 7) + TOP_BORDER_LINES);
}

///////////////////////////////////////////////////////////////////////////////

using byte_pattern = std::vector<std::optional<uint8_t>>;
constexpr auto any_byte = std::nullopt;

struct hook_entry
{
    hook_entry(int page, byte_pattern&& match, int offset = 0)
        : page{ page }, match{ std::move(match) }, addr_offset(offset) { }

    int page;
    byte_pattern match;
    int addr_offset;    // offset from match to address of interest
    std::optional<uint16_t> addr;
};

std::vector<hook_entry> rom_hooks =
{
    // IMEXIT: pop bc; pop af; ei; ret  [@0057 in ROM 3.0]
    hook_entry{ ROM0, byte_pattern{ 0xc1, 0xf1, 0xfb, 0xc9 }, +3 },

    // WTFK: call readkey; jr z,wtfk [@0FA2 in ROM 3.0]
    hook_entry{ ROM0, byte_pattern{ 0xcd, any_byte, any_byte, 0x28, 0xfb }, +3 },

    // READKEY: rst 30; <addr>; jr z,+2; xor a; ret [@1CB2 in ROM 3.0]
    hook_entry{ ROM0, byte_pattern{ 0xf7, any_byte, any_byte, 0x28, 0x02, 0xaf, 0xc9 }, +1 },

    // BOOTNR: call BOOTEX; RST 8; defb 80; ret; [@D8DF in ROM 3.0, missing from 1.0]
    hook_entry{ ROM1, byte_pattern{ 0xcd, any_byte, any_byte, 0xcf, 0x80, 0xc9 }, },

    // MSDML: ld (hl),a; inc hl; djnz e; call nz,nn [@D4D6 in ROM 3.0, missing from <= 1.4]
    hook_entry { ROM1, byte_pattern{ 0x77, 0x23, 0x10, 0xf8, 0xc4 } },

    // LOADEXIT: ld a,l; cp 1; ret [@E739 in ROM 3.0]
    hook_entry { ROM1, byte_pattern{ 0x7d, 0xfe, 0x01, 0xc9 } },

    // LOADFAIL: xor h; ret nz; ld a,c [@E6F5 in ROM 3.0]
    hook_entry { ROM1, byte_pattern{ 0xac, 0xc0, 0x79 } },

    // SVLDCOM: ex af,af'; ld a,(bordcol); out (border),a [@E612 in ROM 3.0]
    hook_entry { ROM1, byte_pattern{ 0x08, 0x3a, 0x4b, 0x5c, 0xd3, 0xfe } },

    // LDSTRT: call nn; ret z; ld b,8 [@E678 in ROM 3.0]
    hook_entry { ROM1, byte_pattern{ 0xcd, any_byte, any_byte, 0xc8, 0x06, 0x08 }, +4 },

    // EDGLP: inc c; ret z; xor b [@2053 in ROM 3.0]
    hook_entry { ROM0, byte_pattern{ 0x0c, 0xc8, 0xa8 } },
};

std::optional<uint16_t> rom_hook_addr(RomHook h)
{
    const auto& hook = rom_hooks[static_cast<int>(h)];
    if (hook.addr && AddrPage(*hook.addr) == hook.page)
        return hook.addr;

    return std::nullopt;
}

static bool mem_match(const uint8_t* ptr, const byte_pattern& match)
{
    for (auto& b : match)
    {
        if (b.has_value() && *b != *ptr)
            return false;
        ptr++;
    }

    return true;
}

static void update_rom_hooks()
{
    for (auto& hook : rom_hooks)
    {
        std::vector<size_t> matches;

        const auto page_ptr = PageReadPtr(hook.page);
        size_t page_len = MEM_PAGE_SIZE - hook.match.size();

        for (size_t i = 0; i < page_len; ++i)
        {
            if (mem_match(page_ptr + i, hook.match))
            {
                auto base = SectionOffset((hook.page == ROM1) ? Section::D : Section::A);
                matches.push_back(base + i + hook.addr_offset);
            }
        }

        if (matches.size() == 1)
            hook.addr = static_cast<uint16_t>(matches[0]);
        else
            hook.addr = std::nullopt;
    }
}
