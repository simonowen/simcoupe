// Part of SimCoupe - A SAM Coupé emulator
//
// Mouse.h: Mouse interface
//
//  Copyright (c) 1996-2001  Allan Skillman
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

#ifndef MOUSE_H
#define MOUSE_H

class Mouse
{
    public:
        static void Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

        static BYTE Read (DWORD dwTime_);
        static void Move (int nDeltaX_, int nDeltaY_);
        static void SetButton (int nButton_, bool fPressed_=true);
};

#endif
