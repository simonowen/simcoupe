 // Part of SimCoupe - A SAM Coupe emulator
//
// CPU.cpp: Z80 processor emulation and main emulation loop
//
//  Copyright (c) 1999-2015 Simon Owen
//  Copyright (c) 2000-2003 Dave Laundon
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

// Changes 1999-2000 by Simon Owen:
//  - general revamp and reformat, with execution now polled for each frame
//  - very rough contended memory timings by doubling basic timings
//  - frame/line interrupt and flash frequency values corrected

// Changes 2000-2001 by Dave Laundon
//  - perfect contended memory timings on each memory/port access
//  - new cpu event model to reduce the per-instruction overhead
//  - MIDI OUT interrupt timings corrected

#include "SimCoupe.h"
#include "CPU.h"

#include "BlueAlpha.h"
#include "Debug.h"
#include "Events.h"
#include "Frame.h"
#include "GUI.h"
#include "Input.h"
#include "Keyin.h"
#include "SAMIO.h"
#include "Memory.h"
#include "Mouse.h"
#include "Options.h"
#include "Tape.h"
#include "UI.h"

sam_cpu cpu;

////////////////////////////////////////////////////////////////////////////////
//  H E L P E R   M A C R O S


bool g_fBreak, g_fPaused;
int g_nTurbo;

#ifdef _DEBUG
bool debug_break;
#endif


namespace CPU
{
uint32_t frame_cycles;
bool reset_asserted = false;
uint16_t last_in_port, last_out_port;
uint8_t last_in_val, last_out_val;

bool Init(bool fFirstInit_/*=false*/)
{
    bool fRet = true;

    if (fFirstInit_)
    {
        InitEvents();
        AddEvent(EventType::FrameInterrupt, 0);
        AddEvent(EventType::InputUpdate, CPU_CYCLES_PER_FRAME * 3 / 4);

        fRet &= Memory::Init(true) && IO::Init();
    }

    Reset(true);
    Reset(false);

    return fRet;
}

void Exit(bool fReInit_)
{
    IO::Exit(fReInit_);
    Memory::Exit(fReInit_);

    // TODO: remove after switching to use "physical" offsets instead of pointers
    if (!fReInit_)
        Breakpoint::RemoveAll();
}



void ExecuteChunk()
{
    if (reset_asserted)
    {
        CPU::frame_cycles = CPU_CYCLES_PER_FRAME;
        CheckEvents(CPU::frame_cycles);
        return;
    }

    for (g_fBreak = false; !g_fBreak; )
    {
        cpu.on_step();

        CheckEvents(CPU::frame_cycles);

        if ((~IO::State().status & STATUS_INT_MASK) && Memory::full_contention)
            cpu.on_handle_active_int();

        if (cpu.get_iregp_kind() != z80::iregp::hl)
            continue;

#ifdef _DEBUG
        if (debug_break)
        {
            Debug::Start();
            debug_break = false;
        }
#endif

        if (Breakpoint::breakpoints.empty())
            continue;

        Debug::AddTraceRecord();

        if (auto bp_index = Breakpoint::Hit())
        {
            CheckEvents(CPU::frame_cycles);
            Debug::Start(bp_index);
        }
    }
}

void Run()
{
    while (UI::CheckEvents())
    {
        if (g_fPaused)
            continue;

        Frame::Begin();

        if (!Debug::IsActive() && !GUI::IsModal())
            ExecuteChunk();

        Frame::End();

        if (CPU::frame_cycles >= CPU_CYCLES_PER_FRAME)
        {
            EventFrameEnd(CPU_CYCLES_PER_FRAME);

            IO::FrameUpdate();
            Debug::FrameEnd();
            Frame::Flyback();

            CPU::frame_cycles %= CPU_CYCLES_PER_FRAME;
        }
    }

    TRACE("Quitting main emulation loop...\n");
}

void Reset(bool active)
{
    if (GetOption(fastreset) && reset_asserted && !active)
        g_nTurbo |= TURBO_BOOT;

    reset_asserted = active;
    if (reset_asserted)
    {
        cpu.set_is_halted(false);
        cpu.set_iff1(false);
        cpu.set_pc(0);
        cpu.set_ir(0);

        Keyin::Stop();
        Tape::Stop();

        IO::Init();
        Memory::Init();

        Debug::Refresh();
    }
}

void NMI()
{
    cpu.initiate_nmi();
    Debug::Refresh();
}

} // namespace CPU
