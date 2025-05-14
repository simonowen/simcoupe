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

#include "Debug.h"
#include "Memory.h"
#include "Options.h"
#include "SAM.h"
#include "SAMIO.h"
#include "Tape.h"

constexpr uint16_t IM1_INTERRUPT_HANDLER = 0x0038;
constexpr uint16_t NMI_INTERRUPT_HANDLER = 0x0066;

constexpr uint8_t OP_NOP = 0x00;
constexpr uint8_t OP_DJNZ = 0x10;
constexpr uint8_t OP_JR = 0x18;
constexpr uint8_t OP_HALT = 0x76;
constexpr uint8_t OP_JP = 0xc3;
constexpr uint8_t OP_RET = 0xc9;
constexpr uint8_t OP_CALL = 0xcd;
constexpr uint8_t OP_JPHL = 0xe9;
constexpr uint8_t OP_DI = 0xf3;

constexpr uint8_t IX_PREFIX = 0xdd;
constexpr uint8_t IY_PREFIX = 0xfd;
constexpr uint8_t CB_PREFIX = 0xcb;
constexpr uint8_t ED_PREFIX = 0xed;

namespace CPU
{
bool Init(bool fFirstInit_ = false);
void Exit(bool fReInit_ = false);

void Run();
void ExecuteChunk();

void Reset(bool active);
void NMI();

extern uint32_t frame_cycles;
extern bool reset_asserted;
extern uint16_t last_in_port, last_out_port;
extern uint8_t last_in_val, last_out_val;
}

extern bool g_fBreak, g_fPaused;
extern int g_nTurbo;

#ifdef _DEBUG
extern bool debug_break;
#endif


struct sam_cpu : public z80::z80_cpu<sam_cpu>
{
    using base = z80::z80_cpu<sam_cpu>;
    using base::sf_mask, base::zf_mask, base::yf_mask, base::hf_mask;
    using base::xf_mask, base::pf_mask, base::nf_mask, base::cf_mask;

    void on_tick(unsigned t)
    {
        CPU::frame_cycles += t;
    }

    void on_mreq_wait(z80::fast_u16 addr)
    {
        on_tick(Memory::WaitStates(CPU::frame_cycles, addr));
    }

    void on_iorq_wait(z80::fast_u16 port)
    {
        on_tick(IO::WaitStates(CPU::frame_cycles, port));
    }

    z80::fast_u8 on_read(z80::fast_u16 addr)
    {
        return Memory::Read(addr);
    }

    void on_write(z80::fast_u16 addr, z80::fast_u8 val)
    {
        Memory::Write(addr, val);
    }

    z80::fast_u8 on_input(z80::fast_u16 port)
    {
        CPU::last_in_port = port;
        CPU::last_in_val = IO::In(port);
        return CPU::last_in_val;
    }

    void on_output(z80::fast_u16 port, z80::fast_u8 val)
    {
        CPU::last_out_port = port;
        CPU::last_out_val = val;
        IO::Out(port, val);
    }

    z80::z80_variant on_get_z80_variant()
    {
        return GetOption(cmosz80) ?
            z80::z80_variant::cmos :
            z80::z80_variant::common;
    }

    void on_ei()
    {
        IO::EiHook();
        base::on_ei();
    }

    void on_ret()
    {
        Debug::OnRet();
        base::on_ret();
    }

    void on_ret_cc(z80::condition cc)
    {
        if (cc == z80::condition::z)
        {
            Debug::RetZHook();
            if (Tape::RetZHook())
                return;
        }

        base::on_ret_cc(cc);
    }

    void on_rst(z80::fast_u16 nn)
    {
        if (nn == 48)
            IO::Rst48Hook();
        else if (nn == 8 && IO::Rst8Hook())
            return;

        base::on_rst(nn);
    }

    z80::fast_u8 on_get_int_vector()
    {
        if (!GetOption(im2random))
        {
            return base::on_get_int_vector();
        }

        static uint8_t last_busval{ 0xff };
        constexpr uint8_t step{ 37 };
        last_busval += step;
        return last_busval;
    }
};

extern sam_cpu cpu;
