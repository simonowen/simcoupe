// Part of SimCoupe - A SAM Coupe emulator
//
// CPU.h: Z80 processor emulation and main emulation loop
//
//  Copyright (c) 2000-2003 Dave Laundon
//  Copyright (c) 1999-2014 Simon Owen
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

#pragma once

#include "SAM.h"
#include "SAMIO.h"
#include "Util.h"

// NOTE: ENDIAN-SENSITIVE!
struct REGPAIR
{
    union
    {
        uint16_t    w;
#ifdef __BIG_ENDIAN__
        struct { uint8_t h, l; } b;  // Big endian
#else
        struct { uint8_t l, h; } b;  // Little endian
#endif
    };
};

struct Z80Regs
{
    REGPAIR af, bc, de, hl;
    REGPAIR af_, bc_, de_, hl_;
    REGPAIR ix, iy;
    REGPAIR sp, pc;

    uint8_t    i, r, r7;
    uint8_t    iff1, iff2, im;
};

enum class EventType
{
    None,
    FrameInterrupt, FrameInterruptEnd,
    LineInterrupt, LineInterruptEnd,
    MidiOutStart, MidiOutEnd, MidiTxfmstEnd,
    MouseReset, BlueAlphaClock, TapeEdge,
    AsicReady, InputUpdate
};

struct CPU_EVENT
{
    EventType type{ EventType::None };
    uint32_t due_time = 0;
    CPU_EVENT* pNext = nullptr;
};

namespace CPU
{
bool Init(bool fFirstInit_ = false);
void Exit(bool fReInit_ = false);

void Run();
bool IsContentionActive();
void UpdateContention(bool fActive_ = true);
void ExecuteEvent(const CPU_EVENT& sThisEvent);
void ExecuteChunk();

void Reset(bool fPress_);
void NMI();

void InitTests();
}


extern Z80Regs regs;
extern uint32_t g_dwCycleCounter;
extern bool g_fReset, g_fBreak, g_fPaused;
extern int g_nTurbo;
extern uint8_t* pbMemRead1, * pbMemRead2, * pbMemWrite1, * pbMemWrite2;

enum { TURBO_BOOT = 0x01, TURBO_KEY = 0x02, TURBO_DISK = 0x04, TURBO_TAPE = 0x08, TURBO_KEYIN = 0x10 };

#ifdef _DEBUG
extern bool g_fDebug;
#endif

const uint8_t OP_NOP = 0x00;     // Z80 opcode for NOP
const uint8_t OP_DJNZ = 0x10;     // Z80 opcode for DJNZ
const uint8_t OP_JR = 0x18;     // Z80 opcode for JR
const uint8_t OP_HALT = 0x76;     // Z80 opcode for HALT
const uint8_t OP_JP = 0xc3;     // Z80 opcode for JP
const uint8_t OP_RET = 0xc9;     // Z80 opcode for RET
const uint8_t OP_CALL = 0xcd;     // Z80 opcode for CALL
const uint8_t OP_DI = 0xf3;     // Z80 opcode for DI
const uint8_t OP_EI = 0xfb;     // Z80 opcode for EI
const uint8_t OP_JPHL = 0xe9;     // Z80 opcode for JP (HL)

const uint8_t IX_PREFIX = 0xdd;    // Opcode prefix used for IX instructions
const uint8_t IY_PREFIX = 0xfd;    // Opcode prefix used for IY instructions
const uint8_t CB_PREFIX = 0xcb;    // Prefix for CB instruction set
const uint8_t ED_PREFIX = 0xed;    // Prefix for ED instruction set


const uint16_t IM1_INTERRUPT_HANDLER = 0x0038;      // Interrupt mode 1 handler address
const uint16_t NMI_INTERRUPT_HANDLER = 0x0066;      // Non-maskable interrupt handler address

// This has been accurately measured on a real SAM using various tests (contact me for further details)
const int INT_ACTIVE_TIME = 128;            // tstates interrupt is active and will be triggered


// Round a tstate value up to a given power of 2 (-1); and so the line total rounds up to the next whole multiple
#define ROUND(t,n)          ((t)|((n)-1))
#define A_ROUND(t,n)        (ROUND(g_dwCycleCounter+(t),n) - g_dwCycleCounter)

// Bit values for the F register
#define FLAG_C  0x01
#define FLAG_N  0x02
#define FLAG_P  0x04
#define FLAG_V  FLAG_P
#define FLAG_3  0x08
#define FLAG_H  0x10
#define FLAG_5  0x20
#define FLAG_Z  0x40
#define FLAG_S  0x80

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

#define AF_     regs.af_.w
#define BC_     regs.bc_.w
#define DE_     regs.de_.w
#define HL_     regs.hl_.w

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
#define PCH     regs.pc.b.h
#define PCL     regs.pc.b.l

#define R       regs.r
#define R7      regs.r7
#define I       regs.i
#define IFF1    regs.iff1
#define IFF2    regs.iff2
#define IM      regs.im
#define IR      ((I << 8) | (R7 & 0x80) | (R & 0x7f))


const int MAX_EVENTS = 16;

extern CPU_EVENT asCpuEvents[MAX_EVENTS], * psNextEvent, * psFreeEvent;


// Initialise the CPU events queue
inline void InitCpuEvents()
{
    for (int n = 0; n < MAX_EVENTS; n++)
        asCpuEvents[n].pNext = &asCpuEvents[(n + 1) % MAX_EVENTS];

    psFreeEvent = asCpuEvents;
    psNextEvent = nullptr;
}

// Add a CPU event into the queue
inline void AddCpuEvent(EventType nEvent_, uint32_t dwTime_)
{
    CPU_EVENT* psNextFree = psFreeEvent->pNext;
    CPU_EVENT** ppsEvent = &psNextEvent;

    // Search through the queue while the events come before the new one
    // New events with equal time are inserted after existing entries
    while (*ppsEvent && (*ppsEvent)->due_time <= dwTime_)
        ppsEvent = &((*ppsEvent)->pNext);

    // Set this event (note - psFreeEvent will never be nullptr)
    psFreeEvent->type = nEvent_;
    psFreeEvent->due_time = dwTime_;

    // Link the events
    psFreeEvent->pNext = *ppsEvent;
    *ppsEvent = psFreeEvent;
    psFreeEvent = psNextFree;
}

// Remove events of a specific type from the queue
inline void CancelCpuEvent(EventType nEvent_)
{
    CPU_EVENT** ppsEvent = &psNextEvent;

    while (*ppsEvent)
    {
        if ((*ppsEvent)->type != nEvent_)
            ppsEvent = &((*ppsEvent)->pNext);
        else
        {
            CPU_EVENT* psNext = (*ppsEvent)->pNext;
            (*ppsEvent)->pNext = psFreeEvent;
            psFreeEvent = *ppsEvent;
            *ppsEvent = psNext;
        }
    }
}

// Return time until the next event of a specific type
inline uint32_t GetEventTime(EventType nEvent_)
{
    CPU_EVENT* psEvent;

    for (psEvent = psNextEvent; psEvent; psEvent = psEvent->pNext)
    {
        if (psEvent->type == nEvent_)
            return psEvent->due_time - g_dwCycleCounter;
    }

    return 0;
}

// Update the line/global counters and check for pending events
inline void CheckCpuEvents()
{
    // Check for pending CPU events (note - psNextEvent will never be nullptr *at this stage*)
    while (g_dwCycleCounter >= psNextEvent->due_time)
    {
        // Get the event from the queue and remove it before new events are added
        auto sThisEvent = *psNextEvent;
        psNextEvent->pNext = psFreeEvent;
        psFreeEvent = psNextEvent;
        psNextEvent = sThisEvent.pNext;
        CPU::ExecuteEvent(sThisEvent);
    }
}

// Subtract a frame's worth of time from all events
inline void CpuEventFrame(uint32_t dwFrameTime_)
{
    // Process all queued events, due sometime in the next or a later frame
    for (CPU_EVENT* psEvent = psNextEvent; psEvent; psEvent = psEvent->pNext)
        psEvent->due_time -= dwFrameTime_;
}
