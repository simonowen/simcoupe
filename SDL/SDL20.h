// Part of SimCoupe - A SAM Coupe emulator
//
// SDL20.h: Hardware accelerated textures for SDL 2.0
//
//  Copyright (c) 1999-2014 Simon Owen
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

#ifdef HAVE_LIBSDL2

#include "Video.h"

struct SDLRendererDeleter { void operator()(SDL_Renderer* renderer) { SDL_DestroyRenderer(renderer); } };
using unique_sdl_renderer = unique_resource<SDL_Renderer*, nullptr, SDLRendererDeleter>;

struct SDLWindowDeleter { void operator()(SDL_Window* window) { SDL_DestroyWindow(window); } };
using unique_sdl_window = unique_resource<SDL_Window*, nullptr, SDLWindowDeleter>;

struct SDLTextureDeleter { void operator()(SDL_Texture* texture) { SDL_DestroyTexture(texture); } };
using unique_sdl_texture = unique_resource<SDL_Texture*, nullptr, SDLTextureDeleter>;

struct SDLPaletteDeleter { void operator()(SDL_Palette* palette) { SDL_FreePalette(palette); } };
using unique_sdl_palette = unique_resource<SDL_Palette*, nullptr, SDLPaletteDeleter>;


class SDLTexture final : public IVideoBase
{
public:
    SDLTexture();
    ~SDLTexture();

public:
    bool Init() override;
    Rect DisplayRect() const override;
    void ResizeWindow(int height) const override;
    std::pair<int, int> MouseRelative() override;
    void OptionsChanged() override;
    void Update(const FrameBuffer& fb) override;

protected:
    void UpdatePalette();
    void ResizeSource(int width, int height);
    void ResizeTarget(int width, int height);
    void ResizeIntermediate(bool smooth);
    bool DrawChanges(const FrameBuffer& fb);
    void Render();
    void SaveWindowPosition();
    void RestoreWindowPosition();

private:
    unique_sdl_window m_window;
    unique_sdl_renderer m_renderer;
    unique_sdl_texture m_screen_texture;
    unique_sdl_texture m_scaled_texture;
    unique_sdl_texture m_composed_texture;
    unique_sdl_texture m_prev_composed_texture;
    unique_sdl_palette m_palette_texture;

    SDL_Rect m_rSource{};
    SDL_Rect m_rTarget{};
    SDL_Rect m_rDisplay{};

    bool m_smooth{ true };

    int m_int_scale{ 1 };
};

#endif // HAVE_LIBSDL2
