// Part of SimCoupe - A SAM Coupe emulator
//
// IDEDisk.cpp: Platform-specific IDE direct disk access
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "SimCoupe.h"
#include "IDEDisk.h"

#if defined(__linux__) || defined(__APPLE__)

// Rather than including the kernel headers, we'll define some values from them
#define HDIO_GETGEO         0x0301      // get device geometry
#define BLKGETSIZE          0x1260      // return device size /512 (long *arg)

// Disk geometry structure returned by HDIO_GETGEO
struct hd_geometry {
    unsigned char heads;
    unsigned char sectors;
    unsigned short cylinders;
    unsigned long start;
};


bool DeviceHardDisk::Open(bool fReadOnly_/*=false*/)
{
    m_hDevice = open(m_strPath.c_str(), O_EXCL | (fReadOnly_ ? O_RDONLY : O_RDWR));
    if (m_hDevice == -1)
        m_hDevice = open(m_strPath.c_str(), O_EXCL | O_RDONLY);

    if (IsOpen())
    {
        int nSize;

        // Read the drive geometry and size
#ifdef __linux__
        if (ioctl(m_hDevice, BLKGETSIZE, &nSize) >= 0)
#else
        long long llSize = 0;
        if (ioctl(m_hDevice, DKIOCGETBLOCKCOUNT, &llSize) >= 0 && (nSize = static_cast<int>(llSize)))
#endif
        {
            // Extract the disk geometry and size in sectors
            // We round down to the nearest 1K to fix a single sector error with some CF card readers
            m_sGeometry.uTotalSectors = nSize & ~1;

            // Generate suitable identify data to report
            SetIdentifyData(nullptr);

            // For safety, only deal with existing BDOS or SDIDE hard disks
            if (IsBDOSDisk() || IsSDIDEDisk())
                return true;
        }
    }

    Close();
    return false;
}

void DeviceHardDisk::Close()
{
    if (IsOpen())
    {
        close(m_hDevice);
        m_hDevice = -1;
    }
}

bool DeviceHardDisk::ReadSector(unsigned int uSector_, uint8_t* pb_)
{
    off_t uOffset = uSector_ << 9, uSize = 1 << 9;
    return lseek(m_hDevice, uOffset, SEEK_SET) == uOffset &&
        read(m_hDevice, pb_, uSize) == uSize;
}

bool DeviceHardDisk::WriteSector(unsigned int uSector_, uint8_t* pb_)
{
    off_t uOffset = uSector_ << 9, uSize = 1 << 9;
    return lseek(m_hDevice, uOffset, SEEK_SET) == uOffset &&
        write(m_hDevice, pb_, uSize) == uSize;
}

#else

// Dummy implementation for non-Linux SDL versions
bool DeviceHardDisk::Open(bool fReadOnly_/*=false*/) { return false; }
void DeviceHardDisk::Close() { }
bool DeviceHardDisk::ReadSector(unsigned int, uint8_t*) { return false; }
bool DeviceHardDisk::WriteSector(unsigned int, uint8_t*) { return false; }

#endif
