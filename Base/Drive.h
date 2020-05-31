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
const int FLOPPY_MOTOR_TIMEOUT = (10 / (FLOPPY_RPM / 60)) * EMULATED_FRAMES_PER_SECOND;

const unsigned int FLOPPY_ACTIVE_FRAMES = 5;   // Frames the floppy is considered active after a command


class CDrive final : public CDiskDevice
{
public:
    CDrive();
    CDrive(std::unique_ptr<CDisk> pDisk_);
    CDrive(const CDrive&) = delete;
    void operator= (const CDrive&) = delete;
    ~CDrive() { Eject(); }

public:
    uint8_t In(uint16_t wPort_) override;
    void Out(uint16_t wPort_, uint8_t bVal_) override;
    void FrameEnd() override;

public:
    bool Insert(const char* pcszSource_, bool fAutoLoad_ = false) override;
    void Eject() override;
    bool Save() override { return m_pDisk && m_pDisk->Save(); }
    void Reset() override;

public:
    const char* DiskPath() const override { return m_pDisk ? m_pDisk->GetPath() : ""; }
    const char* DiskFile() const override { return m_pDisk ? m_pDisk->GetFile() : ""; }

    bool HasDisk() const override { return m_pDisk != nullptr; }
    bool DiskModified() const override { return m_pDisk && m_pDisk->IsModified(); }
    bool IsLightOn() const override { return IsMotorOn(); }

    void SetDiskModified(bool fModified_ = true) override { if (m_pDisk) m_pDisk->SetModified(fModified_); }

protected:
    bool GetSector(uint8_t index_, IDFIELD* pID_, uint8_t* pbStatus_);
    bool FindSector(IDFIELD* pID_);
    uint8_t ReadSector(uint8_t* pbData_, unsigned int* puSize_);
    uint8_t WriteSector(uint8_t* pbData_, unsigned int* puSize_);
    uint8_t ReadAddress(IDFIELD* pID_);
    void ReadTrack(uint8_t* pbTrack_, unsigned int uSize_);
    uint8_t VerifyTrack();
    uint8_t WriteTrack(uint8_t* pbTrack_, unsigned int uSize_);

protected:
    void ModifyStatus(uint8_t bEnable_, uint8_t bReset_);
    void ModifyReadStatus();
    void ExecuteNext();

    bool IsMotorOn() const { return (m_sRegs.bStatus & MOTOR_ON) != 0; }

protected:
    std::unique_ptr<CDisk> m_pDisk;   // The disk currently inserted in the drive, if any
    uint8_t m_bSide = 0;        // Side from port address

    VL1772Regs m_sRegs{};          // VL1772 controller registers
    uint8_t m_bHeadCyl = 0;        // Physical track the drive head is above
    uint8_t m_bSectorIndex = 0;    // Sector iteration index

    uint8_t m_abBuffer[MAX_TRACK_SIZE]; // Buffer big enough for anything we'll read or write
    uint8_t* m_pbBuffer = nullptr;
    unsigned int m_uBuffer = 0;
    uint8_t m_bDataStatus = 0;     // Status value for end of data, where the data CRC can be checked

    int m_nState = 0;           // Command state, for tracking multi-stage execution
    int m_nMotorDelay = 0;      // Delay before switching motor off
};
