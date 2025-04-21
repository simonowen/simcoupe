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
#include "Options.h"

#include <winioctl.h>

// SAMdiskHelper definitions, for non-admin device access
#define PIPENAME    "\\\\.\\pipe\\SAMdiskHelper"
#define SAMDISKHELPER_VERSION   0x01050000
#define FN_VERSION  1
#define FN_OPEN     2

#pragma pack(1)
struct PIPEMESSAGE
{
    union
    {
        struct {
            DWORD dwMessage;
            char szPath[MAX_PATH];
        } Input;

        struct {
            DWORD dwError;
            union
            {
                struct { DWORD version; } Version;
                struct { DWORD64 hDevice; } Open;
            };
        } Output;
    };
};
#pragma pack()


DeviceHardDisk::DeviceHardDisk(const std::string& disk_path) :
    HardDisk(disk_path)
{
    m_pbSector = (PBYTE)VirtualAlloc(nullptr, 1 << 9, MEM_COMMIT, PAGE_READWRITE);
}

DeviceHardDisk::~DeviceHardDisk()
{
    Close();

    if (m_pbSector)
        VirtualFree(m_pbSector, 0, MEM_RELEASE);
}


bool DeviceHardDisk::IsRecognised(const std::string& disk_path)
{
    unsigned int index{};
    char colon{};
    return std::sscanf(disk_path.c_str(), "%u%c", &index, &colon) == 2;
}


bool DeviceHardDisk::Open(bool read_only)
{
    if (!IsRecognised(m_strPath.c_str()))
        return false;

    char* pszEnd = nullptr;
    ULONG ulDevice = strtoul(m_strPath.c_str(), &pszEnd, 10);
    if (ulDevice == ULONG_MAX)
        return false;

    auto device_path = fmt::format(R"(\\.\PhysicalDrive{})", ulDevice);
    DWORD dwWrite = read_only ? 0 : GENERIC_WRITE;
    m_hDevice = CreateFile(device_path.c_str(), GENERIC_READ | dwWrite, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    DWORD dwError = GetLastError();

    // If a direct open failed, try via SAMdiskHelper
    if (!IsOpen())
    {
        DWORD dwRead{};
        PIPEMESSAGE msg{};
        msg.Input.dwMessage = FN_OPEN;
        lstrcpyn(msg.Input.szPath, device_path.c_str(), sizeof(msg.Input.szPath) - 1);

        if (CallNamedPipe(PIPENAME, &msg, sizeof(msg.Input), &msg, sizeof(msg.Output), &dwRead, NMPWAIT_NOWAIT))
        {
            if (dwRead == sizeof(msg.Output) && msg.Output.dwError == 0)
            {
                m_hDevice = reinterpret_cast<HANDLE>(msg.Output.Open.hDevice);

                memset(&msg, 0, sizeof(msg));
                msg.Input.dwMessage = FN_VERSION;
                if (CallNamedPipe(PIPENAME, &msg, sizeof(msg.Input), &msg, sizeof(msg.Output), &dwRead, NMPWAIT_NOWAIT) &&
                    msg.Output.Version.version < SAMDISKHELPER_VERSION &&
                    msg.Output.Version.version > static_cast<DWORD>(GetOption(samdiskhelper)))
                {
                    Message(MsgType::Info, "The installed SAMdiskHelper is outdated. Please consider upgrading to a newer version.");
                    SetOption(samdiskhelper, SAMDISKHELPER_VERSION);
                }
            }
            else if (msg.Output.dwError)
            {
                dwError = msg.Output.dwError;
            }
        }
    }

    if (!IsOpen())
    {
        if (dwError != ERROR_FILE_NOT_FOUND && dwError != ERROR_PATH_NOT_FOUND)
            TRACE("Failed to open {} ({:08x})\n", device_path, dwError);
    }
    else if (!Lock(read_only))
    {
        TRACE("Failed to get exclusive access to {}\n", device_path);
    }
    else if (!m_pbSector)
    {
        TRACE("Out of memory for CDeviceHardDisk sector buffer\n");
    }
    else
    {
        DWORD dwRet{};
        DWORD ab[2048]{};
        auto pdge = reinterpret_cast<DISK_GEOMETRY_EX*>(ab);

        // Read the drive geometry, checking for a disk device
        if (DeviceIoControl(m_hDevice, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, &ab, sizeof(ab), &dwRet, nullptr))
        {
            m_sGeometry.uTotalSectors = static_cast<UINT>(pdge->DiskSize.QuadPart / pdge->Geometry.BytesPerSector);

            // Generate suitable identify data to report
            SetIdentifyData(nullptr);

            // For safety, only deal with existing BDOS or SDIDE hard disks, or disks under the BDOS limit (~53GB)
            if (IsBDOSDisk() || IsSDIDEDisk() || m_sGeometry.uTotalSectors <= 104'858'050U)
                return true;
        }
    }

    Close();
    SetLastError(dwError);
    return false;
}

void DeviceHardDisk::Close()
{
    if (IsOpen())
    {
        Unlock();
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
}

bool DeviceHardDisk::Lock(bool read_only)
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

        DWORD ab[256];
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
                    else if (!read_only && !DeviceIoControl(h, FSCTL_DISMOUNT_VOLUME, nullptr, 0, nullptr, 0, &dwRet, nullptr))
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

void DeviceHardDisk::Unlock()
{
    if (m_hLock != INVALID_HANDLE_VALUE)
    {
        DWORD dwRet;
        DeviceIoControl(m_hLock, FSCTL_UNLOCK_VOLUME, nullptr, 0, nullptr, 0, &dwRet, nullptr);

        CloseHandle(m_hLock);
        m_hLock = INVALID_HANDLE_VALUE;
    }
}

bool DeviceHardDisk::ReadSector(UINT uSector_, uint8_t* pb_)
{
    LARGE_INTEGER liOffset = { { uSector_ << 9 } };
    auto dwLow = static_cast<DWORD>(liOffset.QuadPart & 0xffffffff);
    auto lHigh = static_cast<LONG>(liOffset.QuadPart >> 32);
    DWORD dwSize = 1 << 9;
    DWORD dwRead;

    if (SetFilePointer(m_hDevice, dwLow, &lHigh, FILE_BEGIN) == 0xffffffff)
        TRACE("CDeviceHardDisk::ReadSector: seek failed ({})\n", GetLastError());
    else if (!ReadFile(m_hDevice, m_pbSector, dwSize, &dwRead, nullptr))
        TRACE("CDeviceHardDisk::ReadSector: read failed ({}) [size={}]\n", GetLastError(), dwSize);
    else if (dwRead != dwSize)
        TRACE("CDeviceHardDisk::ReadSector: short read of {} bytes\n", dwRead);
    else
    {
        memcpy(pb_, m_pbSector, dwSize);
        return true;
    }

    return false;
}

bool DeviceHardDisk::WriteSector(UINT uSector_, uint8_t* pb_)
{
    LARGE_INTEGER liOffset{ { uSector_ << 9 } };
    auto dwLow = static_cast<DWORD>(liOffset.QuadPart & 0xffffffff);
    auto lHigh = static_cast<LONG>(liOffset.QuadPart >> 32);
    DWORD dwSize = 1 << 9;
    DWORD dwWritten;

    memcpy(m_pbSector, pb_, dwSize);

    return SetFilePointer(m_hDevice, dwLow, &lHigh, FILE_BEGIN) != 0xffffffff &&
        WriteFile(m_hDevice, m_pbSector, dwSize, &dwWritten, nullptr) && dwWritten == dwSize;
}

std::vector<std::string> DeviceHardDisk::GetDeviceList()
{
    std::vector<std::string> device_list;

    for (DWORD dw = 0; dw < 10; dw++)
    {
        DWORD dwRet;
        auto device_path = fmt::format(R"(\\.\PhysicalDrive{})", dw);

        // Open with limited rights, so we can fetch some details without Administrator access
        HANDLE h = CreateFile(device_path.c_str(), 0, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            continue;

        DWORD ab[2048]{};
        if (!DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, &ab, sizeof(ab), &dwRet, nullptr))
        {
            CloseHandle(h);
            continue;
        }

        auto pdge = reinterpret_cast<DISK_GEOMETRY_EX*>(ab);
        if (pdge->DiskSize.QuadPart > 0 && pdge->Geometry.BytesPerSector == 512)
        {
            auto size_desc = AbbreviateSize(pdge->DiskSize.QuadPart);

            STORAGE_PROPERTY_QUERY spq = {};
            spq.QueryType = PropertyStandardQuery;
            spq.PropertyId = StorageDeviceProperty;

            PSTORAGE_DEVICE_DESCRIPTOR pDevDesc = reinterpret_cast<PSTORAGE_DEVICE_DESCRIPTOR>(ab);
            pDevDesc->Size = sizeof(ab);

            if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &spq, sizeof(spq), pDevDesc, pDevDesc->Size, &dwRet, nullptr) &&
                pDevDesc->ProductIdOffset &&
                pDevDesc->BusType == BusTypeUsb)
            {
                // Extract make/model if defined
                const char* pcszMake = pDevDesc->VendorIdOffset ? reinterpret_cast<const char*>(pDevDesc) + pDevDesc->VendorIdOffset : "";
                const char* pcszModel = pDevDesc->ProductIdOffset ? reinterpret_cast<const char*>(pDevDesc) + pDevDesc->ProductIdOffset : "";

                auto str = trim(fmt::format("{}: {}{}", dw, pcszMake, pcszModel));
                str += fmt::format(" ({})", size_desc);
                device_list.push_back(str);
            }
        }

        CloseHandle(h);
    }

    return device_list;
}
