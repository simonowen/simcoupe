// Part of SimCoupe - A SAM Coupe emulator
//
// Floppy.h: Real floppy access (requires fdrawcmd.sys)
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

#pragma once

#include "fdrawcmd.h"   // https://simonowen.com/fdrawcmd/fdrawcmd.h
#include "Stream.h"

struct SECTOR
{
    uint8_t cyl;
    uint8_t head;
    uint8_t sector;
    uint8_t size;
    uint8_t status;
    std::vector<uint8_t> data;
};

struct TRACK
{
    uint8_t cyl = 0;
    uint8_t head = 0xff;
    std::vector<SECTOR> sectors;
};

struct Win32HandleCloser { void operator()(HANDLE h) { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); } };
using unique_HANDLE = unique_resource<HANDLE, nullptr, Win32HandleCloser>;

class FloppyStream final : public Stream
{
public:
    FloppyStream(const std::string& filepath, bool read_only);

    static bool IsSupported();
    static bool IsAvailable();
    static bool IsRecognised(const std::string& filepath);

    void Close() override;

    bool IsBusy(uint8_t& status, bool wait);
    fs::file_time_type LastWriteTime() const override { return {}; }

    // The normal stream functions are not used
    size_t GetSize() override { return 0; }
    void Rewind() override { }
    size_t Read(void*, size_t) override { return 0; }
    size_t Write(const void*, size_t) override { return 0; }

    void StartCommand(uint8_t command, std::shared_ptr<TRACK> track, int sector_index = 0);

protected:
    bool Ioctl(DWORD ioctl_code, LPVOID in_ptr = nullptr, size_t in_size = 0, LPVOID out_ptr = nullptr, size_t out_size = 0);
    uint8_t ReadSector(int sector_index);
    uint8_t WriteSector(int sector_index);
    uint8_t ReadSimpleTrack();
    uint8_t ReadCustomTrack();
    uint8_t FormatTrack();

    void ThreadProc();

    unique_HANDLE m_hdev = INVALID_HANDLE_VALUE;
    int m_sectors = 0;

    uint8_t m_command = 0;
    int m_sector_index = 0;
    std::shared_ptr<TRACK> m_track;

    std::thread m_thread;
    std::optional<uint8_t> m_status;
};
