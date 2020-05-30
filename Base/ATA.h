// Part of SimCoupe - A SAM Coupe emulator
//
// ATA.h: ATA hard disk emulation
//
//  Copyright (c) 1999-2012 Simon Owen
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

// ATA controller registers
struct ATAregs
{
    uint16_t wData;            // 0x1f0

    uint8_t bError;            // 0x1f1, init=1 (read)
    uint8_t bFeatures;         // 0x1f1 (write)

    uint8_t bSectorCount;      // 0x1f2, init=1
    uint8_t bSector;           // 0x1f3, init=1

    uint8_t bCylinderLow;      // 0x1f4, init=0
    uint8_t bCylinderHigh;     // 0x1f5, init=0

    uint8_t bDeviceHead;       // 0x1f6, init=0

    uint8_t bStatus;           // 0x1f7 (read)
    uint8_t bCommand;          // 0x1f7 (write)

//  uint8_t bAltStatus;        // 0x3f6 (read) - same as 0x1f7 above
    uint8_t bDeviceControl;    // 0x3f6

//  uint8_t bDriveAddress;     // 0x3f7 (read) - value built on demand
};


// Structure of IDENTIFY DEVICE command response
struct IDENTIFYDEVICE
{
    union
    {
        uint8_t byte[512];
        uint16_t word[256];
    };
};

// Address lines
const uint8_t ATA_CS0 = 0x08;           // Chip select 0 (negative logic)
const uint8_t ATA_CS1 = 0x10;           // Chip select 1 (negative logic)
const uint8_t ATA_CS_MASK = 0x18;       // Chip select mask
const uint8_t ATA_DA_MASK = 0x07;       // Device address mask

// Device Control Register
const uint8_t ATA_DCR_SRST = 0x04;      // Host Software Reset
const uint8_t ATA_DCR_nIEN = 0x02;      // Interrupt enable (negative logic)

// Status Register
const uint8_t ATA_STATUS_BUSY = 0x80;   // Busy - no host access to Command Block Registers
const uint8_t ATA_STATUS_DRDY = 0x40;   // Device is ready to accept commands
const uint8_t ATA_STATUS_DWF = 0x20;    // Device write fault
const uint8_t ATA_STATUS_DSC = 0x10;    // Device seek complete
const uint8_t ATA_STATUS_DRQ = 0x08;    // Data request - device ready to send or receive data
const uint8_t ATA_STATUS_CORR = 0x04;   // Correctable data error encountered and corrected
const uint8_t ATA_STATUS_INDEX = 0x02;  // Index mark detected on disk (once per revolution)
const uint8_t ATA_STATUS_ERROR = 0x01;  // Previous command ended in error

// Error Register
const uint8_t ATA_ERROR_BBK = 0x80;     // Pre-EIDE: bad block mark detected, new meaning: CRC error during transfer
const uint8_t ATA_ERROR_UNC = 0x40;     // Uncorrectable ECC error encountered
const uint8_t ATA_ERROR_MC = 0x20;      // Media changed
const uint8_t ATA_ERROR_IDNF = 0x10;    // Requested sector's ID field not found
const uint8_t ATA_ERROR_MCR = 0x08;     // Media Change Request
const uint8_t ATA_ERROR_ABRT = 0x04;    // Command aborted due to device status error or invalid command
const uint8_t ATA_ERROR_TK0NF = 0x02;   // Track 0 not found during execution of Recalibrate command
const uint8_t ATA_ERROR_AMNF = 0x01;    // Data address mark not found after correct ID field found

// Device/Head Register
const uint8_t ATA_DEVICE_0 = 0x00;      // Device 0
const uint8_t ATA_DEVICE_1 = 0x10;      // Device 1
const uint8_t ATA_DEVICE_MASK = 0x10;   // Selected device mask
const uint8_t ATA_HEAD_MASK = 0x0f;     // Head bit mask


struct ATA_GEOMETRY
{
    unsigned int uTotalSectors;
    unsigned int uCylinders, uHeads, uSectors;
};


// Base class for a generic ATA device
class CATADevice
{
public:
    CATADevice();
    CATADevice(const CATADevice&) = delete;
    void operator= (const CATADevice&) = delete;
    virtual ~CATADevice() = default;

public:
    void Reset(bool fSoft_ = false);
    uint16_t In(uint16_t wPort_);
    void Out(uint16_t wPort_, uint16_t wVal_);

public:
    const ATA_GEOMETRY* GetGeometry() const { return &m_sGeometry; };
    void SetDeviceAddress(uint8_t bDevice_) { m_bDevice = bDevice_; }
    void SetLegacy(bool fLegacy_) { m_fLegacy = fLegacy_; }

public:
    virtual bool ReadSector(unsigned int uSector_, uint8_t* pb_) = 0;
    virtual bool WriteSector(unsigned int uSector_, uint8_t* pb_) = 0;

protected:
    bool ReadWriteSector(bool fWrite_);
    void SetIdentifyData(IDENTIFYDEVICE* pid_);

protected:
    static void CalculateGeometry(ATA_GEOMETRY* pg_);
    static void SetIdentifyString(const char* pcszValue_, void* pv_, size_t cb_);

protected:
    uint8_t m_bDevice = ATA_DEVICE_0;   // Device address (as ATA_DEVICE_x)
    ATAregs m_sRegs{};                  // AT device registers
    IDENTIFYDEVICE m_sIdentify{};       // Identify device data
    ATA_GEOMETRY m_sGeometry{};         // Device geometry

    uint8_t m_abSectorData[512];        // Sector buffer used for all reads and writes

    unsigned int m_uBuffer = 0;         // Number of bytes available for reading, or expected for writing
    uint8_t* m_pbBuffer = nullptr;      // Current position in sector buffer for read/write operations

    bool m_f8bitOnReset = false;    // 8-bit data transfer state to set on soft reset
    bool m_f8bit = false;           // true if 8-bit data transfers are enabled
    bool m_fLegacy = false;         // true if we're to support legacy requests
};
