// Part of SimCoupe - A SAM Coupe emulator
//
// CPU.cpp: Z80 processor emulation and main emulation loop
//
//  Copyright (c) 2000-2003  Dave Laundon
//  Copyright (c) 1999-2003  Simon Owen
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
#include "Debug.h"
#include "Display.h"
#include "Frame.h"
#include "GUI.h"
#include "Input.h"
#include "IO.h"
#include "Memory.h"
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


#define a       regs.AF.B.h_
#define f       regs.AF.B.l_
#define b       regs.BC.B.h_
#define c       regs.BC.B.l_
#define d       regs.DE.B.h_
#define e       regs.DE.B.l_
#define h       regs.HL.B.h_
#define l       regs.HL.B.l_

#define af      regs.AF.W
#define bc      regs.BC.W
#define de      regs.DE.W
#define hl      regs.HL.W

#define a1      regs.AF_.B.h_
#define f1      regs.AF_.B.l_
#define b1      regs.BC_.B.h_
#define c1      regs.BC_.B.l_
#define d1      regs.DE_.B.h_
#define e1      regs.DE_.B.l_
#define h1      regs.HL_.B.h_
#define l1      regs.HL_.B.l_

#define alt_af  regs.AF_.W
#define alt_bc  regs.BC_.W
#define alt_de  regs.DE_.W
#define alt_hl  regs.HL_.W

#define ix      regs.IX.W
#define iy      regs.IY.W
#define sp      regs.SP.W
#define pc      regs.PC.W

#define ixh     regs.IX.B.h_
#define ixl     regs.IX.B.l_
#define iyh     regs.IY.B.h_
#define iyl     regs.IY.B.l_
#define sp_h    regs.SP.B.h_
#define sp_l    regs.SP.B.l_

#define r       regs.R
#define i       regs.I          // This daft one means we can't use 'i' as a 'for' variable in this module!
#define iff1    regs.IFF1
#define iff2    regs.IFF2
#define im      regs.IM


inline void CheckInterrupt ();
inline void Mode0Interrupt ();
inline void Mode1Interrupt ();
inline void Mode2Interrupt ();

// Since Java has no macros, changing helpers to be inlines like this should make things easier
inline void rflags (BYTE b_, BYTE c_) { f = c_ | parity(b_); }


////////////////////////////////////////////////////////////////////////////////
//  H E L P E R   M A C R O S


// Update g_nLineCycle for one memory access
// This is the basic three T-State CPU memory access
// Longer memory M-Cycles should have the extra T-States added after MEM_ACCESS
// Logic -  if in RAM:
//              if we are in the main screen area, or one of the extra MODE 1 contended areas:
//                  CPU can only access memory 1 out of every 8 T-States
//              else
//                  CPU can only access memory 1 out of every 4 T-States
#define MEM_ACCESS(a)   ((g_nLineCycle += 3) |= (afContendedPages[VPAGE(a)]) ? pMemAccess[g_nLineCycle >> 6] : 0)

// Update g_nLineCycle for one port access
// This is the basic four T-State CPU I/O access
// Longer I/O M-Cycles should have the extra T-States added after PORT_ACCESS
// Logic -  if ASIC-controlled port:
//              CPU can only access I/O port 1 out of every 8 T-States
#define PORT_ACCESS(a)  ((g_nLineCycle += 4) |= ((a) >= BASE_ASIC_PORT) ? 7 : 0)


BYTE g_bOpcode;             // The currently executing or previously executed instruction
int g_nLine;                // Scan line being generated (0 is the top of the generated display, not the main screen)
int g_nLineCycle;           // Cycles so far in the current scanline
int g_nPrevLineCycle;       // Cycles before current instruction began

bool fReset, g_fBreak, g_fPaused, g_fTurbo;
int g_nFastBooting;

DWORD g_dwCycleCounter;     // Global cycle counter used for various timings

bool fDelayedEI;            // Flag and counter to carry out a delayed EI

#ifdef _DEBUG
bool g_fDebug;              // Debug only helper variable, to trigger the debugger when set
#endif

// Memory access contention table
const int MEM_ACCESS_LINE = TSTATES_PER_LINE >> 6;
int aMemAccesses[10 * MEM_ACCESS_LINE], *pMemAccessBase, *pMemAccess, nMemAccessIndex;
bool fMemContention;

// Memory access tracking for the debugger
BYTE *pbMemRead1, *pbMemRead2, *pbMemWrite1, *pbMemWrite2;

Z80Regs regs;
DWORD radjust;

WORD* pHlIxIy, *pNewHlIxIy;
CPU_EVENT   asCpuEvents[MAX_EVENTS], *psNextEvent, *psFreeEvent;
DWORD dwLastTime, dwFPSTime;


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
            b2 = ~(b2 ^ (b2 >> 1)) & F_PARITY;
            g_abParity[n] = (n & 0xa8) |    // S, 5, 3
                            ((!n) << 6) |   // Z
                            b2;             // P

#ifdef USE_FLAG_TABLES
            g_abInc[n] = (n & 0xa8) | ((!n) << 6) | ((!( n & 0xf)) << 4) | ((n == 0x80) << 2);
            g_abDec[n] = (n & 0xa8) | ((!n) << 6) | ((!(~n & 0xf)) << 4) | ((n == 0x7f) << 2) | F_NADD;
#endif
        }

        // Perform some initial tests to confirm the emulator is functioning correctly!
        InitTests();

        // Most of the registers tend to only power-on defaults, and are not affected by a reset
        af = bc = de = hl = alt_af = alt_bc = alt_de = alt_hl = ix = iy = 0xffff;

        // Build the memory access contention table
        // Note - instructions often overlap to the next line (hence the duplicates).
        //  0, 1, 4 - border lines
        //  2, 3    - screen lines
        //  5 to 9  - mode 1 versions of the same
        // Lines 0 and 2 (5 and 7) are used for normal border or screen lines, with 1 and 3 (6 and 8) being duplicates
        // so if we overlap to the next line we will still get the correct contention.
        // Lines 1 and 3 (6 and 8) are used for the last line of the border or screen so that if we overlap to the next
        // line we will get the new correct contention.
        // Line 0 is used continuously if we are in mode 3 or 4 and the screen is off.
        pMemAccess = pMemAccessBase = aMemAccesses;
        fMemContention = true;
        nMemAccessIndex = 0;
        for (int t = 0; t < MEM_ACCESS_LINE; ++t)
        {
            int m = t * TSTATES_PER_LINE / MEM_ACCESS_LINE;
            aMemAccesses[0 * MEM_ACCESS_LINE + t] =
            aMemAccesses[1 * MEM_ACCESS_LINE + t] =
            aMemAccesses[4 * MEM_ACCESS_LINE + t] = 3;
            aMemAccesses[2 * MEM_ACCESS_LINE + t] =
            aMemAccesses[3 * MEM_ACCESS_LINE + t] =
                (m >= BORDER_PIXELS && m < BORDER_PIXELS + SCREEN_PIXELS) ? 7 : 3;
            aMemAccesses[5 * MEM_ACCESS_LINE + t] =
            aMemAccesses[6 * MEM_ACCESS_LINE + t] =
            aMemAccesses[9 * MEM_ACCESS_LINE + t] = (m & 0x40) ? 7 : 3;
            aMemAccesses[7 * MEM_ACCESS_LINE + t] =
            aMemAccesses[8 * MEM_ACCESS_LINE + t] = ((m & 0x40) ||
                (m >= BORDER_PIXELS && m < BORDER_PIXELS + SCREEN_PIXELS)) ? 7 : 3;
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


// Work out if we're in a vertical part of the screen that may be affected by contention
inline void SetContention ()
{
    pMemAccess = fMemContention ? (pMemAccessBase + nMemAccessIndex) : aMemAccesses;
}

// Update contention flags based on mode/screen-off changes
void CPU::UpdateContention ()
{
    fMemContention = !(BORD_SOFF && VMPR_MODE_3_OR_4);
    pMemAccessBase = aMemAccesses + ((vmpr_mode ^ MODE_1) ? 0 : (5 * MEM_ACCESS_LINE));
    SetContention();
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

// 16-bit push and pop
#define push(val)   ( sp -= 2, timed_write_word_reversed(sp,val) )
#define pop(var)    ( var = timed_read_word(sp), sp += 2 )


// Execute the CPU event specified
void CPU::ExecuteEvent (CPU_EVENT sThisEvent)
{
    switch (sThisEvent.nEvent)
    {
        case evtStdIntStart :
            // Check for a LINE interrupt on the following line
            if ((line_int < SCREEN_LINES) && (g_nLine == (line_int + TOP_BORDER_LINES - 1)))
            {
                // Signal the LINE interrupt, and start the interrupt counter
                status_reg &= ~STATUS_INT_LINE;
                AddCpuEvent(evtStdIntEnd, sThisEvent.dwTime + INT_ACTIVE_TIME);
            }
            // Check for a FRAME interrupt on the last line
            else if (g_nLine == (HEIGHT_LINES - 1))
            {
                // Signal a FRAME interrupt, and start the interrupt counter
                status_reg &= ~STATUS_INT_FRAME;
                AddCpuEvent(evtStdIntEnd, sThisEvent.dwTime + INT_ACTIVE_TIME);
            }
            break;

        case evtStdIntEnd :
            // Reset the interrupt as we're done
            status_reg |= (STATUS_INT_FRAME | STATUS_INT_LINE);
            break;

        case evtMidiOutIntStart :
            // Begin the MIDI_OUT interrupt and add an event to end it
            status_reg &= ~STATUS_INT_MIDIOUT;
            AddCpuEvent(evtMidiOutIntEnd, sThisEvent.dwTime + MIDI_INT_ACTIVE_TIME);
            break;

        case evtMidiOutIntEnd :
            // Reset the interrupt and clear the 'transmitting' bit in LPEN as we're done
            status_reg |= STATUS_INT_MIDIOUT;
            lpen &= ~LPEN_TXFMST;
            break;

        case evtEndOfLine :
            // Subtract a line's worth of cycles and move to the next line down
            g_nPrevLineCycle -= TSTATES_PER_LINE;
            g_nLineCycle -= TSTATES_PER_LINE;
            g_nLine++;
            // Add an event for the next line
            AddCpuEvent(evtEndOfLine, sThisEvent.dwTime + TSTATES_PER_LINE);

            // If we're at the end of the frame, signal it
            if (g_nLine >= HEIGHT_LINES)
                g_fBreak = true;
            else
            {
                // Work out if we're in a vertical part of the screen that may be affected by contention
                nMemAccessIndex = MEM_ACCESS_LINE * (
                    (((g_nLine >= TOP_BORDER_LINES) && (g_nLine < TOP_BORDER_LINES + SCREEN_LINES)) ? 2 : 0) +
                    ((g_nLine == TOP_BORDER_LINES - 1) || (g_nLine == TOP_BORDER_LINES + SCREEN_LINES - 1))
                );
                SetContention();

                // Are we on a line that may potentially require an interrupt at the start of the right border?
                if (((g_nLine >= (TOP_BORDER_LINES - 1)) && (g_nLine < (TOP_BORDER_LINES - 1 + SCREEN_LINES))) ||
                    (g_nLine == (HEIGHT_LINES - 1)))
                {
                    // Add an event to check for LINE/FRAME interrupts
                    AddCpuEvent(evtStdIntStart, sThisEvent.dwTime + INT_START_TIME);
                }
            }

            break;

        case evtInputUpdate :
            // Update the input in the centre of the screen (well away from the frame boundary) to avoid the ROM
            // keyboard scanner discarding key presses when it thinks keys have bounced.  In old versions this was
            // the cause of the first key press on the boot screen only clearing it (took AGES to track down!)
            IO::UpdateInput();

            // Schedule the next input check at the same time in the next frame
            AddCpuEvent(evtInputUpdate, sThisEvent.dwTime + TSTATES_PER_FRAME);
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
        // Effectively halt the CPU for the full frame
        g_nLineCycle += TSTATES_PER_FRAME;
        CheckCpuEvents();
    }

    if (!Debug::IsBreakpointSet())
    {
        // Loop until we've reached the end of the frame
        for (g_fBreak = false ; !g_fBreak ; )
        {
            // Keep track of the current and previous state of whether we're processing an indexed instruction
            pHlIxIy = pNewHlIxIy;
            pNewHlIxIy = &hl;

            // Fetch... (and advance PC)
            g_bOpcode = timed_read_code_byte(pc++);
            radjust++;

            // ... Decode ...
            switch (g_bOpcode)
            {
#include "Z80ops.h"     // ... Execute!
            }

            // Update the line/global counters and check/process for pending events
            CheckCpuEvents();

            // Are there any active interrupts?
            if (status_reg != STATUS_INT_NONE && iff1)
                CheckInterrupt();

#ifdef _DEBUG
            if (g_fDebug) g_fDebug = !Debug::Start();
#endif
        }
    }
    else
    {
        // Loop until we've reached the end of the frame
        for (g_fBreak = false ; !g_fBreak ; )
        {
            // Keep track of the current and previous state of whether we're processing an indexed instruction
            pHlIxIy = pNewHlIxIy;
            pNewHlIxIy = &hl;

            // Fetch... (and advance PC)
            g_bOpcode = timed_read_code_byte(pc++);
            radjust++;

            // ... Decode ...
            switch (g_bOpcode)
            {
#include "Z80ops.h"     // ... Execute!
            }

            // Update the line/global counters and check/process for pending events
            CheckCpuEvents();

            // Are there any active interrupts?
            if (status_reg != STATUS_INT_NONE && iff1)
                CheckInterrupt();

            if (pNewHlIxIy == &hl && Debug::BreakpointHit())
                break;

#ifdef _DEBUG
            if (g_fDebug) g_fDebug = !Debug::Start();
#endif
        }
    }

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
        if (g_nLine >= HEIGHT_LINES)
        {
            // Only update I/O (sound etc.) if we're not fast booting
            if (!g_nFastBooting)
                IO::FrameUpdate();

            // Step back up to start the next frame
            g_nLine -= HEIGHT_LINES;
            Frame::Start();
        }
    }

    TRACE("Quitting main emulation loop...\n");
}


void CPU::Reset (bool fPress_)
{
    // Set CPU operating mode
    fReset = fPress_;

    // Certain registers are initialised on every reset
    i = radjust = im = iff1 = iff2 = 0;
    sp = 0x8000;
    pc = 0x0000;

    // No index prefix seen yet, and no last instruction (for EI/DI look-back)
    pHlIxIy = pNewHlIxIy = &hl;
    g_bOpcode = OP_NOP;

    // Counter used to determine when each line should be drawn
    g_nLineCycle = g_nPrevLineCycle = 0;

    // Initialise the CPU events queue
    for (int n = 0 ; n < MAX_EVENTS ; n++)
        asCpuEvents[n].psNext = &asCpuEvents[(n+1) % MAX_EVENTS];
    psFreeEvent = asCpuEvents;
    psNextEvent = NULL;

    // Schedule the first end of line event, and an update check half way through the frame
    AddCpuEvent(evtEndOfLine, g_dwCycleCounter + TSTATES_PER_LINE);
    AddCpuEvent(evtInputUpdate, g_dwCycleCounter + TSTATES_PER_FRAME/2);

    // Re-initialise memory (for configuration changes) and reset I/O
    Memory::Init();
    IO::Init();

    // Set up the fast reset for first power-on, allowing UP TO 5 seconds before returning to normal mode
    if (!fPress_ && GetOption(fastreset))
        g_nFastBooting = EMULATED_FRAMES_PER_SECOND * 5;

    Debug::Refresh();
}


void CPU::NMI()
{
    // Advance PC if we're stopped on a HALT
    if (timed_read_code_byte(pc) == OP_HALT)
        pc++;

    // Save the current maskable interrupt status in iff2 and disable interrupts
    iff2 = iff1;
    iff1 = 0;
    g_nLineCycle += 2;

    // Call NMI handler at address 0x0066
    push(pc);
    pc = NMI_INTERRUPT_HANDLER;

    CheckCpuEvents();
    Debug::Refresh();
}


inline void CheckInterrupt ()
{
    // Only process if not delayed after a DI/EI and not in the middle of an indexed instruction
    if ((g_bOpcode != OP_EI) && (g_bOpcode != OP_DI) && (pNewHlIxIy == &hl))
    {
        // Disable maskable interrupts to prevent the handler being triggered again immediately
        iff1 = iff2 = 0;

        // Advance PC if we're stopped on a HALT, as we've got a maskable interrupt that it was waiting for
        if (g_bOpcode == OP_HALT)
            pc++;

        // The current interrupt mode determines how we handle the interrupt
        switch (im)
        {
            case 0: Mode0Interrupt(); break;
            case 1: Mode1Interrupt(); break;
            case 2: Mode2Interrupt(); break;
        }
    }
}

inline void Mode0Interrupt ()
{
    // Push PC onto the stack, and execute the interrupt handler
    g_nLineCycle += 6;
    push(pc);
    pc = IM1_INTERRUPT_HANDLER;
}

inline void Mode1Interrupt ()
{
    // Push PC onto the stack, and execute the interrupt handler
    g_nLineCycle += 7;
    push(pc);
    pc = IM1_INTERRUPT_HANDLER;
}

inline void Mode2Interrupt ()
{
    // Push PC onto the stack
    g_nLineCycle += 7;
    push(pc);

    // Fetch the IM 2 handler address from an address formed from I and 0xff (from the bus)
    pc = timed_read_word((i << 8) | 0xff);
}

// Perform some initial tests to confirm the emulator is functioning correctly!
void CPU::InitTests ()
{
    // Sanity check the endian of the registers structure
    hl = 1;
    if (h)
        Message(msgFatal, "Startup test: the Z80Regs structure is the wrong endian for this platform!");

#if 0   // Enable this for in-depth testing of 8-bit arithmetic operations

#define TEST_8(op, bit, flag, condition) \
    if (((f >> (bit)) & 1) != (condition)) \
        Message(msgFatal, "Startup test: " #op " (%d,%d,%d): flag " #flag " is %d, but should be %d!", \
            c, b, carry, ((f >> (bit)) & 1), (condition))
#define TEST_16(op, bit, flag, condition) \
    if (((f >> (bit)) & 1) != (condition)) \
        Message(msgFatal, "Startup test: " #op " (%d,%d,%d): flag " #flag " is %d, but should be %d!", \
            de, bc, carry, ((f >> (bit)) & 1), (condition))

    // Check the state of CPU flags after arithmetic operations
    pHlIxIy = &hl;
    bc = 0;
    de = 0;
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
                f = carry;
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
                f = carry;
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
                    hl = de;
                    f = -carry;
                    add_hl(bc);
                    TEST_16(ADD HL, 0, C, de + bc != hl);
                    TEST_16(ADD HL, 1, N, 0);
                    TEST_16(ADD HL, 2, V, carry);
                    TEST_16(ADD HL, 3, 3, (hl >> 11) & 1);
                    TEST_16(ADD HL, 4, H, (de & 0xFFF) + (bc & 0xFFF) != (hl & 0xFFF));
                    TEST_16(ADD HL, 5, 5, (hl >> 13) & 1);
                    TEST_16(ADD HL, 6, Z, carry);
                    TEST_16(ADD HL, 7, S, carry);

                    // 16-bit ADC (separate routine from ADD)
                    hl = de;
                    f = carry;
                    adc_hl(bc);
                    TEST_16(ADC HL, 0, C, de + bc + carry != hl);
                    TEST_16(ADC HL, 1, N, 0);
                    TEST_16(ADC HL, 2, V, (signed short)de + (signed short)bc + carry != (signed short)hl);
                    TEST_16(ADC HL, 3, 3, (hl >> 11) & 1);
                    TEST_16(ADC HL, 4, H, (de & 0xFFF) + (bc & 0xFFF) + carry != (hl & 0xFFF));
                    TEST_16(ADC HL, 5, 5, (hl >> 13) & 1);
                    TEST_16(ADC HL, 6, Z, hl == 0);
                    TEST_16(ADC HL, 7, S, (signed short)hl < 0);

                    // 16-bit SBC
                    hl = de;
                    f = carry;
                    sbc_hl(bc);
                    TEST_16(SBC HL, 0, C, de - bc - carry != hl);
                    TEST_16(SBC HL, 1, N, 1);
                    TEST_16(SBC HL, 2, V, (signed short)de - (signed short)bc - carry != (signed short)hl);
                    TEST_16(SBC HL, 3, 3, (hl >> 11) & 1);
                    TEST_16(SBC HL, 4, H, (de & 0xFFF) - (bc & 0xFFF) - carry != (hl & 0xFFF));
                    TEST_16(SBC HL, 5, 5, (hl >> 13) & 1);
                    TEST_16(SBC HL, 6, Z, hl == 0);
                    TEST_16(SBC HL, 7, S, (signed short)hl < 0);
                }
                while ((++d, ++e) != 0);   // Doing the full range of de takes too long...
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
