// Part of SimCoupe - A SAM Coupe emulator
//
// WAV.h: WAV audio recording
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

#ifndef WAV_H
#define WAV_H

namespace WAV
{
bool Start(bool fSegment_ = false);
void Stop();
void Toggle(bool fSegment_ = false);
bool IsRecording();

void AddFrame(const BYTE* pb_, int nLen_);
}

#endif // WAV_H
