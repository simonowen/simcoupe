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
    HardDisk(const char* pcszDisk_);

public:
    static std::unique_ptr<HardDisk> OpenObject(const char* pcszDisk_, bool fReadOnly_ = false);
    virtual bool Open(bool fReadOnly_ = false) = 0;

public:
    bool IsSDIDEDisk();
    bool IsBDOSDisk(bool* pfByteSwapped = nullptr);

protected:
    std::string m_strPath;
};


class HDFHardDisk final : public HardDisk
{
public:
    HDFHardDisk(const char* pcszDisk_);
    HDFHardDisk(const HDFHardDisk&) = delete;
    void operator= (const HDFHardDisk&) = delete;
    ~HDFHardDisk() { Close(); }

public:
    static bool Create(const char* pcszDisk_, unsigned int uTotalSectors_);

public:
    bool IsOpen() const { return m_hfDisk != nullptr; }
    bool Open(bool fReadOnly_ = false) override;
    bool Create(unsigned int uTotalSectors_);
    void Close();

    bool ReadSector(unsigned int uSector_, uint8_t* pb_) override;
    bool WriteSector(unsigned int uSector_, uint8_t* pb_) override;

protected:
    FILE* m_hfDisk = nullptr;
    unsigned int m_uDataOffset = 0;
    unsigned int m_uSectorSize = 0;
};
