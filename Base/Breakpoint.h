// Part of SimCoupe - A SAM Coupe emulator
//
// Breakpoint.h: Debugger breakpoints
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

#pragma once

#include "Expr.h"

enum BreakpointType { btNone, btTemp, btUntil, btExecute, btMemory, btPort, btInt };
enum AccessType { atNone, atRead, atWrite, atReadWrite };


struct BREAKTEMP
{
    const void* pPhysAddr;
};

struct BREAKEXEC
{
    const void* pPhysAddr;
};

struct BREAKMEM
{
    const void* pPhysAddrFrom;
    const void* pPhysAddrTo;
    AccessType nAccess;
};

struct BREAKPORT
{
    uint16_t wMask;
    uint16_t wCompare;
    AccessType nAccess;
};

struct BREAKINT
{
    uint8_t bMask;
};


struct BREAKPT
{
    BREAKPT(BreakpointType nType_, const Expr& expr)
        : nType(nType_), expr(expr) { }

    BreakpointType nType;
    Expr expr;
    bool fEnabled = true;

    union
    {
        BREAKEXEC Temp;
        BREAKEXEC Exec;
        BREAKMEM Mem;
        BREAKPORT Port;
        BREAKINT Int;
    };

    BREAKPT* pNext = nullptr;
};


class Breakpoint
{
public:
    static bool IsSet();
    static bool IsHit();
    static void Add(BREAKPT* pBreak_);
    static void AddTemp(void* pPhysAddr_, const Expr& expr);
    static void AddUntil(const Expr& expr);
    static void AddExec(void* pPhysAddr_, const Expr& expr);
    static void AddMemory(void* pPhysAddr_, AccessType nAccess_, const Expr& expr, int nLength_ = 1);
    static void AddPort(uint16_t wPort_, AccessType nAccess_, const Expr& expr);
    static void AddInterrupt(uint8_t bIntMask_, const Expr& expr);
    static const char* GetDesc(BREAKPT* pBreak_);
    static BREAKPT* GetAt(int nIndex_);
    static bool IsExecAddr(uint16_t wAddr_);
    static int GetIndex(BREAKPT* pBreak_);
    static int GetExecIndex(void* pPhysAddr_);
    static bool RemoveAt(int nIndex_);
    static void RemoveAll();
};
