// Part of SimCoupe - A SAM Coupe emulator
//
// IDEDisk.cpp: Platform-specific IDE direct disk access
//
//  Copyright (c) 2003-2015 Simon Owen
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

// SAMdiskHelper definitions, for non-admin device access
#define PIPENAME    "\\\\.\\pipe\\SAMdiskHelper"
#define FN_OPEN     2

#pragma pack(1)
typedef struct {
    union {
        struct {
            DWORD dwMessage;
            char szPath[MAX_PATH];
        } Input;

        struct {
            DWORD dwError;
            DWORD64 hDevice;
        } Output;
    };
} PIPEMESSAGE;
#pragma pack()


CDeviceHardDisk::CDeviceHardDisk(const char* pcszDisk_)
    : CHardDisk(pcszDisk_)
{
    m_pbSector = (PBYTE)VirtualAlloc(nullptr, 1 << 9, MEM_COMMIT, PAGE_READWRITE);
}

CDeviceHardDisk::~CDeviceHardDisk()
{
    Close();

    if (m_pbSector)
        VirtualFree(m_pbSector, 0, MEM_RELEASE);
}


bool CDeviceHardDisk::IsRecognised(const char* pcszDisk_)
{
    char* pszEnd = nullptr;

    // Accept a device number followed by a colon, and anything beyond that
    return isdigit(pcszDisk_[0]) && strtoul(pcszDisk_, &pszEnd, 10) != ULONG_MAX && *pszEnd == ':';
}


bool CDeviceHardDisk::Open(bool fReadOnly_/*=false*/)
{
    if (!IsRecognised(m_strPath.c_str()))
        return false;

    char* pszEnd = nullptr;
    ULONG ulDevice = strtoul(m_strPath.c_str(), &pszEnd, 10);
    if (ulDevice == ULONG_MAX)
        return false;

    char sz[MAX_PATH] = {};
    snprintf(sz, sizeof(sz) - 1, "\\\\.\\PhysicalDrive%lu", ulDevice);

    DWORD dwWrite = fReadOnly_ ? 0 : GENERIC_WRITE;
    m_hDevice = CreateFile(sz, GENERIC_READ | dwWrite, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    DWORD dwError = GetLastError();

    // If a direct open failed, try via SAMdiskHelper
    if (!IsOpen())
    {
        DWORD dwRead;
        PIPEMESSAGE msg = {};
        msg.Input.dwMessage = FN_OPEN;
        lstrcpyn(msg.Input.szPath, sz, sizeof(msg.Input.szPath) - 1);

        if (CallNamedPipe(PIPENAME, &msg, sizeof(msg.Input), &msg, sizeof(msg.Output), &dwRead, NMPWAIT_NOWAIT))
        {
            if (dwRead == sizeof(msg.Output) && msg.Output.dwError == 0)
                m_hDevice = reinterpret_cast<HANDLE>(msg.Output.hDevice);
            else if (msg.Output.dwError)
                dwError = msg.Output.dwError;
        }
    }

    if (!IsOpen())
    {
        if (dwError != ERROR_FILE_NOT_FOUND && dwError != ERROR_PATH_NOT_FOUND)
            TRACE("Failed to open %s (%#08lx)\n", sz, dwError);
    }
    else if (!Lock(fReadOnly_))
    {
        TRACE("Failed to get exclusive access to %s\n", sz);
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
        if (DeviceIoControl(m_hDevice, IOCTL_DISK_GET_PARTITION_INFO, nullptr, 0, &pi, sizeof(pi), &dwRet, nullptr))
        {
            // Extract the disk geometry and size in sectors
            // We round down to the nearest 1K to fix a single sector error with some CF card readers
            m_sGeometry.uTotalSectors = static_cast<UINT>(pi.PartitionLength.QuadPart >> 9) & ~1U;

            // Generate suitable identify data to report
            SetIdentifyData(nullptr);

            // For safety, only deal with existing BDOS or SDIDE hard disks
            if (IsBDOSDisk() || IsSDIDEDisk())
                return true;
        }
    }

    Close();
    SetLastError(dwError);
    return false;
}

void CDeviceHardDisk::Close()
{
    if (IsOpen())
    {
        Unlock();
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
}

bool CDeviceHardDisk::Lock(bool fReadOnly_/*=false*/)
{
    DWORD dwRet;

    // Determine our physical drive number
    STORAGE_DEVICE_NUMBER sdn;
    if (!DeviceIoControl(m_hDevice, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0, &sdn, sizeof(sdn), &dwRet, nullptr))
        return 0;

    DWORD dwDrives = GetLogicalDrives();

    for (int i = 0; i <= 'Z' - 'A'; i++)
    {
        // Skip non-existent drives
        if (!(dwDrives & (1 << i)))
            continue;

        // Form the root path to the volume
        char szDrive[] = "\\\\.\\_:";
        szDrive[4] = 'A' + i;

        // Open it without accessing the drive contents
        HANDLE h = CreateFile(szDrive, 0, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            continue;

        BYTE ab[256];
        PVOLUME_DISK_EXTENTS pvde = reinterpret_cast<PVOLUME_DISK_EXTENTS>(ab);

        // Get the extents of the volume, which may span multiple physical drives
        if (DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0, pvde, sizeof(ab), &dwRet, nullptr))
        {
            // Check each extent against the supplied drive number, and mark any matches
            for (UINT u = 0; u < pvde->NumberOfDiskExtents; u++)
            {
                if (pvde->Extents[u].DiskNumber == sdn.DeviceNumber)
                {
                    // Re-open the device in read-write mode
                    CloseHandle(h);
                    h = CreateFile(szDrive, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

                    if (h == INVALID_HANDLE_VALUE)
                        TRACE("!!! Failed to re-open device\n");
                    else if (!DeviceIoControl(h, FSCTL_LOCK_VOLUME, nullptr, 0, nullptr, 0, &dwRet, nullptr))
                        TRACE("!!! Failed to lock volume\n");
                    else if (!fReadOnly_ && !DeviceIoControl(h, FSCTL_DISMOUNT_VOLUME, nullptr, 0, nullptr, 0, &dwRet, nullptr))
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

void CDeviceHardDisk::Unlock()
{
    if (m_hLock != INVALID_HANDLE_VALUE)
    {
        DWORD dwRet;
        DeviceIoControl(m_hLock, FSCTL_UNLOCK_VOLUME, nullptr, 0, nullptr, 0, &dwRet, nullptr);

        CloseHandle(m_hLock);
        m_hLock = INVALID_HANDLE_VALUE;
    }
}

bool CDeviceHardDisk::ReadSector(UINT uSector_, BYTE* pb_)
{
    LARGE_INTEGER liOffset = { uSector_ << 9 };
    DWORD dwLow = static_cast<DWORD>(liOffset.QuadPart & 0xffffffff), dwSize = 1 << 9, dwRead;
    LONG lHigh = static_cast<LONG>(liOffset.QuadPart >> 32);

    if (SetFilePointer(m_hDevice, dwLow, &lHigh, FILE_BEGIN) == 0xffffffff)
        TRACE("CDeviceHardDisk::ReadSector: seek failed (%lu)\n", GetLastError());
    else if (!ReadFile(m_hDevice, m_pbSector, dwSize, &dwRead, nullptr))
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

bool CDeviceHardDisk::WriteSector(UINT uSector_, BYTE* pb_)
{
    LARGE_INTEGER liOffset = { uSector_ << 9 };
    DWORD dwLow = static_cast<DWORD>(liOffset.QuadPart & 0xffffffff), dwSize = 1 << 9, dwWritten;
    LONG lHigh = static_cast<LONG>(liOffset.QuadPart >> 32);

    memcpy(m_pbSector, pb_, dwSize);

    return SetFilePointer(m_hDevice, dwLow, &lHigh, FILE_BEGIN) != 0xffffffff &&
        WriteFile(m_hDevice, m_pbSector, dwSize, &dwWritten, nullptr) && dwWritten == dwSize;
}

const char* CDeviceHardDisk::GetDeviceList()
{
    static char szList[1024];
    char* pszList = szList;
    szList[0] = '\0';

    BYTE ab[2048];

    for (DWORD dw = 0; dw < 10; dw++)
    {
        DWORD dwRet;

        char sz[32] = {};
        snprintf(sz, sizeof(sz) - 1, "\\\\.\\PhysicalDrive%lu", dw);

        // Open with limited rights, so we can fetch some details without Administrator access
        HANDLE h = CreateFile(sz, 0, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            continue;

        // Read the partition table
        if (!DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, nullptr, 0, &ab, sizeof(ab), &dwRet, nullptr))
        {
            CloseHandle(h);
            continue;
        }

        DRIVE_LAYOUT_INFORMATION_EX* pdli = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX*>(ab);

        // Require disks with at least one non-empty partition that begins at the start of the device.
        if (pdli->PartitionCount && pdli->PartitionEntry[0].PartitionLength.QuadPart && !pdli->PartitionEntry[0].StartingOffset.QuadPart)
        {
            // Generate a user friendly abbreviation of the disk size
            UINT uTotalSectors = static_cast<UINT>(pdli->PartitionEntry[0].PartitionLength.QuadPart >> 9) & ~1U;
            const char* pcszSize = AbbreviateSize(static_cast<uint64_t>(uTotalSectors) * 512);

            STORAGE_PROPERTY_QUERY spq = {};
            spq.QueryType = PropertyStandardQuery;
            spq.PropertyId = StorageDeviceProperty;

            PSTORAGE_DEVICE_DESCRIPTOR pDevDesc = reinterpret_cast<PSTORAGE_DEVICE_DESCRIPTOR>(ab);
            pDevDesc->Size = sizeof(ab);

            if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &spq, sizeof(spq), pDevDesc, pDevDesc->Size, &dwRet, nullptr) && pDevDesc->ProductIdOffset)
            {
                // Extract make/model if defined
                const char* pcszMake = pDevDesc->VendorIdOffset ? reinterpret_cast<const char*>(pDevDesc) + pDevDesc->VendorIdOffset : "";
                const char* pcszModel = pDevDesc->ProductIdOffset ? reinterpret_cast<const char*>(pDevDesc) + pDevDesc->ProductIdOffset : "";

                char sz[256];
                snprintf(sz, sizeof(sz), "%lu: %s%s", dw, pcszMake, pcszModel);

                // Strip trailing spaces
                for (char* p = sz + strlen(sz) - 1; p >= sz && *p == ' '; *p-- = '\0');

                // Append the size string
                snprintf(sz + strlen(sz), sizeof(sz) - strlen(sz), " (%s)", pcszSize);

                if (strlen(pszList) + strlen(sz) + 2 < sizeof(szList))
                {
                    strcpy(pszList, sz);
                    pszList += strlen(pszList) + 1;
                }
            }
        }

        CloseHandle(h);
    }

    // Double-terminate the list
    *pszList = '\0';

    return szList;
}
