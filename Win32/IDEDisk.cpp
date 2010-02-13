// Part of SimCoupe - A SAM Coupe emulator
//
// IDEDisk.cpp: Platform-specific IDE direct disk access
//
//  Copyright (c) 2003-2010 Simon Owen
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


CDeviceHardDisk::CDeviceHardDisk (const char* pcszDisk_)
    : CHardDisk(pcszDisk_), m_hDevice(INVALID_HANDLE_VALUE), m_hLock(INVALID_HANDLE_VALUE)
{
    m_pbSector = (PBYTE)VirtualAlloc(NULL, 1<<9, MEM_COMMIT, PAGE_READWRITE);
}

CDeviceHardDisk::~CDeviceHardDisk ()
{
    Close();

    if (m_pbSector)
        VirtualFree(m_pbSector, 0, MEM_RELEASE);
}


bool CDeviceHardDisk::Open ()
{
    m_hDevice = CreateFile(m_pszDisk, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, NULL, NULL);

    if (!IsOpen())
    {
        if (GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND)
            TRACE("Failed to open %s (%#08lx)\n", m_pszDisk, GetLastError());
    }
    else if (!Lock())
    {
        TRACE("Failed to get exclusive access to %s\n", m_pszDisk);
    }
    else if (!m_pbSector)
    {
        TRACE("Out of memory for CDeviceHardDisk sector buffer\n");
    }
    else
    {
        DWORD dwRet;
        PARTITION_INFORMATION pi;

        // Read the drive geometry (possibly fake) and size, checking for a disk device
        if (DeviceIoControl(m_hDevice, IOCTL_DISK_GET_PARTITION_INFO, NULL, 0, &pi, sizeof pi, &dwRet, NULL))
        {
            // Extract the disk geometry and size in sectors, and calculate a suitable CHS to report
            // We round down to the nearest 1K to fix a single sector error with some CF card readers
            m_sGeometry.uTotalSectors = static_cast<UINT>(pi.PartitionLength.QuadPart >> 9) & ~1U;
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
            ATAPUT(pdi->wControllerType, 1);        // Single port, single sector
            ATAPUT(pdi->wBufferSize512, 1);         // 512 bytes
            ATAPUT(pdi->wLongECCBytes, 4);
            ATAPUT(pdi->wReadWriteMulti, 0);        // no multi-sector handling
            ATAPUT(pdi->wCapabilities, 0x0200);     // LBA supported

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
        Unlock();
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
}

bool CDeviceHardDisk::Lock ()
{
    DWORD dwRet;

    // Determine our physical drive number
    STORAGE_DEVICE_NUMBER sdn;
    if (!DeviceIoControl(m_hDevice, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL,0, &sdn, sizeof(sdn), &dwRet, NULL))
        return 0;

    DWORD dwDrives = GetLogicalDrives();

    for (int i = 0 ; i <= 'Z'-'A' ; i++)
    {
        // Skip non-existent drives
        if (!(dwDrives & (1 << i)))
            continue;

        // Form the root path to the volume
        char szDrive[] = "\\\\.\\_:";
        szDrive[4] = 'A'+i;

        // Open it without accessing the drive contents
        HANDLE h = CreateFile(szDrive, 0, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (h == INVALID_HANDLE_VALUE)
            continue;

        BYTE ab[256];
        PVOLUME_DISK_EXTENTS pvde = reinterpret_cast<PVOLUME_DISK_EXTENTS>(ab);

        // Get the extents of the volume, which may span multiple physical drives
        if (DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL,0, pvde, sizeof(ab), &dwRet, NULL))
        {
            // Check each extent against the supplied drive number, and mark any matches
            for (UINT u = 0 ; u < pvde->NumberOfDiskExtents ; u++)
            {
                if (pvde->Extents[u].DiskNumber == sdn.DeviceNumber)
                {
                    // Re-open the device in read-write mode
                    CloseHandle(h);
                    h = CreateFile(szDrive, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

                    if (h == INVALID_HANDLE_VALUE)
                        TRACE("!!! Failed to re-open device\n");
                    else if (!DeviceIoControl(h, FSCTL_LOCK_VOLUME, NULL,0, NULL,0, &dwRet, NULL))
                        TRACE("!!! Failed to lock volume\n");
                    else if (!DeviceIoControl(h, FSCTL_DISMOUNT_VOLUME, NULL,0, NULL,0, &dwRet, NULL))
                        TRACE("!!! Failed to dismount volume\n");
                    else
                    {
                        m_hLock = h;
                        return true;
                    }

                    CloseHandle(h);
                    return false;
                }
            }
        }

        CloseHandle(h);
    }

    return true;
}

void CDeviceHardDisk::Unlock ()
{
    if (m_hLock != INVALID_HANDLE_VALUE)
    {
        DWORD dwRet;
        DeviceIoControl(m_hLock, FSCTL_UNLOCK_VOLUME, NULL,0, NULL,0, &dwRet, NULL);

        CloseHandle(m_hLock);
        m_hLock = INVALID_HANDLE_VALUE;
    }
}

bool CDeviceHardDisk::ReadSector (UINT uSector_, BYTE* pb_)
{
    LARGE_INTEGER liOffset = { uSector_<<9 };
    DWORD dwLow = static_cast<DWORD>(liOffset.QuadPart & 0xffffffff), dwSize = 1<<9, dwRead;
    LONG lHigh = static_cast<LONG>(liOffset.QuadPart >> 32);

    if (SetFilePointer(m_hDevice, dwLow, &lHigh, FILE_BEGIN) == 0xffffffff)
        TRACE("CDeviceHardDisk::ReadSector: seek failed (%lu)\n", GetLastError());
    else if (!ReadFile(m_hDevice, m_pbSector, dwSize, &dwRead, NULL))
        TRACE("CDeviceHardDisk::ReadSector: read failed (%lu) [pb_=%08lx dwSize=%lu]\n", GetLastError(), pb_, dwSize);
    else if (dwRead != dwSize)
        TRACE("CDeviceHardDisk::ReadSector: short read of %lu bytes\n", dwRead);
    else
    {
        memcpy(pb_, m_pbSector, dwSize);
        return true;
    }

    return false;
}

bool CDeviceHardDisk::WriteSector (UINT uSector_, BYTE* pb_)
{
    LARGE_INTEGER liOffset = { uSector_<<9 };
    DWORD dwLow = static_cast<DWORD>(liOffset.QuadPart & 0xffffffff), dwSize = 1<<9, dwWritten;
    LONG lHigh = static_cast<LONG>(liOffset.QuadPart >> 32);

    memcpy(m_pbSector, pb_, dwSize);

    return SetFilePointer(m_hDevice, dwLow, &lHigh, FILE_BEGIN) != 0xffffffff &&
            WriteFile(m_hDevice, m_pbSector, dwSize, &dwWritten, NULL) && dwWritten == dwSize;
}
