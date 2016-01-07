// Part of SimCoupe - A SAM Coupe emulator
//
// Input.h: Win32 mouse and DirectInput keyboard input
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

#ifndef INPUT_H
#define INPUT_H

class Input
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

        static void Update ();
        static bool FilterMessage (HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_);

        static bool IsMouseAcquired ();
        static void AcquireMouse (bool fAcquire_);
        static void Purge ();

        static int MapChar (int nChar_, int *pnMods_=nullptr);

        static void FillJoystickCombo (HWND hwndCombo_);    // Used by the Win32 GUI in UI.cpp
};

#endif
