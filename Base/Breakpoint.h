// Part of SimCoupe - A SAM Coupe emulator
//
// Breakpoint.h: Debugger breakpoints
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

#ifndef BREAKPOINT_H
#define BREAKPOINT_H

#include "Expr.h"

enum BreakpointType { btNone, btTemp, btExecute, btMemory, btPort, btInt };
enum AccessType { atNone, atRead, atWrite, atReadWrite };


typedef struct tagBREAKTEMP
{
    const void *pPhysAddr;
}
BREAKTEMP;

typedef struct tagBREAKEXEC
{
    const void *pPhysAddr;
}
BREAKEXEC;

typedef struct tagBREAKMEM
{
    const void *pPhysAddrFrom;
    const void *pPhysAddrTo;
}
BREAKMEM;

typedef struct tagBREAKPORT
{
    WORD wMask;
    WORD wCompare;
}
BREAKPORT;

typedef struct tagBREAKINT
{
    BYTE bMask;
}
BREAKINT;


typedef struct tagBREAKPT
{
    BreakpointType nType;
    AccessType nAccess;
    bool fEnabled;

    union
    {
        BREAKEXEC Temp;
        BREAKEXEC Exec;
        BREAKMEM Mem;
        BREAKPORT Port;
        BREAKINT Int;
    };

    EXPR* pExpr;
    struct tagBREAKPT* pNext;
}
BREAKPT;


class Breakpoint
{
    public:
        static bool IsSet ();
        static bool IsHit ();
        static void Add (BREAKPT *pBreak_);
        static void AddTemp (void *pPhysAddr_, EXPR *pExpr_);
        static void AddExec (void *pPhysAddr_, EXPR *pExpr_);
        static void AddMemory (void *pPhysAddr_, AccessType nAccess_, EXPR *pExpr_, int nLength_=1);
        static void AddPort (WORD wPort_, AccessType nAccess_, EXPR *pExpr_);
        static void AddInterrupt (BYTE bIntMask_, EXPR *pExpr_);
        static const char *GetDesc (BREAKPT *pBreak_);
        static BREAKPT *GetAt (int nIndex_);
        static bool IsExecAddr (WORD wAddr_);
        static int GetIndex (BREAKPT *pBreak_);
        static int GetExecIndex (void *pPhysAddr_);
        static bool RemoveAt (int nIndex_);
        static void RemoveAll ();
};

#endif // BREAKPOINT_H
