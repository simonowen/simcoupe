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
#include "Util.h"

extern uint8_t pMemory[];

namespace Frame
{
bool Init();
void Exit();

void Flyback();
void Begin();
void End();

void Update();

void TouchLines(int from, int to);
inline void TouchLine(int nLine_) { TouchLines(nLine_, nLine_); }

std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> GetAsicData();
void ChangeMode(uint8_t bNewVmpr_);
void ChangeScreen(uint8_t bNewBorder_);

void Sync();
void Redraw();
void SavePNG();
void SaveSSX();

int Width();
int Height();

void SetView(int num_cells, int num_lines);
void SetStatus(std::string&& str);

template <typename ...Args>
void SetStatus(const std::string& format, Args&& ... args)
{
    SetStatus(fmt::format(format, std::forward<Args>(args)...));
}

constexpr std::pair<int, int> GetRasterPos(uint32_t cycle_counter)
{
    int line{}, line_cycle{};

    if (cycle_counter >= CPU_CYCLES_PER_SIDE_BORDER)
    {
        auto screen_cycles = g_dwCycleCounter - CPU_CYCLES_PER_SIDE_BORDER;
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

}


inline bool IsScreenLine(int nLine_) { return nLine_ >= (TOP_BORDER_LINES) && nLine_ < (TOP_BORDER_LINES + GFX_SCREEN_LINES); }
inline uint8_t AttrBg(uint8_t bAttr_) { return (((bAttr_) >> 3) & 0xf); }
inline uint8_t AttrFg(uint8_t bAttr_) { return ((((bAttr_) >> 3) & 8) | ((bAttr_) & 7)); }

extern bool g_flash_phase;

extern int s_view_top, s_view_bottom;
extern int s_view_left, s_view_right;

extern uint16_t g_awMode1LineToByte[GFX_SCREEN_LINES];

////////////////////////////////////////////////////////////////////////////////

class ScreenWriter
{
public:
    void SetMode(uint8_t bVal_);
    void UpdateLine(FrameBuffer& fb, int line, int from, int to);
    std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> GetAsicData();

    void ModeChange(uint8_t* pLine, int line, int cell, uint8_t new_vmpr);
    void ScreenChange(uint8_t* pLine, int line, int cell, uint8_t new_border);

protected:
    void BorderLine(uint8_t* line, int from, int to);
    void BlackLine(uint8_t* line, int from, int to);
    void LeftBorder(uint8_t* line, int from, int to);
    void RightBorder(uint8_t* line, int from, int to);

    void Mode1Line(uint8_t* pLine, int line, int from, int to);
    void Mode2Line(uint8_t* pLine, int line, int from, int to);
    void Mode3Line(uint8_t* pLine, int line, int from, int to);
    void Mode4Line(uint8_t* pLine, int line, int from, int to);

protected:
    int m_display_mem_offset{};
};

////////////////////////////////////////////////////////////////////////////////

inline void ScreenWriter::LeftBorder(uint8_t* pLine, int from, int to)
{
    auto left = std::max(s_view_left, from);
    auto right = std::min(to, SIDE_BORDER_CELLS);

    if (left < right)
        memset(pLine + ((left - s_view_left) << 4), clut[border_col], (right - left) << 4);
}

inline void ScreenWriter::RightBorder(uint8_t* pLine, int from, int to)
{
    auto left = std::max((GFX_WIDTH_CELLS - SIDE_BORDER_CELLS), from);
    auto right = std::min(to, s_view_right);

    if (left < right)
        memset(pLine + ((left - s_view_left) << 4), clut[border_col], (right - left) << 4);
}

inline void ScreenWriter::BorderLine(uint8_t* pLine, int from, int to)
{
    auto left = std::max(s_view_left, from);
    auto right = std::min(to, s_view_right);

    if (left < right)
        memset(pLine + ((left - s_view_left) << 4), clut[border_col], (right - left) << 4);
}

inline void ScreenWriter::BlackLine(uint8_t* pLine, int from, int to)
{
    auto left = std::max(s_view_left, from);
    auto right = std::min(to, s_view_right);

    if (left < right)
        memset(pLine + ((left - s_view_left) << 4), 0, (right - left) << 4);
}


inline void ScreenWriter::Mode1Line(uint8_t* pLine, int line, int from, int to)
{
    line -= TOP_BORDER_LINES;

    LeftBorder(pLine, from, to);

    auto left = std::max(SIDE_BORDER_CELLS, from);
    auto right = std::min(to, SIDE_BORDER_CELLS + GFX_SCREEN_CELLS);

    if (left < right)
    {
        auto pFrame = pLine + ((left - s_view_left) << 4);
        auto pDataMem = pMemory + m_display_mem_offset + g_awMode1LineToByte[line] + (left - SIDE_BORDER_CELLS);
        auto pAttrMem = pMemory + m_display_mem_offset + 6144 + ((line & 0xf8) << 2) + (left - SIDE_BORDER_CELLS);

        for (auto i = left; i < right; i++)
        {
            auto data = *pDataMem++;
            auto attr = *pAttrMem++;
            auto ink_idx = AttrFg(attr);
            auto paper_idx = AttrBg(attr);

            if (g_flash_phase && (attr & 0x80))
                std::swap(ink_idx, paper_idx);

            auto ink = clut[ink_idx];
            auto paper = clut[paper_idx];

            pFrame[0] = pFrame[1] = (data & 0x80) ? ink : paper;
            pFrame[2] = pFrame[3] = (data & 0x40) ? ink : paper;
            pFrame[4] = pFrame[5] = (data & 0x20) ? ink : paper;
            pFrame[6] = pFrame[7] = (data & 0x10) ? ink : paper;
            pFrame[8] = pFrame[9] = (data & 0x08) ? ink : paper;
            pFrame[10] = pFrame[11] = (data & 0x04) ? ink : paper;
            pFrame[12] = pFrame[13] = (data & 0x02) ? ink : paper;
            pFrame[14] = pFrame[15] = (data & 0x01) ? ink : paper;

            pFrame += 16;
        }
    }

    RightBorder(pLine, from, to);
}

inline void ScreenWriter::Mode2Line(uint8_t* pLine, int line, int from, int to)
{
    line -= TOP_BORDER_LINES;

    LeftBorder(pLine, from, to);

    auto left = std::max(SIDE_BORDER_CELLS, from);
    auto right = std::min(to, SIDE_BORDER_CELLS + GFX_SCREEN_CELLS);

    if (left < right)
    {
        auto pFrame = pLine + ((left - s_view_left) << 4);
        auto pDataMem = pMemory + m_display_mem_offset + (line << 5) + (left - SIDE_BORDER_CELLS);
        auto pAttrMem = pDataMem + 0x2000;

        for (auto i = left; i < right; i++)
        {
            auto data = *pDataMem++;
            auto attr = *pAttrMem++;
            auto ink_idx = AttrFg(attr);
            auto paper_idx = AttrBg(attr);

            if (g_flash_phase && (attr & 0x80))
                std::swap(ink_idx, paper_idx);

            auto ink = clut[ink_idx];
            auto paper = clut[paper_idx];

            pFrame[0] = pFrame[1] = (data & 0x80) ? ink : paper;
            pFrame[2] = pFrame[3] = (data & 0x40) ? ink : paper;
            pFrame[4] = pFrame[5] = (data & 0x20) ? ink : paper;
            pFrame[6] = pFrame[7] = (data & 0x10) ? ink : paper;
            pFrame[8] = pFrame[9] = (data & 0x08) ? ink : paper;
            pFrame[10] = pFrame[11] = (data & 0x04) ? ink : paper;
            pFrame[12] = pFrame[13] = (data & 0x02) ? ink : paper;
            pFrame[14] = pFrame[15] = (data & 0x01) ? ink : paper;

            pFrame += 16;
        }
    }

    RightBorder(pLine, from, to);
}

inline void ScreenWriter::Mode3Line(uint8_t* pLine, int line, int from, int to)
{
    line -= TOP_BORDER_LINES;

    LeftBorder(pLine, from, to);

    auto left = std::max(SIDE_BORDER_CELLS, from);
    auto right = std::min(to, SIDE_BORDER_CELLS + GFX_SCREEN_CELLS);

    if (left < right)
    {
        auto pFrame = pLine + ((left - s_view_left) << 4);
        auto pMem = pMemory + m_display_mem_offset + (line << 7) + ((left - SIDE_BORDER_CELLS) << 2);

        for (auto i = left; i < right; i++)
        {
            uint8_t data{};

            data = pMem[0];
            pFrame[0] = mode3clut[data >> 6];
            pFrame[1] = mode3clut[(data & 0x30) >> 4];
            pFrame[2] = mode3clut[(data & 0x0c) >> 2];
            pFrame[3] = mode3clut[(data & 0x03)];

            data = pMem[1];
            pFrame[4] = mode3clut[data >> 6];
            pFrame[5] = mode3clut[(data & 0x30) >> 4];
            pFrame[6] = mode3clut[(data & 0x0c) >> 2];
            pFrame[7] = mode3clut[(data & 0x03)];

            data = pMem[2];
            pFrame[8] = mode3clut[data >> 6];
            pFrame[9] = mode3clut[(data & 0x30) >> 4];
            pFrame[10] = mode3clut[(data & 0x0c) >> 2];
            pFrame[11] = mode3clut[(data & 0x03)];

            data = pMem[3];
            pFrame[12] = mode3clut[data >> 6];
            pFrame[13] = mode3clut[(data & 0x30) >> 4];
            pFrame[14] = mode3clut[(data & 0x0c) >> 2];
            pFrame[15] = mode3clut[(data & 0x03)];

            pFrame += 16;

            pMem += 4;
        }
    }

    RightBorder(pLine, from, to);
}

inline void ScreenWriter::Mode4Line(uint8_t* pLine, int line, int from, int to)
{
    line -= TOP_BORDER_LINES;

    LeftBorder(pLine, from, to);

    auto left = std::max(SIDE_BORDER_CELLS, from);
    auto right = std::min(to, SIDE_BORDER_CELLS + GFX_SCREEN_CELLS);

    if (left < right)
    {
        auto pFrame = pLine + ((left - s_view_left) << 4);
        auto pMem = pMemory + m_display_mem_offset + ((left - SIDE_BORDER_CELLS) << 2) + (line << 7);

        for (auto i = left; i < right; i++)
        {
            uint8_t data{};

            data = pMem[0];
            pFrame[0] = pFrame[1] = clut[data >> 4];
            pFrame[2] = pFrame[3] = clut[data & 0x0f];

            data = pMem[1];
            pFrame[4] = pFrame[5] = clut[data >> 4];
            pFrame[6] = pFrame[7] = clut[data & 0x0f];

            data = pMem[2];
            pFrame[8] = pFrame[9] = clut[data >> 4];
            pFrame[10] = pFrame[11] = clut[data & 0x0f];

            data = pMem[3];
            pFrame[12] = pFrame[13] = clut[data >> 4];
            pFrame[14] = pFrame[15] = clut[data & 0x0f];

            pFrame += 16;
            pMem += 4;
        }
    }

    RightBorder(pLine, from, to);
}

inline void ScreenWriter::ModeChange(uint8_t* pLine, int line, int cell, uint8_t bNewVmpr_)
{
    int screen_line = line - TOP_BORDER_LINES;
    uint8_t ab[4];

    // Fetch the 4 display data bytes for the original mode
    auto [b0, b1, b2, b3] = GetAsicData();

    // Perform the necessary massaging the ASIC does to prepare for display
    if (VMPR_MODE_3_OR_4)
    {
        ab[0] = ab[1] = (b0 & 0x80) | ((b0 << 3) & 0x40) |
            ((b1 >> 2) & 0x20) | ((b1 << 1) & 0x10) |
            ((b2 >> 4) & 0x08) | ((b2 >> 1) & 0x04) |
            ((b3 >> 6) & 0x02) | (b3 & 0x01);
        ab[2] = ab[3] = b2;
    }
    else
    {
        ab[0] = (b0 & 0x77) | (b0 & 0x80) | ((b0 >> 3) & 0x08);
        ab[1] = (b1 & 0x77) | ((b1 << 2) & 0x80) | ((b1 >> 1) & 0x08);
        ab[2] = (b2 & 0x77) | ((b0 << 4) & 0x80) | ((b0 << 1) & 0x08);
        ab[3] = (b3 & 0x77) | ((b1 << 6) & 0x80) | ((b1 << 3) & 0x08);
    }

    // The target mode decides how the data actually appears in the transition block
    switch (bNewVmpr_ & VMPR_MODE_MASK)
    {
    case MODE_1:
    {
        auto pData = pMemory + m_display_mem_offset + g_awMode1LineToByte[screen_line] + (cell - SIDE_BORDER_CELLS);
        auto pAttr = pMemory + m_display_mem_offset + MODE12_DATA_BYTES + ((screen_line & 0xf8) << 2) + (cell - SIDE_BORDER_CELLS);
        auto bData = *pData;
        auto bAttr = *pAttr;

        // TODO: avoid temporary write
        *pData = ab[0];
        *pAttr = ab[2];
        Mode1Line(pLine, line, cell, cell + 1);
        *pData = bData;
        *pAttr = bAttr;
        break;
    }

    case MODE_2:
    {
        auto pData = pMemory + m_display_mem_offset + (screen_line << 5) + (cell - SIDE_BORDER_CELLS);
        auto pAttr = pData + MODE2_ATTR_OFFSET;
        auto bData = *pData;
        auto bAttr = *pAttr;

        // TODO: avoid temporary write
        *pData = ab[0];
        *pAttr = ab[2];
        Mode2Line(pLine, line, cell, cell + 1);
        *pData = bData;
        *pAttr = bAttr;
        break;
    }

    default:
    {
        auto pb = pMemory + m_display_mem_offset + (screen_line << 7) + ((cell - SIDE_BORDER_CELLS) << 2);
        auto pdw = reinterpret_cast<uint32_t*>(pb);
        auto dw = *pdw;

        // TODO: avoid temporary write
        pb[0] = ab[0];
        pb[1] = ab[1];
        pb[2] = ab[2];
        pb[3] = ab[3];

        if ((bNewVmpr_ & VMPR_MODE_MASK) == MODE_3)
            Mode3Line(pLine, line, cell, cell + 1);
        else
            Mode4Line(pLine, line, cell, cell + 1);

        *pdw = dw;
        break;
    }
    }
}

inline void ScreenWriter::ScreenChange(uint8_t* pLine, int /*line*/, int cell, uint8_t new_border)
{
    auto pFrame = pLine + ((cell - s_view_left) << 4);

    // Part of the first pixel is the previous border colour, from when the screen was disabled.
    // We don't have the resolution to show only part, but using the most significant colour bits
    // in the least significant position will reduce the intensity enough to be close
    pFrame[0] = clut[border_col] >> 4;

    pFrame[1] = pFrame[2] = pFrame[3] =
        pFrame[4] = pFrame[5] = pFrame[6] = pFrame[7] =
        pFrame[8] = pFrame[9] = pFrame[10] = pFrame[11] =
        pFrame[12] = pFrame[13] = pFrame[14] = pFrame[15] = clut[BORD_COL(new_border)];
}
