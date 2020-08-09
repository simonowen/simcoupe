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
#include "Memory.h"
#include "Options.h"
#include "OSD.h"
#include "PNG.h"
#include "Sound.h"
#include "SSX.h"
#include "Util.h"
#include "UI.h"

constexpr uint8_t FLOPPY_LED_COLOUR = GREEN_5;
constexpr uint8_t ATOM_LED_COLOUR = RED_6;
constexpr uint8_t ATOMLITE_LED_COLOUR = 89;
constexpr uint8_t LED_OFF_COLOUR = GREY_2;

constexpr auto STATUS_ACTIVE_TIME = std::chrono::milliseconds(2500);
constexpr auto FPS_IN_TURBO_MODE = 5;

int s_view_top, s_view_bottom;
int s_view_left, s_view_right;
bool g_flash_phase;

std::unique_ptr<FrameBuffer> pFrameBuffer;
std::unique_ptr<FrameBuffer> pGuiScreen;
std::unique_ptr<ScreenWriter> pFrame;

static bool draw_frame;
static bool save_png;
static bool save_ssx;

static int last_line, last_cell;
static int num_frames;
static std::chrono::steady_clock::time_point status_time;

static std::string status_text;
static std::string profile_text;
char szScreenPath[MAX_PATH];

struct REGION
{
    int w, h;
}
asViews[] =
{
    { GFX_SCREEN_CELLS, GFX_SCREEN_LINES },
    { GFX_SCREEN_CELLS + 2, GFX_SCREEN_LINES + 20 },
    { GFX_SCREEN_CELLS + 4, GFX_SCREEN_LINES + 48 },
    { GFX_SCREEN_CELLS + 4, GFX_SCREEN_LINES + 74 },
    { GFX_WIDTH_CELLS, GFX_HEIGHT_LINES },
};

namespace Frame
{
static void DrawOSD(FrameBuffer& fb);

bool Init()
{
    Exit();

    unsigned int view_idx = GetOption(borders);
    if (view_idx >= std::size(asViews))
        view_idx = 0;

    s_view_left = (GFX_WIDTH_CELLS - asViews[view_idx].w) >> 1;
    s_view_right = s_view_left + asViews[view_idx].w;

    if ((s_view_top = (GFX_HEIGHT_LINES - asViews[view_idx].h) >> 1))
        s_view_top += (TOP_BORDER_LINES - BOTTOM_BORDER_LINES) >> 1;
    s_view_bottom = s_view_top + asViews[view_idx].h;

    auto width = (s_view_right - s_view_left) * GFX_PIXELS_PER_CELL ;
    auto height = (s_view_bottom - s_view_top);

    pFrameBuffer = std::make_unique<FrameBuffer>(width, height);
    pGuiScreen = std::make_unique<FrameBuffer>(width, height * 2);

    pFrame = std::make_unique<ScreenWriter>();
    pFrame->SetMode(vmpr);

    Flyback();

    return true;
}

void Exit()
{
    GIF::Stop();
    AVI::Stop();

    pFrame.reset();
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

void SetView(int num_cells, int num_lines)
{
    auto view_idx = GetOption(borders);
    if (view_idx < 0 || view_idx >= static_cast<int>(std::size(asViews)))
        view_idx = 0;

    asViews[view_idx].w = num_cells;
    asViews[view_idx].h = num_lines;
}

void Update()
{
    if (!draw_frame)
        return;

    auto [line, line_cycle] = Frame::GetRasterPos(g_dwCycleCounter);
    auto cell = line_cycle / CPU_CYCLES_PER_CELL;

    auto from = std::max(last_line, s_view_top);
    auto to = std::min(line, s_view_bottom - 1);

    if (line == last_line)
    {
        if (cell > last_cell)
        {
            pFrame->UpdateLine(*pFrameBuffer, line, last_cell, cell);
            last_cell = cell;
        }
    }
    else
    {
        if (from <= to)
        {
            if (from == last_line)
            {
                pFrame->UpdateLine(*pFrameBuffer, last_line, last_cell, GFX_WIDTH_CELLS);
                from++;
            }

            if (to == line)
            {
                pFrame->UpdateLine(*pFrameBuffer, line, 0, cell);
                to--;
            }

            for (int i = from; i <= to; ++i)
            {
                pFrame->UpdateLine(*pFrameBuffer, i, 0, GFX_WIDTH_CELLS);
            }
        }

        last_line = line;
        last_cell = cell;
    }
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


void Begin()
{
    display_changed = false;
    num_frames++;
}

void End()
{
    if (draw_frame)
    {
        Update();

        if (GUI::IsActive())
        {
            for (int i = 0; i < Height(); i++)
            {
                auto pLine = pFrameBuffer->GetLine(i >> 1);
                auto width = pFrameBuffer->Width();

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

        Redraw();
    }

    Sync();
}

void Flyback()
{
    last_line = last_cell = 0;

    static uint8_t flash_frame = 0;
    if (!(++flash_frame % MODE12_FLASH_FRAMES))
        g_flash_phase = !g_flash_phase;

    if (!status_text.empty())
    {
        auto now = std::chrono::steady_clock::now();
        if ((now - status_time) > STATUS_ACTIVE_TIME)
            status_text.clear();
    }
}

void Sync()
{
    using namespace std::chrono;
    using namespace std::literals::chrono_literals;

    auto now = high_resolution_clock::now();

    if (GetOption(turbodisk) && (pFloppy1->IsActive() || pFloppy2->IsActive()))
        g_nTurbo |= TURBO_DISK;
    else
        g_nTurbo &= ~TURBO_DISK;

    if (g_nTurbo & TURBO_BOOT)
    {
        draw_frame = GUI::IsActive();
    }
    else if (!GUI::IsActive() && g_nTurbo && !(g_nTurbo & TURBO_KEY))
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

    static std::optional<high_resolution_clock::time_point> last_profiled;
    if ((now - *last_profiled) >= 1s)
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

std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> GetAsicData()
{
    return pFrame->GetAsicData();
}

void ChangeMode(uint8_t new_vmpr)
{
    auto [line, line_cycle] = Frame::GetRasterPos(g_dwCycleCounter);
    auto cell = line_cycle / CPU_CYCLES_PER_CELL;

    if (IsScreenLine(line))
    {
        if (cell < (SIDE_BORDER_CELLS + GFX_SCREEN_CELLS))
        {
            if (((vmpr_mode ^ new_vmpr) & VMPR_MDE1_MASK) && cell >= SIDE_BORDER_CELLS)
            {
                auto pLine = pFrameBuffer->GetLine(line - s_view_top);
                pFrame->ModeChange(pLine, line, cell, new_vmpr);
                last_cell++;
            }
        }
    }

    pFrame->SetMode(new_vmpr);
}

void ChangeScreen(uint8_t new_border)
{
    auto [line, line_cycle] = Frame::GetRasterPos(g_dwCycleCounter);
    auto cell = line_cycle / CPU_CYCLES_PER_CELL;

    if (line >= s_view_top && line < s_view_bottom && cell >= s_view_left && cell < s_view_right)
    {
        auto pLine = pFrameBuffer->GetLine(line - s_view_top);
        pFrame->ScreenChange(pLine, line, cell, new_border);
        last_cell++;
    }
}

void TouchLines(int from, int to)
{
    if (to >= last_line && from <= (int)((g_dwCycleCounter - CPU_CYCLES_PER_SIDE_BORDER) / CPU_CYCLES_PER_LINE))
        Update();
}

} // namespace Frame


void ScreenWriter::SetMode(uint8_t new_vmpr)
{
    int page = (new_vmpr & VMPR_MDE1_MASK) ? (new_vmpr & VMPR_PAGE_MASK & ~1) : new_vmpr & VMPR_PAGE_MASK;
    m_display_mem_offset = PageReadOffset(page);
}

void ScreenWriter::UpdateLine(FrameBuffer& fb, int line, int from, int to)
{
    if (line >= s_view_top && line < s_view_bottom)
    {
        auto pLine = fb.GetLine(line - s_view_top);

        if (BORD_SOFF && VMPR_MODE_3_OR_4)
        {
            BlackLine(pLine, from, to);
        }
        else if (line < TOP_BORDER_LINES || line >= (TOP_BORDER_LINES + GFX_SCREEN_LINES))
        {
            BorderLine(pLine, from, to);
        }
        else
        {
            switch (VMPR_MODE)
            {
            case MODE_1: Mode1Line(pLine, line, from, to); break;
            case MODE_2: Mode2Line(pLine, line, from, to); break;
            case MODE_3: Mode3Line(pLine, line, from, to); break;
            case MODE_4: Mode4Line(pLine, line, from, to); break;
            }
        }
    }
}

std::tuple<uint8_t, uint8_t, uint8_t, uint8_t> ScreenWriter::GetAsicData()
{
    int line = g_dwCycleCounter / CPU_CYCLES_PER_LINE;
    int cell = (g_dwCycleCounter % CPU_CYCLES_PER_LINE) >> 3;

    line -= TOP_BORDER_LINES;
    cell -= SIDE_BORDER_CELLS + SIDE_BORDER_CELLS;

    if (cell < 0) { line--; cell = GFX_SCREEN_CELLS - 1; }
    if (line < 0 || line >= GFX_SCREEN_LINES) { line = GFX_SCREEN_LINES - 1; cell = GFX_SCREEN_CELLS - 1; }

    if (VMPR_MODE_3_OR_4)
    {
        auto pMem = pMemory + m_display_mem_offset + (line * MODE34_BYTES_PER_LINE) + (cell * GFX_DATA_BYTES_PER_CELL);
        return std::make_tuple(pMem[0], pMem[1], pMem[2], pMem[3]);
    }
    else if (VMPR_MODE == MODE_1)
    {
        auto pDataMem = pMemory + m_display_mem_offset + g_awMode1LineToByte[line] + cell;
        auto pAttrMem = pMemory + m_display_mem_offset + MODE12_DATA_BYTES + ((line & 0xf8) * GFX_DATA_BYTES_PER_CELL) + cell;
        return std::make_tuple(*pDataMem, *pDataMem, *pAttrMem, *pAttrMem);
    }
    else
    {
        auto pDataMem = pMemory + m_display_mem_offset + (line << 5) + cell;
        auto pAttrMem = pDataMem + MODE2_ATTR_OFFSET;
        return std::make_tuple(*pDataMem, *pDataMem, *pAttrMem, *pAttrMem);
    }
}
