// Part of SimCoupe - A SAM Coupe emulator
//
// IDEDisk.cpp: Platform-specific IDE direct disk access
//
//  Copyright (c) 2003 Simon Owen
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

#ifdef __linux__

#include <linux/fs.h>
#include <linux/hdreg.h>

bool CDeviceHardDisk::Open (const char* pcszDisk_)
{
    m_hDevice = open(pcszDisk_, O_EXCL|O_RDWR);

    if (IsOpen())
    {
        struct hd_geometry dg;
        int nSize;

        // Read the drive geometry and size
        if (ioctl(m_hDevice, HDIO_GETGEO, &dg) >= 0 && ioctl(m_hDevice, BLKGETSIZE, &nSize) >= 0)
        {
            // Extract the disk geometry and size in sectors
            m_sGeometry.uCylinders = nSize / (dg.heads * dg.sectors);   // Don't use dg.cylinders!
            m_sGeometry.uHeads = dg.heads;
            m_sGeometry.uSectors = dg.sectors;
            m_sGeometry.uTotalSectors = nSize;

            // Changed a possible LBA-adjusted geometry to CHS
            NormaliseGeometry(&m_sGeometry);

            // For safety, only deal with existing BDOS or SDIDE hard disks
            if (IsBDOSDisk() || IsSDIDEDisk())
                return true;
        }
    }

    Close();
    return false;
}

void CDeviceHardDisk::Close ()
{
    if (IsOpen())
    {
        close(m_hDevice);
        m_hDevice = -1;
    }
}

bool CDeviceHardDisk::ReadSector (UINT uSector_, BYTE* pb_)
{
    off_t uOffset = uSector_ << 9, uSize = 1 << 9;
    return lseek(m_hDevice, uOffset, SEEK_SET) == uOffset &&
            read(m_hDevice, pb_, uSize) == uSize;
}

bool CDeviceHardDisk::WriteSector (UINT uSector_, BYTE* pb_)
{
    off_t uOffset = uSector_ << 9, uSize = 1 << 9;
    return lseek(m_hDevice, uOffset, SEEK_SET) == uOffset &&
            write(m_hDevice, pb_, uSize) == uSize;
}

#else

// Dummy implementation for non-Linux SDL versions
bool CDeviceHardDisk::Open (const char*) { return false; }
void CDeviceHardDisk::Close () { }
bool CDeviceHardDisk::ReadSector (UINT, BYTE*) { return false; }
bool CDeviceHardDisk::WriteSector (UINT, BYTE*) { return false; }

#endif
