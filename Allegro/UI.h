// Part of SimCoupe - A SAM Coupe emulator
//
// UI.h: Allegro user interface
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef UI_H
#define UI_H

#include "Util.h"

class UI
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

        static bool CheckEvents ();
        static bool DoAction (int nAction_, bool fPressed_=true);
        static void ShowMessage (eMsgType eType_, const char* pszMessage_);
        static void ResizeWindow (bool fUseOption_=false);

        static void ProcessKey (BYTE bKey_, BYTE bMods_);
};


extern bool g_fActive;

#endif  // UI_H
