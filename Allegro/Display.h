// Part of SimCoupe - A SAM Coupe emulator
//
// Display.h: Allegro display rendering
//
//  Copyright (c) 1999-2002  Simon Owen
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

#ifndef DISPLAY_H
#define DISPLAY_H

#include "CScreen.h"
#include "Video.h"


class Display
{
    public:
        typedef struct { int x,y, w,h; } RECT;

    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

        static void SetLineDirty (int nLine_) { pafDirty[nLine_] = true; }
        static void SetDirty ();

        static void Update (CScreen* pScreen_);

        static void DisplayToSamSize (int* pnX_, int* pnY_);
        static void SamToDisplaySize (int* pnX_, int* pnY_);
        static void DisplayToSamPoint (int* pnX_, int* pnY_);
        static void SamToDisplayPoint (int* pnX_, int* pnY_);

        static bool* pafDirty;
};

#endif  // DISPLAY_H
