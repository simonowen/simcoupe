// Part of SimCoupe - A SAM Coupe emulator
//
// Joystick.h: Common joystick handling
//
//  Copyright (c) 1999-2011  Simon Owen
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

#ifndef JOYSTICK_H
#define JOYSTICK_H

const int MAX_JOYSTICKS = 2;

namespace Joystick
{
    void Init (bool fFirstInit_=false);
    void Exit (bool fReInit_=false);

    void SetX (int nJoystick_, int nPosition_);
    void SetY (int nJoystick_, int nPosition_);
    void SetPosition (int nJoystick_, int nPosition_);
    void SetButton (int nJoystick_, int nButton_, bool fPressed_);
    void SetButtons (int nJoystick_, DWORD dwButtons_);

    BYTE ReadSinclair1 (int nJoystick_);
    BYTE ReadSinclair2 (int nJoystick_);
    BYTE ReadKempston (int nJoystick_);
}

enum { jtNone, jtJoystick1, jtJoystick2, jtKempston };
enum eHostJoy { HJ_CENTRE=0, HJ_LEFT=1, HJ_RIGHT=2, HJ_UP=4, HJ_DOWN=8, HJ_FIRE=16 };

#endif
