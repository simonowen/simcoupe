// Part of SimCoupe - A SAM Coupe emulator
//
// Frame.cpp: Display frame generation
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

#include "SimCoupe.h"
#include "Frame.h"

#include "Audio.h"
#include "AtaAdapter.h"
#include "AVI.h"
#include "Debug.h"
#include "Drive.h"
#include "GIF.h"
#include "GUI.h"
#include "Keyin.h"
#include "Memory.h"
#include "Options.h"
#include "SavePNG.h"
#include "Sound.h"
#include "SSX.h"
#include "Tape.h"
#include "UI.h"

constexpr uint8_t FLOPPY_LED_COLOUR = GREEN_5;
constexpr uint8_t ATOM_LED_COLOUR = RED_6;
constexpr uint8_t ATOMLITE_LED_COLOUR = 89;
constexpr uint8_t LED_OFF_COLOUR = GREY_2;

constexpr auto STATUS_ACTIVE_TIME = std::chrono::milliseconds(2500);
constexpr auto FPS_IN_TURBO_MODE = 5;

namespace Frame
{
struct REGION { int w, h; };
const std::vector<REGION> view_areas = {
    { GFX_SCREEN_CELLS, GFX_SCREEN_LINES },          // no border
    { GFX_SCREEN_CELLS + 2, GFX_SCREEN_LINES + 16 }, // 8 pixel border
    { GFX_SCREEN_CELLS + 4, GFX_SCREEN_LINES + 76 }, // TV visible (action safe, 93%)
    { GFX_SCREEN_CELLS + 8, GFX_SCREEN_LINES + 96 }, // full active
};

static void DrawOSD(FrameBuffer& fb);

std::unique_ptr<FrameBuffer> pFrameBuffer;
std::unique_ptr<FrameBuffer> pGuiScreen;

bool draw_frame;
bool save_png;
bool save_ssx;

int s_view_top, s_view_bottom;
int s_view_left, s_view_right;

int last_line, last_cell;

uint8_t* display_mem;
std::array<uint8_t, 4> mode3clut;

std::chrono::steady_clock::time_point status_time;
std::string status_text;
std::string profile_text;

bool Init()
{
    Exit();

    auto view_idx = std::min(GetOption(visiblearea), static_cast<int>(view_areas.size()) - 1);

    s_view_left = (GFX_WIDTH_CELLS - view_areas[view_idx].w) >> 1;
    s_view_right = s_view_left + view_areas[view_idx].w;

    if ((s_view_top = (GFX_HEIGHT_LINES - view_areas[view_idx].h) >> 1))
        s_view_top += (TOP_BORDER_LINES - BOTTOM_BORDER_LINES) >> 1;
    s_view_bottom = s_view_top + view_areas[view_idx].h;

    auto width = (s_view_right - s_view_left) * GFX_PIXELS_PER_CELL ;
    auto height = (s_view_bottom - s_view_top);

    pFrameBuffer = std::make_unique<FrameBuffer>(width, height);
    pGuiScreen = std::make_unique<FrameBuffer>(width, height * 2);

    Flyback();
    return true;
}

void Exit()
{
    GIF::Stop();
    AVI::Stop();

    pFrameBuffer.reset();
    pGuiScreen.reset();
}


int Width()
{
    return pGuiScreen ? pGuiScreen->Width() : 1;
}

int Height()
{
    return pGuiScreen ? pGuiScreen->Height() : 1;
}

int AspectWidth()
{
    auto aspect_ratio = GetOption(tvaspect) ? GFX_DISPLAY_ASPECT_RATIO : 1.0f;
    return static_cast<int>(std::round(Width() * aspect_ratio));
}

void Update()
{
    if (!draw_frame)
        return;

    display_mem = pMemory + PageReadOffset(IO::VisibleScreenPage());

    if ((IO::State().vmpr & VMPR_MODE_MASK) == VMPR_MODE_3)
    {
        const auto& io_state = IO::State();
        uint8_t mode3_bcd48 = (io_state.hmpr & HMPR_MD3COL_MASK) >> 3;
        mode3clut = {
            io_state.clut[mode3_bcd48 | 0],
            io_state.clut[mode3_bcd48 | 2], // note: swapped entries
            io_state.clut[mode3_bcd48 | 1],
            io_state.clut[mode3_bcd48 | 3]
        };
    }

    auto [line, line_cycle] = Frame::GetRasterPos(CPU::frame_cycles);
    auto cell = line_cycle / CPU_CYCLES_PER_CELL;

    auto from = std::max(last_line, s_view_top);
    auto to = std::min(line, s_view_bottom - 1);

    if (line == last_line)
    {
        if (cell > last_cell)
        {
            UpdateLine(*pFrameBuffer, line, last_cell, cell);
            last_cell = cell;
        }
    }
    else
    {
        if (from <= to)
        {
            if (from == last_line)
            {
                UpdateLine(*pFrameBuffer, last_line, last_cell, GFX_WIDTH_CELLS);
                from++;
            }

            if (to == line)
            {
                UpdateLine(*pFrameBuffer, line, 0, cell);
                to--;
            }

            for (int i = from; i <= to; ++i)
            {
                UpdateLine(*pFrameBuffer, i, 0, GFX_WIDTH_CELLS);
            }
        }

        last_line = line;
        last_cell = cell;
    }
}

std::unique_ptr<FrameBuffer> RedrawnDisplay()
{
    auto frame_buffer = std::make_unique<FrameBuffer>(*pFrameBuffer);

    for (int i = s_view_top; i < s_view_bottom; ++i)
    {
        UpdateLine(*frame_buffer, i, 0, GFX_WIDTH_CELLS);
    }

    return frame_buffer;
}

static void DrawRaster(FrameBuffer& fb)
{
    static const std::vector<int> raster_colours
    {
        GREY_1, GREY_2, GREY_3, GREY_4, GREY_5, GREY_6, GREY_7, GREY_8,
        GREY_8, GREY_7, GREY_6, GREY_5, GREY_4, GREY_3, GREY_2, GREY_1
    };

    if (last_cell < s_view_left || last_cell >= s_view_right ||
        last_line < s_view_top || last_line >= s_view_bottom)
    {
        return;
    }

    int line_offset = (last_cell - s_view_left) << 4;
    int line = (last_line - s_view_top) * 2;

    static uint8_t phase = 0;
    auto colour = raster_colours[++phase & 0xf];

    auto pLine0 = fb.GetLine(line);
    auto pLine1 = fb.GetLine(line + 1);
    pLine0[line_offset] = pLine0[line_offset + 1] = colour;
    pLine1[line_offset] = pLine1[line_offset + 1] = colour;
}


void End()
{
    Update();

    if (GUI::IsActive())
    {
        std::unique_ptr<FrameBuffer> full_frame_buffer;
        auto pBuffer = pFrameBuffer.get();

        if (Debug::IsActive() && !GetOption(rasterdebug))
        {
            full_frame_buffer = RedrawnDisplay();
            pBuffer = full_frame_buffer.get();
        }

        for (int i = 0; i < Height(); i++)
        {
            auto pLine = pBuffer->GetLine(i >> 1);
            auto width = pBuffer->Width();

            memcpy(pGuiScreen->GetLine(i), pLine, width);
        }

        if (Debug::IsActive())
            DrawRaster(*pGuiScreen);

        GUI::Draw(*pGuiScreen);
    }
    else
    {
        if (save_png)
        {
            PNG::Save(*pFrameBuffer);
            save_png = false;
        }

        if (save_ssx)
        {
            auto main_x = (SIDE_BORDER_CELLS - s_view_left) << 4;
            auto main_y = (TOP_BORDER_LINES - s_view_top);
            SSX::Save(*pFrameBuffer, main_x, main_y);
            save_ssx = false;
        }

        GIF::AddFrame(*pFrameBuffer);
        AVI::AddFrame(*pFrameBuffer);

        DrawOSD(*pFrameBuffer);
    }

    if (draw_frame)
        Redraw();

    Sync();
}

void Flyback()
{
    last_line = last_cell = 0;

    if (!status_text.empty())
    {
        auto now = std::chrono::steady_clock::now();
        if ((now - status_time) > STATUS_ACTIVE_TIME)
            status_text.clear();
    }
}

bool TurboMode()
{
    if (g_nTurbo != 0)
        return true;

    if (GetOption(turbotape) && Tape::IsPlaying())
        return true;

    if (GetOption(turbodisk) && (pFloppy1->IsActive() || pFloppy2->IsActive()))
        return true;

    if (Keyin::IsTyping())
        return true;

    return false;
}

void Sync()
{
    using namespace std::chrono;
    using namespace std::literals::chrono_literals;
    auto now = high_resolution_clock::now();

    if ((g_nTurbo & TURBO_BOOT) && !GUI::IsActive())
    {
        draw_frame = false;
    }
    else if (!(g_nTurbo & TURBO_KEY) && !GUI::IsActive() && TurboMode())
    {
        static high_resolution_clock::time_point last_drawn;
        draw_frame = ((now - last_drawn) >= (1s / static_cast<float>(FPS_IN_TURBO_MODE)));
        if (draw_frame)
            last_drawn = now;
    }
    else
    {
        draw_frame = true;
    }

    static int num_frames;
    num_frames++;

    static std::optional<high_resolution_clock::time_point> last_profiled;
    if (!last_profiled || (now - *last_profiled) >= 1s)
    {
        if (last_profiled)
        {
            auto fps = 1s / ((now - *last_profiled) / static_cast<float>(num_frames));
            auto percent = fps / ACTUAL_FRAMES_PER_SECOND * 100;
            profile_text = fmt::format("{:.0f}%", percent);
        }

        last_profiled = now;
        num_frames = 0;
    }

    if (GUI::IsActive())
    {
        static uint8_t abSilence[SAMPLE_FREQ * BYTES_PER_SAMPLE / EMULATED_FRAMES_PER_SECOND];
        Audio::AddData(abSilence, sizeof(abSilence));
    }
}

void Redraw()
{
    if (GUI::IsActive())
        Video::Update(*pGuiScreen);
    else
        Video::Update(*pFrameBuffer);
}

void DrawOSD(FrameBuffer& fb)
{
    auto width = fb.Width();
    auto height = fb.Height();

    if (GetOption(drivelights))
    {
        int x = 2;
        int y = ((GetOption(drivelights) - 1) & 1) ? height - 4 : 2;

        if (GetOption(drive1))
        {
            uint8_t bColour = pFloppy1->IsLightOn() ? FLOPPY_LED_COLOUR : LED_OFF_COLOUR;
            fb.FillRect(x, y, 14, 2, bColour);
        }

        if (GetOption(drive2))
        {
            bool atom_active = pAtom->IsActive() || pAtomLite->IsActive();
            auto atom_colour = pAtom->IsActive() ? ATOM_LED_COLOUR : ATOMLITE_LED_COLOUR;
            auto colour = pFloppy2->IsLightOn() ? FLOPPY_LED_COLOUR : (atom_active ? atom_colour : LED_OFF_COLOUR);
            fb.FillRect(x + 18, y, 14, 2, colour);
        }
    }

    auto font = sPropFont;
    fb.SetFont(font);

    if (GetOption(profile) && !GUI::IsActive())
    {
        int x = width - fb.StringWidth(profile_text);
        fb.DrawString(x, 2, BLACK, profile_text);
        fb.DrawString(x - 2, 1, WHITE, profile_text);
    }

    if (GetOption(status) && !status_text.empty())
    {
        int x = width - fb.StringWidth(status_text);
        fb.DrawString(x, height - font->height - 1, BLACK, status_text);
        fb.DrawString(x - 2, height - font->height - 2, WHITE, status_text);
    }
}

void SavePNG()
{
    save_png = true;
}

void SaveSSX()
{
    save_ssx = true;
}

void SetStatus(std::string&& str)
{
    status_text = std::move(str);
    status_time = std::chrono::steady_clock::now();
}

////////////////////////////////////////////////////////////////////////////////

void ModeChanged(uint8_t new_vmpr)
{
    auto [line, line_cycle] = Frame::GetRasterPos(CPU::frame_cycles);
    if (IsScreenLine(line))
    {
        auto cell = line_cycle / CPU_CYCLES_PER_CELL;
        if (cell < (SIDE_BORDER_CELLS + GFX_SCREEN_CELLS))
        {
            if (((IO::State().vmpr ^ new_vmpr) & VMPR_MDE1_MASK) && cell >= SIDE_BORDER_CELLS)
            {
                auto pLine = pFrameBuffer->GetLine(line - s_view_top);
                ModeArtefact(pLine, line, cell, new_vmpr);
                last_cell++;
            }
        }
    }
}

void BorderChanged(uint8_t new_border)
{
    auto [line, line_cycle] = Frame::GetRasterPos(CPU::frame_cycles);
    auto cell = line_cycle / CPU_CYCLES_PER_CELL;

    if (line >= s_view_top && line < s_view_bottom && cell >= s_view_left && cell < s_view_right)
    {
        auto pLine = pFrameBuffer->GetLine(line - s_view_top);
        BorderArtefact(pLine, line, cell, new_border);
        last_cell++;
    }
}

void TouchLines(int from, int to)
{
    if (to >= last_line && from <= (int)((CPU::frame_cycles - CPU_CYCLES_PER_SIDE_BORDER) / CPU_CYCLES_PER_LINE))
        Update();
}

void UpdateLine(FrameBuffer& fb, int line, int from, int to)
{
    if (line >= s_view_top && line < s_view_bottom)
    {
        auto pLine = fb.GetLine(line - s_view_top);

        if (IO::ScreenDisabled())
        {
            BlackLine(pLine, from, to);
        }
        else if (line < TOP_BORDER_LINES || line >= (TOP_BORDER_LINES + GFX_SCREEN_LINES))
        {
            BorderLine(pLine, from, to);
        }
        else
        {
            switch (IO::State().vmpr & VMPR_MODE_MASK)
            {
            case VMPR_MODE_1: Mode1Line(pLine, line, from, to); break;
            case VMPR_MODE_2: Mode2Line(pLine, line, from, to); break;
            case VMPR_MODE_3: Mode3Line(pLine, line, from, to); break;
            case VMPR_MODE_4: Mode4Line(pLine, line, from, to); break;
            }
        }
    }
}

std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> GetAsicData()
{
    display_mem = pMemory + PageReadOffset(IO::VisibleScreenPage());

    int line = CPU::frame_cycles / CPU_CYCLES_PER_LINE;
    int cell = (CPU::frame_cycles % CPU_CYCLES_PER_LINE) >> 3;

    line -= TOP_BORDER_LINES;
    cell -= SIDE_BORDER_CELLS + SIDE_BORDER_CELLS;

    if (cell < 0) { line--; cell = GFX_SCREEN_CELLS - 1; }
    if (line < 0 || line >= GFX_SCREEN_LINES) { line = GFX_SCREEN_LINES - 1; cell = GFX_SCREEN_CELLS - 1; }

    if (IO::ScreenMode3or4())
    {
        auto pMem = display_mem + (line * MODE34_BYTES_PER_LINE) + (cell * GFX_DATA_BYTES_PER_CELL);
        return std::make_tuple(pMem[0], pMem[1], pMem[2], pMem[3]);
    }
    else if ((IO::State().vmpr & VMPR_MODE_MASK) == VMPR_MODE_1)
    {
        auto pDataMem = display_mem + g_awMode1LineToByte[line] + cell;
        auto pAttrMem = display_mem + MODE12_DATA_BYTES + ((line & 0xf8) * GFX_DATA_BYTES_PER_CELL) + cell;
        return std::make_tuple(*pDataMem, *pDataMem, *pAttrMem, *pAttrMem);
    }
    else
    {
        auto pDataMem = display_mem + (line << 5) + cell;
        auto pAttrMem = pDataMem + MODE2_ATTR_OFFSET;
        return std::make_tuple(*pDataMem, *pDataMem, *pAttrMem, *pAttrMem);
    }
}

void LeftBorder(uint8_t* pLine, int from, int to)
{
    auto left = std::max(s_view_left, from);
    auto right = std::min(to, SIDE_BORDER_CELLS);

    if (left < right)
    {
        auto colour = IO::State().clut[BORDER_COLOUR(IO::State().border)];
        memset(pLine + ((left - s_view_left) << 4), colour, (right - left) << 4);
    }
}

void RightBorder(uint8_t* pLine, int from, int to)
{
    auto left = std::max((GFX_WIDTH_CELLS - SIDE_BORDER_CELLS), from);
    auto right = std::min(to, s_view_right);

    if (left < right)
    {
        auto colour = IO::State().clut[BORDER_COLOUR(IO::State().border)];
        memset(pLine + ((left - s_view_left) << 4), colour, (right - left) << 4);
    }
}

void BorderLine(uint8_t* pLine, int from, int to)
{
    auto left = std::max(s_view_left, from);
    auto right = std::min(to, s_view_right);

    if (left < right)
    {
        auto colour = IO::State().clut[BORDER_COLOUR(IO::State().border)];
        memset(pLine + ((left - s_view_left) << 4), colour, (right - left) << 4);
    }
}

void BlackLine(uint8_t* pLine, int from, int to)
{
    auto left = std::max(s_view_left, from);
    auto right = std::min(to, s_view_right);

    if (left < right)
        memset(pLine + ((left - s_view_left) << 4), 0, (right - left) << 4);
}

void Mode1Line(uint8_t* pLine, int line, int from, int to)
{
    const auto& clut = IO::State().clut;
    line -= TOP_BORDER_LINES;

    LeftBorder(pLine, from, to);

    auto left = std::max(SIDE_BORDER_CELLS, from);
    auto right = std::min(to, SIDE_BORDER_CELLS + GFX_SCREEN_CELLS);

    if (left < right)
    {
        auto pFrame = pLine + ((left - s_view_left) << 4);
        auto pDataMem = display_mem + g_awMode1LineToByte[line] + (left - SIDE_BORDER_CELLS);
        auto pAttrMem = display_mem + MODE12_DATA_BYTES + ((line & 0xf8) << 2) + (left - SIDE_BORDER_CELLS);

        for (auto i = left; i < right; i++)
        {
            auto data = *pDataMem++;
            auto attr = *pAttrMem++;
            auto ink_idx = attr_fg(attr);
            auto paper_idx = attr_bg(attr);

            if (IO::flash_phase && (attr & 0x80))
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

void Mode2Line(uint8_t* pLine, int line, int from, int to)
{
    const auto& clut = IO::State().clut;
    line -= TOP_BORDER_LINES;

    LeftBorder(pLine, from, to);

    auto left = std::max(SIDE_BORDER_CELLS, from);
    auto right = std::min(to, SIDE_BORDER_CELLS + GFX_SCREEN_CELLS);

    if (left < right)
    {
        auto pFrame = pLine + ((left - s_view_left) << 4);
        auto pDataMem = display_mem + (line << 5) + (left - SIDE_BORDER_CELLS);
        auto pAttrMem = pDataMem + MODE2_ATTR_OFFSET;

        for (auto i = left; i < right; i++)
        {
            auto data = *pDataMem++;
            auto attr = *pAttrMem++;
            auto ink_idx = attr_fg(attr);
            auto paper_idx = attr_bg(attr);

            if (IO::flash_phase && (attr & 0x80))
                std::swap(ink_idx, paper_idx);

            auto ink = clut[ink_idx];
            auto paper = IO::State().clut[paper_idx];

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

void Mode3Line(uint8_t* pLine, int line, int from, int to)
{
    line -= TOP_BORDER_LINES;

    LeftBorder(pLine, from, to);

    auto left = std::max(SIDE_BORDER_CELLS, from);
    auto right = std::min(to, SIDE_BORDER_CELLS + GFX_SCREEN_CELLS);

    if (left < right)
    {
        auto pFrame = pLine + ((left - s_view_left) << 4);
        auto pMem = display_mem + (line << 7) + ((left - SIDE_BORDER_CELLS) << 2);

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

void Mode4Line(uint8_t* pLine, int line, int from, int to)
{
    const auto& clut = IO::State().clut;
    line -= TOP_BORDER_LINES;

    LeftBorder(pLine, from, to);

    auto left = std::max(SIDE_BORDER_CELLS, from);
    auto right = std::min(to, SIDE_BORDER_CELLS + GFX_SCREEN_CELLS);

    if (left < right)
    {
        auto pFrame = pLine + ((left - s_view_left) << 4);
        auto pMem = display_mem + ((left - SIDE_BORDER_CELLS) << 2) + (line << 7);

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

void ModeArtefact(uint8_t* pLine, int line, int cell, uint8_t new_vmpr)
{
    int screen_line = line - TOP_BORDER_LINES;
    uint8_t ab[4];

    // Fetch the 4 display data bytes for the original mode
    auto [b0, b1, b2, b3] = GetAsicData();

    // Perform the necessary massaging the ASIC does to prepare for display
    if (IO::ScreenMode3or4())
    {
        ab[0] = ab[1] =
            ((b0 >> 0) & 0x80) | ((b0 << 3) & 0x40) |
            ((b1 >> 2) & 0x20) | ((b1 << 1) & 0x10) |
            ((b2 >> 4) & 0x08) | ((b2 >> 1) & 0x04) |
            ((b3 >> 6) & 0x02) | ((b3 >> 0) & 0x01);
        ab[2] = ab[3] = b2;
    }
    else
    {
        ab[0] = (b0 & 0x77) | ((b0 << 0) & 0x80) | ((b0 >> 3) & 0x08);
        ab[1] = (b1 & 0x77) | ((b1 << 2) & 0x80) | ((b1 >> 1) & 0x08);
        ab[2] = (b2 & 0x77) | ((b0 << 4) & 0x80) | ((b0 << 1) & 0x08);
        ab[3] = (b3 & 0x77) | ((b1 << 6) & 0x80) | ((b1 << 3) & 0x08);
    }

    // The target mode decides how the data actually appears in the transition block
    switch (new_vmpr & VMPR_MODE_MASK)
    {
    case VMPR_MODE_1:
    {
        auto pData = display_mem + g_awMode1LineToByte[screen_line] + (cell - SIDE_BORDER_CELLS);
        auto pAttr = display_mem + MODE12_DATA_BYTES + ((screen_line & 0xf8) << 2) + (cell - SIDE_BORDER_CELLS);
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

    case VMPR_MODE_2:
    {
        auto pData = display_mem + (screen_line << 5) + (cell - SIDE_BORDER_CELLS);
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
        auto pb = display_mem + (screen_line << 7) + ((cell - SIDE_BORDER_CELLS) << 2);
        auto pdw = reinterpret_cast<uint32_t*>(pb);
        auto dw = *pdw;

        // TODO: avoid temporary write
        pb[0] = ab[0];
        pb[1] = ab[1];
        pb[2] = ab[2];
        pb[3] = ab[3];

        if ((new_vmpr & VMPR_MODE_MASK) == VMPR_MODE_3)
            Mode3Line(pLine, line, cell, cell + 1);
        else
            Mode4Line(pLine, line, cell, cell + 1);

        *pdw = dw;
        break;
    }
    }
}

void BorderArtefact(uint8_t* pLine, int /*line*/, int cell, uint8_t new_border)
{
    auto pFrame = pLine + ((cell - s_view_left) << 4);

    // Part of the first pixel is the previous border colour, from when the screen was disabled.
    // We don't have the resolution to show only part, but using the most significant colour bits
    // in the least significant position will reduce the intensity enough to be close
    pFrame[0] = IO::State().clut[BORDER_COLOUR(IO::State().border)] >> 4;

    pFrame[1] = pFrame[2] = pFrame[3] =
        pFrame[4] = pFrame[5] = pFrame[6] = pFrame[7] =
        pFrame[8] = pFrame[9] = pFrame[10] = pFrame[11] =
        pFrame[12] = pFrame[13] = pFrame[14] = pFrame[15] = IO::State().clut[BORDER_COLOUR(new_border)];
}

} // namespace Frame
