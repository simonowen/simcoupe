// Part of SimCoupe - A SAM Coupe emulator
//
// Tape.h: Tape handling
//
//  Copyright (c) 1999-2015 Simon Owen
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

namespace Tape
{
bool IsRecognised(const std::string& filepath);
bool IsPlaying();
bool IsInserted();

std::string GetPath();
std::string GetFile();
#ifdef HAVE_LIBSPECTRUM
libspectrum_tape* GetTape();
std::string GetBlockDetails(libspectrum_tape_block* block);
#endif

bool Insert(const std::string& filepath);
void Eject();
void Play();
void Stop();

void NextEdge(uint32_t dwTime_);
bool LoadTrap();

bool EiHook();
bool RetZHook();
bool InFEHook();
}
