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


static void SetIdentityString (char* psz_, int nSize_, const char* pcsz_)
{
    // Copy the string, padded with spaces and not NULL terminated
    memset(psz_, ' ', nSize_);
    memcpy(psz_, pcsz_, strlen(pcsz_));

    // Byte-swap the string
    for (int i = 0 ; i < nSize_ ; i += 2)
    {
        char t = psz_[i];
        psz_[i] = psz_[i+1];
        psz_[i+1] = t;
    }
}


bool CDeviceHardDisk::Open (const char* pcszDisk_)
{
    m_hDevice = CreateFile(pcszDisk_, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, NULL, NULL);

    if (!IsOpen())
    {
        if (GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND)
            TRACE("Failed to open %s (%#08lx)\n", pcszDisk_, GetLastError());
    }
    else
    {
        DWORD dwRet;
        DISK_GEOMETRY dg;
        PARTITION_INFORMATION pi;

        // Read the drive geometry (possibly fake) and size, checking for a disk device
        if (DeviceIoControl(m_hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &dg, sizeof dg, &dwRet, NULL) &&
            DeviceIoControl(m_hDevice, IOCTL_DISK_GET_PARTITION_INFO, NULL, 0, &pi, sizeof pi, &dwRet, NULL))
        {
            // Extract the disk geometry and size in sectors, and normalise to CHS
            m_sGeometry.uCylinders = static_cast<UINT>(dg.Cylinders.QuadPart);
            m_sGeometry.uHeads = dg.TracksPerCylinder;
            m_sGeometry.uSectors = dg.SectorsPerTrack;
            m_sGeometry.uTotalSectors = static_cast<UINT>(pi.PartitionLength.QuadPart/dg.BytesPerSector);
            NormaliseGeometry(&m_sGeometry);

            // Fill the identity structure as appropriate
            memset(&m_sIdentity, 0, sizeof m_sIdentity);
            m_sIdentity.wCaps = 0x2241;         // Fixed device, motor control, hard sectored, <= 5Mbps
            m_sIdentity.wLogicalCylinders = m_sGeometry.uCylinders;
            m_sIdentity.wLogicalHeads = m_sGeometry.uHeads;
            m_sIdentity.wBytesPerTrack = m_sGeometry.uSectors << 9;
            m_sIdentity.wBytesPerSector = 1 << 9;
            m_sIdentity.wSectorsPerTrack = m_sGeometry.uSectors;
            m_sIdentity.wControllerType = 1;    // Single port, single sector
            m_sIdentity.wBufferSize512 = m_sIdentity.wBytesPerSector >> 9;
            m_sIdentity.wLongECCBytes = 4;

            SetIdentityString(m_sIdentity.szSerialNumber, sizeof m_sIdentity.szSerialNumber, "090");
            SetIdentityString(m_sIdentity.szFirmwareRev,  sizeof m_sIdentity.szFirmwareRev, "0.90");
            SetIdentityString(m_sIdentity.szModelNumber,  sizeof m_sIdentity.szModelNumber, "SAM IDE Device");

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
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
}

bool CDeviceHardDisk::ReadSector (UINT uSector_, BYTE* pb_)
{
    LARGE_INTEGER liOffset = { uSector_<<9 };
    DWORD dwLow = static_cast<DWORD>(liOffset.QuadPart & 0xffffffff), dwSize = 1<<9, dwRead;
    LONG lHigh = static_cast<LONG>(liOffset.QuadPart >> 32);

    return SetFilePointer(m_hDevice, dwLow, &lHigh, FILE_BEGIN) != 0xffffffff &&
            ReadFile(m_hDevice, pb_, dwSize, &dwRead, NULL) && dwRead == dwSize;
}

bool CDeviceHardDisk::WriteSector (UINT uSector_, BYTE* pb_)
{
    LARGE_INTEGER liOffset = { uSector_<<9 };
    DWORD dwLow = static_cast<DWORD>(liOffset.QuadPart & 0xffffffff), dwSize = 1<<9, dwWritten;
    LONG lHigh = static_cast<LONG>(liOffset.QuadPart >> 32);

    return SetFilePointer(m_hDevice, dwLow, &lHigh, FILE_BEGIN) != 0xffffffff &&
            WriteFile(m_hDevice, pb_, dwSize, &dwWritten, NULL) && dwWritten == dwSize;
}
