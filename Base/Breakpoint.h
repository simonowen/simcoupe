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

enum class BreakType { Temp, Until, Execute, Memory, Port, Interrupt };
enum class AccessType { ReadWrite, Read, Write };


struct BreakExec
{
    const void* phys_addr{};
};

struct BreakMem
{
    const void* phys_addr_from{};
    const void* phys_addr_to{};
    AccessType access{};
};

struct BreakPort
{
    uint16_t mask{};
    uint16_t compare{};
    AccessType access{};
};

struct BreakInt
{
    uint8_t mask{};
};

struct Breakpoint
{
    Breakpoint(BreakType type, const Expr& expr) :
        enabled(true), type(type), expr(expr) { }

    bool enabled;
    BreakType type;
    Expr expr;
    std::variant<BreakExec, BreakMem, BreakPort, BreakInt> data;

    static std::vector<Breakpoint> breakpoints;

    static std::optional<int> Hit();
    static void Add(Breakpoint&& bp);
    static void AddTemp(void* pPhysAddr_, const Expr& expr = {});
    static void AddUntil(const Expr& expr);
    static void AddExec(void* pPhysAddr_, const Expr& expr = {});
    static void AddMemory(void* pPhysAddr_, AccessType nAccess_, const Expr& expr, int nLength_ = 1);
    static void AddPort(uint16_t wPort_, AccessType nAccess_, const Expr& expr = {});
    static void AddInterrupt(uint8_t bIntMask_, const Expr& expr = {});
    static Breakpoint* GetAt(int index);
    static std::optional<int> GetExecIndex(void* pPhysAddr_);
    static void Remove(int index);
    static void RemoveType(BreakType type);
    static void RemoveAll();
};

std::string to_string(const Breakpoint& bp);
std::string to_string(AccessType access);
