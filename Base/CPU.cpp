// Part of SimCoupe - A SAM Coupe emulator
//
// CPU.cpp: Z80 processor emulation and main emulation loop
//
//  Copyright (c) 1999-2015 Simon Owen
//  Copyright (c) 2000-2003 Dave Laundon
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

// Changes 1999-2000 by Simon Owen:
//  - general revamp and reformat, with execution now polled for each frame
//  - very rough contended memory timings by doubling basic timings
//  - frame/line interrupt and flash frequency values corrected

// Changes 2000-2001 by Dave Laundon
//  - perfect contended memory timings on each memory/port access
//  - new cpu event model to reduce the per-instruction overhead
//  - MIDI OUT interrupt timings corrected

#include "SimCoupe.h"
#include "CPU.h"

#include "BlueAlpha.h"
#include "Debug.h"
#include "Frame.h"
#include "GUI.h"
#include "Input.h"
#include "SAMIO.h"
#include "Memory.h"
#include "Mouse.h"
#include "Options.h"
#include "Tape.h"
#include "UI.h"


#undef USE_FLAG_TABLES      // Experimental - disabled for now

// Look up table for the parity (and other common flags) for logical operations
uint8_t g_abParity[256];
#define parity(a) (g_abParity[a])

#ifdef USE_FLAG_TABLES
uint8_t g_abInc[256], g_abDec[256];
#endif

#define rflags(b_,c_)   (REG_F = (c_) | parity(b_))


////////////////////////////////////////////////////////////////////////////////
//  H E L P E R   M A C R O S


// Update g_dwCycleCounter for one memory access
// This is the basic three T-State CPU memory access
// Longer memory M-Cycles should have the extra T-States added after MEM_ACCESS
// Logic -  if in RAM:
//              if we are in the main screen area, or one of the extra MODE 1 contended areas:
//                  CPU can only access memory 1 out of every 8 T-States
//              else
//                  CPU can only access memory 1 out of every 4 T-States
#define MEM_ACCESS(a)   do { g_dwCycleCounter += 3; if (afSectionContended[AddrSection(a)]) g_dwCycleCounter += pMemContention[g_dwCycleCounter]; } while (0)

// Update g_nLineCycle for one port access
// This is the basic four T-State CPU I/O access
// Longer I/O M-Cycles should have the extra T-States added after PORT_ACCESS
// Logic -  if ASIC-controlled port:
//              CPU can only access I/O port 1 out of every 8 T-States
#define PORT_ACCESS(a)  do { g_dwCycleCounter += 4; if ((a) >= BASE_ASIC_PORT) g_dwCycleCounter += abPortContention[g_dwCycleCounter&7]; } while (0)


uint8_t bOpcode;
bool g_fReset, g_fBreak, g_fPaused;
int g_nTurbo;

uint32_t g_dwCycleCounter;     // Global cycle counter used for various timings

#ifdef _DEBUG
bool g_fDebug;              // Debug only helper variable, to trigger the debugger when set
#endif

// Memory access tracking for the debugger
uint8_t* pbMemRead1, * pbMemRead2, * pbMemWrite1, * pbMemWrite2;

Z80Regs regs;

uint16_t* pHlIxIy, * pNewHlIxIy;
CPU_EVENT asCpuEvents[MAX_EVENTS], * psNextEvent, * psFreeEvent;


namespace CPU
{
// Memory access contention table
static uint8_t abContention1[CPU_CYCLES_PER_FRAME + 64], abContention234[CPU_CYCLES_PER_FRAME + 64], abContention4T[CPU_CYCLES_PER_FRAME + 64];
static const uint8_t* pMemContention = abContention1;
static bool fContention = true;
static const uint8_t abPortContention[] = { 6, 5, 4, 3, 2, 1, 0, 7 };
//                                      T1 T2 T3 T4 T1 T2 T3 T4

inline void CheckInterrupt();


bool Init(bool fFirstInit_/*=false*/)
{
    bool fRet = true;

    // Power on initialisation requires some extra initialisation
    if (fFirstInit_)
    {
        InitCpuEvents();

        // Schedule the first end of line event, and an update check 3/4 through the frame
        AddCpuEvent(EventType::FrameInterrupt, CPU_CYCLES_PER_FRAME);
        AddCpuEvent(EventType::InputUpdate, CPU_CYCLES_PER_FRAME * 3 / 4);

        // Build the parity lookup table (including other flags for logical operations)
        for (int n = 0x00; n <= 0xff; n++)
        {
            uint8_t b2 = n ^ (n >> 4);
            b2 ^= (b2 << 2);
            b2 = ~(b2 ^ (b2 >> 1))& FLAG_P;
            g_abParity[n] = (n & 0xa8) |    // S, 5, 3
                ((!n) << 6) |   // Z
                b2;             // P
#ifdef USE_FLAG_TABLES
            g_abInc[n] = (n & 0xa8) | ((!n) << 6) | ((!(n & 0xf)) << 4) | ((n == 0x80) << 2);
            g_abDec[n] = (n & 0xa8) | ((!n) << 6) | ((!(~n & 0xf)) << 4) | ((n == 0x7f) << 2) | FLAG_N;
#endif
        }

        // Perform some initial tests to confirm the emulator is functioning correctly!
        InitTests();

        // Clear all registers, but set IX/IY to 0xffff
        memset(&regs, 0, sizeof(regs));
        REG_IX = REG_IY = 0xffff;

        // Build the memory access contention tables
        for (unsigned int t2 = 0; t2 < std::size(abContention1); t2++)
        {
            int nLine = t2 / CPU_CYCLES_PER_LINE, nLineCycle = t2 % CPU_CYCLES_PER_LINE;
            bool fScreen = nLine >= TOP_BORDER_LINES && nLine < TOP_BORDER_LINES + GFX_SCREEN_LINES &&
                nLineCycle >= CPU_CYCLES_PER_SIDE_BORDER + CPU_CYCLES_PER_SIDE_BORDER;
            bool fMode1 = !(nLineCycle & 0x40);

            abContention1[t2] = ((t2 + 1) | ((fScreen | fMode1) ? 7 : 3)) - 1 - t2;
            abContention234[t2] = ((t2 + 1) | (fScreen ? 7 : 3)) - 1 - t2;
            abContention4T[t2] = ((t2 + 1) | 3) - 1 - t2;
        }

        // Set up RAM and initial I/O settings
        fRet &= Memory::Init(true) && IO::Init(true);
    }

    // Perform a general reset by pressing and releasing the reset button
    Reset(true);
    Reset(false);

    return fRet;
}

void Exit(bool fReInit_/*=false*/)
{
    IO::Exit(fReInit_);
    Memory::Exit(fReInit_);

    // TODO: remove after switching to use "physical" offsets instead of pointers
    if (!fReInit_)
        Breakpoint::RemoveAll();
}


bool IsContentionActive()
{
    return fContention;
}

// Update the active memory contention table based
void UpdateContention(bool fActive_/*=true*/)
{
    fContention = fActive_;

    pMemContention = !fActive_ ? abContention4T :
        (vmpr_mode == MODE_1) ? abContention1 :
        (BORD_SOFF && VMPR_MODE_3_OR_4) ? abContention4T :
        abContention234;
}


// Read an instruction byte and update timing
inline uint8_t timed_read_code_byte(uint16_t addr)
{
    MEM_ACCESS(addr);
    return read_byte(addr);
}

// Read a data byte and update timing
inline uint8_t timed_read_byte(uint16_t addr)
{
    MEM_ACCESS(addr);
    return *(pbMemRead1 = AddrReadPtr(addr));
}

// Read an instruction word and update timing
inline uint16_t timed_read_code_word(uint16_t addr)
{
    MEM_ACCESS(addr);
    MEM_ACCESS(addr + 1);
    return read_word(addr);
}

// Read a data word and update timing
inline uint16_t timed_read_word(uint16_t addr)
{
    MEM_ACCESS(addr);
    MEM_ACCESS(addr + 1);
    return *(pbMemRead1 = AddrReadPtr(addr)) | (*(pbMemRead2 = AddrReadPtr(addr + 1)) << 8);
}

// Write a byte and update timing
inline void timed_write_byte(uint16_t addr, uint8_t contents)
{
    MEM_ACCESS(addr);
    check_video_write(addr);
    pbMemWrite1 = AddrReadPtr(addr); // breakpoints act on read location!
    *AddrWritePtr(addr) = contents;
}

// Write a word and update timing
inline void timed_write_word(uint16_t addr, uint16_t contents)
{
    MEM_ACCESS(addr);
    check_video_write(addr);
    pbMemWrite1 = AddrReadPtr(addr);
    *AddrWritePtr(addr) = contents & 0xff;

    MEM_ACCESS(addr + 1);
    check_video_write(addr + 1);
    pbMemWrite2 = AddrReadPtr(addr + 1);
    *AddrWritePtr(addr + 1) = contents >> 8;
}

// Write a word and update timing (high-byte first - used by stack functions)
inline void timed_write_word_reversed(uint16_t addr, uint16_t contents)
{
    MEM_ACCESS(addr + 1);
    check_video_write(addr + 1);
    pbMemWrite2 = AddrReadPtr(addr + 1);
    *AddrWritePtr(addr + 1) = contents >> 8;

    MEM_ACCESS(addr);
    check_video_write(addr);
    pbMemWrite1 = AddrReadPtr(addr);
    *AddrWritePtr(addr) = contents & 0xff;
}


// Execute the CPU event specified
void ExecuteEvent(const CPU_EVENT& sThisEvent)
{
    switch (sThisEvent.type)
    {
    case EventType::FrameInterrupt:
        status_reg &= ~STATUS_INT_FRAME;
        AddCpuEvent(EventType::FrameInterruptEnd, sThisEvent.due_time + INT_ACTIVE_TIME);
        AddCpuEvent(EventType::FrameInterrupt, sThisEvent.due_time + CPU_CYCLES_PER_FRAME);

        g_fBreak = true;
        break;

    case EventType::FrameInterruptEnd:
        status_reg |= STATUS_INT_FRAME;
        break;

    case EventType::LineInterrupt:
        status_reg &= ~STATUS_INT_LINE;
        AddCpuEvent(EventType::LineInterruptEnd, sThisEvent.due_time + INT_ACTIVE_TIME);
        AddCpuEvent(EventType::LineInterrupt, sThisEvent.due_time + CPU_CYCLES_PER_FRAME);
        break;

    case EventType::LineInterruptEnd:
        status_reg |= STATUS_INT_LINE;
        break;

    case EventType::MidiOutStart:
        status_reg &= ~STATUS_INT_MIDIOUT;
        AddCpuEvent(EventType::MidiOutEnd, sThisEvent.due_time + MIDI_INT_ACTIVE_TIME);
        AddCpuEvent(EventType::MidiTxfmstEnd, sThisEvent.due_time + MIDI_TXFMST_ACTIVE_TIME);
        break;

    case EventType::MidiOutEnd:
        status_reg |= STATUS_INT_MIDIOUT;
        break;

    case EventType::MidiTxfmstEnd:
        lpen &= ~LPEN_TXFMST;
        break;

    case EventType::MouseReset:
        pMouse->Reset();
        break;

    case EventType::BlueAlphaClock:
        pSampler->Clock(sThisEvent.due_time);
        break;

    case EventType::TapeEdge:
        Tape::NextEdge(sThisEvent.due_time);
        break;

    case EventType::AsicReady:
        IO::WakeAsic();
        break;

    case EventType::InputUpdate:
        IO::UpdateInput();
        AddCpuEvent(EventType::InputUpdate, sThisEvent.due_time + CPU_CYCLES_PER_FRAME);
        break;

    case EventType::None:
        break;
    }
}


// Execute until end of frame or breakpoint
void ExecuteChunk()
{
    if (g_fReset)
    {
        g_dwCycleCounter = CPU_CYCLES_PER_FRAME;
        CheckCpuEvents();
        return;
    }

    auto no_breakpoints = Breakpoint::breakpoints.empty();

    for (g_fBreak = false; !g_fBreak; )
    {
        pHlIxIy = pNewHlIxIy;
        pNewHlIxIy = &REG_HL;

        bOpcode = timed_read_code_byte(REG_PC++);
        REG_R++;

        switch (bOpcode)
        {
#include "Z80ops.h"
        }

        CheckCpuEvents();

        if (status_reg != STATUS_INT_NONE && REG_IFF1)
            CheckInterrupt();

        if (pNewHlIxIy != &REG_HL || no_breakpoints)
            continue;

        Debug::AddTraceRecord();

        if (auto bp_index = Breakpoint::Hit())
        {
            Debug::Start(bp_index);
        }
#ifdef _DEBUG
        else if (g_fDebug)
        {
            Debug::Start();
            g_fDebug = false;
        }
#endif
    }
}

// The main Z80 emulation loop
void Run()
{
    // Loop until told to quit
    while (UI::CheckEvents())
    {
        if (g_fPaused)
            continue;

        // Prepare start of frame image, in case we've already started it
        Frame::Begin();

        // CPU execution continues unless the debugger is active or there's a modal GUI dialog active
        if (!Debug::IsActive() && !GUI::IsModal())
            ExecuteChunk();

        // Finish end of frame image, in case we haven't finished it
        Frame::End();

        // The real end of the SAM frame requires some additional handling
        if (g_dwCycleCounter >= CPU_CYCLES_PER_FRAME)
        {
            CpuEventFrame(CPU_CYCLES_PER_FRAME);

            IO::FrameUpdate();
            Debug::FrameEnd();
            Frame::Flyback();

            // Step back up to start the next frame
            g_dwCycleCounter %= CPU_CYCLES_PER_FRAME;
        }
    }

    TRACE("Quitting main emulation loop...\n");
}


void Reset(bool fPress_)
{
    if (GetOption(fastreset) && g_fReset && !fPress_)
    {
        g_nTurbo |= TURBO_BOOT;
    }

    g_fReset = fPress_;

    if (g_fReset)
    {
        // Certain registers are initialised on every reset
        REG_IFF1 = REG_IFF2 = REG_IM = 0;
        REG_I = REG_R = 0;
        REG_SP = REG_AF = 0xffff;
        REG_PC = 0x0000;
        bOpcode = 0x00; // not after EI

        // Index prefix not active
        pHlIxIy = pNewHlIxIy = &REG_HL;

        IO::Init();
        Memory::Init();

        Debug::Refresh();
    }
}


void NMI()
{
    // R is incremented when the interrupt is acknowledged
    REG_R++;

    // Disable interrupts
    REG_IFF1 = 0;

    // Advance PC if we're stopped on a HALT
    if (read_byte(REG_PC) == OP_HALT)
        REG_PC++;

    g_dwCycleCounter += 2;
    push(REG_PC);
    REG_PC = NMI_INTERRUPT_HANDLER;

    // Refresh the debugger for the NMI
    Debug::Refresh();
}


inline void CheckInterrupt()
{
    // Only process if not delayed after a DI/EI and not in the middle of an indexed instruction
    if (bOpcode != OP_EI && bOpcode != OP_DI && (pNewHlIxIy == &REG_HL))
    {
        // If we're running in debugger timing mode, skip the interrupt handler
        if (!IsContentionActive())
            return;

        // R is incremented when the interrupt is acknowledged
        REG_R++;

        // Disable maskable interrupts to prevent the handler being triggered again immediately
        REG_IFF1 = REG_IFF2 = 0;

        // Advance PC if we're stopped on a HALT
        if (bOpcode == OP_HALT)
            REG_PC++;

        // The current interrupt mode determines how we handle the interrupt
        switch (REG_IM)
        {
        case 0:
        {
            g_dwCycleCounter += 6;
            push(REG_PC);
            REG_PC = IM1_INTERRUPT_HANDLER;
            break;
        }

        case 1:
        {
            g_dwCycleCounter += 7;
            push(REG_PC);
            REG_PC = IM1_INTERRUPT_HANDLER;
            break;
        }

        case 2:
        {
            g_dwCycleCounter += 7;
            push(REG_PC);
            REG_PC = timed_read_word((REG_I << 8) | 0xff);
            break;
        }
        }
    }
}


// Perform some initial tests to confirm the emulator is functioning correctly!
void InitTests()
{
    // Sanity check the endian of the registers structure.  If this fails you'll need to add a new
    // symbol test to the top of SimCoupe.h, to help identify the new little-endian platform
    REG_HL = 1;
    if (REG_H)
        Message(MsgType::Fatal, "Z80Regs structure is the wrong endian for this platform!");
}

} // namespace CPU
