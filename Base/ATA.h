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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef ATA_H
#define ATA_H

// ATA controller registers
typedef struct tagATAregs
{
    WORD wData;             // 0x1f0

    BYTE bError;            // 0x1f1, init=1 (read)
    BYTE bFeatures;         // 0x1f1 (write)

    BYTE bSectorCount;      // 0x1f2, init=1
    BYTE bSector;           // 0x1f3, init=1

    BYTE bCylinderLow;      // 0x1f4, init=0
    BYTE bCylinderHigh;     // 0x1f5, init=0

    BYTE bDeviceHead;       // 0x1f6, init=0

    BYTE bStatus;           // 0x1f7 (read)
    BYTE bCommand;          // 0x1f7 (write)

//  BYTE bAltStatus;        // 0x3f6 (read) - same as 0x1f7 above
    BYTE bDeviceControl;    // 0x3f6

//  BYTE bDriveAddress;     // 0x3f7 (read) - value built on demand
} ATAregs;


// Structure of IDENTIFY DEVICE command response
typedef struct
{
    union
    {
        BYTE byte[512];
        WORD word[256];
    };
}
IDENTIFYDEVICE;

// Address lines
const BYTE ATA_CS0          = 0x08;     // Chip select 0 (negative logic)
const BYTE ATA_CS1          = 0x10;     // Chip select 1 (negative logic)
const BYTE ATA_CS_MASK      = 0x18;     // Chip select mask
const BYTE ATA_DA_MASK      = 0x07;     // Device address mask

// Device Control Register
const BYTE ATA_DCR_SRST     = 0x04;     // Host Software Reset
const BYTE ATA_DCR_nIEN     = 0x02;     // Interrupt enable (negative logic)

// Status Register
const BYTE ATA_STATUS_BUSY  = 0x80;     // Busy - no host access to Command Block Registers
const BYTE ATA_STATUS_DRDY  = 0x40;     // Device is ready to accept commands
const BYTE ATA_STATUS_DWF   = 0x20;     // Device write fault
const BYTE ATA_STATUS_DSC   = 0x10;     // Device seek complete
const BYTE ATA_STATUS_DRQ   = 0x08;     // Data request - device ready to send or receive data
const BYTE ATA_STATUS_CORR  = 0x04;     // Correctable data error encountered and corrected
const BYTE ATA_STATUS_INDEX = 0x02;     // Index mark detected on disk (once per revolution)
const BYTE ATA_STATUS_ERROR = 0x01;     // Previous command ended in error

// Error Register
const BYTE ATA_ERROR_BBK    = 0x80;     // Pre-EIDE: bad block mark detected, new meaning: CRC error during transfer
const BYTE ATA_ERROR_UNC    = 0x40;     // Uncorrectable ECC error encountered
const BYTE ATA_ERROR_MC     = 0x20;     // Media changed
const BYTE ATA_ERROR_IDNF   = 0x10;     // Requested sector's ID field not found
const BYTE ATA_ERROR_MCR    = 0x08;     // Media Change Request
const BYTE ATA_ERROR_ABRT   = 0x04;     // Command aborted due to device status error or invalid command
const BYTE ATA_ERROR_TK0NF  = 0x02;     // Track 0 not found during execution of Recalibrate command
const BYTE ATA_ERROR_AMNF   = 0x01;     // Data address mark not found after correct ID field found

// Device/Head Register
const BYTE ATA_DEVICE_0     = 0x00;     // Device 0
const BYTE ATA_DEVICE_1     = 0x10;     // Device 1
const BYTE ATA_DEVICE_MASK  = 0x10;     // Selected device mask
const BYTE ATA_HEAD_MASK    = 0x0f;     // Head bit mask


typedef struct
{
    UINT uTotalSectors;
    UINT uCylinders, uHeads, uSectors;
}
ATA_GEOMETRY;


// Base class for a generic ATA device
class CATADevice
{
    public:
        CATADevice ();
        CATADevice (const CATADevice &) = delete;
        void operator= (const CATADevice &) = delete;
        virtual ~CATADevice () = default;

    public:
        void Reset (bool fSoft_=false);
        WORD In (WORD wPort_);
        void Out (WORD wPort_, WORD wVal_);

    public:
        const ATA_GEOMETRY* GetGeometry() const { return &m_sGeometry; };
        void SetDeviceAddress (BYTE bDevice_) { m_bDevice = bDevice_; }
        void SetLegacy (bool fLegacy_) { m_fLegacy = fLegacy_; }

    public:
        virtual bool ReadSector (UINT uSector_, BYTE* pb_) = 0;
        virtual bool WriteSector (UINT uSector_, BYTE* pb_) = 0;

    protected:
        bool ReadWriteSector (bool fWrite_);
        void SetIdentifyData (IDENTIFYDEVICE *pid_);

    protected:
        static void CalculateGeometry (ATA_GEOMETRY* pg_);
        static void SetIdentifyString (const char* pcszValue_, void* pv_, size_t cb_);

    protected:
        BYTE m_bDevice = ATA_DEVICE_0;  // Device address (as ATA_DEVICE_x)
        ATAregs m_sRegs {};             // AT device registers
        IDENTIFYDEVICE m_sIdentify {};  // Identify device data
        ATA_GEOMETRY m_sGeometry {};    // Device geometry

        BYTE m_abSectorData[512];       // Sector buffer used for all reads and writes

        UINT m_uBuffer = 0;             // Number of bytes available for reading, or expected for writing
        BYTE *m_pbBuffer = nullptr;     // Current position in sector buffer for read/write operations

        bool m_f8bitOnReset = false;    // 8-bit data transfer state to set on soft reset
        bool m_f8bit = false;           // true if 8-bit data transfers are enabled
        bool m_fLegacy = false;         // true if we're to support legacy requests
};

#endif  // ATA_H
