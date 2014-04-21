// Part of SimCoupe - A SAM Coupe emulator
//
// IDEDisk.h: Platform-specific IDE direct disk access
//
//  Copyright (c) 2003-2014 Simon Owen
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

#ifndef IDEDISK_H
#define IDEDISK_H

#include "HardDisk.h"

class CDeviceHardDisk : public CHardDisk
{
    public:
        CDeviceHardDisk (const char* pcszDisk_);
        ~CDeviceHardDisk ();

    public:
        bool IsOpen () const { return m_hDevice != INVALID_HANDLE_VALUE; }
        bool Open (bool fReadOnly_=false);
        void Close ();

        bool ReadSector (UINT uSector_, BYTE* pb_);
        bool WriteSector (UINT uSector_, BYTE* pb_);

    protected:
        bool Lock (bool fReadOnly_=false);
        void Unlock ();

    protected:
        HANDLE m_hDevice, m_hLock;
        BYTE *m_pbSector;
        DWORD m_dwDriveLetters; // drive letters on our physical drive
};

#endif
