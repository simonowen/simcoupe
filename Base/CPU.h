// Part of SimCoupe - A SAM Coupé emulator
//
// CPU.h: Z80 processor emulation and main emulation loop
//
//  Copyright (c) 1996-2001  Allan Skillman
//  Copyright (c) 2000-2001  Dave Laundon
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

#ifndef Z80_H
#define Z80_H

#include "SAM.h"
#include "IO.h"
#include "Util.h"


// CPU Event structure
typedef struct _CPU_EVENT
{
    int     nEvent;
    DWORD   dwTime;
    struct  _CPU_EVENT  *psNext;
}
CPU_EVENT;


namespace CPU
{
    bool Init (bool fPowerOnInit_=true);
    void Exit (bool fReInit_=false);

    void Run ();
    void UpdateContention ();
    void ExecuteEvent (CPU_EVENT sThisEvent);

    void NMI ();
};

extern DWORD g_dwCycleCounter;
extern int g_nLine, g_nLineCycle, g_nPrevLineCycle;
extern bool g_fDebugging;
extern bool g_fFlashPhase;


const BYTE OP_NOP   = 0x00;     // Z80 opcode for NOP
const BYTE OP_HALT  = 0x76;     // Z80 opcode for HALT
const BYTE OP_EI    = 0xfb;     // Z80 opcode for EI
const BYTE OP_DI    = 0xf3;     // Z80 opcode for DI


const WORD IM1_INTERRUPT_HANDLER = 0x0038;      // Interrupt mode 1 handler address
const WORD NMI_INTERRUPT_HANDLER = 0x0066;      // Non-maskable interrupt handler address

// This has been accurately measured on a real SAM using various tests (contact me for further details)
const int INT_ACTIVE_TIME = 128;            // tstates interrupt is active and will be triggered
const int INT_START_TIME = TSTATES_PER_LINE - BORDER_PIXELS + 1;


// Round a tstate value up to a given power of 2 (-1); and so the line total rounds up to the next whole multiple
#define ROUND(t,n)          ((t)|((n)-1))
#define A_ROUND(t,n)        (ROUND(g_nLineCycle+(t),n) - g_nLineCycle)

// Z80 pseudo states to keep track of what we're doing
enum { Z80_none, Z80_nmi, Z80_reset, Z80_pause };

// Bit values for the F register
const BYTE F_CARRY      = 0x01;
const BYTE F_NADD       = 0x02;     // For BCD (DAA)
const BYTE F_PARITY     = 0x04;
const BYTE F_OVERFLOW   = 0x04;
const BYTE F_HCARRY     = 0x10;     // For BCD (DAA)
const BYTE F_ZERO       = 0x40;
const BYTE F_NEG        = 0x80;


// NOTE: ENDIAN-SENSITIVE!
typedef struct
{
    union
    {
        WORD    W;

#ifndef __BIG_ENDIAN__
        struct { BYTE l_, h_; } B;  // Little endian
#else
        struct { BYTE h_, l_; } B;  // Big endian
#endif
    };
}
REGPAIR;

typedef struct
{
    REGPAIR AF, BC, DE, HL;
    REGPAIR AF_, BC_, DE_, HL_;
    REGPAIR IX, IY;
    REGPAIR SP, PC;

    BYTE    I, R;
    BYTE    IFF1, IFF2, IM;
}
Z80Regs;

extern Z80Regs regs;


// CPU Event Queue data
enum    { evtStdIntStart, evtStdIntEnd, evtMidiOutIntStart, evtMidiOutIntEnd, evtEndOfLine };

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
