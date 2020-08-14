// Part of SimCoupe - A SAM Coupe emulator
//
// HardDisk.h: Hard disk abstraction layer
//
//  Copyright (c) 2004-2014 Simon Owen
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

#include "SAMIO.h"
#include "ATA.h"

const unsigned int HDD_ACTIVE_FRAMES = 2;    // Frames the HDD is considered active after a command


class HardDisk : public ATADevice
{
public:
    HardDisk(const std::string& disk_path);
    virtual ~HardDisk() = default;

public:
    static std::unique_ptr<HardDisk> OpenObject(const std::string& disk_path, bool read_only = false);
    virtual bool Open(bool read_only = false) = 0;

public:
    bool IsSDIDEDisk();
    bool IsBDOSDisk(bool* pfByteSwapped = nullptr);

protected:
    std::string m_strPath;
};


class HDFHardDisk final : public HardDisk
{
public:
    HDFHardDisk(const std::string& disk_path);

public:
    static bool Create(const std::string& disk_path, unsigned int uTotalSectors_);

public:
    bool IsOpen() const { return m_file; }
    bool Open(bool read_only = false) override;
    bool Create(unsigned int uTotalSectors_);
    void Close();

    bool ReadSector(unsigned int uSector_, uint8_t* pb_) override;
    bool WriteSector(unsigned int uSector_, uint8_t* pb_) override;

protected:
    unique_FILE m_file;
    unsigned int m_uDataOffset = 0;
    unsigned int m_uSectorSize = 0;
};
