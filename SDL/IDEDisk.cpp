// Part of SimCoupe - A SAM Coupe emulator
//
// IDEDisk.cpp: Platform-specific IDE direct disk access
//
//  Copyright (c) 2003-2006 Simon Owen
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


bool CDeviceHardDisk::Open ()
{
    m_hDevice = open(m_pszDisk, O_EXCL|O_RDWR);

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
            // Extract the disk geometry and size in sectors, and calculate a suitable CHS to report
            // We round down to the nearest 1K to fix a single sector error with some CF card readers
            m_sGeometry.uTotalSectors = nSize & ~1;
            CalculateGeometry(&m_sGeometry);

            // Clear any existing identity data
            DEVICEIDENTITY *pdi = reinterpret_cast<DEVICEIDENTITY*>(m_abIdentity);
            memset(&m_abIdentity, 0, sizeof(m_abIdentity));

            // Fill the identity structure as appropriate
            ATAPUT(pdi->wCaps, 0x2241);                              // Fixed device, motor control, hard sectored, <= 5Mbps
            ATAPUT(pdi->wLogicalCylinders, m_sGeometry.uCylinders);
            ATAPUT(pdi->wLogicalHeads, m_sGeometry.uHeads);
            ATAPUT(pdi->wBytesPerTrack, m_sGeometry.uSectors << 9);
            ATAPUT(pdi->wBytesPerSector, 1 << 9);
            ATAPUT(pdi->wSectorsPerTrack, m_sGeometry.uSectors);
            ATAPUT(pdi->wControllerType, 1);                         // Single port, single sector
            ATAPUT(pdi->wBufferSize512, 1);
            ATAPUT(pdi->wLongECCBytes, 4);

            CHardDisk::SetIdentityString(pdi->szSerialNumber, sizeof(pdi->szSerialNumber), "100");
            CHardDisk::SetIdentityString(pdi->szFirmwareRev,  sizeof(pdi->szFirmwareRev), "1.0");
            CHardDisk::SetIdentityString(pdi->szModelNumber,  sizeof(pdi->szModelNumber), "SAM IDE Device");

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
bool CDeviceHardDisk::Open () { return false; }
void CDeviceHardDisk::Close () { }
bool CDeviceHardDisk::ReadSector (UINT, BYTE*) { return false; }
bool CDeviceHardDisk::WriteSector (UINT, BYTE*) { return false; }

#endif
