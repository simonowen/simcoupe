// Part of SimCoupe - A SAM Coupe emulator
//
// CPU.cpp: Z80 processor emulation and main emulation loop
//
//  Copyright (c) 2000-2003  Dave Laundon
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

// Changes 1999-2000 by Simon Owen:
//  - general revamp and reformat, with execution now polled for each frame
//  - very rough contended memory timings by doubling basic timings
//  - frame/line interrupt and flash frequency values corrected

// Changes 2000-2001 by Dave Laundon
//  - perfect contended memory timings on each memory/port access
//  - new cpu event model to reduce the per-instruction overhead
//  - MIDI OUT interrupt timings corrected

// ToDo:
//  - tidy things up a bit, particularly the register macros
//  - general state saving (CPU registers already in a structure for it)

#include "SimCoupe.h"
#include "CPU.h"

#include "BlueAlpha.h"
#include "Debug.h"
#include "Display.h"
#include "Frame.h"
#include "GUI.h"
#include "Input.h"
#include "IO.h"
#include "Memory.h"
#include "Mouse.h"
#include "Options.h"
#include "Profile.h"
#include "UI.h"
#include "Util.h"


#undef USE_FLAG_TABLES      // Experimental - disabled for now

// Look up table for the parity (and other common flags) for logical operations
BYTE g_abParity[256];
#define parity(a) (g_abParity[a])

#ifdef USE_FLAG_TABLES
BYTE g_abInc[256], g_abDec[256];
#endif

inline void CheckInterrupt ();

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
#define MEM_ACCESS(a)   do { g_dwCycleCounter += 3; if (afContendedPages[VPAGE(a)]) g_dwCycleCounter += pContention[g_dwCycleCounter]; } while (0)

// Update g_nLineCycle for one port access
// This is the basic four T-State CPU I/O access
// Longer I/O M-Cycles should have the extra T-States added after PORT_ACCESS
// Logic -  if ASIC-controlled port:
//              CPU can only access I/O port 1 out of every 8 T-States
#define PORT_ACCESS(a)  { (g_dwCycleCounter += 4) += ((a) >= BASE_ASIC_PORT) ? abPortContention[g_dwCycleCounter&7] : 0; }


BYTE bOpcode;
bool fReset, g_fBreak, g_fPaused, g_fTurbo;
int g_nFastBooting;

DWORD g_dwCycleCounter;     // Global cycle counter used for various timings

#ifdef _DEBUG
bool g_fDebug;              // Debug only helper variable, to trigger the debugger when set
#endif

// Memory access contention table
BYTE abContention1[TSTATES_PER_FRAME+64], abContention234[TSTATES_PER_FRAME+64], abContentionOff[TSTATES_PER_FRAME+64];
BYTE *pContention = abContention1;
//                         T1 T2 T3 T4 T1 T2 T3 T4
BYTE abPortContention[] = { 6, 5, 4, 3, 2, 1, 0, 7 };

// Memory access tracking for the debugger
BYTE *pbMemRead1, *pbMemRead2, *pbMemWrite1, *pbMemWrite2;

Z80Regs regs;

WORD* pHlIxIy, *pNewHlIxIy;
CPU_EVENT   asCpuEvents[MAX_EVENTS], *psNextEvent, *psFreeEvent;


bool CPU::Init (bool fFirstInit_/*=false*/)
{
    bool fRet = true;

    // Power on initialisation requires some extra initialisation
    if (fFirstInit_)
    {
        // Build the parity lookup table (including other flags for logical operations)
        for (int n = 0x00 ; n <= 0xff ; n++)
        {
            BYTE b2 = n ^ (n >> 4);
            b2 ^= (b2 << 2);
            b2 = ~(b2 ^ (b2 >> 1)) & FLAG_P;
            g_abParity[n] = (n & 0xa8) |    // S, 5, 3
                            ((!n) << 6) |   // Z
                            b2;             // P
#ifdef USE_FLAG_TABLES
            g_abInc[n] = (n & 0xa8) | ((!n) << 6) | ((!( n & 0xf)) << 4) | ((n == 0x80) << 2);
            g_abDec[n] = (n & 0xa8) | ((!n) << 6) | ((!(~n & 0xf)) << 4) | ((n == 0x7f) << 2) | FLAG_N;
#endif
        }

        // Perform some initial tests to confirm the emulator is functioning correctly!
        InitTests();

        // Clear all registers, but set IX/IY to 0xffff
        memset(&regs, 0, sizeof(regs));
        IX = IY = 0xffff;

        // Build the memory access contention tables
        for (UINT t2 = 0 ; t2 < sizeof(abContention1)/sizeof(abContention1[0]) ; t2++)
        {
            int nLine = t2 / TSTATES_PER_LINE, nLineCycle = t2 % TSTATES_PER_LINE;
            bool fScreen = nLine >= TOP_BORDER_LINES && nLine < TOP_BORDER_LINES+SCREEN_LINES &&
                           nLineCycle >= BORDER_PIXELS+BORDER_PIXELS;
            bool fMode1 = !(nLineCycle & 0x40);

            abContention1[t2] = ((t2+1)|((fScreen|fMode1)?7:3)) - 1 - t2;
            abContention234[t2] = ((t2+1)|(fScreen?7:3)) - 1 - t2;
            abContentionOff[t2] = ((t2+1)|3) - 1 - t2;
        }

        // Set up RAM and initial I/O settings
        fRet &= Memory::Init(true) && IO::Init(true);
    }

    // Perform a general reset by pressing and releasing the reset button
    Reset(true);
    Reset(false);

    return fRet;
}

void CPU::Exit (bool fReInit_/*=false*/)
{
    IO::Exit(fReInit_);
    Memory::Exit(fReInit_);
}


// Update contention table based on mode/screen-off changes
void CPU::UpdateContention ()
{
    pContention = (vmpr_mode == MODE_1) ? abContention1 :
                  (BORD_SOFF && VMPR_MODE_3_OR_4) ? abContentionOff : abContention234;
}


// Read an instruction byte and update timing
inline BYTE timed_read_code_byte (WORD addr)
{
    MEM_ACCESS(addr);
    return read_byte(addr);
}

// Read a data byte and update timing
inline BYTE timed_read_byte (WORD addr)
{
    MEM_ACCESS(addr);
    return *(pbMemRead1 = phys_read_addr(addr));
}

// Read an instruction word and update timing
inline WORD timed_read_code_word (WORD addr)
{
    MEM_ACCESS(addr);
    MEM_ACCESS(addr + 1);
    return read_word(addr);
}

// Read a data word and update timing
inline WORD timed_read_word (WORD addr)
{
    MEM_ACCESS(addr);
    MEM_ACCESS(addr + 1);
    return *(pbMemRead1 = phys_read_addr(addr)) | (*(pbMemRead2 = phys_read_addr(addr + 1)) << 8);
}

// Write a byte and update timing
inline void timed_write_byte (WORD addr, BYTE contents)
{
    MEM_ACCESS(addr);
    check_video_write(addr);
    *(pbMemWrite1 = phys_write_addr(addr)) = contents;
}

// Write a word and update timing
inline void timed_write_word (WORD addr, WORD contents)
{
    MEM_ACCESS(addr);
    check_video_write(addr);
    *(pbMemWrite1 = phys_write_addr(addr)) = contents & 0xff;
    MEM_ACCESS(addr + 1);
    check_video_write(addr + 1);
    *(pbMemWrite2 = phys_write_addr(addr + 1)) = contents >> 8;
}

// Write a word and update timing (high-byte first - used by stack functions)
inline void timed_write_word_reversed (WORD addr, WORD contents)
{
    MEM_ACCESS(addr + 1);
    check_video_write(addr + 1);
    *(pbMemWrite2 = phys_write_addr(addr + 1)) = contents >> 8;
    MEM_ACCESS(addr);
    check_video_write(addr);
    *(pbMemWrite1 = phys_write_addr(addr)) = contents & 0xff;
}


// Execute the CPU event specified
void CPU::ExecuteEvent (CPU_EVENT sThisEvent)
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
            Mouse::Reset();
            break;

        case evtBlueAlphaClock:
            // Clock the sampler, scheduling the next event if it's still running
            if (BlueAlphaSampler::Clock())
                AddCpuEvent(evtBlueAlphaClock, sThisEvent.dwTime + BLUE_ALPHA_CLOCK_TIME);
            break;
    }
}


// Execute until the end of a frame, or a breakpoint, whichever comes first
void CPU::ExecuteChunk ()
{
    ProfileStart(CPU);

    // Is the reset button is held in?
    if (fReset)
    {
        // Advance to the end of the frame
        g_dwCycleCounter = TSTATES_PER_FRAME;
    }

// Execute the first CPU core block in low-res mode, or if only 1 CPU core is compiled in
#if defined(USE_LOWRES) || defined(USE_ONECPUCORE)
    if (1)
#else
    if (Debug::IsBreakpointSet())
#endif
    {
        // Loop until we've reached the end of the frame
        for (g_fBreak = false ; !g_fBreak ; )
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

// Don't bother checking for breakpoints if the debugger isn't available
#if !defined(USE_LOWRES)
            // If we're not in an IX/IY instruction, check for breakpoints
            if (pNewHlIxIy == &HL && Debug::BreakpointHit())
                break;
#endif

#ifdef _DEBUG
            if (g_fDebug) g_fDebug = !Debug::Start();
#endif
        }
    }
#if !defined(USE_LOWRES) && !defined(USE_ONECPUCORE)
    else
    {
        // Loop until we've reached the end of the frame
        for (g_fBreak = false ; !g_fBreak ; )
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
#endif  // !defined(USE_LOWRES) && !defined(USE_ONECPUCORE)

    ProfileEnd();
}


// The main Z80 emulation loop
void CPU::Run ()
{
    // Loop until told to quit
    while (UI::CheckEvents())
    {
        if (g_fPaused)
            continue;

        // If fast booting is active, don't draw any video
        if (g_nFastBooting)
            fDrawFrame = GUI::IsActive() || !--g_nFastBooting;

        // CPU execution continues unless the debugger is active or there's a modal GUI dialog active
        if (!Debug::IsActive() && !GUI::IsModal())
            ExecuteChunk();

        // Complete and display the frame contents
        Frame::Complete();

        // The real end of the SAM frame requires some additional handling
        if (g_dwCycleCounter >= TSTATES_PER_FRAME)
        {
            CpuEventFrame(TSTATES_PER_FRAME);

            // Only update I/O (sound etc.) if we're not fast booting
            if (!g_nFastBooting)
            {
                IO::FrameUpdate();
                Debug::FrameEnd();
            }

            // Step back up to start the next frame
            g_dwCycleCounter %= TSTATES_PER_FRAME;
            Frame::Start();
        }
    }

    TRACE("Quitting main emulation loop...\n");
}


void CPU::Reset (bool fPress_)
{
    // Set CPU operating mode
    fReset = fPress_;

    if (fReset)
    {
        // Certain registers are initialised on every reset
        I = R = R7 = IFF1 = IFF2 = 0;
        PC = 0x0000;
        regs.halted = 0;
        bOpcode = 0x00; // not after EI

        // Index prefix not active
        pHlIxIy = pNewHlIxIy = &HL;

        // Very start of frame
        g_dwCycleCounter = 0;

        // Initialise the CPU events queue
        for (int n = 0 ; n < MAX_EVENTS ; n++)
            asCpuEvents[n].psNext = &asCpuEvents[(n+1) % MAX_EVENTS];
        psFreeEvent = asCpuEvents;
        psNextEvent = NULL;

        // Schedule the first end of line event, and an update check 3/4 through the frame
        AddCpuEvent(evtEndOfFrame, TSTATES_PER_FRAME);
        AddCpuEvent(evtInputUpdate, TSTATES_PER_FRAME*3/4);

        // Re-initialise memory (for configuration changes) and reset I/O
        IO::Init();
        Memory::Init();
    }
    // Set up the fast reset for first power-on, allowing UP TO 5 seconds before returning to normal mode
    else if  (GetOption(fastreset))
        g_nFastBooting = EMULATED_FRAMES_PER_SECOND * 5;

    Debug::Refresh();
}


void CPU::NMI()
{
    // Disable interrupts
    IFF1 = 0;

    // Advance PC if we're stopped on a HALT
    if (regs.halted)
    {
        PC++;
        regs.halted = 0;
    }

    push(PC);

    // Call NMI handler at address 0x0066
    PC = NMI_INTERRUPT_HANDLER;
    g_dwCycleCounter += 2;

    Debug::Refresh();
}


inline void CheckInterrupt ()
{
    // Only process if not delayed after a DI/EI and not in the middle of an indexed instruction
    if (bOpcode != OP_EI && bOpcode != OP_DI && (pNewHlIxIy == &HL))
    {
        // Disable maskable interrupts to prevent the handler being triggered again immediately
        IFF1 = IFF2 = 0;

        // Advance PC if we're stopped on a HALT
        if (regs.halted)
        {
            PC++;
            regs.halted = 0;
        }

        // Save the current PC on the stack
        push(PC);

        // The current interrupt mode determines how we handle the interrupt
        switch (IM)
        {
            case 0:
            {
                PC = IM1_INTERRUPT_HANDLER;
                g_dwCycleCounter += 6;
                break;
            }

            case 1:
            {
                PC = IM1_INTERRUPT_HANDLER;
                g_dwCycleCounter += 7;
                break;
            }

            case 2:
            {
                // Fetch the IM 2 handler address from an address formed from I and 0xff (from the bus)
                PC = timed_read_word((I << 8) | 0xff);
                g_dwCycleCounter += 7;
                break;
            }
        }
    }
}


// Perform some initial tests to confirm the emulator is functioning correctly!
void CPU::InitTests ()
{
    // Sanity check the endian of the registers structure.  If this fails you'll need to add a new
    // symbol test to the top of SimCoupe.h, to help identify the new little-endian platform
    HL = 1;
    if (H)
        Message(msgFatal, "Startup test: the Z80Regs structure is the wrong endian for this platform!");

#if 0   // Enable this for in-depth testing of 8-bit arithmetic operations

#define TEST_8(op, bit, flag, condition) \
    if (((F >> (bit)) & 1) != (condition)) \
        Message(msgFatal, "Startup test: " #op " (%d,%d,%d): flag " #flag " is %d, but should be %d!", \
            c, b, carry, ((F >> (bit)) & 1), (condition))
#define TEST_16(op, bit, flag, condition) \
    if (((F >> (bit)) & 1) != (condition)) \
        Message(msgFatal, "Startup test: " #op " (%d,%d,%d): flag " #flag " is %d, but should be %d!", \
            DE, BC, carry, ((F >> (bit)) & 1), (condition))

    // Check the state of CPU flags after arithmetic operations
    pHlIxIy = &HL;
    BC = 0;
    DE = 0;
    BYTE carry = 0;
    do
    {
        // NEG
        a = b;
        neg;
        TEST_8(NEG, 0, C, 0 - b != a);
        TEST_8(NEG, 1, N, 1);
        TEST_8(NEG, 2, V, (signed char)0 - (signed char)b != (signed char)a);
        TEST_8(NEG, 3, 3, (a >> 3) & 1);
        TEST_8(NEG, 4, H, (0 & 0xF) - (b & 0xF) != (a & 0xF));
        TEST_8(NEG, 5, 5, (a >> 5) & 1);
        TEST_8(NEG, 6, Z, a == 0);
        TEST_8(NEG, 7, S, (signed char)a < 0);

        do
        {
            // AND
            a = b;
            and_a(c);
            TEST_8(AND, 0, C, 0);
            TEST_8(AND, 1, N, 0);
            TEST_8(AND, 2, P, (1 ^ a ^ (a >> 1) ^ (a >> 2) ^ (a >> 3) ^ (a >> 4) ^ (a >> 5) ^ (a >> 6) ^ (a >> 7)) & 1);
            TEST_8(AND, 3, 3, (a >> 3) & 1);
            TEST_8(AND, 4, H, 1);
            TEST_8(AND, 5, 5, (a >> 5) & 1);
            TEST_8(AND, 6, Z, a == 0);
            TEST_8(AND, 7, S, (signed char)a < 0);

            // OR
            a = b;
            or_a(c);
            TEST_8(OR, 0, C, 0);
            TEST_8(OR, 1, N, 0);
            TEST_8(OR, 2, P, (1 ^ a ^ (a >> 1) ^ (a >> 2) ^ (a >> 3) ^ (a >> 4) ^ (a >> 5) ^ (a >> 6) ^ (a >> 7)) & 1);
            TEST_8(OR, 3, 3, (a >> 3) & 1);
            TEST_8(OR, 4, H, 0);
            TEST_8(OR, 5, 5, (a >> 5) & 1);
            TEST_8(OR, 6, Z, a == 0);
            TEST_8(OR, 7, S, (signed char)a < 0);

            // XOR
            a = b;
            xor_a(c);
            TEST_8(XOR, 0, C, 0);
            TEST_8(XOR, 1, N, 0);
            TEST_8(XOR, 2, P, (1 ^ a ^ (a >> 1) ^ (a >> 2) ^ (a >> 3) ^ (a >> 4) ^ (a >> 5) ^ (a >> 6) ^ (a >> 7)) & 1);
            TEST_8(XOR, 3, 3, (a >> 3) & 1);
            TEST_8(XOR, 4, H, 0);
            TEST_8(XOR, 5, 5, (a >> 5) & 1);
            TEST_8(XOR, 6, Z, a == 0);
            TEST_8(XOR, 7, S, (signed char)a < 0);

            // CP
            a = c;
            cp_a(b);
            a = c - b;
            TEST_8(CP, 0, C, c - b != a);
            TEST_8(CP, 1, N, 1);
            TEST_8(CP, 2, V, (signed char)c - (signed char)b != (signed char)a);
            TEST_8(CP, 3, 3, (b >> 3) & 1);
            TEST_8(CP, 4, H, (c & 0xF) - (b & 0xF) != (a & 0xF));
            TEST_8(CP, 5, 5, (b >> 5) & 1);
            TEST_8(CP, 6, Z, a == 0);
            TEST_8(CP, 7, S, (signed char)a < 0);

            do
            {
                // 8-bit ADD/ADC (common routine for both)
                a = c;
                F = carry;
                adc_a(b);
                TEST_8(ADD/ADC A, 0, C, c + b + carry != a);
                TEST_8(ADD/ADC A, 1, N, 0);
                TEST_8(ADD/ADC A, 2, V, (signed char)c + (signed char)b + carry != (signed char)a);
                TEST_8(ADD/ADC A, 3, 3, (a >> 3) & 1);
                TEST_8(ADD/ADC A, 4, H, (c & 0xF) + (b & 0xF) + carry != (a & 0xF));
                TEST_8(ADD/ADC A, 5, 5, (a >> 5) & 1);
                TEST_8(ADD/ADC A, 6, Z, a == 0);
                TEST_8(ADD/ADC A, 7, S, (signed char)a < 0);

                // 8-bit SUB/SBC (common routine for both)
                a = c;
                F = carry;
                sbc_a(b);
                TEST_8(SUB/SBC A, 0, C, c - b - carry != a);
                TEST_8(SUB/SBC A, 1, N, 1);
                TEST_8(SUB/SBC A, 2, V, (signed char)c - (signed char)b - carry != (signed char)a);
                TEST_8(SUB/SBC A, 3, 3, (a >> 3) & 1);
                TEST_8(SUB/SBC A, 4, H, (c & 0xF) - (b & 0xF) - carry != (a & 0xF));
                TEST_8(SUB/SBC A, 5, 5, (a >> 5) & 1);
                TEST_8(SUB/SBC A, 6, Z, a == 0);
                TEST_8(SUB/SBC A, 7, S, (signed char)a < 0);

#if 1   // Enable this for in-depth testing of 16-bit arithmetic operations
                do
                {
                    // 16-bit ADD (separate routine from ADC)
                    // Use the two carry states to test unaffected flags remain unchanged
                    HL = DE;
                    F = -carry;
                    add_hl(BC);
                    TEST_16(ADD HL, 0, C, DE + BC != HL);
                    TEST_16(ADD HL, 1, N, 0);
                    TEST_16(ADD HL, 2, V, carry);
                    TEST_16(ADD HL, 3, 3, (HL >> 11) & 1);
                    TEST_16(ADD HL, 4, H, (DE & 0xFFF) + (BC & 0xFFF) != (HL & 0xFFF));
                    TEST_16(ADD HL, 5, 5, (HL >> 13) & 1);
                    TEST_16(ADD HL, 6, Z, carry);
                    TEST_16(ADD HL, 7, S, carry);

                    // 16-bit ADC (separate routine from ADD)
                    HL = DE;
                    F = carry;
                    adc_hl(BC);
                    TEST_16(ADC HL, 0, C, DE + BC + carry != HL);
                    TEST_16(ADC HL, 1, N, 0);
                    TEST_16(ADC HL, 2, V, (signed short)DE + (signed short)BC + carry != (signed short)HL);
                    TEST_16(ADC HL, 3, 3, (HL >> 11) & 1);
                    TEST_16(ADC HL, 4, H, (DE & 0xFFF) + (BC & 0xFFF) + carry != (HL & 0xFFF));
                    TEST_16(ADC HL, 5, 5, (HL >> 13) & 1);
                    TEST_16(ADC HL, 6, Z, HL == 0);
                    TEST_16(ADC HL, 7, S, (signed short)HL < 0);

                    // 16-bit SBC
                    HL = DE;
                    F = carry;
                    sbc_hl(BC);
                    TEST_16(SBC HL, 0, C, DE - BC - carry != HL);
                    TEST_16(SBC HL, 1, N, 1);
                    TEST_16(SBC HL, 2, V, (signed short)DE - (signed short)BC - carry != (signed short)HL);
                    TEST_16(SBC HL, 3, 3, (HL >> 11) & 1);
                    TEST_16(SBC HL, 4, H, (DE & 0xFFF) - (BC & 0xFFF) - carry != (HL & 0xFFF));
                    TEST_16(SBC HL, 5, 5, (HL >> 13) & 1);
                    TEST_16(SBC HL, 6, Z, HL == 0);
                    TEST_16(SBC HL, 7, S, (signed short)HL < 0);
                }
                while ((++d, ++e) != 0);   // Doing the full range of DE takes too long...
#endif  // 16-bit tests (which can take a while)
            }
            while ((carry = !carry) != 0);
        }
        while (++c != 0);
    }
    while (++b != 0);

#undef TEST_16
#undef TEST_8

#endif  // 8-bit tests
}
