// Part of SimCoupe - A SAM Coupe emulator
//
// IDEDisk.h: Platform-specific IDE direct disk access
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef IDEDISK_H
#define IDEDISK_H

#include "HardDisk.h"

class CDeviceHardDisk : public CHardDisk
{
    public:
        CDeviceHardDisk (const char* pcszDisk_) : CHardDisk(pcszDisk_) { }
        ~CDeviceHardDisk () { }

    public:
        bool Open ();
        void Close ();

        bool ReadSector (UINT uSector_, BYTE* pb_);
        bool WriteSector (UINT uSector_, BYTE* pb_);
};

#endif
