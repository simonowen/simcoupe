// Part of SimCoupe - A SAM Coupe emulator
//
// Drive.h: VL1772-02 floppy disk controller emulation
//
//  Copyright (c) 1999-2012 Simon Owen
//  Copyright (c) 1996-2001 Allan Skillman
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
#include "Options.h"

#include "VL1772.h"
#include "Disk.h"


// Time motor stays on after no further activity:  10 revolutions at 300rpm (2 seconds)
constexpr auto FLOPPY_MOTOR_TIMEOUT = (10 / (FLOPPY_RPM / 60)) * EMULATED_FRAMES_PER_SECOND;

const unsigned int FLOPPY_ACTIVE_FRAMES = 5;   // Frames the floppy is considered active after a command


class Drive final : public DiskDevice
{
public:
    Drive();
    ~Drive() { Eject(); }

    uint8_t In(uint16_t wPort_) override;
    void Out(uint16_t wPort_, uint8_t bVal_) override;
    void FrameEnd() override;

    bool Insert(const std::string& disk_path) override;
    bool Insert(const std::vector<uint8_t>& mem_file) override;
    void Eject() override;
    void Flush() override;
    void Reset() override;

    std::string DiskPath() const override { return m_disk ? m_disk->GetPath() : ""; }
    std::string DiskFile() const override { return m_disk ? m_disk->GetFile() : ""; }

    bool HasDisk() const override { return m_disk != nullptr; }
    bool IsLightOn() const override { return (m_regs.status & MOTOR_ON) != 0; }

protected:
    std::pair<uint8_t, IDFIELD> GetSector(uint8_t index);
    std::optional<IDFIELD> FindSector();
    std::pair<uint8_t, std::vector<uint8_t>> ReadSector();
    uint8_t WriteSector(const std::vector<uint8_t>& data);
    std::pair<uint8_t, IDFIELD> ReadAddress();
    std::vector<uint8_t> ReadTrack();
    uint8_t VerifyTrack();
    uint8_t WriteTrack(const std::vector<uint8_t>& data);

    void ModifyStatus(uint8_t bEnable_, uint8_t bReset_);
    void ModifyReadStatus();
    void ExecuteNext();

    std::unique_ptr<Disk> m_disk;

    VL1772Regs m_regs{};
    uint8_t m_cyl = 0;
    uint8_t m_head = 0;
    uint8_t m_sector_index = 0;

    std::vector<uint8_t> m_buffer;
    size_t m_buffer_pos = 0;
    size_t m_status_reads_with_data = 0;
    uint8_t m_data_status = 0;

    int m_state = 0;
    int m_motor_off_frames = 0;
};
