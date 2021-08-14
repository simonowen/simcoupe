// Part of SimCoupe - A SAM Coupe emulator
//
// AVI.h: AVI movie recording
//
//  Copyright (c) 1999-2012 Simon Owen
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

#include "FrameBuffer.h"

namespace AVI
{
enum : int { HALFSIZE = 1, FULLSIZE = 0 };

bool Start(int flags);
void Stop();
void Toggle(int flags);
bool IsRecording();

void AddFrame(const FrameBuffer& fb);
void AddFrame(const uint8_t* buffer, unsigned int len);
}
