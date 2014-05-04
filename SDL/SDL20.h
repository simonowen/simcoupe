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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef SDL20_H
#define SDL20_H

#ifdef USE_SDL2

#include "Video.h"

class SDLTexture : public VideoBase
{
    public:
        SDLTexture ();
        ~SDLTexture ();

    public:
        int GetCaps () const;
        bool Init (bool fFirstInit_);

        void Update (CScreen* pScreen_, bool *pafDirty_);
        void UpdateSize ();
        void UpdatePalette ();

        void DisplayToSamSize (int* pnX_, int* pnY_);
        void DisplayToSamPoint (int* pnX_, int* pnY_);

    protected:
        bool DrawChanges (CScreen* pScreen_, bool *pafDirty_);

    private:
        SDL_Window *m_pWindow;
        SDL_Renderer *m_pRenderer;
        SDL_Texture *m_pTexture, *m_pScanlineTexture;

        int m_nDepth;
        bool m_fFilter;

        SDL_Rect m_rTarget;
};

#endif // USE_SDL2

#endif // SDL20_H
