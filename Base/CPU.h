// Part of SimCoupe - A SAM Coupe emulator
//
// CPU.h: Z80 processor emulation and main emulation loop
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
extern DWORD g_dwCycleCounter;
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


// Round a tstate value up to a given power of 2 (-1); and so the line total rounds up to the next whole multiple
#define ROUND(t,n)          ((t)|((n)-1))
#define A_ROUND(t,n)        (ROUND(g_dwCycleCounter+(t),n) - g_dwCycleCounter)

// Bit values for the F register
#define FLAG_C	0x01
#define FLAG_N	0x02
#define FLAG_P	0x04
#define FLAG_V	FLAG_P
#define FLAG_3	0x08
#define FLAG_H	0x10
#define FLAG_5	0x20
#define FLAG_Z	0x40
#define FLAG_S	0x80


// CPU Event structure
typedef struct _CPU_EVENT
{
    int     nEvent;
    DWORD   dwTime;
    struct  _CPU_EVENT  *psNext;
}
CPU_EVENT;


// NOTE: ENDIAN-SENSITIVE!
typedef struct
{
    union
    {
        WORD    w;
#ifdef __BIG_ENDIAN__
	struct { BYTE h, l; } b;  // Big endian
#else
	struct { BYTE l, h; } b;  // Little endian
#endif
    };
}
REGPAIR;

typedef struct _Z80Regs
{
    REGPAIR af, bc, de, hl;
    REGPAIR af_, bc_, de_, hl_;
    REGPAIR ix, iy;
    REGPAIR sp, pc;

    BYTE    i, r, r7;
    BYTE    iff1, iff2, im;
    BYTE    halted;
}
Z80Regs;

#define A       regs.af.b.h
#define F       regs.af.b.l
#define B       regs.bc.b.h
#define C       regs.bc.b.l
#define D       regs.de.b.h
#define E       regs.de.b.l
#define H       regs.hl.b.h
#define L       regs.hl.b.l

#define AF      regs.af.w
#define BC      regs.bc.w
#define DE      regs.de.w
#define HL      regs.hl.w

#define A_      regs.af_.b.h
#define F_      regs.af_.b.l
#define B_      regs.bc_.b.h
#define C_      regs.bc_.b.l
#define D_      regs.de_.b.h
#define E_      regs.de_.b.l
#define H_      regs.hl_.b.h
#define L_      regs.hl_.b.l

#define AF_		regs.af_.w
#define BC_		regs.bc_.w
#define DE_		regs.de_.w
#define HL_		regs.hl_.w

#define IX      regs.ix.w
#define IY      regs.iy.w
#define SP      regs.sp.w
#define PC      regs.pc.w

#define IXH     regs.ix.b.h
#define IXL     regs.ix.b.l
#define IYH     regs.iy.b.h
#define IYL     regs.iy.b.l
#define SPH     regs.sp.b.h
#define SPL     regs.sp.b.l

#define R       regs.r
#define R7      regs.r7
#define I       regs.i
#define IFF1    regs.iff1
#define IFF2    regs.iff2
#define IM      regs.im
#define IR		((I << 8) | (R7 & 0x80) | (R & 0x7f))


// CPU Event Queue data
enum    { evtStdIntEnd, evtLineIntStart, evtEndOfFrame, evtMidiOutIntStart, evtMidiOutIntEnd, evtEndOfLine, evtInputUpdate, evtMouseReset };

const int MAX_EVENTS = 16;

extern CPU_EVENT asCpuEvents[MAX_EVENTS], *psNextEvent, *psFreeEvent;


// Add a CPU event into the queue
inline void AddCpuEvent (int nEvent_, DWORD dwTime_)
{
    CPU_EVENT *psNextFree = psFreeEvent->psNext;
    CPU_EVENT **ppsEvent = &psNextEvent;

    // Search through the queue while the events come before the new one
    // New events with equal time are inserted after existing entries
    while (*ppsEvent && (*ppsEvent)->dwTime <= dwTime_)
        ppsEvent = &((*ppsEvent)->psNext);

    // Set this event (note - psFreeEvent will never be NULL)
    psFreeEvent->nEvent = nEvent_;
    psFreeEvent->dwTime = dwTime_;

    // Link the events
    psFreeEvent->psNext = *ppsEvent;
    *ppsEvent = psFreeEvent;
    psFreeEvent = psNextFree;
}

// Remove events of a specific type from the queue
inline void CancelCpuEvent (int nEvent_)
{
    CPU_EVENT **ppsEvent = &psNextEvent;

    while (*ppsEvent)
    {
        if ((*ppsEvent)->nEvent != nEvent_)
            ppsEvent = &((*ppsEvent)->psNext);
        else
        {
            CPU_EVENT *psNext = (*ppsEvent)->psNext;
            (*ppsEvent)->psNext = psFreeEvent;
            psFreeEvent = *ppsEvent;
            *ppsEvent = psNext;
        }
    }
}

// Update the line/global counters and check for pending events
inline void CheckCpuEvents ()
{
    // Check for pending CPU events (note - psNextEvent will never be NULL *at this stage*)
    while (g_dwCycleCounter >= psNextEvent->dwTime)
    {
        // Get the event from the queue and remove it before new events are added
        CPU_EVENT sThisEvent = *psNextEvent;
        psNextEvent->psNext = psFreeEvent;
        psFreeEvent = psNextEvent;
        psNextEvent = sThisEvent.psNext;
        CPU::ExecuteEvent(sThisEvent);
    }
}

// Subtract a frame's worth of time from all events
inline void CpuEventFrame (DWORD dwFrameTime_)
{
    // Process all queued events, due sometime in the next or a later frame
    for (CPU_EVENT *psEvent = psNextEvent ; psEvent ; psEvent = psEvent->psNext)
        psEvent->dwTime -= dwFrameTime_;
}

#endif  // Z80_H
