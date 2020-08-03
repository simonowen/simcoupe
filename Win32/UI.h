// Part of SimCoupe - A SAM Coupe emulator
//
// UI.h: Win32 user interface
//
//  Copyright (c) 1999-2014 Simon Owen
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

#pragma once

#include "Actions.h"
#include "Video.h"

class UI
{
public:
    static bool Init();
    static void Exit();

    static std::unique_ptr<IVideoBase> CreateVideo();
    static bool CheckEvents();

    static bool DoAction(Action action, bool pressed = true);
    static void ShowMessage(MsgType type, const std::string& message);
};

extern HWND g_hwnd;
