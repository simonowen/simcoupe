// Part of SimCoupe - A SAM Coupe emulator
//
// IDEDisk.cpp: Platform-specific IDE direct disk access
//
//  Copyright (c) 2003-2005 Simon Owen
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

#include "SimCoupe.h"
#include "IDEDisk.h"

// Dummy implementation
bool CDeviceHardDisk::Open () { return false; }
void CDeviceHardDisk::Close () { }
bool CDeviceHardDisk::ReadSector (UINT, BYTE*) { return false; }
bool CDeviceHardDisk::WriteSector (UINT, BYTE*) { return false; }
