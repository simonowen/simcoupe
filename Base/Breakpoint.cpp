// Part of SimCoupe - A SAM Coupe emulator
//
// Breakpoint.cpp: Debugger breakpoints
//
//  Copyright (c) 2012-2014 Simon Owen
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
#include "Breakpoint.h"

#include "Debug.h"
#include "Memory.h"


static BREAKPT *pBreakpoints;


bool Breakpoint::IsSet ()
{
    return pBreakpoints != nullptr;
}

// Return whether any of the active breakpoints have been hit
bool Breakpoint::IsHit ()
{
    // Fetch the 'physical' address of PC
    void* pPC = AddrReadPtr(PC);

    // Check all active breakpoints
    for (BREAKPT *p = pBreakpoints ; p ; p = p->pNext)
    {
        // Skip the breakpoint if it's disabled
        if (!p->fEnabled)
            continue;

        switch (p->nType)
        {
            // Dummy
            case btNone:
                break;

            // Until expression (handled below)
            case btUntil:
                break;

            // Execution
            case btExecute:
                if (p->Exec.pPhysAddr == pPC)
                    break;

                continue;

            // Memory access
            case btMemory:
                // Read
                if ((p->Mem.nAccess & atRead) &&
                    ((pbMemRead1 >= p->Mem.pPhysAddrFrom && pbMemRead1 <= p->Mem.pPhysAddrTo) ||
                     (pbMemRead2 >= p->Mem.pPhysAddrFrom && pbMemRead2 <= p->Mem.pPhysAddrTo)))
                {
                    pbMemRead1 = pbMemRead2 = nullptr;
                    break;
                }

                // Write
                if ((p->Mem.nAccess & atWrite) &&
                    ((pbMemWrite1 >= p->Mem.pPhysAddrFrom && pbMemWrite1 <= p->Mem.pPhysAddrTo) ||
                     (pbMemWrite2 >= p->Mem.pPhysAddrFrom && pbMemWrite2 <= p->Mem.pPhysAddrTo)))
                {
                    pbMemWrite1 = pbMemWrite2 = nullptr;
                    break;
                }

                continue;

            // Port access
            case btPort:
                // Read
                if ((p->Port.nAccess & atRead) && ((wPortRead & p->Port.wMask) == p->Port.wCompare))
                {
                    wPortRead = 0;
                    break;
                }

                // Write
                if ((p->Port.nAccess & atWrite) && ((wPortWrite & p->Port.wMask) == p->Port.wCompare))
                {
                    wPortWrite = 0;
                    break;
                }

                continue;

            // Interrupt
            case btInt:
                // Masked interrupt active?
                if (~status_reg & p->Int.bMask)
                {
                    // The interrupt handler address depends on the current interrupt mode
                    WORD wIntHandler = (IM == 2) ? read_word((I << 8) | 0xff) : IM1_INTERRUPT_HANDLER;

                    // Start of interrupt handler?
                    if (PC == wIntHandler)
                        break;
                }

                continue;

            // Temporary breakpoint
            case btTemp:
                // Execution match or expression to evaluate?
                if (p->Exec.pPhysAddr == pPC || p->pExpr)
                    break;

                continue;
        }

        // Skip the breakpoint if there's a condition that's false
        if (p->pExpr && !Expr::Eval(p->pExpr))
            continue;

        // Breakpoint hit!
        return Debug::Start(p);
    }

    return false;
}

void Breakpoint::Add (BREAKPT *pBreak_)
{
    if (!pBreakpoints)
        pBreakpoints = pBreak_;
    else if (pBreak_->nType == btTemp)
    {
        // Temporary breakpoints at head of list, for easy removal
        pBreak_->pNext = pBreakpoints;
        pBreakpoints = pBreak_;
    }
    else
    {
        // Normal breakpoints at end of list
        BREAKPT *p = pBreakpoints;
        while (p->pNext) p = p->pNext;
        p->pNext = pBreak_;
    }

    // Break from the main execution loop to activate breakpoint testing
    g_fBreak = true;
}

bool Breakpoint::IsExecAddr (WORD wAddr_)
{
    BYTE* pPhys = AddrReadPtr(wAddr_);

    for (BREAKPT* p = pBreakpoints ; p ; p = p->pNext)
        if (p->nType == btExecute && p->Exec.pPhysAddr == pPhys)
            return true;

    return false;
}

int Breakpoint::GetExecIndex (void *pPhysAddr_)
{
    int nIndex = 0;

    for (BREAKPT* p = pBreakpoints ; p ; p = p->pNext, nIndex++)
        if (p->nType == btExecute && p->Exec.pPhysAddr == pPhysAddr_)
            return nIndex;

    return -1;
}

void Breakpoint::AddTemp (void *pPhysAddr_, EXPR *pExpr_)
{
    // Add a new temporary breakpoint for the supplied address and/or expression
    BREAKPT *pNew = new BREAKPT(btTemp, pExpr_);
    pNew->Temp.pPhysAddr = pPhysAddr_;
    Add(pNew);
}

void Breakpoint::AddUntil (EXPR *pExpr_)
{
    // Add Until breakpoint for the supplied expression
    Add(new BREAKPT(btUntil, pExpr_));
}

void Breakpoint::AddExec (void *pPhysAddr_, EXPR *pExpr_)
{
    // Add a new execution breakpoint for the supplied address
    BREAKPT *pNew = new BREAKPT(btExecute, pExpr_);
    pNew->Exec.pPhysAddr = pPhysAddr_;
    Add(pNew);
}

void Breakpoint::AddMemory (void *pPhysAddr_, AccessType nAccess_, EXPR *pExpr_, int nLength_/*=1*/)
{
    // Add a new memory breakpoint for the supplied address and access type
    BREAKPT *pNew = new BREAKPT(btMemory, pExpr_);

    pNew->Mem.pPhysAddrFrom = pPhysAddr_;
    pNew->Mem.pPhysAddrTo = reinterpret_cast<BYTE*>(pPhysAddr_)+nLength_-1;
    pNew->Mem.nAccess = nAccess_;

    Add(pNew);
}

void Breakpoint::AddPort (WORD wPort_, AccessType nAccess_, EXPR *pExpr_)
{
    // Add a new I/O breakpoint for the supplied port and access type
    BREAKPT *pNew = new BREAKPT(btPort, pExpr_);

    pNew->Port.wCompare = wPort_;
    pNew->Port.wMask = (wPort_ <= 0xff) ? 0xff : 0xffff;
    pNew->Port.nAccess = nAccess_;

    Add(pNew);
}

void Breakpoint::AddInterrupt (BYTE bIntMask_, EXPR *pExpr_)
{
    // Search for an existing interrupt breakpoint to update
    // If we have an expression or the existing has, the pointer comparison is expected to fail
    BREAKPT *p = pBreakpoints;
    for ( ; p && (p->nType != btInt || p->pExpr != pExpr_) ; p = p->pNext);

    // If we found one, merge in the new bits
    if (p)
    {
        p->Int.bMask |= bIntMask_;
        return;
    }

    // Add a new interrupt breakpoint for the supplied lines
    BREAKPT *pNew = new BREAKPT(btInt, pExpr_);
    pNew->Int.bMask = bIntMask_;
    Add(pNew);
}

const char *Breakpoint::GetDesc (BREAKPT *pBreak_)
{
    static char sz[512];
    char *psz = sz;
    const void *pPhysAddr = nullptr;
    UINT uExtent = 0;

    switch (pBreak_->nType)
    {
        case btTemp:
            psz += sprintf(psz, "TEMP");
            break;

        case btUntil:
        {
            psz += sprintf(psz, "UNTIL %s", pBreak_->pExpr->pcszExpr);
            break;
        }

        case btExecute:
        {
            pPhysAddr = pBreak_->Exec.pPhysAddr;
            const char *pcszPageDesc = Memory::PageDesc(PtrPage(pPhysAddr), true);
            int nPageOffset = PtrOffset(pPhysAddr);
            psz += sprintf(psz, "EXEC %s:%04X", pcszPageDesc, nPageOffset);
            break;
        }

        case btMemory:
        {
            pPhysAddr = pBreak_->Mem.pPhysAddrFrom;
            const char *pcszPageDesc = Memory::PageDesc(PtrPage(pPhysAddr), true);
            int nPageOffset = PtrOffset(pPhysAddr);
            psz += sprintf(psz, "MEM %s:%04X", pcszPageDesc, nPageOffset);

            if (pBreak_->Mem.pPhysAddrTo != pPhysAddr)
            {
                uExtent = (UINT)((BYTE*)pBreak_->Mem.pPhysAddrTo-(BYTE*)pPhysAddr);
                psz += sprintf(psz, " %X", uExtent+1);
            }

            switch (pBreak_->Mem.nAccess)
            {
                case atNone: break;
                case atRead: psz += sprintf(psz, " R"); break;
                case atWrite: psz += sprintf(psz, " W"); break;
                case atReadWrite: psz += sprintf(psz, " RW"); break;
            }
            break;
        }

        case btPort:
        {
            if (pBreak_->Port.wCompare <= 0xff)
                psz += sprintf(psz, "PORT %02X", pBreak_->Port.wCompare);
            else
                psz += sprintf(psz, "PORT %04X", pBreak_->Port.wCompare);

            switch (pBreak_->Port.nAccess)
            {
                case atNone: break;
                case atRead: psz += sprintf(psz, " R"); break;
                case atWrite: psz += sprintf(psz, " W"); break;
                case atReadWrite: psz += sprintf(psz, " RW"); break;
            }
            break;
        }

        case btInt:
        {
            psz += sprintf(psz, "INT");

            if (pBreak_->Int.bMask & STATUS_INT_FRAME) psz += sprintf(psz, ",FRAME");
            if (pBreak_->Int.bMask & STATUS_INT_LINE) psz += sprintf(psz, ",LINE");
            if (pBreak_->Int.bMask & STATUS_INT_MIDIOUT) psz += sprintf(psz, ",MIDIOUT");
            if (pBreak_->Int.bMask & STATUS_INT_MIDIIN) psz += sprintf(psz, ",MIDIIN");
            if (sz[3] == ',') sz[3] = ' ';

            break;
        }

        default:
            psz += sprintf(psz, "???");
            break;
    }

    if (pPhysAddr)
    {
        int nAddr1 = -1, nAddr2 = -1;

        int nPage = PtrPage(pPhysAddr);
        int nOffset = PtrOffset(pPhysAddr);

        if (nPage == AddrPage(0x0000)) { nAddr2 = nAddr1; nAddr1 = 0x0000+nOffset; }
        if (nPage == AddrPage(0x4000)) { nAddr2 = nAddr1; nAddr1 = 0x4000+nOffset; }
        if (nPage == AddrPage(0x8000)) { nAddr2 = nAddr1; nAddr1 = 0x8000+nOffset; }
        if (nPage == AddrPage(0xc000)) { nAddr2 = nAddr1; nAddr1 = 0xc000+nOffset; }

        if (nAddr2 != -1)
        {
            if (uExtent)
                psz += sprintf(psz, " (%04X-%04X,%04X-%04X)", nAddr2, nAddr2+uExtent, nAddr1, nAddr1+uExtent);
            else
                psz += sprintf(psz, " (%04X,%04X)", nAddr2, nAddr1);
        }
        else if (nAddr1 != -1)
        {
            if (uExtent)
                psz += sprintf(psz, " (%04X-%04X)", nAddr1, nAddr1+uExtent);
            else
                psz += sprintf(psz, " (%04X)", nAddr1);
        }
    }

    if (pBreak_->pExpr && pBreak_->nType != btUntil)
        psz += sprintf(psz, " if %s", pBreak_->pExpr->pcszExpr);

    return sz;
}

int Breakpoint::GetIndex (BREAKPT *pBreak_)
{
    BREAKPT *p = pBreakpoints;
    int nIndex = 0;

    for ( ; p && p != pBreak_ ; p = p->pNext, nIndex++);

    return p ? nIndex : -1;
}

BREAKPT *Breakpoint::GetAt (int nIndex_)
{
    BREAKPT *p = pBreakpoints;
    for ( ; p && nIndex_-- > 0 ; p = p->pNext);
    return p;
}

bool Breakpoint::RemoveAt (int nIndex_)
{
    BREAKPT *p = pBreakpoints;
    BREAKPT *pPrev = nullptr;

    for ( ; p && nIndex_-- > 0 ; pPrev = p, p = p->pNext);

    if (p)
    {
        // Unlink it from the chain
        if (pPrev)
            pPrev->pNext = p->pNext;
        else
            pBreakpoints = p->pNext;

        delete p;
        return true;
    }

    return false;
}

void Breakpoint::RemoveAll ()
{
    while (pBreakpoints)
    {
        BREAKPT *p = pBreakpoints;
        pBreakpoints = pBreakpoints->pNext;
        delete p;
    }
}
