// Part of SimCoupe - A SAM Coupe emulator
//
// CPU.h: Z80 processor emulation and main emulation loop
//
//  Copyright (c) 2000-2003  Dave Laundon
//  Copyright (c) 1999-2006  Simon Owen
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

#ifndef Z80_H
#define Z80_H

#include "SAM.h"
#include "IO.h"
#include "Util.h"

struct _CPU_EVENT;
struct _Z80Regs;

class CPU
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

        static void Run ();
        static void UpdateContention ();
        static void ExecuteEvent (struct _CPU_EVENT sThisEvent);
        static void ExecuteChunk ();

        static void Reset (bool fPress_);
        static void NMI ();

        static void InitTests ();
};


extern struct _Z80Regs regs;
extern DWORD g_dwCycleCounter, radjust;
extern int g_nLine, g_nLineCycle, g_nPrevLineCycle;
extern bool g_fBreak, g_fPaused, g_fTurbo;
extern int g_nFastBooting;
extern BYTE *pbMemRead1, *pbMemRead2, *pbMemWrite1, *pbMemWrite2;

const BYTE OP_NOP   = 0x00;     // Z80 opcode for NOP
const BYTE OP_DJNZ  = 0x10;     // Z80 opcode for DJNZ
const BYTE OP_JR    = 0x18;     // Z80 opcode for JR
const BYTE OP_HALT  = 0x76;     // Z80 opcode for HALT
const BYTE OP_JP    = 0xc3;     // Z80 opcode for JP
const BYTE OP_RET   = 0xc9;     // Z80 opcode for RET
const BYTE OP_CALL  = 0xcd;     // Z80 opcode for CALL
const BYTE OP_DI    = 0xf3;     // Z80 opcode for DI
const BYTE OP_EI    = 0xfb;     // Z80 opcode for EI
const BYTE OP_JPHL  = 0xe9;     // Z80 opcode for JP (HL)

const BYTE IX_PREFIX = 0xdd;    // Opcode prefix used for IX instructions
const BYTE IY_PREFIX = 0xfd;    // Opcode prefix used for IY instructions


const WORD IM1_INTERRUPT_HANDLER = 0x0038;      // Interrupt mode 1 handler address
const WORD NMI_INTERRUPT_HANDLER = 0x0066;      // Non-maskable interrupt handler address

// This has been accurately measured on a real SAM using various tests (contact me for further details)
const int INT_ACTIVE_TIME = 128;            // tstates interrupt is active and will be triggered
const int INT_START_TIME = TSTATES_PER_LINE - BORDER_PIXELS + 1;


// Round a tstate value up to a given power of 2 (-1); and so the line total rounds up to the next whole multiple
#define ROUND(t,n)          ((t)|((n)-1))
#define A_ROUND(t,n)        (ROUND(g_nLineCycle+(t),n) - g_nLineCycle)


// Bit values for the F register
const BYTE F_CARRY      = 0x01;
const BYTE F_NADD       = 0x02;     // For BCD (DAA)
const BYTE F_PARITY     = 0x04;
const BYTE F_OVERFLOW   = 0x04;
const BYTE F_HCARRY     = 0x10;     // For BCD (DAA)
const BYTE F_ZERO       = 0x40;
const BYTE F_NEG        = 0x80;


// CPU Event structure
typedef struct _CPU_EVENT
{
    int     nEvent;
    DWORD   dwTime;
    struct  _CPU_EVENT  *psNext;
}
CPU_EVENT;


#ifndef __BIG_ENDIAN__
typedef struct { BYTE l_, h_; } REGBYTE;  // Little endian
#else
typedef struct { BYTE h_, l_; } REGBYTE;  // Big endian
#endif

// NOTE: ENDIAN-SENSITIVE!
typedef struct
{
    union
    {
        WORD    W;
        REGBYTE B;
    };
}
REGPAIR;

typedef struct _Z80Regs
{
    REGPAIR AF, BC, DE, HL;
    REGPAIR AF_, BC_, DE_, HL_;
    REGPAIR IX, IY;
    REGPAIR SP, PC;

    BYTE    I, R;
    BYTE    IFF1, IFF2, IM;
}
Z80Regs;


// CPU Event Queue data
enum    { evtStdIntStart, evtStdIntEnd, evtMidiOutIntStart, evtMidiOutIntEnd, evtEndOfLine, evtInputUpdate };

const int MAX_EVENTS = 16;

extern CPU_EVENT asCpuEvents[MAX_EVENTS], *psNextEvent, *psFreeEvent;


// Add a CPU event into the queue
inline void AddCpuEvent (int nEvent_, DWORD dwTime_)
{
    CPU_EVENT *psNextFree = psFreeEvent->psNext;
    CPU_EVENT **ppsEvent = &psNextEvent;

    // Search through the queue while the events come before the new one
    while (*ppsEvent && ((int)(dwTime_ - (*ppsEvent)->dwTime) > 0))
        ppsEvent = &((*ppsEvent)->psNext);

    // Set this event (note - psFreeEvent will never be NULL)
    psFreeEvent->nEvent = nEvent_;
    psFreeEvent->dwTime = dwTime_;

    // Link the events
    psFreeEvent->psNext = *ppsEvent;
    *ppsEvent = psFreeEvent;
    psFreeEvent = psNextFree;
}

// Update the line/global counters and check for pending events
inline void CheckCpuEvents ()
{
    // Add the instruction time to the global cycle counter
    g_dwCycleCounter += (g_nLineCycle - g_nPrevLineCycle);
    g_nPrevLineCycle = g_nLineCycle;

    // Check for pending CPU events (note - psNextEvent will never be NULL *at this stage*)
    while ((int)(g_dwCycleCounter - psNextEvent->dwTime) >= 0)
    {
        // Get the event from the queue and remove it before new events are added
        CPU_EVENT sThisEvent = *psNextEvent;
        psNextEvent->psNext = psFreeEvent;
        psFreeEvent = psNextEvent;
        psNextEvent = sThisEvent.psNext;
        CPU::ExecuteEvent(sThisEvent);
    }
}

#endif  // Z80_H
