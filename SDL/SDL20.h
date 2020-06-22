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

class SDLTexture final : public IVideoRenderer
{
public:
    SDLTexture();
    SDLTexture(const SDLTexture&) = delete;
    void operator= (const SDLTexture&) = delete;
    ~SDLTexture();

public:
    int GetCaps() const override;
    bool Init() override;

    void Update(const FrameBuffer& fb) override;
    void UpdateSize() override;
    void UpdatePalette() override;

    void DisplayToSamSize(int* pnX_, int* pnY_) override;
    void DisplayToSamPoint(int* pnX_, int* pnY_) override;

protected:
    bool DrawChanges(const FrameBuffer& fb);

private:
    SDL_Window* m_pWindow = nullptr;
    SDL_Renderer* m_pRenderer = nullptr;
    SDL_Texture* m_pTexture = nullptr;

    int m_nDepth = 0;
    bool m_fFilter = false;

    SDL_Rect m_rTarget{};
};

#endif // HAVE_LIBSDL2
