// Part of SimCoupe - A SAM Coupe emulator
//
// HardDisk.cpp: Hard disk abstraction layer
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
//
// Notes:
//  - HDF spec: http://web.archive.org/web/20080615063233/http://www.ramsoft.bbk.org/tech/rs-hdf.txt

#include "SimCoupe.h"

#include "HardDisk.h"
#include "IDEDisk.h"


HardDisk::HardDisk(const std::string& disk_path) :
    m_strPath(disk_path)
{
}

bool HardDisk::IsSDIDEDisk()
{
    // Check for the HDOS free space file-info-block in sector 1
    uint8_t ab[512];
    return ReadSector(1, ab) && !memcmp(ab + 14, "Free_space", 10);
}

bool HardDisk::IsBDOSDisk(bool* pfByteSwapped_)
{
    bool fBDOS = false, fByteSwapped = false;
    uint8_t ab[512];

    // Read the MBR for a possible BDOS boot sector
    if (ReadSector(0, ab))
    {
        // Clear bits 7 and 5 (case) for the signature check
        for (int i = 0; i < 4; i++) { ab[i + 0x000] &= ~0xa0; ab[i + 0x100] &= ~0xa0; }

        // Check for byte-swapped signature (Atom)
        if (!memcmp(ab + 0x000, "OBTO", 4))
            fBDOS = fByteSwapped = true;

        // Then normal signature (Atom Lite)
        else if (!memcmp(ab + 0x100, "BOOT", 4))
            fBDOS = true;
    }

    // Calculate the base sector position using the disk CHS geometry, which
    // is estimated from the LBA sector count when using CF cards and IDE HDDs.
    unsigned int uTotalSectors = m_sGeometry.uCylinders * m_sGeometry.uHeads * m_sGeometry.uSectors;
    unsigned int uBase = 1 + (uTotalSectors / 1600 / 32) + 1;

    // Check for BDOS signature in the first record
    if (!fBDOS && ReadSector(uBase, ab))
    {
        // Atom?
        if (!memcmp(ab + 232, "DBSO", 4))
            fBDOS = fByteSwapped = true;

        // Atom Lite? (or Trinity media under 8GB)
        else if (!memcmp(ab + 232, "BDOS", 4))
            fBDOS = true;
    }

    // Optionally return the byte-swap status
    if (pfByteSwapped_)
        *pfByteSwapped_ = fByteSwapped;

    return fBDOS;
}


struct RS_IDE
{
    char szSignature[6];                // RS-IDE
    uint8_t bEOF;                       // 0x1a
    uint8_t bRevision;                  // 0x10 for v1.0, 0x11 for v1.1
    uint8_t bFlags;                     // b0 = halved sector data, b1 = ATAPI (HDF 1.1+)
    uint8_t bOffsetLow, bOffsetHigh;    // Offset from start of file to HDD data
    uint8_t abReserved[11];             // Must be zero
                                        // Identify data follows: 106 bytes for HDF 1.0, 512 for HDF 1.1+
};


HDFHardDisk::HDFHardDisk(const std::string& disk_path)
    : HardDisk(disk_path)
{
}

/*static*/ std::unique_ptr<HardDisk> HardDisk::OpenObject(const std::string& disk_path, bool read_only)
{
    if (disk_path.empty())
        return nullptr;

    if (auto disk = std::make_unique<DeviceHardDisk>(disk_path); disk->Open(read_only))
    {
        return disk;
    }

    if (auto disk = std::make_unique<HDFHardDisk>(disk_path); disk->Open(read_only))
        disk.reset();

    return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool HDFHardDisk::Create(const std::string& disk_path, unsigned int uTotalSectors_)
{
    auto pDisk = std::make_unique<HDFHardDisk>(disk_path);
    return pDisk && pDisk->Create(uTotalSectors_);
}

bool HDFHardDisk::Create(unsigned int uTotalSectors_)
{
    bool ret = false;

    Close();

    // No filename?
    if (m_strPath.empty())
        return false;

    // HDF v1.1 header, including full identify sector
    unsigned int uDataOffset = sizeof(RS_IDE) + sizeof(m_sIdentify);
    RS_IDE sHeader = { {'R','S','-','I','D','E'}, 0x1a, 0x11, 0x00,
                       static_cast<uint8_t>(uDataOffset & 0xff), static_cast<uint8_t>(uDataOffset >> 8) };

    // Create the file in binary mode
    unique_FILE file = fopen(m_strPath.c_str(), "wb");
    if (file)
    {
        // Set the sector count, and generate suitable identify data
        m_sGeometry.uTotalSectors = uTotalSectors_;
        SetIdentifyData(nullptr);

        // Calculate the total disk data size
        off_t lDataSize = uTotalSectors_ * 512;
        uint8_t bNull = 0;

        // Write the header, and extend the file up to the full size
        ret = fwrite(&sHeader, sizeof(sHeader), 1, file) &&
            fwrite(&m_sIdentify, sizeof(m_sIdentify), 1, file) &&
            !fseek(file, lDataSize - sizeof(bNull), SEEK_CUR) &&
            fwrite(&bNull, sizeof(bNull), 1, file);

        file.reset();

        if (!ret)
        {
            std::error_code error;
            fs::remove(m_strPath, error);
        }
    }

    return ret;
}


bool HDFHardDisk::Open(bool read_only)
{
    Close();

    // No disk?
    if (m_strPath.empty())
        return false;

    // Open read-write, falling back on read-only (not ideal!)
    if ((!read_only && (m_file = fopen(m_strPath.c_str(), "r+b"))) || (m_file = fopen(m_strPath.c_str(), "rb")))
    {
        RS_IDE sHeader;

        // Read and check the header is valid/supported
        if (fread(&sHeader, 1, sizeof(sHeader), m_file) != sizeof(sHeader) || (sHeader.bFlags & 3) ||
            memcmp(sHeader.szSignature, "RS-IDE", sizeof(sHeader.szSignature)))
            TRACE("!!! Invalid or incompatible HDF file\n");
        else
        {
            struct stat st;

            // Clear out any existing identify data
            memset(&m_sIdentify, 0, sizeof(m_sIdentify));

            // Determine the offset to the device data and the sector size (fixed for now)
            m_uDataOffset = (sHeader.bOffsetHigh << 8) | sHeader.bOffsetLow;
            m_uSectorSize = 512;

            // Calculate how much of the header is identify data, and limit to the structure size
            unsigned int uIdentifyLen = m_uDataOffset - sizeof(sHeader);
            if (uIdentifyLen > sizeof(m_sIdentify))
                uIdentifyLen = sizeof(m_sIdentify);

            // Read the identify data
            if (m_uDataOffset < sizeof(sHeader) || fread(&m_sIdentify, 1, uIdentifyLen, m_file) != uIdentifyLen)
                TRACE("HDF data offset is invalid!\n");
            else if (fstat(fileno(m_file), &st) == 0)
            {
                m_sGeometry.uTotalSectors = static_cast<unsigned int>((st.st_size - m_uDataOffset) / m_uSectorSize);

                // Update the identify data
                SetIdentifyData(&m_sIdentify);
            }

            return true;
        }
    }

    Close();
    return false;
}

void HDFHardDisk::Close()
{
    m_file.reset();
}

bool HDFHardDisk::ReadSector(unsigned int uSector_, uint8_t* pb_)
{
    off_t lOffset = m_uDataOffset + static_cast<off_t>(uSector_)* m_uSectorSize;
    return m_file && !fseek(m_file, lOffset, SEEK_SET) && (fread(pb_, 1, m_uSectorSize, m_file) == m_uSectorSize);
}

bool HDFHardDisk::WriteSector(unsigned int uSector_, uint8_t* pb_)
{
    off_t lOffset = m_uDataOffset + static_cast<off_t>(uSector_)* m_uSectorSize;
    return m_file && !fseek(m_file, lOffset, SEEK_SET) && (fwrite(pb_, 1, m_uSectorSize, m_file) == m_uSectorSize);
}
