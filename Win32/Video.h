// Part of SimCoupe - A SAM Coupé emulator
//
// Video.h: Win32 core video functionality using DirectDraw
//
//  Copyright (c) 1999-2001  Simon Owen
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

namespace Video
{
    bool Init (bool fFirstInit_=false);
    void Exit (bool fReInit_=false);

    void Update ();
    void UpdatePalette ();
    bool CreatePalettes (bool fDimmed_=false);
}


const int PALETTE_OFFSET = 10;		// Offset into palette to start from (we need to leave the first 10 for Windows' use)

extern DWORD aulPalette[N_PALETTE_COLOURS];
extern WORD awY[N_PALETTE_COLOURS], awU[N_PALETTE_COLOURS], awV[N_PALETTE_COLOURS];
extern LPDIRECTDRAWSURFACE pddsPrimary, pddsFront, pddsBack;

class CFrame;
extern CFrame* g_pFrame;

#endif  // VIDEO_H
