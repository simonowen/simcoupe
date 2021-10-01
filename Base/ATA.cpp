// Part of SimCoupe - A SAM Coupe emulator
//
// ATA.cpp: ATA hard disk emulation
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

#include "SimCoupe.h"

#include "ATA.h"
#include "Frame.h"

// ToDo: support secondary device on the same interface

ATADevice::ATADevice()
{
    Reset();
}

// Device hard reset
void ATADevice::Reset(bool fSoft_/*=false*/)
{
    // Set specific registers to zero
    m_sRegs.bCylinderLow = m_sRegs.bCylinderHigh = m_sRegs.bDeviceHead = 0;

    // Set some registers to one
    m_sRegs.bError = m_sRegs.bSectorCount = m_sRegs.bSector = 0x01;

    // Device is settled and ready
    m_sRegs.bStatus = ATA_STATUS_DRDY | ATA_STATUS_DSC;

    // No data available for reading or required for writing
    m_data_offset = m_sector_data.size();

    // Set appropriate 8-bit data transfer state
    m_f8bit = m_f8bitOnReset = fSoft_ ? m_f8bitOnReset : false;
}


uint16_t ATADevice::In(uint16_t wPort_)
{
    uint16_t wRet = 0xffff;

    // If the request isn't for our device, ignore it
    if ((m_sRegs.bDeviceHead ^ m_bDevice) & ATA_DEVICE_MASK)
        return 0x0000;

    switch (~wPort_ & ATA_CS_MASK)
    {
    case ATA_CS0:
        switch (wPort_ & ATA_DA_MASK)
        {
            // Data register
        case 0:
        {
            // Return zero if no more data is available
            if (m_data_offset != m_sector_data.size())
            {
                // Read a byte
                m_sRegs.wData = m_sector_data[m_data_offset++];

                // In 16-bit mode read a second byte
                if (!m_f8bit)
                    m_sRegs.wData |= m_sector_data[m_data_offset++] << 8;

                if (m_data_offset == m_sector_data.size())
                {
                    TRACE("ATA: All data read\n");
                    if (m_sRegs.bCommand != 0xec && --m_sRegs.bSectorCount > 0)
                    {
                        NextSector();

                        if (!ReadWriteSector(false))
                        {
                            m_sRegs.bStatus |= ATA_STATUS_ERROR;
                            m_sRegs.bError = ATA_ERROR_UNC;
                            break;
                        }
                        else
                        {
                            m_data_offset = 0;
                        }
                    }
                }
            }

            // Return the data register
            wRet = m_sRegs.wData;

            break;
        }

        case 1:  wRet = m_sRegs.bError;             break;
        case 2:  wRet = m_sRegs.bSectorCount;       break;
        case 3:  wRet = m_sRegs.bSector;            break;
        case 4:  wRet = m_sRegs.bCylinderLow;       break;
        case 5:  wRet = m_sRegs.bCylinderHigh;      break;
        case 6:  wRet = m_sRegs.bDeviceHead | 0xa0; break;  // bits 7+5 always set

        // Status register
        case 7:
        {
            // Update the DRQ bit to show whether data is expected or available
            if (m_data_offset != m_sector_data.size())
                m_sRegs.bStatus |= ATA_STATUS_DRQ;
            else
                m_sRegs.bStatus &= ~ATA_STATUS_DRQ;

            // Return the current status
            wRet = m_sRegs.bStatus;

            break;
        }

        default:
            TRACE("ATA: Unhandled read from {:04x}\n", wPort_);
            break;
        }
        break;

    case ATA_CS1:
        switch (wPort_ & ATA_DA_MASK)
        {
            // Alternate status register
        case 6:
            wRet = m_sRegs.bStatus;
            break;

        case 7:
            TRACE("ATA: READ Drive Address\n");

            // Only older HDD devices respond to this, newer ones such as CF cards don't
            if (m_fLegacy)
            {
                // Bit 7 set, bit 6 clear (write gate), inverted head bits in b5-2
                wRet = 0x80 | ((~m_sRegs.bDeviceHead & ATA_HEAD_MASK) << 2);

                // Set bits 0+1, leaving the active device bit clear
                if (~m_sRegs.bDeviceHead & ATA_DEVICE_MASK) // primary?
                    wRet |= 0x02;
                else
                    wRet |= 0x01;
            }

            break;

        default:
            TRACE("ATA: Unhandled read from {:04x}\n", wPort_);
            break;
        }
        break;
    }

    return wRet;
}


void ATADevice::Out(uint16_t wPort_, uint16_t wVal_)
{
    uint8_t bVal = wVal_ & 0xff;

    switch (~wPort_ & ATA_CS_MASK)
    {
    case ATA_CS0:
    {
        // Ignore base register writes in reset condition
        if (m_sRegs.bDeviceControl & ATA_DCR_SRST)
            break;

        switch (wPort_ & ATA_DA_MASK)
        {
        case 0:
        {
            // Data expected?
            if (m_data_offset != m_sector_data.size())
            {
                // Write a byte
                m_sector_data[m_data_offset++] = wVal_ & 0xff;

                // In 16-bit mode write a second byte
                if (!m_f8bit)
                    m_sector_data[m_data_offset++] = wVal_ >> 8;

                // Received eveything we need?
                if (m_data_offset == m_sector_data.size())
                {
                    TRACE("ATA: Received all data\n");

                    // What we do with the data depends on the current command
                    switch (m_sRegs.bCommand)
                    {
                    case 0x30:  // Write Sectors (with retries)
                    case 0x31:  // Write Sectors
                    case 0xc5:  // Write Multiple
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
                            TRACE(" {} sectors left in multi-sector write...\n", m_sRegs.bSectorCount);

                            NextSector();

                            // Set the sector buffer pointer and how much we have available to read
                            m_data_offset = 0;
                        }
                    }
                    break;
                    }
                }
            }

            return;
        }

        case 1:
            TRACE("ATA: WRITE features = {:02x}\n", bVal);
            m_sRegs.bFeatures = bVal;
            break;

        case 2:
            TRACE("ATA: WRITE sector count = {:02x}\n", bVal);
            m_sRegs.bSectorCount = bVal;
            break;

        case 3:
            TRACE("ATA: WRITE sector number = {:02x}\n", bVal);
            m_sRegs.bSector = bVal;
            break;

        case 4:
            TRACE("ATA: WRITE cylinder low = {:02x}\n", bVal);
            m_sRegs.bCylinderLow = bVal;
            break;

        case 5:
            TRACE("ATA: WRITE cylinder high = {:02x}\n", bVal);
            m_sRegs.bCylinderHigh = bVal;
            break;

        case 6:
        {
            TRACE("ATA: WRITE device/head = {:02x}\n", bVal);
            m_sRegs.bDeviceHead = bVal;

            // Device ready and selected head settled
            m_sRegs.bStatus = ATA_STATUS_DRDY | ATA_STATUS_DSC;
        }
        break;

        case 7:
        {
            // Assume the command will succeed for now
            m_sRegs.bStatus = ATA_STATUS_DRDY | ATA_STATUS_DSC;

            // Clear the error register
            m_sRegs.bError = 0;

            // Ignore the command if it's not for our device, unless it's Execute Device Diagnostic
            if (((m_sRegs.bDeviceHead ^ m_bDevice) & ATA_DEVICE_MASK) && m_sRegs.bCommand != 0x90)
                break;

            switch (m_sRegs.bCommand = bVal)
            {
            case 0x20:  // Read Sectors (with retries)
            case 0x21:  // Read Sectors
            case 0x40:  // Read Verify Sectors (with retries)
            case 0x41:  // Read Verify Sectors
            case 0xc4:  // Read Multiple
            {
                TRACE("ATA: Disk command: Read Sectors\n");

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
                    m_data_offset = 0;
                }
            }
            break;

            case 0x30:  // Write Sectors (with retries)
            case 0x31:  // Write Sectors
            case 0xc5:  // Write Multiple
            {
                TRACE("ATA: Disk command: Write Sectors\n");

                // Clear the sector buffer and indicate we're ready to receive data
                memset(&m_sector_data, 0, sizeof(m_sector_data));
                m_sRegs.bStatus |= ATA_STATUS_DRQ;

                // Set the sector buffer pointer and how much space we have available for writing
                m_data_offset = 0;
            }
            break;

            case 0x90:
                TRACE("ATA: Disk command: Execute Device Diagnostic\n");
                m_sRegs.bError = 1;  // device 0 present, device 1 passed/absent
                break;

            case 0xe0:  // Standby Immediate
            case 0xe2:  // Standby
                TRACE("ATA: Disk command: Standby [Immediate]\n");
                break;

            case 0xe1:  // Idle Immediate
            case 0xe3:  // Idle
                TRACE("ATA: Disk command: Idle [Immediate]\n");
                break;

            case 0xe5:  // Check Power Mode
                TRACE("ATA: Disk command: Check Power Mode\n");
                m_sRegs.bSectorCount = 0xff;  // device is active
                break;

            case 0xe6:  // Sleep
                TRACE("ATA: Disk command: Sleep\n");
                break;

            case 0xec:
            {
                TRACE("ATA: Disk command: IDENTIFY\n");
                memcpy(&m_sector_data, &m_sIdentify, sizeof(m_sIdentify));
                m_data_offset = 0;
            }
            break;

            case 0xef:
                TRACE("ATA: Disk write: Set features ({:02x})\n", m_sRegs.bFeatures);

                switch (m_sRegs.bFeatures)
                {
                case 0x01:
                    TRACE(" Enable 8-bit data transfers\n");
                    m_f8bit = true;
                    break;

                case 0x66:
                    TRACE(" Use current features as defaults\n");
                    m_f8bitOnReset = m_f8bit;
                    break;

                case 0x81:
                    TRACE(" Disable 8-bit data transfers\n");
                    m_f8bit = false;
                    m_data_offset = m_sector_data.size();
                    break;

                case 0xcc:
                    TRACE(" Restoring power-on default features\n");
                    m_f8bitOnReset = false;
                    break;

                default:
                    TRACE(" !!! Unsupported feature!\n");
                    m_sRegs.bStatus = ATA_STATUS_DRDY | ATA_STATUS_ERROR;
                    m_sRegs.bError = ATA_ERROR_ABRT;
                    break;
                }
                break;

            default:
                TRACE("ATA: !!! Unrecognised command ({:02x})\n", bVal);
                m_sRegs.bStatus = ATA_STATUS_DRDY | ATA_STATUS_ERROR;
                m_sRegs.bError = ATA_ERROR_ABRT;

                break;
            }
            break;
        }
        break;

        default:
            TRACE("ATA: Unhandled write to {:04x} with {:02x}\n", wPort_, bVal);
            break;
        }
        break;
    }

    case ATA_CS1:
    {
        switch (wPort_ & ATA_DA_MASK)
        {
        case 6:
        {
            TRACE("ATA: Device control register set to {:02x}\n", bVal);
            m_sRegs.bDeviceControl = bVal;

            // If SRST is set, perform a soft reset
            if (m_sRegs.bDeviceControl & ATA_DCR_SRST)
            {
                TRACE(" Performing software reset\n");
                Reset(true);
            }

            break;
        }

        default:
            TRACE("ATA: Unhandled write to {:04x} with {:02x}\n", wPort_, bVal);
            break;
        }
        break;
    }

    default:
        TRACE("ATA: Unhandled write to {:04x} with {:02x}\n", wPort_, bVal);
        break;
    }
}


bool ATADevice::ReadWriteSector(bool fWrite_)
{
    unsigned int uSector = 0;

    // LBA request?
    if (m_sRegs.bDeviceHead & 0x40)
    {
        // Form the 28-bit LBA address
        uSector = ((m_sRegs.bDeviceHead & 0x0f) << 24) | (m_sRegs.bCylinderHigh << 16) | (m_sRegs.bCylinderLow << 8) | m_sRegs.bSector;

        // Fail if the location is outside the disk geometry
        if (uSector >= m_sGeometry.uTotalSectors)
            return false;

        TRACE("{} LBA={}\n", fWrite_ ? "Writing" : "Reading", uSector);
    }
    else // CHS request
    {
        // Collect the CHS values
        uint16_t wCylinder = (static_cast<uint16_t>(m_sRegs.bCylinderHigh) << 8) | m_sRegs.bCylinderLow;
        uint8_t bHead = m_sRegs.bDeviceHead & ATA_HEAD_MASK;
        uint8_t bSector = m_sRegs.bSector;

        // Fail if the location is outside the disk geometry
        if (!bSector || bSector > m_sGeometry.uSectors || bHead > m_sGeometry.uHeads || wCylinder > m_sGeometry.uCylinders)
            return false;

        // Calculate the logical block number from the CHS position
        uSector = (wCylinder * m_sGeometry.uHeads + bHead) * m_sGeometry.uSectors + (bSector - 1);
        TRACE("{} CHS {}:{}:{}  [LBA={}]\n", fWrite_ ? "Writing" : "Reading", wCylinder, bHead, bSector, uSector);
    }

    if (fWrite_)
        return WriteSector(uSector, m_sector_data.data());

    return ReadSector(uSector, m_sector_data.data());
}

void ATADevice::NextSector()
{
    if (m_sRegs.bDeviceHead & 0x40)
    {
        if (!++m_sRegs.bSector &&
            !++m_sRegs.bCylinderLow &&
            !++m_sRegs.bCylinderHigh &&
            !(++m_sRegs.bDeviceHead & 0x0f))
        {
            m_sRegs.bDeviceHead = (m_sRegs.bDeviceHead - 1) & 0xf0;
        }
    }
    else if (++m_sRegs.bSector > m_sGeometry.uSectors)
    {
        m_sRegs.bSector = 1;

        m_sRegs.bDeviceHead =
            (m_sRegs.bDeviceHead & ~ATA_HEAD_MASK) |
            ((m_sRegs.bDeviceHead + 1) & ATA_HEAD_MASK);

        if ((m_sRegs.bDeviceHead & ATA_HEAD_MASK) == m_sGeometry.uHeads)
        {
            if (!++m_sRegs.bCylinderLow)
            {
                m_sRegs.bCylinderHigh++;
            }

            if (((m_sRegs.bCylinderHigh << 8) | m_sRegs.bCylinderLow) == m_sGeometry.uCylinders)
            {
                m_sRegs.bCylinderHigh = m_sRegs.bCylinderLow = 0;
            }
        }
    }
}

void ATADevice::SetIdentifyData(IDENTIFYDEVICE* pid_)
{
    // Do we have data to set?
    if (pid_)
    {
        // Copy the supplied data
        memcpy(&m_sIdentify, pid_, sizeof(m_sIdentify));

        // Update CHS
        m_sGeometry.uCylinders = m_sIdentify.word[1];
        m_sGeometry.uHeads = m_sIdentify.word[3];
        m_sGeometry.uSectors = m_sIdentify.word[6];

        return;
    }

    // Calculate suitable CHS values for the sector count
    CalculateGeometry(&m_sGeometry);

    // Wipe any existing data
    memset(&m_sIdentify, 0, sizeof(m_sIdentify));

    // Advertise CFA features
    m_sIdentify.word[0] = 0x848a;

    // CHS values
    m_sIdentify.word[1] = m_sGeometry.uCylinders;
    m_sIdentify.word[3] = m_sGeometry.uHeads;
    m_sIdentify.word[6] = m_sGeometry.uSectors;

    // Form an 8-character string from the current date, to use as firmware revision
    auto now = time(nullptr);
    auto date_str = fmt::format("{:%Y%m%d}", fmt::localtime(now));
    auto serial_str = fmt::format("{}.{}.{}", SIMCOUPE_MAJOR_VERSION, SIMCOUPE_MINOR_VERSION, SIMCOUPE_PATCH_VERSION);

    // Serial number, firmware revision and model number
    SetIdentifyString(serial_str, &m_sIdentify.word[10], 20);
    SetIdentifyString(date_str, &m_sIdentify.word[23], 8);
    SetIdentifyString("SimCoupe Device", &m_sIdentify.word[27], 40);

    // Read/write multiple supports 1 sector blocks
    m_sIdentify.word[47] = 1;

    // LBA supported
    m_sIdentify.word[49] = (1 << 9);

    // Current override CHS values
    m_sIdentify.word[53] = (1 << 0);
    m_sIdentify.word[54] = m_sIdentify.word[1];
    m_sIdentify.word[55] = m_sIdentify.word[3];
    m_sIdentify.word[56] = m_sIdentify.word[6];

    // Max CHS sector count is just C*H*S with maximum values for each
    unsigned int uMaxSectorsCHS = 16383 * 16 * 63;
    unsigned int uTotalSectorsCHS = (m_sGeometry.uTotalSectors > uMaxSectorsCHS) ? uMaxSectorsCHS : m_sGeometry.uTotalSectors;
    m_sIdentify.word[57] = static_cast<uint16_t>(uTotalSectorsCHS & 0xffff);
    m_sIdentify.word[58] = static_cast<uint16_t>((uTotalSectorsCHS >> 16) & 0xffff);

    // Max LBA28 sector count is 0x0fffffff
    unsigned int uMaxSectorsLBA28 = (1U << 28) - 1;
    unsigned int uTotalSectorsLBA28 = (m_sGeometry.uTotalSectors > uMaxSectorsLBA28) ? uMaxSectorsLBA28 : m_sGeometry.uTotalSectors;
    m_sIdentify.word[60] = uTotalSectorsLBA28 & 0xffff;
    m_sIdentify.word[61] = (uTotalSectorsLBA28 >> 16) & 0xffff;

    // Advertise CFA features, for 8-bit mode
    m_sIdentify.word[83] |= (1 << 2) | (1 << 14);
    m_sIdentify.word[84] |= (1 << 14);
    m_sIdentify.word[86] |= (1 << 2);
    m_sIdentify.word[87] |= (1 << 14);
}


// Calculate a suitable CHS geometry covering the supplied number of sectors
/*static*/ void ATADevice::CalculateGeometry(ATA_GEOMETRY* pg_)
{
    unsigned int uCylinders, uHeads, uSectors;

    // If the sector count is exactly divisible by 16*63, use them for heads and sectors
    if ((pg_->uTotalSectors % (16 * 63)) == 0)
    {
        uHeads = 16;
        uSectors = 63;
    }
    else
    {
        // Start the head count to give balanced figures for smaller drives
        uHeads = (pg_->uTotalSectors >= 65536) ? 8 : (pg_->uTotalSectors >= 32768) ? 4 : 2;
        uSectors = 32;
    }

    // Loop until we're (ideally) within 1024 cylinders
    while ((pg_->uTotalSectors / uHeads / uSectors) > 1023)
    {
        if (uHeads < 16)
            uHeads *= 2;
        else if (uSectors != 63)
            uSectors = 63;
        else
            break;
    }

    // Calculate the cylinder limit at or below the total size
    uCylinders = pg_->uTotalSectors / uHeads / uSectors;

    // Update the supplied structure
    pg_->uCylinders = (uCylinders > 16383) ? 16383 : uCylinders;
    pg_->uHeads = uHeads;
    pg_->uSectors = uSectors;
}


/*static*/ void ATADevice::SetIdentifyString(const std::string& str , void* pv_, size_t cb_)
{
    auto pb = reinterpret_cast<uint8_t*>(pv_);

    // Fill with spaces, then copy the string over it (excluding the null terminator)
    memset(pb, ' ', cb_);
    memcpy(pb, str.data(), cb_ = str.length());

    // Byte-swap the string for the expected endian
    for (size_t i = 0; i < cb_; i += 2)
        std::swap(pb[i], pb[i + 1]);
}
