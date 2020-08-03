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

struct SDLRendererDeleter
{
    using pointer = SDL_Renderer*;
    void operator()(pointer p) { SDL_DestroyRenderer(p); }
};
using unique_sdl_renderer = std::unique_ptr<SDL_Renderer, SDLRendererDeleter>;

struct SDLWindowDeleter
{
    using pointer = SDL_Window*;
    void operator()(pointer p) { SDL_DestroyWindow(p); }
};
using unique_sdl_window = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

struct SDLTextureDeleter
{
    using pointer = SDL_Texture*;
    void operator()(pointer p) { SDL_DestroyTexture(p); }
};
using unique_sdl_texture = std::unique_ptr<SDL_Texture, SDLTextureDeleter>;

struct SDLPaletteDeleter
{
    using pointer = SDL_Palette*;
    void operator()(pointer p) { SDL_FreePalette(p); }
};
using unique_sdl_palette = std::unique_ptr<SDL_Texture, SDLPaletteDeleter>;


class SDLTexture final : public IVideoBase
{
public:
    SDLTexture();
    ~SDLTexture();

public:
    Rect DisplayRect() const override;
    void ResizeWindow(int height) const override;
    std::pair<int, int> MouseRelative() override;
    void OptionsChanged() override;
    void Update(const FrameBuffer& fb) override;

protected:
    bool Init();
    void UpdatePalette();
    void ResizeSource(int width, int height);
    void ResizeTarget(int width, int height);
    void ResizeIntermediate(bool smooth);
    bool DrawChanges(const FrameBuffer& fb);
    void Render();
    void SaveWindowPosition();
    void RestoreWindowPosition();

private:
    unique_sdl_window m_pWindow;
    unique_sdl_renderer m_pRenderer;
    unique_sdl_texture m_pTexture;
    unique_sdl_texture m_pScaledTexture;
    unique_sdl_palette m_paletteTex;

    SDL_Rect m_rSource{};
    SDL_Rect m_rTarget{};
    SDL_Rect m_rDisplay{};

    bool m_smooth{ true };

    int m_int_scale{ 1 };
};

#endif // HAVE_LIBSDL2
