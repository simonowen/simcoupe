// Part of SimCoupe - A SAM Coupe emulator
//
// Floppy.cpp: Real floppy access (requires fdrawcmd.sys)
//
//  Copyright (c) 1999-2015 Simon Owen
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
#include "Floppy.h"

#include "Disk.h"
#include "Options.h"

////////////////////////////////////////////////////////////////////////////////

struct ServiceHandleCloser { void operator()(SC_HANDLE h) { CloseServiceHandle(h); } };
using unique_SC_HANDLE = unique_resource<SC_HANDLE, nullptr, ServiceHandleCloser>;

bool FloppyStream::IsSupported()
{
    if (unique_SC_HANDLE scm = OpenSCManager(nullptr, nullptr, GENERIC_READ))
    {
        if (unique_SC_HANDLE service = OpenService(scm, "fdc", GENERIC_READ))
        {
            SERVICE_STATUS ss{};
            if (QueryServiceStatus(service, &ss))
            {
                // If fdc.sys is running then fdrawcmd.sys is supported
                return (ss.dwCurrentState == SERVICE_RUNNING);
            }
        }
    }

    return false;
}

bool FloppyStream::IsAvailable()
{
    DWORD driver_version = 0x00000000;

    auto h = CreateFile("\\\\.\\fdrawcmd", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD ret{};
        DeviceIoControl(h, IOCTL_FDRAWCMD_GET_VERSION, nullptr, 0, &driver_version, sizeof(driver_version), &ret, nullptr);
        CloseHandle(h);
    }

    // Ensure we're using fdrawcmd.sys >= 1.0.x.x
    return (driver_version & 0xffff0000) >= (FDRAWCMD_VERSION & 0xffff0000);
}

bool FloppyStream::IsRecognised(const std::string& filepath)
{
    auto device = tolower(filepath);
    return device == "a:" || device == "b:";
}

FloppyStream::FloppyStream(const std::string& filepath, bool read_only)
    : Stream(filepath, read_only)
{
    if (IsAvailable())
    {
        std::string dev_path = (tolower(filepath) == "a:") ? R"(\\.\fdraw0)" : R"(\\.\fdraw1)";
        m_hdev = CreateFile(dev_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        m_short_name = m_path;
    }

    Close();
}

void FloppyStream::Close()
{
    if (m_thread.joinable())
        m_thread.join();

    m_sectors = GetOption(stdfloppy) ? MGT_DISK_SECTORS : 0;
}

void FloppyStream::StartCommand(uint8_t command, std::shared_ptr<TRACK> track, int sector_index)
{
    if (m_thread.joinable())
        m_thread.join();

    m_command = command;
    m_track = track;
    m_sector_index = sector_index;

    m_status.reset();
    m_thread = std::thread(&FloppyStream::ThreadProc, this);
}

bool FloppyStream::IsBusy(uint8_t& status, bool wait)
{
    if (wait && m_thread.joinable())
        m_thread.join();

    if (!m_status.has_value())
        return true;

    status = m_status.value();
    return false;
}

///////////////////////////////////////////////////////////////////////////////

bool FloppyStream::Ioctl(DWORD ioctl_code, LPVOID in_ptr, size_t in_size, LPVOID out_ptr, size_t out_size)
{
    DWORD ret{};
    if (DeviceIoControl(m_hdev, ioctl_code, in_ptr, static_cast<DWORD>(in_size), out_ptr, static_cast<DWORD>(out_size), &ret, nullptr))
        return true;

    TRACE("!!! Ioctl {} failed with {:08x}\n", ioctl_code, GetLastError());
    return false;
}

uint8_t FloppyStream::ReadSector(int sector_index)
{
    auto& sector = m_track->sectors[sector_index];

    FD_READ_WRITE_PARAMS rwp{};
    rwp.flags = FD_OPTION_MFM;
    rwp.phead = m_track->head;
    rwp.cyl = sector.cyl;
    rwp.head = sector.head;
    rwp.sector = sector.sector;
    rwp.size = sector.size;
    rwp.eot = sector.sector + 1;
    rwp.gap = 0x0a;
    rwp.datalen = 0xff;

    auto sector_size = SizeFromSizeCode(sector.size);
    sector.data.resize(sector_size);

    uint8_t status = 0;
    if (!Ioctl(IOCTL_FDCMD_READ_DATA, &rwp, sizeof(rwp), sector.data.data(), sector.data.size()))
        status = (GetLastError() == ERROR_CRC) ? CRC_ERROR : RECORD_NOT_FOUND;

    FD_CMD_RESULT res{};
    Ioctl(IOCTL_FD_GET_RESULT, nullptr, 0, &res, sizeof(res));

    if (res.st2 & 0x40)
        status |= DELETED_DATA;

    return status;
}

uint8_t FloppyStream::WriteSector(int sector_index)
{
    auto& sector = m_track->sectors[sector_index];

    FD_READ_WRITE_PARAMS rwp{};
    rwp.flags = FD_OPTION_MFM;
    rwp.phead = m_track->head;
    rwp.cyl = sector.cyl;
    rwp.head = sector.head;
    rwp.sector = sector.sector;
    rwp.size = sector.size;
    rwp.eot = sector.sector + 1;
    rwp.gap = 0x0a;
    rwp.datalen = 0xff;

    auto data_ptr = const_cast<uint8_t*>(sector.data.data());

    if (!Ioctl(IOCTL_FDCMD_WRITE_DATA, &rwp, sizeof(rwp), data_ptr, sector.data.size()))
    {
        switch (GetLastError())
        {
        case ERROR_WRITE_PROTECT:       return WRITE_PROTECT;
        case ERROR_SECTOR_NOT_FOUND:    return RECORD_NOT_FOUND;
        default:                        return WRITE_FAULT;
        }
    }

    return 0;
}

uint8_t FloppyStream::FormatTrack()
{
    struct
    {
        BYTE flags;
        BYTE phead;
        BYTE size, sectors, gap, fill;
        std::array<FD_ID_HEADER, 64> headers;
    } format{};

    format.flags = FD_OPTION_MFM;
    format.phead = m_track->head;
    format.sectors = static_cast<uint8_t>(m_track->sectors.size());

    if (m_track->sectors.empty())
    {
        format.sectors = 1;
        format.size = 6;
        format.gap = 1;
        format.fill = 0x00;
    }
    else
    {
        auto& last_sector = m_track->sectors.back();
        auto sector_size = SizeFromSizeCode(last_sector.size);
        format.size = last_sector.size;
        format.fill = last_sector.data.back();  // last byte used for filler

        auto gap3 = ((MAX_TRACK_SIZE - 50) - (m_track->sectors.size() * (62 + 1 + sector_size))) / m_track->sectors.size();
        if (gap3 > 46) gap3 = 46;
        format.gap = static_cast<uint8_t>(gap3);
    }

    for (size_t i = 0; i < m_track->sectors.size(); ++i)
    {
        auto& sector = m_track->sectors[i];
        format.headers[i].cyl = sector.cyl;
        format.headers[i].head = sector.head;
        format.headers[i].sector = sector.sector;
        format.headers[i].size = sector.size;
    }

    if (!Ioctl(IOCTL_FDCMD_FORMAT_TRACK, &format, sizeof(format)))
        return (GetLastError() == ERROR_WRITE_PROTECT) ? WRITE_PROTECT : WRITE_FAULT;

    // Write any in-place format data, needed by Pro-Dos.
    uint8_t status = 0;
    for (auto start = 0, step = 2; start < step; ++start)
    {
        for (auto i = start; !status && i < static_cast<int>(m_track->sectors.size()); i += step)
        {
            auto& data = m_track->sectors[i].data;
            auto it = std::find_if(data.begin(), data.end(), [&](auto b) { return b != format.fill; });
            if (it != data.end())
                status = WriteSector(i);
        }
    }

    return status;
}

uint8_t FloppyStream::ReadSimpleTrack()
{
    m_track->sectors.clear();

    FD_READ_WRITE_PARAMS rwp{};
    rwp.flags = FD_OPTION_MFM;
    rwp.phead = m_track->head;
    rwp.cyl = m_track->cyl;
    rwp.head = m_track->head;
    rwp.sector = MGT_FIRST_SECTOR;
    rwp.size = 2;
    rwp.eot = MGT_FIRST_SECTOR + m_sectors;
    rwp.gap = 0x0a;
    rwp.datalen = 0xff;

    std::vector<uint8_t> track_data(MGT_TRACK_SIZE);
    if (!Ioctl(IOCTL_FDCMD_READ_DATA, &rwp, sizeof(rwp), track_data.data(), track_data.size()))
    {
        switch (GetLastError())
        {
        case ERROR_FLOPPY_ID_MARK_NOT_FOUND:
            return 0;

        case ERROR_SECTOR_NOT_FOUND:
        {
            FD_CMD_RESULT res{};
            Ioctl(IOCTL_FD_GET_RESULT, nullptr, 0, &res, sizeof(res));

            if (res.sector != MGT_DISK_SECTORS)
                return RECORD_NOT_FOUND;

            m_sectors = DOS_DISK_SECTORS;
            break;
        }

        default:
            return RECORD_NOT_FOUND;
        }
    }

    m_track->sectors.resize(m_sectors);

    for (auto i = 0; i < m_sectors; ++i)
    {
        auto& sector = m_track->sectors[i];
        sector.cyl = m_track->cyl;
        sector.head = m_track->head;
        sector.sector = MGT_FIRST_SECTOR + i;
        sector.size = 2;
        sector.data = std::vector<uint8_t>(
            track_data.begin() + i * NORMAL_SECTOR_SIZE,
            track_data.begin() + (i + 1) * NORMAL_SECTOR_SIZE);
    }

    return 0;
}

uint8_t FloppyStream::ReadCustomTrack()
{
    struct
    {
        uint8_t count;
        std::array<FD_ID_HEADER, 64> headers;
    } scan{};

    FD_SCAN_PARAMS sp{};
    sp.head = m_track->head;
    sp.flags = FD_OPTION_MFM;

    if (!Ioctl(IOCTL_FD_SCAN_TRACK, &sp, sizeof(sp), &scan, sizeof(scan)))
        return RECORD_NOT_FOUND;

    m_track->sectors.resize(scan.count);

    for (auto start = 0, step = 2; start < step; ++start)
    {
        for (auto i = start; i < static_cast<int>(m_track->sectors.size()); i += step)
        {
            auto& sector = m_track->sectors[i];
            const auto& result = scan.headers[i];
            auto sector_size = SizeFromSizeCode(result.size);

            sector.cyl = result.cyl;
            sector.head = result.head;
            sector.sector = result.sector;
            sector.size = result.size;
            sector.data.resize(sector_size);
            sector.status = ReadSector(i);
        }
    }

    m_sectors = 0;
    return 0;
}

void FloppyStream::ThreadProc()
{
    TRACE("Starting command {} for cyl {} head {}\n", m_command, m_track->cyl, m_track->head);

    FD_SEEK_PARAMS sp{};
    sp.cyl = m_track->cyl;
    sp.head = m_track->head;

    Ioctl(IOCTL_FDCMD_SEEK, &sp, sizeof(sp));

    auto status = 0;
    switch (m_command)
    {
    case READ_MSECTOR:
        if (m_sectors)
            status = ReadSimpleTrack();

        if (!m_sectors || status)
            status = ReadCustomTrack();
        break;

    case WRITE_1SECTOR:
        status = WriteSector(m_sector_index);
        break;

    case WRITE_TRACK:
        status = FormatTrack();
        break;

    default:
        status = LOST_DATA;
        break;
    }

    TRACE("Finished command {} with status {:02x}\n", m_command, status);
    m_status = status;
}
