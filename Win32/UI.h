// Part of SimCoupe - A SAM Coupe emulator
//
// UI.h: Win32 user interface
//
//  Copyright (c) 1999-2005  Simon Owen
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

#ifndef UI_H
#define UI_H

class UI
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit=false);

        static bool CheckEvents ();
        static bool DoAction (int nAction_, bool fPressed_=true);
        static void ShowMessage (eMsgType eType_, const char* pszMessage_);
        static void ResizeWindow (bool fUseOption_=false);
};


// Some bits needed by other modules
extern bool g_fActive;
extern HWND g_hwnd;
extern HANDLE g_hEvent;
extern HINSTANCE __hinstance;

#endif  // UI_H
