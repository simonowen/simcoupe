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
#include "Util.h"


#undef USE_FLAG_TABLES      // Experimental - disabled for now

// Look up table for the parity (and other common flags) for logical operations
BYTE g_abParity[256];
#define parity(a) (g_abParity[a])

#ifdef USE_FLAG_TABLES
BYTE g_abInc[256], g_abDec[256];
#endif

#define rflags(b_,c_)   (F = (c_) | parity(b_))


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


BYTE bOpcode;
bool g_fReset, g_fBreak, g_fPaused;
int g_nTurbo;

DWORD g_dwCycleCounter;     // Global cycle counter used for various timings

#ifdef _DEBUG
bool g_fDebug;              // Debug only helper variable, to trigger the debugger when set
#endif

// Memory access tracking for the debugger
BYTE* pbMemRead1, * pbMemRead2, * pbMemWrite1, * pbMemWrite2;

Z80Regs regs;

WORD* pHlIxIy, * pNewHlIxIy;
CPU_EVENT asCpuEvents[MAX_EVENTS], * psNextEvent, * psFreeEvent;


namespace CPU
{
// Memory access contention table
static BYTE abContention1[TSTATES_PER_FRAME + 64], abContention234[TSTATES_PER_FRAME + 64], abContention4T[TSTATES_PER_FRAME + 64];
static const BYTE* pMemContention = abContention1;
static bool fContention = true;
static const BYTE abPortContention[] = { 6, 5, 4, 3, 2, 1, 0, 7 };
//                                      T1 T2 T3 T4 T1 T2 T3 T4

inline void CheckInterrupt();


bool Init(bool fFirstInit_/*=false*/)
{
    bool fRet = true;

    // Power on initialisation requires some extra initialisation
    if (fFirstInit_)
    {
        InitCpuEvents();

        // Build the parity lookup table (including other flags for logical operations)
        for (int n = 0x00; n <= 0xff; n++)
        {
            BYTE b2 = n ^ (n >> 4);
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
        IX = IY = 0xffff;

        // Build the memory access contention tables
        for (UINT t2 = 0; t2 < _countof(abContention1); t2++)
        {
            int nLine = t2 / TSTATES_PER_LINE, nLineCycle = t2 % TSTATES_PER_LINE;
            bool fScreen = nLine >= TOP_BORDER_LINES && nLine < TOP_BORDER_LINES + SCREEN_LINES &&
                nLineCycle >= BORDER_PIXELS + BORDER_PIXELS;
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
inline BYTE timed_read_code_byte(WORD addr)
{
    MEM_ACCESS(addr);
    return read_byte(addr);
}

// Read a data byte and update timing
inline BYTE timed_read_byte(WORD addr)
{
    MEM_ACCESS(addr);
    return *(pbMemRead1 = AddrReadPtr(addr));
}

// Read an instruction word and update timing
inline WORD timed_read_code_word(WORD addr)
{
    MEM_ACCESS(addr);
    MEM_ACCESS(addr + 1);
    return read_word(addr);
}

// Read a data word and update timing
inline WORD timed_read_word(WORD addr)
{
    MEM_ACCESS(addr);
    MEM_ACCESS(addr + 1);
    return *(pbMemRead1 = AddrReadPtr(addr)) | (*(pbMemRead2 = AddrReadPtr(addr + 1)) << 8);
}

// Write a byte and update timing
inline void timed_write_byte(WORD addr, BYTE contents)
{
    MEM_ACCESS(addr);
    check_video_write(addr);
    pbMemWrite1 = AddrReadPtr(addr); // breakpoints act on read location!
    *AddrWritePtr(addr) = contents;
}

// Write a word and update timing
inline void timed_write_word(WORD addr, WORD contents)
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
inline void timed_write_word_reversed(WORD addr, WORD contents)
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
void ExecuteEvent(CPU_EVENT sThisEvent)
{
    switch (sThisEvent.nEvent)
    {
    case evtStdIntEnd:
        // Reset the interrupt as we're done
        status_reg |= (STATUS_INT_FRAME | STATUS_INT_LINE);
        break;

    case evtMidiOutIntStart:
        // Begin the MIDI_OUT interrupt and add an event to end it
        status_reg &= ~STATUS_INT_MIDIOUT;
        AddCpuEvent(evtMidiOutIntEnd, sThisEvent.dwTime + MIDI_INT_ACTIVE_TIME);
        break;

    case evtMidiOutIntEnd:
        // Reset the interrupt and clear the 'transmitting' bit in LPEN as we're done
        status_reg |= STATUS_INT_MIDIOUT;
        lpen &= ~LPEN_TXFMST;
        break;

    case evtLineIntStart:
    {
        // Begin the line interrupt and add an event to end it
        status_reg &= ~STATUS_INT_LINE;
        AddCpuEvent(evtStdIntEnd, sThisEvent.dwTime + INT_ACTIVE_TIME);

        AddCpuEvent(evtLineIntStart, sThisEvent.dwTime + TSTATES_PER_FRAME);
        break;
    }

    case evtEndOfFrame:
    {
        // Signal a FRAME interrupt, and start the interrupt counter
        status_reg &= ~STATUS_INT_FRAME;
        AddCpuEvent(evtStdIntEnd, sThisEvent.dwTime + INT_ACTIVE_TIME);

        AddCpuEvent(evtEndOfFrame, sThisEvent.dwTime + TSTATES_PER_FRAME);

        // Signal end of the frame
        g_fBreak = true;
        break;
    }

    case evtInputUpdate:
        // Update the input in the centre of the screen (well away from the frame boundary) to avoid the ROM
        // keyboard scanner discarding key presses when it thinks keys have bounced.  In old versions this was
        // the cause of the first key press on the boot screen only clearing it (took AGES to track down!)
        IO::UpdateInput();

        // Schedule the next input check at the same position in the next frame
        AddCpuEvent(evtInputUpdate, sThisEvent.dwTime + TSTATES_PER_FRAME);
        break;

    case evtMouseReset:
        pMouse->Reset();
        break;

    case evtBlueAlphaClock:
        // Clock the sampler, scheduling the next event if it's still running
        if (pBlueAlpha->Clock())
            AddCpuEvent(evtBlueAlphaClock, sThisEvent.dwTime + BLUE_ALPHA_CLOCK_TIME);
        break;

    case evtAsicStartup:
        // ASIC is now responsive
        IO::WakeAsic();
        break;

    case evtTapeEdge:
        Tape::NextEdge(sThisEvent.dwTime);
        break;
    }
}


// Execute until the end of a frame, or a breakpoint, whichever comes first
void ExecuteChunk()
{
    // Is the reset button is held in?
    if (g_fReset)
    {
        // Advance to the end of the frame
        g_dwCycleCounter = TSTATES_PER_FRAME;
    }

    // Execute the first CPU core if only 1 CPU core is compiled in
#if defined(USE_ONECPUCORE)
    if (1)
#else
    if (Debug::IsBreakpointSet())
#endif
    {
        // Loop until we've reached the end of the frame
        for (g_fBreak = false; !g_fBreak; )
        {
            // Keep track of the current and previous state of whether we're processing an indexed instruction
            pHlIxIy = pNewHlIxIy;
            pNewHlIxIy = &HL;

            // Fetch... (and advance PC)
            bOpcode = timed_read_code_byte(PC++);
            R++;

            // ... Decode ...
            switch (bOpcode)
            {
#include "Z80ops.h"     // ... Execute!
            }

            // Update the line/global counters and check/process for pending events
            CheckCpuEvents();

            // Are there any active interrupts?
            if (status_reg != STATUS_INT_NONE && IFF1)
                CheckInterrupt();

            // If we're not in an IX/IY instruction, check for breakpoints
            if (pNewHlIxIy == &HL && Debug::BreakpointHit())
                break;

#ifdef _DEBUG
            if (g_fDebug) g_fDebug = !Debug::Start();
#endif
        }
    }
#if !defined(USE_ONECPUCORE)
    else
    {
        // Loop until we've reached the end of the frame
        for (g_fBreak = false; !g_fBreak; )
        {
            // Keep track of the current and previous state of whether we're processing an indexed instruction
            pHlIxIy = pNewHlIxIy;
            pNewHlIxIy = &HL;

            // Fetch... (and advance PC)
            bOpcode = timed_read_code_byte(PC++);
            R++;

            // ... Decode ...
            switch (bOpcode)
            {
#include "Z80ops.h"     // ... Execute!
            }

            // Update the line/global counters and check/process for pending events
            CheckCpuEvents();

            // Are there any active interrupts?
            if (status_reg != STATUS_INT_NONE && IFF1)
                CheckInterrupt();

#ifdef _DEBUG
            if (g_fDebug) g_fDebug = !Debug::Start();
#endif
        }
    }
#endif  // !defined(USE_ONECPUCORE)
}


// The main Z80 emulation loop
void Run()
{
    // Loop until told to quit
    while (UI::CheckEvents())
    {
        if (g_fPaused)
            continue;

        // If fast booting is active, don't draw any video
        if (g_nTurbo & TURBO_BOOT)
            fDrawFrame = GUI::IsActive();

        // Prepare start of frame image, in case we've already started it
        Frame::Begin();

        // CPU execution continues unless the debugger is active or there's a modal GUI dialog active
        if (!Debug::IsActive() && !GUI::IsModal())
            ExecuteChunk();

        // Finish end of frame image, in case we haven't finished it
        Frame::End();

        // The real end of the SAM frame requires some additional handling
        if (g_dwCycleCounter >= TSTATES_PER_FRAME)
        {
            CpuEventFrame(TSTATES_PER_FRAME);

            IO::FrameUpdate();
            Debug::FrameEnd();
            Frame::Flyback();

            // Step back up to start the next frame
            g_dwCycleCounter %= TSTATES_PER_FRAME;
        }
    }

    TRACE("Quitting main emulation loop...\n");
}


void Reset(bool fPress_)
{
    // Set CPU operating mode
    g_fReset = fPress_;

    if (g_fReset)
    {
        // Certain registers are initialised on every reset
        I = R = R7 = IFF1 = IFF2 = 0;
        PC = 0x0000;
        bOpcode = 0x00; // not after EI

        // Index prefix not active
        pHlIxIy = pNewHlIxIy = &HL;

        // Clear the CPU events queue
        InitCpuEvents();

        // Schedule the first end of line event, and an update check 3/4 through the frame
        AddCpuEvent(evtEndOfFrame, TSTATES_PER_FRAME);
        AddCpuEvent(evtInputUpdate, TSTATES_PER_FRAME * 3 / 4);

        // Re-initialise memory (for configuration changes) and reset I/O
        IO::Init();
        Memory::Init();

        // Refresh the debugger and re-test breakpoints
        Debug::Refresh();
    }
    // Set up the fast reset for first power-on
    else if (GetOption(fastreset))
        g_nTurbo |= TURBO_BOOT;
}


void NMI()
{
    // R is incremented when the interrupt is acknowledged
    R++;

    // Disable interrupts
    IFF1 = 0;

    // Advance PC if we're stopped on a HALT
    if (read_byte(PC) == OP_HALT)
        PC++;

    g_dwCycleCounter += 2;
    push(PC);
    PC = NMI_INTERRUPT_HANDLER;

    // Refresh the debugger for the NMI
    Debug::Refresh();
}


inline void CheckInterrupt()
{
    // Only process if not delayed after a DI/EI and not in the middle of an indexed instruction
    if (bOpcode != OP_EI && bOpcode != OP_DI && (pNewHlIxIy == &HL))
    {
        // If we're running in debugger timing mode, skip the interrupt handler
        if (!IsContentionActive())
            return;

        // R is incremented when the interrupt is acknowledged
        R++;

        // Disable maskable interrupts to prevent the handler being triggered again immediately
        IFF1 = IFF2 = 0;

        // Advance PC if we're stopped on a HALT
        if (bOpcode == OP_HALT)
            PC++;

        // The current interrupt mode determines how we handle the interrupt
        switch (IM)
        {
        case 0:
        {
            g_dwCycleCounter += 6;
            push(PC);
            PC = IM1_INTERRUPT_HANDLER;
            break;
        }

        case 1:
        {
            g_dwCycleCounter += 7;
            push(PC);
            PC = IM1_INTERRUPT_HANDLER;
            break;
        }

        case 2:
        {
            g_dwCycleCounter += 7;
            push(PC);
            PC = timed_read_word((I << 8) | 0xff);
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
    HL = 1;
    if (H)
        Message(msgFatal, "Startup test: the Z80Regs structure is the wrong endian for this platform!");
}

} // namespace CPU
