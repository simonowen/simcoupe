// Part of SimCoupe - A SAM Coupe emulator
//
// SDL20.cpp: Hardware accelerated textures for SDL 2.0
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
#include "SDL20.h"

#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "UI.h"

#ifdef HAVE_LIBSDL2

static uint32_t aulPalette[N_PALETTE_COLOURS];

SDLTexture::SDLTexture()
{
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");

#ifdef SDL_VIDEO_FULLSCREEN_SPACES
    SDL_SetHint(SDL_VIDEO_FULLSCREEN_SPACES, "1");
#endif

    if (!Init())
        throw std::runtime_error("SDL initialisation failed");
}

SDLTexture::~SDLTexture()
{
    SaveWindowPosition();
}

Rect SDLTexture::DisplayRect() const
{
    return { m_rDisplay.x, m_rDisplay.y, m_rDisplay.w, m_rDisplay.h };
}

bool SDLTexture::Init()
{
    int width = Frame::Width();
    int height = Frame::Height();

    Uint32 flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
    m_pWindow.reset(
        SDL_CreateWindow(
            WINDOW_CAPTION,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            width * 2,
            height * 2,
            flags));

    if (!m_pWindow)
        return false;

    SDL_SetWindowMinimumSize(m_pWindow.get(), width / 2, height / 2);

    m_pRenderer.reset(
        SDL_CreateRenderer(m_pWindow.get(), -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE));

    if (!m_pRenderer)
        return false;

    OptionsChanged();
    RestoreWindowPosition();
    SDL_ShowWindow(m_pWindow.get());

    return true;
}


void SDLTexture::OptionsChanged()
{
    uint8_t fill_intensity = GetOption(blackborder) ? 0 : 25;
    SDL_SetRenderDrawColor(m_pRenderer.get(), fill_intensity, fill_intensity, fill_intensity, 0xff);

    m_rSource.w = m_rSource.h = 0;
    m_rTarget.w = m_rTarget.h = 0;
}

void SDLTexture::Update(const FrameBuffer& fb)
{
    if (DrawChanges(fb))
        Render();
}

void SDLTexture::ResizeWindow(int height) const
{
    auto width = static_cast<float>(height)* Frame::Width() / Frame::Height();
    if (GetOption(ratio5_4))
        width *= 1.25f;

    SDL_SetWindowSize(m_pWindow.get(), static_cast<int>(width + 0.5f), height);
}

std::pair<int, int> SDLTexture::MouseRelative()
{
    SDL_Point mouse{};
    SDL_GetMouseState(&mouse.x, &mouse.y);

    SDL_Point centre{ m_rTarget.w / 2, m_rTarget.h / 2 };
    auto dx = mouse.x - centre.x;
    auto dy = mouse.y - centre.y;

    auto pix_x = static_cast<float>(m_rDisplay.w) / Frame::Width() * 2;
    auto pix_y = static_cast<float>(m_rDisplay.h) / Frame::Height() * 2;

    auto dx_sam = static_cast<int>(dx / pix_x);
    auto dy_sam = static_cast<int>(dy / pix_y);

    if (dx_sam || dy_sam)
    {
        auto x_remain = static_cast<int>(std::fmod(dx, pix_x));
        auto y_remain = static_cast<int>(std::fmod(dy, pix_y));
        SDL_WarpMouseInWindow(nullptr, centre.x + x_remain, centre.y + y_remain);
    }

    return { dx_sam, dy_sam };
}

void SDLTexture::UpdatePalette()
{
    if (!m_pTexture)
        return;

    int bpp;
    Uint32 format, r_mask, g_mask, b_mask, a_mask;
    SDL_QueryTexture(m_pTexture.get(), &format, nullptr, nullptr, nullptr);
    SDL_PixelFormatEnumToMasks(format, &bpp, &r_mask, &g_mask, &b_mask, &a_mask);

    auto palette = IO::Palette();
    for (size_t i = 0; i < palette.size(); ++i)
    {
        auto& colour = palette[i];
        aulPalette[i] = RGB2Native(
            colour.red, colour.green, colour.blue, 0xff,
            r_mask, g_mask, b_mask, a_mask);
    }
}

bool SDLTexture::DrawChanges(const FrameBuffer& fb)
{
    bool is_fullscreen = (SDL_GetWindowFlags(m_pWindow.get()) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    if (is_fullscreen != GetOption(fullscreen))
    {
        SDL_SetWindowFullscreen(
            m_pWindow.get(),
            GetOption(fullscreen) ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    }

    int width = fb.Width();
    int height = fb.Height();

    int window_width{};
    int window_height{};
    SDL_GetWindowSize(m_pWindow.get(), &window_width, &window_height);

    bool smooth = !GUI::IsActive() && GetOption(smooth);
    bool smooth_changed = smooth != m_smooth;
    bool source_changed = (width != m_rSource.w) || (height != m_rSource.h);
    bool target_changed = window_width != m_rTarget.w || window_height != m_rTarget.h;

    if (source_changed)
        ResizeSource(width, height);

    if (target_changed)
        ResizeTarget(window_width, window_height);

    if (source_changed || target_changed || smooth_changed)
        ResizeIntermediate(smooth);

    if (!m_pTexture)
        return false;

    int texture_pitch = 0;
    uint8_t* pTexture = nullptr;
    if (SDL_LockTexture(m_pTexture.get(), nullptr, (void**)&pTexture, &texture_pitch) != 0)
        return false;

    int width_cells = width / GFX_PIXELS_PER_CELL;
    auto pLine = fb.GetLine(0);
    long line_pitch = fb.Width();

    for (int y = 0; y < height; ++y)
    {
        auto pdw = reinterpret_cast<uint32_t*>(pTexture);
        auto pb = pLine;

        for (int x = 0; x < width_cells; ++x)
        {
            for (int i = 0; i < GFX_PIXELS_PER_CELL; ++i)
                pdw[i] = aulPalette[pb[i]];

            pdw += GFX_PIXELS_PER_CELL;
            pb += GFX_PIXELS_PER_CELL;

        }

        pTexture += texture_pitch;
        pLine += line_pitch;
    }

    SDL_UnlockTexture(m_pTexture.get());
    return true;
}

void SDLTexture::Render()
{
    SDL_Rect rScaledTexture{};
    SDL_QueryTexture(m_pScaledTexture.get(), nullptr, nullptr, &rScaledTexture.w, &rScaledTexture.h);

    // Integer scale original image using point sampling.
    SDL_SetRenderTarget(m_pRenderer.get(), m_pScaledTexture.get());
    SDL_RenderCopy(m_pRenderer.get(), m_pTexture.get(), nullptr, nullptr);

    // Draw to window with remaining scaling using linear sampling.
    SDL_SetRenderTarget(m_pRenderer.get(), nullptr);
    SDL_RenderClear(m_pRenderer.get());
    SDL_RenderCopy(m_pRenderer.get(), m_pScaledTexture.get(), nullptr, &m_rDisplay);
    SDL_RenderPresent(m_pRenderer.get());
}

void SDLTexture::ResizeSource(int source_width, int source_height)
{
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    m_pTexture.reset(
        SDL_CreateTexture(
            m_pRenderer.get(),
            SDL_PIXELFORMAT_UNKNOWN,
            SDL_TEXTUREACCESS_STREAMING,
            source_width,
            source_height));

    UpdatePalette();

    m_rSource.w = source_width;
    m_rSource.h = source_height;
}

void SDLTexture::ResizeTarget(int target_width, int target_height)
{
    int width = Frame::Width();
    int height = Frame::Height();
    if (GetOption(ratio5_4)) width = width * 5 / 4;

    int width_fit = width * target_height / height;
    int height_fit = height * target_width / width;

    if (width_fit <= target_width)
    {
        width = width_fit;
        height = target_height;
    }
    else if (height_fit <= target_height)
    {
        width = target_width;
        height = height_fit;
    }

    m_rDisplay.x = (target_width - width) / 2;
    m_rDisplay.y = (target_height - height) / 2;
    m_rDisplay.w = width;
    m_rDisplay.h = height;

    m_rTarget.w = target_width;
    m_rTarget.h = target_height;
}

void SDLTexture::ResizeIntermediate(bool smooth)
{
    int width_scale = (m_rTarget.w + (m_rSource.w - 1)) / m_rSource.w;
    int height_scale = (m_rTarget.h + (m_rSource.h - 1)) / m_rSource.h;

    if (smooth)
    {
        width_scale = 1;
        height_scale = 2;
    }

    int width = m_rSource.w * width_scale;
    int height = m_rSource.h * height_scale;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    m_pScaledTexture.reset(
        SDL_CreateTexture(
            m_pRenderer.get(),
            SDL_PIXELFORMAT_UNKNOWN,
            SDL_TEXTUREACCESS_TARGET,
            width,
            height));

    m_smooth = smooth;
}

void SDLTexture::SaveWindowPosition()
{
    SDL_SetWindowFullscreen(m_pWindow.get(), 0);

    auto window_flags = SDL_GetWindowFlags(m_pWindow.get());
    auto maximised = (window_flags & SDL_WINDOW_MAXIMIZED) ? 1 : 0;
    SDL_RestoreWindow(m_pWindow.get());

    int x, y, width, height;
    SDL_GetWindowPosition(m_pWindow.get(), &x, &y);
    SDL_GetWindowSize(m_pWindow.get(), &width, &height);

    SetOption(windowpos, fmt::format("{},{},{},{},{}", x, y, width, height, maximised).c_str());
}

void SDLTexture::RestoreWindowPosition()
{
    int x, y, width, height, maximised;
    if (sscanf(GetOption(windowpos), "%d,%d,%d,%d,%d", &x, &y, &width, &height, &maximised) == 5)
    {
        SDL_SetWindowPosition(m_pWindow.get(), x, y);
        SDL_SetWindowSize(m_pWindow.get(), width, height);

        if (maximised)
            SDL_MaximizeWindow(m_pWindow.get());
    }
}

#endif // HAVE_LIBSDL2
