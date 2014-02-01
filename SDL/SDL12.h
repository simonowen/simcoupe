// Part of SimCoupe - A SAM Coupe emulator
//
// SDL12.h: Software surfaces for SDL 1.2
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

#ifndef SDL12_H
#define SDL12_H

#ifndef USE_SDL2

#include "Video.h"

class SDLSurface : public VideoBase
{
    public:
        SDLSurface ();
        ~SDLSurface ();

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
        SDL_Surface *pFront, *pBack, *pIcon;
        int nDesktopWidth, nDesktopHeight;

        SDL_Rect m_rTarget;
};

#endif // !USE_SDL2

#endif // SDL12_H
