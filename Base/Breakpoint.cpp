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

#include "CPU.h"
#include "Debug.h"
#include "Memory.h"

std::vector<Breakpoint> Breakpoint::breakpoints;

std::optional<int> Breakpoint::Hit()
{
    auto pPC = AddrReadPtr(cpu.get_pc());

    auto index = -1;
    for (const auto& bp : breakpoints)
    {
        index++;

        if (!bp.enabled)
            continue;

        switch (bp.type)
        {
        case BreakType::Until:
            break;

        case BreakType::Execute:
            if (auto exec = std::get_if<BreakExec>(&bp.data))
            {
                if (exec->phys_addr == pPC)
                    break;
            }
            continue;

        case BreakType::Memory:
            if (auto mem = std::get_if<BreakMem>(&bp.data))
            {
                if ((mem->access == AccessType::Read || mem->access == AccessType::ReadWrite) &&
                    ((Memory::last_phys_read1 >= mem->phys_addr_from && Memory::last_phys_read1 <= mem->phys_addr_to) ||
                        (Memory::last_phys_read2 >= mem->phys_addr_from && Memory::last_phys_read2 <= mem->phys_addr_to)))
                {
                    Memory::last_phys_read1 = Memory::last_phys_read2 = nullptr;
                    break;
                }

                if ((mem->access == AccessType::Write || mem->access == AccessType::ReadWrite) &&
                    ((Memory::last_phys_write1 >= mem->phys_addr_from && Memory::last_phys_write1 <= mem->phys_addr_to) ||
                        (Memory::last_phys_write2 >= mem->phys_addr_from && Memory::last_phys_write2 <= mem->phys_addr_to)))
                {
                    Memory::last_phys_write1 = Memory::last_phys_write2 = nullptr;
                    break;
                }
            }
            continue;

        case BreakType::Port:
            if (auto port = std::get_if<BreakPort>(&bp.data))
            {
                if ((port->access == AccessType::Read || port->access == AccessType::ReadWrite) &&
                    ((CPU::last_in_port & port->mask) == port->compare))
                {
                    CPU::last_in_port = 0;
                    break;
                }

                if ((port->access == AccessType::Write || port->access == AccessType::ReadWrite) &&
                    ((CPU::last_out_port & port->mask) == port->compare))
                {
                    CPU::last_out_port = 0;
                    break;
                }
            }
            continue;

        case BreakType::Interrupt:
            if (auto intr = std::get_if<BreakInt>(&bp.data))
            {
                if (~IO::State().status & intr->mask)
                {
                    auto handler_addr = (cpu.get_int_mode() == 2) ?
                        read_word((cpu.get_i() << 8) | 0xff) :
                        IM1_INTERRUPT_HANDLER;

                    if (cpu.get_pc() == handler_addr)
                        break;
                }
            }
            continue;

        case BreakType::Temp:
            if (auto exec = std::get_if<BreakExec>(&bp.data))
            {
                if (exec->phys_addr == pPC || bp.expr)
                    break;
            }
            continue;
        }

        if (bp.expr && !bp.expr.Eval())
            continue;

        return index;
    }

    return std::nullopt;
}

void Breakpoint::Add(Breakpoint&& bp)
{
    // Allow address-only shorthand for Until conditions.
    if ((bp.type == BreakType::Until || bp.type == BreakType::Temp) && bp.expr)
    {
        if (auto val = bp.expr.TokenValue(Expr::TokenType::Number))
        {
            if (auto value = std::get_if<int>(&*val))
            {
                std::stringstream ss;
                ss << "PC==" << std::hex << *value;
                bp.expr = Expr::Compile(ss.str());
            }
        }
    }

    breakpoints.push_back(std::move(bp));
}

std::optional<int> Breakpoint::GetExecIndex(void* pPhysAddr)
{
    auto it = std::find_if(breakpoints.begin(), breakpoints.end(),
        [&](auto& bp)
        {
            if (bp.type != BreakType::Execute)
                return false;

            if (auto exec = std::get_if<BreakExec>(&bp.data))
                return exec->phys_addr == pPhysAddr;

            return false;
        });

    if (it != breakpoints.end())
    {
        return static_cast<int>(std::distance(breakpoints.begin(), it));
    }

    return std::nullopt;
}

void Breakpoint::AddTemp(void* pPhysAddr_, const Expr& expr)
{
    Breakpoint bp{ BreakType::Temp, expr };
    bp.data = BreakExec{ pPhysAddr_ };
    Add(std::move(bp));
}

void Breakpoint::AddUntil(const Expr& expr)
{
    Add(Breakpoint{ BreakType::Until, expr });
}

void Breakpoint::AddExec(void* pPhysAddr_, const Expr& expr)
{
    Breakpoint bp{ BreakType::Execute, expr };
    bp.data = BreakExec{ pPhysAddr_ };
    Add(std::move(bp));
}

void Breakpoint::AddMemory(void* pPhysAddr_, AccessType access, const Expr& expr, int nLength_/*=1*/)
{
    BreakMem mem{};
    mem.phys_addr_from = pPhysAddr_;
    mem.phys_addr_to = reinterpret_cast<uint8_t*>(pPhysAddr_) + nLength_ - 1;
    mem.access = access;

    Breakpoint bp{ BreakType::Memory, expr };
    bp.data = mem;
    Add(std::move(bp));
}

void Breakpoint::AddPort(uint16_t port_addr, AccessType access, const Expr& expr)
{
    BreakPort port{};
    port.compare = port_addr;
    port.mask = (port_addr <= 0xff) ? 0xff : 0xffff;
    port.access = access;

    Breakpoint bp{ BreakType::Port, expr };
    bp.data = port;
    Add(std::move(bp));
}

void Breakpoint::AddInterrupt(uint8_t bIntMask_, const Expr& expr)
{
    auto it = std::find_if(breakpoints.begin(), breakpoints.end(),
        [](auto& bp) { return bp.type == BreakType::Interrupt; });

    if (it != breakpoints.end())
    {
        if (auto intr = std::get_if<BreakInt>(&it->data))
        {
            intr->mask |= bIntMask_;
        }
    }
    else
    {
        Breakpoint bp{ BreakType::Interrupt, expr };
        bp.data = BreakInt{ bIntMask_ };
        Add(std::move(bp));
    }
}

Breakpoint* Breakpoint::GetAt(int index)
{
    if (index >= 0 && index < static_cast<int>(breakpoints.size()))
    {
        return &breakpoints[index];
    }

    return nullptr;
}

void Breakpoint::Remove(int index)
{
    if (index >= 0 && index < static_cast<int>(breakpoints.size()))
    {
        breakpoints.erase(breakpoints.begin() + index);
    }
}

void Breakpoint::RemoveType(BreakType type)
{
    breakpoints.erase(
        std::remove_if(breakpoints.begin(), breakpoints.end(),
            [&](auto& bp) { return bp.type == type; }),
        breakpoints.end());
}

void Breakpoint::RemoveAll()
{
    breakpoints.clear();
}

std::string to_string(AccessType access)
{
    switch (access)
    {
    case AccessType::ReadWrite: return "RW";
    case AccessType::Read: return "R";
    case AccessType::Write: return "W";
    }

    return "";
}

std::string to_string(const Breakpoint& bp)
{
    const void* pPhysAddr{};
    unsigned int extent{};

    std::stringstream ss;

    switch (bp.type)
    {
    case BreakType::Temp:
    {
        ss << "TEMP";
        break;
    }

    case BreakType::Until:
    {
        ss << fmt::format("UNTIL {}", bp.expr.str);
        break;
    }

    case BreakType::Execute:
        if (auto exec = std::get_if<BreakExec>(&bp.data))
        {
            pPhysAddr = exec->phys_addr;
            auto page_desc = Memory::PageDesc(PtrPage(pPhysAddr), true);
            auto page_offset = PtrOffset(pPhysAddr);
            ss << fmt::format("EXEC {}:{:04X}", page_desc, page_offset);
        }
        break;

    case BreakType::Memory:
        if (auto mem = std::get_if<BreakMem>(&bp.data))
        {
            pPhysAddr = mem->phys_addr_from;
            auto page_desc = Memory::PageDesc(PtrPage(pPhysAddr), true);
            auto page_offset = PtrOffset(pPhysAddr);
            ss << fmt::format("MEM {}:{:04X}", page_desc, page_offset);

            if (mem->phys_addr_to != pPhysAddr)
            {
                extent = static_cast<int>((uint8_t*)mem->phys_addr_to - (uint8_t*)pPhysAddr);
                ss << fmt::format(" L{:04X}", (extent + 1));
            }

            ss << fmt::format(" {}", to_string(mem->access));
        }
        break;

    case BreakType::Port:
        if (auto port = std::get_if<BreakPort>(&bp.data))
        {
            if (port->compare <= 0xff)
                ss << fmt::format("PORT {:02X} {}", port->compare, to_string(port->access));
            else
                ss << fmt::format("PORT {:04X} {}", port->compare, to_string(port->access));
        }
        break;

    case BreakType::Interrupt:
        if (auto intr = std::get_if<BreakInt>(&bp.data))
        {
            ss << "INT ";
            if (intr->mask & STATUS_INT_FRAME) ss << "FRAME ";
            if (intr->mask & STATUS_INT_LINE) ss << "LINE ";
            if (intr->mask & STATUS_INT_MIDIOUT) ss << "MIDIOUT ";
            if (intr->mask & STATUS_INT_MIDIIN) ss << "MIDIIN ";
        }
        break;

    default:
        ss << "???";
        break;
    }

    if (pPhysAddr)
    {
        std::optional<int> addr1;
        std::optional<int> addr2;

        auto page = PtrPage(pPhysAddr);
        auto offset = PtrOffset(pPhysAddr);

        if (page == AddrPage(0x0000)) { addr2 = addr1; addr1 = 0x0000 + offset; }
        if (page == AddrPage(0x4000)) { addr2 = addr1; addr1 = 0x4000 + offset; }
        if (page == AddrPage(0x8000)) { addr2 = addr1; addr1 = 0x8000 + offset; }
        if (page == AddrPage(0xc000)) { addr2 = addr1; addr1 = 0xc000 + offset; }

        if (addr2.has_value())
        {
            if (extent)
            {
                ss << fmt::format(" (@{:04X}-{:04X},@{:04X}-{:04X})",
                    *addr2, (*addr2 + extent),
                    *addr1, (*addr1 + extent));
            }
            else
            {
                ss << fmt::format(" (@{:04X},@{:04X})", *addr2, *addr1);
            }
        }
        else if (addr1.has_value())
        {
            if (extent)
            {
                ss << fmt::format(" (@{:04X}-{:04X})", *addr1, (*addr1 + extent));
            }
            else
            {
                ss << fmt::format(" (@{:04X})", *addr1);
            }
        }
    }

    if (bp.expr && bp.type != BreakType::Until)
    {
        ss << fmt::format(" if {}", bp.expr.str);
    }

    return ss.str();
}
