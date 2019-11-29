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

#ifndef DRIVE_H
#define DRIVE_H

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
    CDrive(CDisk* pDisk_ = nullptr);
    CDrive(const CDrive&) = delete;
    void operator= (const CDrive&) = delete;
    ~CDrive() { Eject(); }

public:
    BYTE In(WORD wPort_) override;
    void Out(WORD wPort_, BYTE bVal_) override;
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
    bool GetSector(BYTE index_, IDFIELD* pID_, BYTE* pbStatus_);
    bool FindSector(IDFIELD* pID_);
    BYTE ReadSector(BYTE* pbData_, UINT* puSize_);
    BYTE WriteSector(BYTE* pbData_, UINT* puSize_);
    BYTE ReadAddress(IDFIELD* pID_);
    void ReadTrack(BYTE* pbTrack_, UINT uSize_);
    BYTE VerifyTrack();
    BYTE WriteTrack(BYTE* pbTrack_, UINT uSize_);

protected:
    void ModifyStatus(BYTE bEnable_, BYTE bReset_);
    void ModifyReadStatus();
    void ExecuteNext();

    bool IsMotorOn() const { return (m_sRegs.bStatus & MOTOR_ON) != 0; }

protected:
    CDisk* m_pDisk = nullptr;   // The disk currently inserted in the drive, if any
    BYTE m_bSide = 0;           // Side from port address

    VL1772Regs m_sRegs{};       // VL1772 controller registers
    BYTE m_bHeadCyl = 0;        // Physical track the drive head is above
    BYTE m_bSectorIndex = 0;    // Sector iteration index

    BYTE m_abBuffer[MAX_TRACK_SIZE]; // Buffer big enough for anything we'll read or write
    BYTE* m_pbBuffer = nullptr;
    UINT m_uBuffer = 0;
    BYTE m_bDataStatus = 0;     // Status value for end of data, where the data CRC can be checked

    int m_nState = 0;           // Command state, for tracking multi-stage execution
    int m_nMotorDelay = 0;      // Delay before switching motor off
};

#endif // DRIVE_H
