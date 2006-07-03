// Part of SimCoupe - A SAM Coupe emulator
//
// ATA.cpp: ATA hard disk (and future ATAPI CD-ROM) emulation
//
//  Copyright (c) 1999-2006  Simon Owen
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

#include "SimCoupe.h"

#include "ATA.h"
#include "Frame.h"

// ToDo:
//  - add ATA interface layer to allow chaining of a slave device
//  - implement additional packet commands for ATAPI CD-ROM support


CATADevice::CATADevice ()
{
    memset(&m_sGeometry, 0, sizeof(m_sGeometry));
    memset(&m_abIdentity, 0, sizeof(m_abIdentity));

    Reset();
}

// Device hard reset
void CATADevice::Reset ()
{
    memset(&m_sRegs, 0, sizeof m_sRegs);

    // Set up any non-zero initial register values
    m_sRegs.bSectorCount = m_sRegs.bSector = m_sRegs.bError = 0x01;

    // Device is settled and ready, and not asleep
    m_sRegs.bStatus = ATA_STATUS_DRDY|ATA_STATUS_DSC;
    m_fAsleep = false;

    // No data available for reading or required for writing
    m_pbBuffer = NULL;
    m_uBuffer = 0;

    // Multiple reads/writes disabled for now
    m_nMultiples = 0;
}


WORD CATADevice::In (WORD wPort_)
{
    WORD wRet = 0xffff;

    switch ((wPort_ >> 8) & 3)
    {
        case 1:
            switch (wPort_ & 0xff)
            {
                // Reset
                case 0xdf:
                    // The reset should be when the data port is next read, but we'll assume that will happen next!
                    Reset();
                    break;

                // Data register
                case 0xf0:
                {
                    // Return zero if no more data is available
                    if (!m_uBuffer)
                        TRACE("ATA: Data read when no data available!\n");
                    else
                    {
                        // Pick out the next WORD of data
                        wRet = (static_cast<WORD>(m_pbBuffer[1]) << 8) | m_pbBuffer[0];

                        // Advance to the next WORD of data, if any
                        m_pbBuffer += sizeof(WORD);
                        m_uBuffer -= sizeof(WORD);

                        if (!m_uBuffer)
                            TRACE("ATA: All data read\n");
                    }

                    break;
                }

                case 0xf1:  wRet = m_sRegs.bError;          /*TRACE("ATA: READ error register\n");*/    break;
                case 0xf2:  wRet = m_sRegs.bSectorCount;    /*TRACE("ATA: READ sector count\n");*/      break;
                case 0xf3:  wRet = m_sRegs.bSector;         /*TRACE("ATA: READ sector number\n");*/     break;
                case 0xf4:  wRet = m_sRegs.bCylinderLow;    /*TRACE("ATA: READ cylinder low\n");*/      break;
                case 0xf5:  wRet = m_sRegs.bCylinderHigh;   /*TRACE("ATA: READ cylinder high\n");*/     break;
                case 0xf6:  wRet = m_sRegs.bDeviceHead;     /*TRACE("ATA: READ device/head\n");*/       break;

                // Status register
                case 0xf7:
                {
//                  TRACE("ATA: READ status\n");

                    // Toggle the index bit in the status to make it look like the disk is spinning
                    static int nPulse = 0;
                    if (!m_fAsleep && !(++nPulse %= 10))
                        m_sRegs.bStatus ^= ATA_STATUS_INDEX;

                    // Update the DRQ bit to show whether data is expected or available
                    m_sRegs.bStatus &= ~ATA_STATUS_DRQ;
                    if (m_uBuffer)
                        m_sRegs.bStatus |= ATA_STATUS_DRQ;

                    // Return the current status
                    wRet = m_sRegs.bStatus;

                    // If the request isn't for this device, return nothing
                    // ToDo: check for our actual device number
                    if ((m_sRegs.bDeviceHead & 0xff) >= 0xb0)
                        wRet = 0;
                    break;
                }

                default:
                    TRACE("ATA: Unhandled read from %#04x\n", wPort_);
                    break;
            }
            break;

        case 3:
            switch (wPort_ & 0xff)
            {
                // Alternate status register
                case 0xf6:
                    wRet = m_sRegs.bStatus;
                    break;

                case 0xf7:
                    TRACE("ATA: READ Drive Address (%#02x)\n", m_sRegs.bDriveAddress);
                    wRet = m_sRegs.bDriveAddress;
                    break;

                default:
                    TRACE("ATA: Unhandled read from %#04x\n", wPort_);
                    break;
            }
            break;
    }

    return wRet;
}


void CATADevice::Out (WORD wPort_, WORD wVal_)
{
    // Do nothing if the device is currently busy  (can't happen currently)
    if (m_sRegs.bStatus & ATA_STATUS_BUSY)
        return;

    BYTE bVal = wVal_ & 0xff;

    switch ((wPort_ >> 8) & 3)
    {
        case 1:
        {
            switch (wPort_ & 0xff)
            {
                case 0xf0:
                {
                    // Data expected?
                    if (m_uBuffer)
                    {
                        m_pbBuffer[0] = wVal_ & 0xff;
                        m_pbBuffer[1] = wVal_ >> 8;

                        m_pbBuffer += sizeof(WORD);
                        m_uBuffer -= sizeof(WORD);

                        // Received eveything we need?
                        if (!m_uBuffer)
                        {
                            TRACE("ATA: Received all data\n");

                            // What we do with the data depends on the current command
                            switch (m_sRegs.bCommand)
                            {
                                case 0x30:  // Write Sectors (with retries)
                                case 0x31:  // Write Sectors (without retries)
                                case 0x32:  // Write Long (with retries)
                                case 0x33:  // Write Long (without retries)
                                case 0x3c:  // Write Verify
                                case 0xc5:  // Write Multiple
                                case 0xe9:  // Write Same
                                {
                                    // Write the full sector
                                    if (!ReadWriteSector(true))
                                    {
                                        // Flag an error if the write failed
                                        m_sRegs.bStatus |= ATA_STATUS_ERROR;
                                        m_sRegs.bError = ATA_ERROR_UNC;
                                    }

                                    // Multi-sector write?
                                    else if (--m_sRegs.bSectorCount)
                                    {
                                        TRACE(" %d sectors left in multi-sector write...\n", m_sRegs.bSectorCount);

                                        if (++m_sRegs.bSector > m_sGeometry.uSectors)
                                        {
                                            m_sRegs.bSector = 1;

                                            if (++m_sRegs.bDeviceHead == m_sGeometry.uHeads)
                                            {
                                                m_sRegs.bDeviceHead = 0;

                                                if (!++m_sRegs.bCylinderLow)
                                                    m_sRegs.bCylinderHigh++;
                                            }
                                        }

                                        // Set the sector buffer pointer and how much we have available to read
                                        m_pbBuffer = m_abSectorData;
                                        m_uBuffer = sizeof m_abSectorData;
                                    }
                                }
                                break;

                                // Write Buffer
                                case 0xe8:
                                    // Nothing more to do
                                    break;

                                // Format Track
                                case 0x50:
                                    break;
                            }
                        }
                    }

                    return;
                }

                case 0xf1:
                    TRACE("ATA: WRITE PRECOMPENSATION\n");
                    break;

                case 0xf2:
                    TRACE("ATA: WRITE sector count = %#02x\n", bVal);
                    m_sRegs.bSectorCount = bVal;
                    break;

                case 0xf3:
                    TRACE("ATA: WRITE sector number = %#02x\n", bVal);
                    m_sRegs.bSector = bVal;
                    break;

                case 0xf4:
                    TRACE("ATA: WRITE cylinder low = %#02x\n", bVal);
                    m_sRegs.bCylinderLow = bVal;
                    break;

                case 0xf5:
                    TRACE("ATA: WRITE cylinder high = %#02x\n", bVal);
                    m_sRegs.bCylinderHigh = bVal;
                    break;

                case 0xf6:
                {
                    TRACE("ATA: WRITE device/head = %#02x\n", bVal);
                    m_sRegs.bDeviceHead = bVal;

                    // Set the relevant device active bit in bit 0 or 1
                    m_sRegs.bDriveAddress = (!(wVal_ & 0x10)) ? 0x0001 : 0x0002;

                    // Store the head selected in bits 2 to 5 (bit 7 is always set)
                    m_sRegs.bDriveAddress |= 0x80 | (wVal_ & 0xf) << 2;

                    // Device ready and selected head settled
                    m_sRegs.bStatus = ATA_STATUS_DRDY|ATA_STATUS_DSC;
                }
                break;

                case 0xf7:
                {
                    // Assume the command will succeed for now
                    m_sRegs.bStatus = ATA_STATUS_DRDY|ATA_STATUS_DSC;

                    switch (m_sRegs.bCommand = bVal)
                    {
                        case 0x08:
//                          TRACE("ATA: Disk command: Device Reset\n");
#if 1
                            // No ATAPI support
                            m_sRegs.bStatus = ATA_STATUS_DRDY|ATA_STATUS_ERROR;
                            m_sRegs.bError = ATA_ERROR_ABRT;
#else
                            m_sRegs.bError = 0x01;
                            m_sRegs.bSectorCount = m_sRegs.bSector = 0x01;
                            m_sRegs.bDeviceHead = 0x00;

                            // ATAPI supported, so set up the signature
                            m_sRegs.bCylinderLow = 0x14;
                            m_sRegs.bCylinderHigh = 0xeb;
#endif
                            break;

                        case 0x10:  // Recalibrate
                            if (!ReadWriteSector(false))
                            {
                                m_sRegs.bStatus = ATA_STATUS_DRDY|ATA_STATUS_ERROR;
                                m_sRegs.bError = ATA_ERROR_TK0NF;
                            }
                            break;

                        case 0x20:  // Read Sectors (with retries)
                        case 0x21:  // Read Sectors (without retries)
                        case 0x22:  // Read Long (with retries)
                        case 0x23:  // Read Long (without retries)
                        case 0x40:  // Read Verify Sectors (with retries)
                        case 0x41:  // Read Verify Sectors (without retries)
                        case 0xc4:  // Read Multiple
                        {
                            TRACE("ATA: Disk command: Read Sectors/Long/Multiple (with/without retry)\n");

                            // Read Long can only handle single sectors
                            if ((bVal & ~1) == 0x22 && m_sRegs.bSectorCount != 1)
                            {
                                TRACE("ATA: Read Long sector count must be 1!\n");
                                m_sRegs.bStatus |= ATA_STATUS_ERROR;
                                m_sRegs.bError = ATA_ERROR_ABRT;
                                break;
                            }

                            // Multiple mode requires Set Multiple Mode to be used first
                            else if (bVal == 0xc4 && !m_nMultiples)
                            {
                                TRACE("ATA: Read Multiple block size must be specified first!\n");
                                m_sRegs.bStatus |= ATA_STATUS_ERROR;
                                m_sRegs.bError = ATA_ERROR_ABRT;
                                break;
                            }

                            if (!ReadWriteSector(false))
                            {
                                m_sRegs.bStatus |= ATA_STATUS_ERROR;
                                m_sRegs.bError = ATA_ERROR_UNC;
                                break;
                            }

                            // The data is only presented to the host if this is not a verify
                            if ((bVal & ~1) != 0x40)
                            {
                                // Set the sector buffer pointer and how much we have available to read
                                m_pbBuffer = m_abSectorData;
                                m_uBuffer = sizeof m_abSectorData;
                            }
                        }
                        break;

                        case 0x30:  // Write Sectors (with retries)
                        case 0x31:  // Write Sectors (without retries)
                        case 0x32:  // Write Long (with retries)
                        case 0x33:  // Write Long (without retries)
                        case 0x3c:  // Write Verify
                        case 0xc5:  // Write Multiple
                        case 0xe9:  // Write Same
                        {
                            TRACE("ATA: Disk command: Write Sectors/Long/Multiple (with/without retry)\n");

                            // Read Long can only handle single sectors
                            if ((bVal & ~1) == 0x32 && m_sRegs.bSectorCount != 1)
                            {
                                TRACE("ATA: Write Long sector count must be 1!\n");
                                m_sRegs.bStatus |= ATA_STATUS_ERROR;
                                m_sRegs.bError = ATA_ERROR_ABRT;
                                break;
                            }

                            // Multiple mode requires Set Multiple Mode to be used first
                            else if (bVal == 0xc5 && !m_nMultiples)
                            {
                                TRACE("ATA: Write Multiple block size must be specified first!\n");
                                m_sRegs.bStatus |= ATA_STATUS_ERROR;
                                m_sRegs.bError = ATA_ERROR_ABRT;
                                break;
                            }

                            TRACE("ATA: Disk command: Write Sectors With Retry\n");
                            memset(&m_abSectorData, 0, sizeof m_abSectorData);
                            m_sRegs.bStatus |= ATA_STATUS_DRQ;

                            // Set the sector buffer pointer and how much space we have available for writing
                            m_pbBuffer = m_abSectorData;
                            m_uBuffer = sizeof m_abSectorData;
                        }
                        break;

                        case 0x90:
                            TRACE("ATA: Disk command: Execute Device Diagnostic\n");

                            // Device 0 present, device 1 passed/absent
                            m_sRegs.bError = 1;
                            break;

                        case 0x91:
                            TRACE("ATA: Disk command: Initialize Device Parameters\n");

                            // BDOS doesn't use these, so we'll implement them later!
                            TRACE("  Sectors per track: %d, Heads = %d\n", m_sRegs.bSectorCount, (m_sRegs.bDeviceHead & 0x0f) + 1);

                            break;

                        case 0xa0:
                            TRACE("ATA: Disk command: ATAPI Packet Command\n");
                            m_sRegs.bStatus = ATA_STATUS_DRDY|ATA_STATUS_ERROR;
                            m_sRegs.bError = ATA_ERROR_ABRT;
                            break;

                        case 0xc6:
                            TRACE("ATA: Disk command: Set Multiple Mode (to %d sectors per block)\n", m_sRegs.bSectorCount);
                            m_nMultiples = m_sRegs.bSectorCount;
                            break;
                            
                        case 0x94:
                        case 0xe0:
                            TRACE("ATA: Disk command: Standby Immediate\n");
                            Frame::SetStatus("HDD now in standby mode");
                            m_fAsleep = true;
                            break;

                        case 0x95:
                        case 0xe1:
                            TRACE("ATA: Disk command: Idle Immediate\n");
                            break;

                        case 0x96:
                        case 0xe2:
                            TRACE("ATA: Disk command: Standby (%d)\n", m_sRegs.bSectorCount);
                            break;

                        case 0x97:
                        case 0xe3:
                            TRACE("ATA: Disk command: Idle (%#02x)\n", m_sRegs.bSectorCount);
                            break;

                        case 0xe4:
                            TRACE("ATA: Disk command: Read Buffer\n");
                            m_pbBuffer = m_abSectorData;
                            m_uBuffer = sizeof m_abSectorData;
                            break;

                        case 0x98:
                        case 0xe5:
                            TRACE("ATA: Disk command: Check Power Mode\n");
                            m_sRegs.bSectorCount = m_fAsleep ? 0x00 : 0xff;
                            break;

                        case 0x99:
                        case 0xe6:
                            TRACE("ATA: Disk command: Sleep\n");
                            Frame::SetStatus("HDD now in sleep mode");
                            m_fAsleep = true;
                            break;

                        case 0xe8:
                            TRACE("ATA: Disk command: Write Buffer\n");
                            m_pbBuffer = m_abSectorData;
                            m_uBuffer = sizeof m_abSectorData;
                            break;

                        case 0xec:
                        {
                            TRACE("ATA: Disk command: IDENTIFY\n");
                            memcpy(&m_abSectorData, &m_abIdentity, sizeof(m_abSectorData));
                            m_pbBuffer = m_abSectorData;
                            m_uBuffer = sizeof(m_abSectorData);
                        }
                        break;

                        case 0xef:
                            TRACE("ATA: Disk write: Set features (%#02x)\n", m_sRegs.bFeatures);

                            switch (m_sRegs.bFeatures)
                            {
                                case 0x01:  TRACE(" Enable 8-bit data transfers\n");                                break;
                                case 0x02:  TRACE(" Enable write cache\n");                                         break;
                                case 0x03:  TRACE(" Set transfer mode (to %d)\n", m_sRegs.bSectorCount);            break;
                                case 0x33:  TRACE(" Disable retries\n");                                            break;
                                case 0x44:  TRACE(" Set Read Long vendor length to %d\n", m_sRegs.bSectorCount);    break;
                                case 0x54:  TRACE(" Set cache segments to %d\n", m_sRegs.bSectorCount);             break;
                                case 0x55:  TRACE(" Disable read look-ahead feature\n");                            break;
                                case 0x66:  TRACE(" Disable reverting to power-on defaults\n");                     break;
                                case 0x77:  TRACE(" Disable ECC\n");                                                break;
                                case 0x81:  TRACE(" Disable 8-bit transfers\n");                                    break;
                                case 0x82:  TRACE(" Disable write-cache\n");                                        break;
                                case 0x88:  TRACE(" Enable ECC\n");                                                 break;
                                case 0x99:  TRACE(" Enable retries\n");                                             break;
                                case 0xaa:  TRACE(" Enable read look-ahead feature\n");                             break;
                                case 0xab:  TRACE(" Set maximum pre-fetch to %d\n", m_sRegs.bSectorCount);          break;
                                case 0xbb:  TRACE(" 4 bytes of vendor bytes with Read/Write Long\n");               break;
                                case 0xcc:  TRACE(" Enable reverting to power-on defaults\n");                      break;

                                default:    TRACE(" Unknown features set!\n");                                      break;
                            }
                            break;

                        default:
                            switch (wVal_ & 0xf0)
                            {
                                case 0x10:
                                    TRACE("ATA: Disk command: Recalibrate (%s)\n", m_sRegs.bSector ? "CHS" : "LBA");
                                    break;

                                case 0x70:
                                {
                                    WORD wCylinder = (static_cast<WORD>(m_sRegs.bCylinderHigh) << 8) | m_sRegs.bCylinderLow;
                                    BYTE bHead = (m_sRegs.bDriveAddress >> 2) & 0x0f;
                                    TRACE("ATA: Disk command: Seek (to %u:%d:%d)\n", wCylinder, bHead, m_sRegs.bSector);
                                }
                                break;

                                default:
                                {
                                    // We'll log recognised, but unimplemented commands
                                    switch (m_sRegs.bCommand)
                                    {
                                        case 0x00:  TRACE("ATA: Command: NOP\n");                                   break;
                                        case 0x50:  TRACE("ATA: Command: Format Track (removable)\n");              break;
                                        case 0x92:  TRACE("ATA: Command: Download Microcode\n");                    break;
                                        case 0xa1:  TRACE("ATA: Command: ATAPI Identify\n");                        break;
                                        case 0xb0:  TRACE("ATA: Command: SMART\n");                                 break;
                                        case 0xc8:  TRACE("ATA: Command: Read DMA (with retries)\n");               break;
                                        case 0xc9:  TRACE("ATA: Command: Read DMA (without retries)\n");            break;
                                        case 0xca:  TRACE("ATA: Command: Write DMA (with retries)\n");              break;
                                        case 0xcb:  TRACE("ATA: Command: Write DMA (without retries)\n");           break;
                                        case 0xdb:  TRACE("ATA: Command: Acknowledge Media Changes (removable)\n"); break;
                                        case 0xdc:  TRACE("ATA: Command: Post-Boot (removable)\n");                 break;
                                        case 0xdd:  TRACE("ATA: Command: Pre-Boot (removable)\n");                  break;
                                        case 0xde:  TRACE("ATA: Command: Door Lock (removable)\n");                 break;
                                        case 0xdf:  TRACE("ATA: Command: Door Unlock (removable)\n");               break;
                                        case 0xed:  TRACE("ATA: Command: Media Eject (removable)\n");               break;
                                        case 0xee:  TRACE("ATA: Command: Identify DMA\n");                          break;

                                        default:    TRACE("ATA: Unrecognised command (%#02x)\n", bVal);             break;
                                    }

                                    // Abort the command
                                    m_sRegs.bStatus = ATA_STATUS_DRDY|ATA_STATUS_ERROR;
                                    m_sRegs.bError = ATA_ERROR_ABRT;

                                    break;
                                }
                            }
                            break;
                    }
                    break;
                }
                break;

                default:
                    TRACE("ATA: Unhandled write to %#04x with %#02x\n", wPort_, bVal);
            }
            break;
        }

        case 3:
        {
            switch (wPort_ & 0xff)
            {
                case 0xf6:
                {
                    TRACE("ATA: Device control register set to %#02x\n", bVal);
                    m_sRegs.bDeviceControl = bVal;

                    // If SRST is set, perform a soft reset
                    if (m_sRegs.bDeviceControl & 0x04)
                        Reset();

                    break;
                }

                default:
                    TRACE("ATA: Unhandled write to %#04x with %#02x\n", wPort_, bVal);
            }
        }

        default:
            TRACE("ATA: Unhandled write to %#04x with %#02x\n", wPort_, bVal);
    }
}

bool CATADevice::ReadWriteSector (bool fWrite_)
{
    bool fRet = false;

    WORD wCylinder = (static_cast<WORD>(m_sRegs.bCylinderHigh) << 8) | m_sRegs.bCylinderLow;
    BYTE bHead = (m_sRegs.bDriveAddress >> 2) & 0x0f, bSector = m_sRegs.bSector;

    // Only process requests within the disk geometry
    if (bSector && bSector <= m_sGeometry.uSectors && bHead < m_sGeometry.uHeads && wCylinder < m_sGeometry.uCylinders)
    {
        // Calculate the logical block number from the CHS position
        UINT uSector = (wCylinder * m_sGeometry.uHeads + bHead) * m_sGeometry.uSectors + (bSector - 1);

        TRACE("%s CHS %u:%u:%u  [LBA=%u]\n", fWrite_ ? "Writing" : "Reading", wCylinder, bHead, bSector, uSector);
        if (fWrite_)
            return WriteSector(uSector, m_abSectorData);
        else
            return ReadSector(uSector, m_abSectorData);
    }

    return fRet;
}
