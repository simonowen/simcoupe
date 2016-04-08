// Part of SimCoupe - A SAM Coupe emulator
//
// Video.h: Allegro video handling for surfaces, screen modes, palettes etc.
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef VIDEO_H
#define VIDEO_H

class Video
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

        static void Update ();
        static bool CreatePalettes (bool fDimmed_=false);
};


extern DWORD aulPalette[], aulScanline[];
extern BITMAP *pBack, *pFront;

#endif  // VIDEO_H
