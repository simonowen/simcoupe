// Part of SimCoupe - A SAM Coupe emulator
//
// Frame.h: Display frame generation
//
//  Copyright (c) 1999-2015 Simon Owen
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

// Notes:
//  Contains portions of the drawing code are from the original SAMGRX.C
//  ASIC artefact during mode change identified by Dave Laundon

#pragma once

#include "CPU.h"
#include "SAMIO.h"
#include "FrameBuffer.h"

extern uint8_t pMemory[];

enum { TURBO_BOOT = 0x01, TURBO_KEY = 0x02 };

namespace Frame
{
bool Init();
void Exit();

void Flyback();
void Begin();
void End();

void Update();
bool TurboMode();

void TouchLines(int from, int to);
inline void TouchLine(int nLine_) { TouchLines(nLine_, nLine_); }

std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> GetAsicData();
void ModeChanged(uint8_t bNewVmpr_);
void BorderChanged(uint8_t bNewBorder_);

void Sync();
void Redraw();
void SavePNG();
void SaveSSX();

int Width();
int Height();

void SetStatus(std::string&& str);

template <typename ...Args>
void SetStatus(const std::string& format, Args&& ... args)
{
    SetStatus(fmt::format(format, std::forward<Args>(args)...));
}

constexpr std::pair<int, int> GetRasterPos(uint32_t frame_cycles)
{
    int line{}, line_cycle{};

    if (frame_cycles >= CPU_CYCLES_PER_SIDE_BORDER)
    {
        auto screen_cycles = frame_cycles - CPU_CYCLES_PER_SIDE_BORDER;
        line = screen_cycles / CPU_CYCLES_PER_LINE;
        line_cycle = screen_cycles % CPU_CYCLES_PER_LINE;
    }
    else
    {
        // FIXME: start of interrupt frame is final line of display
        line = 0;
        line_cycle = 0;
    }

    return std::make_pair(line, line_cycle);
}

void ModeArtefact(uint8_t* pLine, int line, int cell, uint8_t new_vmpr);
void BorderArtefact(uint8_t* pLine, int line, int cell, uint8_t new_border);

void UpdateLine(FrameBuffer& fb, int line, int from, int to);

void BorderLine(uint8_t* line, int from, int to);
void BlackLine(uint8_t* line, int from, int to);
void LeftBorder(uint8_t* line, int from, int to);
void RightBorder(uint8_t* line, int from, int to);

void Mode1Line(uint8_t* pLine, int line, int from, int to);
void Mode2Line(uint8_t* pLine, int line, int from, int to);
void Mode3Line(uint8_t* pLine, int line, int from, int to);
void Mode4Line(uint8_t* pLine, int line, int from, int to);

}


constexpr bool IsScreenLine(int line) { return line >= TOP_BORDER_LINES && line < (TOP_BORDER_LINES + GFX_SCREEN_LINES); }
constexpr uint8_t attr_bg(uint8_t attr) { return (attr >> 3) & 0xf; }
constexpr uint8_t attr_fg(uint8_t attr) { return ((attr >> 3) & 8) | (attr & 7); }

extern uint16_t g_awMode1LineToByte[GFX_SCREEN_LINES];
