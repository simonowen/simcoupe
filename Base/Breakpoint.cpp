// Part of SimCoupe - A SAM Coupe emulator
//
// Breakpoint.cpp: Debugger breakpoints
//
//  Copyright (c) 2012 Simon Owen
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

#include "SimCoupe.h"
#include "Breakpoint.h"

#include "Debug.h"
#include "Memory.h"


static BREAKPT *pBreakpoints;


bool Breakpoint::IsSet ()
{
	return pBreakpoints != NULL;
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

            // Execution
            case btExecute:
                if (p->Exec.pPhysAddr == pPC)
                    break;

                continue;

            // Memory access
            case btMemory:
                // Read
                if ((p->nAccess & atRead) &&
                    ((pbMemRead1 >= p->Mem.pPhysAddrFrom && pbMemRead1 <= p->Mem.pPhysAddrTo) ||
                     (pbMemRead2 >= p->Mem.pPhysAddrFrom && pbMemRead2 <= p->Mem.pPhysAddrTo)))
                   break;

                // Write
                if ((p->nAccess & atWrite) &&
                    ((pbMemWrite1 >= p->Mem.pPhysAddrFrom && pbMemWrite1 <= p->Mem.pPhysAddrTo) ||
                     (pbMemWrite1 >= p->Mem.pPhysAddrFrom && pbMemWrite2 <= p->Mem.pPhysAddrTo)))
                   break;

                continue;

            // Port access
            case btPort:
                // Read
                if ((p->nAccess & atRead) && ((wPortRead & p->Port.wMask) == p->Port.wCompare))
                    break;

                // Write
                if ((p->nAccess & atWrite) && ((wPortWrite & p->Port.wMask) == p->Port.wCompare))
                    break;

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
    else
    {
        BREAKPT *p = pBreakpoints;
        while (p->pNext) p = p->pNext;
        p->pNext = pBreak_;
    }
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
    // Add a new execution breakpoint for the supplied address
    BREAKPT *pNew = new BREAKPT;
    pNew->nType = btTemp;
    pNew->nAccess = atNone;
    pNew->fEnabled = true;
    pNew->pExpr = pExpr_;
    pNew->pNext = pBreakpoints;

    pNew->Temp.pPhysAddr = pPhysAddr_;

    // Add to head of list for easy removal
    pBreakpoints = pNew;
}

void Breakpoint::AddExec (void *pPhysAddr_, EXPR *pExpr_)
{
    // Add a new execution breakpoint for the supplied address
    BREAKPT *pNew = new BREAKPT;
    pNew->nType = btExecute;
    pNew->nAccess = atNone;
    pNew->fEnabled = true;
    pNew->pExpr = pExpr_;
    pNew->pNext = NULL;

    pNew->Exec.pPhysAddr = pPhysAddr_;

    Add(pNew);
}

void Breakpoint::AddMemory (WORD wAddr_, AccessType nAccess_, EXPR *pExpr_, int nLength_/*=1*/)
{
    BYTE* pPhys = AddrReadPtr(wAddr_);

    // Add a new execution breakpoint for the supplied address
    BREAKPT *pNew = new BREAKPT;
    pNew->nType = btMemory;
    pNew->nAccess = nAccess_;
    pNew->fEnabled = true;
    pNew->pExpr = pExpr_;
    pNew->pNext = NULL;

    pNew->Mem.pPhysAddrFrom = pPhys;
    pNew->Mem.pPhysAddrTo = pPhys+nLength_-1;

    Add(pNew);
}

void Breakpoint::AddPort (WORD wPort_, AccessType nAccess_, EXPR *pExpr_)
{
    // Add a new execution breakpoint for the supplied address
    BREAKPT *pNew = new BREAKPT;
    pNew->nType = btPort;
    pNew->nAccess = nAccess_;
    pNew->fEnabled = true;
    pNew->pExpr = pExpr_;
    pNew->pNext = NULL;

    pNew->Port.wCompare = wPort_;
    pNew->Port.wMask = (wPort_ <= 0xff) ? 0xff : 0xffff;

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

    // Add a new execution breakpoint for the supplied address
    BREAKPT *pNew = new BREAKPT;
    pNew->nType = btInt;
    pNew->nAccess = atNone;
    pNew->fEnabled = true;
    pNew->Int.bMask = bIntMask_;
    pNew->pExpr = pExpr_;
    pNew->pNext = NULL;

    Add(pNew);
}

const char *Breakpoint::GetDesc (BREAKPT *pBreak_)
{
    static char sz[512];
    char *psz = sz;
    const void *pPhysAddr = NULL;
    size_t nExtent = 0;

    switch (pBreak_->nType)
    {
        case btTemp:
            psz += sprintf(psz, "TEMP");
            break;

        case btExecute:
        {
			pPhysAddr = pBreak_->Exec.pPhysAddr;
            const char *pcszPageDesc = PageDesc(PtrPage(pPhysAddr), true);
            int nPageOffset = PtrOffset(pPhysAddr);
            psz += sprintf(psz, "EXEC %s:%04X", pcszPageDesc, nPageOffset);
            break;
        }

        case btMemory:
        {
			pPhysAddr = pBreak_->Mem.pPhysAddrFrom;
            const char *pcszPageDesc = PageDesc(PtrPage(pPhysAddr), true);
            int nPageOffset = PtrOffset(pPhysAddr);
            psz += sprintf(psz, "MEM %s:%04X", pcszPageDesc, nPageOffset);

			if (pBreak_->Mem.pPhysAddrTo != pPhysAddr)
			{
				nExtent = (BYTE*)pBreak_->Mem.pPhysAddrTo-(BYTE*)pPhysAddr;
				psz += sprintf(psz, " %lX", nExtent+1);
			}

            switch (pBreak_->nAccess)
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

            switch (pBreak_->nAccess)
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

        if (nPage == AddrPage(0x0000)) nAddr2 = nAddr1, nAddr1 = 0x0000+nOffset;
        if (nPage == AddrPage(0x4000)) nAddr2 = nAddr1, nAddr1 = 0x4000+nOffset;
        if (nPage == AddrPage(0x8000)) nAddr2 = nAddr1, nAddr1 = 0x8000+nOffset;
        if (nPage == AddrPage(0xc000)) nAddr2 = nAddr1, nAddr1 = 0xc000+nOffset;

        if (nAddr2 != -1)
        {
			if (nExtent)
				psz += sprintf(psz, " (%04X-%04lX,%04X-%04lX)", nAddr2, nAddr2+nExtent, nAddr1, nAddr1+nExtent);
			else
				psz += sprintf(psz, " (%04X,%04X)", nAddr2, nAddr1);
        }
        else if (nAddr1 != -1)
        {
			if (nExtent)
				psz += sprintf(psz, " (%04X-%04lX)", nAddr1, nAddr1+nExtent);
			else
				psz += sprintf(psz, " (%04X)", nAddr1);
        }
    }

    if (pBreak_->pExpr)
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
    BREAKPT *pPrev = NULL;

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
