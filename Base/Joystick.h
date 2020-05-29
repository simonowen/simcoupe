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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#pragma once

const int MAX_JOYSTICKS = 2;

namespace Joystick
{
void Init(bool fFirstInit_ = false);
void Exit(bool fReInit_ = false);

void SetX(int nJoystick_, int nPosition_);
void SetY(int nJoystick_, int nPosition_);
void SetPosition(int nJoystick_, int nPosition_);
void SetButton(int nJoystick_, int nButton_, bool fPressed_);
void SetButtons(int nJoystick_, uint32_t dwButtons_);

uint8_t ReadSinclair1(int nJoystick_);
uint8_t ReadSinclair2(int nJoystick_);
uint8_t ReadKempston(int nJoystick_);
}

enum { jtNone, jtJoystick1, jtJoystick2, jtKempston };
enum eHostJoy { HJ_CENTRE = 0, HJ_LEFT = 1, HJ_RIGHT = 2, HJ_UP = 4, HJ_DOWN = 8, HJ_FIRE = 16 };
