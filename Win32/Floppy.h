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

#ifdef HAVE_FDRAWCMD_H
#include <fdrawcmd.h>   // https://simonowen.com/fdrawcmd/fdrawcmd.h
#endif

#include "Stream.h"

struct TRACK
{
    int sectors;
    uint8_t cyl, head;     // physical track location
};

struct SECTOR
{
    uint8_t cyl, head, sector, size;
    uint8_t status;
    uint8_t* pbData;
};


class FloppyStream final : public Stream
{
public:
    FloppyStream(const char* pcszDevice_, bool fReadOnly_);
    FloppyStream(const FloppyStream&) = delete;
    void operator= (const FloppyStream&) = delete;
    virtual ~FloppyStream();

public:
    static bool IsSupported();
    static bool IsAvailable();
    static bool IsRecognised(const char* pcszStream_);

public:
    void Close() override;
    unsigned long ThreadProc();

public:
    bool IsOpen() const override { return m_hDevice != INVALID_HANDLE_VALUE; }
    bool IsBusy(uint8_t* pbStatus_, bool fWait_);

    // The normal stream functions are not used
    bool Rewind() override { return false; }
    size_t Read(void*, size_t) override { return 0; }
    size_t Write(void*, size_t) override { return 0; }

    uint8_t StartCommand(uint8_t bCommand_, TRACK* pTrack_ = nullptr, unsigned int uSectorIndex_ = 0);

protected:
    HANDLE m_hDevice = INVALID_HANDLE_VALUE; // Floppy device handle
    HANDLE m_hThread = nullptr; // Worker thread handles
    unsigned int m_uSectors = 0;        // Sector count, or zero for auto-detect (slower)

    uint8_t m_bCommand = 0;        // Current command
    uint8_t m_bStatus;             // Final status

    TRACK* m_pTrack = nullptr;  // Track for command
    unsigned int m_uSectorIndex = 0;    // Zero-based sector for write command
};
