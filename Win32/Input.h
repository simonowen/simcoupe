// Part of SimCoupe - A SAM Coupé emulator
//
// Input.h: Win32 input using DirectInput
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

#ifndef INPUT_H
#define INPUT_H

#include <dinput.h>

namespace Input
{
    bool Init (bool fFirstInit_=false);
    void Exit (bool fReInit_=false);

    void Acquire (bool fAcquireKeyboard_=true, bool fAcquireMouse_=true);
    void Purge (bool fKeyboard_=true, bool fMouse_=true);

    void Update ();


    // Also needed by Win32 UI.cpp
    BOOL CALLBACK EnumJoystickProc (LPCDIDEVICEINSTANCE pdiDevice_, LPVOID lpv_);
}

// Also needed by Win32 UI.cpp
extern LPDIRECTINPUT pdi;

#endif
