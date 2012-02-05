// Part of SimCoupe - A SAM Coupe emulator
//
// Video.h: Win32 core video functionality using DirectDraw
//
//  Copyright (c) 1999-2012 Simon Owen
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

#ifndef VIDEO_H
#define VIDEO_H

class Video
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

        static void Update ();
        static void UpdatePalette ();
        static bool CreatePalettes ();
};


const int PALETTE_OFFSET = 10;      // Offset into physical palette for first SAM colour (Windows uses the first and last 10)

extern DWORD aulPalette[], aulScanline[];
extern LPDIRECTDRAWSURFACE pddsPrimary, pddsFront, pddsBack;
extern LPDIRECTDRAW pdd;

#endif
