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


CHardDisk::CHardDisk(const char* path)
    : m_strPath(path)
{
}

bool CHardDisk::IsSDIDEDisk()
{
    // Check for the HDOS free space file-info-block in sector 1
    BYTE ab[512];
    return ReadSector(1, ab) && !memcmp(ab + 14, "Free_space", 10);
}

bool CHardDisk::IsBDOSDisk(bool* pfByteSwapped_)
{
    bool fBDOS = false, fByteSwapped = false;
    BYTE ab[512];

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
    UINT uTotalSectors = m_sGeometry.uCylinders * m_sGeometry.uHeads * m_sGeometry.uSectors;
    UINT uBase = 1 + (uTotalSectors / 1600 / 32) + 1;

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


typedef struct
{
    char szSignature[6];            // RS-IDE
    BYTE bEOF;                      // 0x1a
    BYTE bRevision;                 // 0x10 for v1.0, 0x11 for v1.1
    BYTE bFlags;                    // b0 = halved sector data, b1 = ATAPI (HDF 1.1+)
    BYTE bOffsetLow, bOffsetHigh;   // Offset from start of file to HDD data
    BYTE abReserved[11];            // Must be zero
                                    // Identify data follows: 106 bytes for HDF 1.0, 512 for HDF 1.1+
}
RS_IDE;


CHDFHardDisk::CHDFHardDisk(const char* pcszDisk_)
    : CHardDisk(pcszDisk_)
{
}

/*static*/ CHardDisk* CHardDisk::OpenObject(const char* pcszDisk_, bool fReadOnly_/*=false*/)
{
    CHardDisk* pDisk = nullptr;

    // Make sure we have a disk to try
    if (!pcszDisk_ || !*pcszDisk_)
        return nullptr;

    // Try for device path first
    if (!pDisk && (pDisk = new CDeviceHardDisk(pcszDisk_)) && !pDisk->Open(fReadOnly_))
    {
        delete pDisk;
        pDisk = nullptr;
    }

    // Try for HDF disk image
    if (!pDisk && (pDisk = new CHDFHardDisk(pcszDisk_)) && !pDisk->Open(fReadOnly_))
    {
        delete pDisk;
        pDisk = nullptr;
    }

    return pDisk;
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CHDFHardDisk::Create(const char* pcszDisk_, UINT uTotalSectors_)
{
    // Attempt to create the new disk
    CHDFHardDisk* pDisk = new CHDFHardDisk(pcszDisk_);
    bool fRet = pDisk && pDisk->Create(uTotalSectors_);

    // Delete the object and return the result
    delete pDisk;
    return fRet;
}

bool CHDFHardDisk::Create(UINT uTotalSectors_)
{
    bool fRet = false;

    Close();

    // No filename?
    if (m_strPath.empty())
        return false;

    // HDF v1.1 header, including full identify sector
    UINT uDataOffset = sizeof(RS_IDE) + sizeof(m_sIdentify);
    RS_IDE sHeader = { {'R','S','-','I','D','E'}, 0x1a, 0x11, 0x00,
                       static_cast<BYTE>(uDataOffset & 0xff), static_cast<BYTE>(uDataOffset >> 8) };

    // Create the file in binary mode
    FILE* pFile = fopen(m_strPath.c_str(), "wb");
    if (pFile)
    {
        // Set the sector count, and generate suitable identify data
        m_sGeometry.uTotalSectors = uTotalSectors_;
        SetIdentifyData(nullptr);

        // Calculate the total disk data size
        off_t lDataSize = uTotalSectors_ * 512;
        BYTE bNull = 0;

        // Write the header, and extend the file up to the full size
        fRet = fwrite(&sHeader, sizeof(sHeader), 1, pFile) &&
            fwrite(&m_sIdentify, sizeof(m_sIdentify), 1, pFile) &&
            !fseek(pFile, lDataSize - sizeof(bNull), SEEK_CUR) &&
            fwrite(&bNull, sizeof(bNull), 1, pFile);

        // Close the file (this may be slow)
        fclose(pFile);

        // Remove the file if unsuccessful
        if (!fRet)
            unlink(m_strPath.c_str());
    }

    return fRet;
}


bool CHDFHardDisk::Open(bool fReadOnly_/*=false*/)
{
    Close();

    // No disk?
    if (m_strPath.empty())
        return false;

    // Open read-write, falling back on read-only (not ideal!)
    if ((!fReadOnly_ && (m_hfDisk = fopen(m_strPath.c_str(), "r+b"))) || (m_hfDisk = fopen(m_strPath.c_str(), "rb")))
    {
        RS_IDE sHeader;

        // Read and check the header is valid/supported
        if (fread(&sHeader, 1, sizeof(sHeader), m_hfDisk) != sizeof(sHeader) || (sHeader.bFlags & 3) ||
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
            UINT uIdentifyLen = m_uDataOffset - sizeof(sHeader);
            if (uIdentifyLen > sizeof(m_sIdentify))
                uIdentifyLen = sizeof(m_sIdentify);

            // Read the identify data
            if (m_uDataOffset < sizeof(sHeader) || fread(&m_sIdentify, 1, uIdentifyLen, m_hfDisk) != uIdentifyLen)
                TRACE("HDF data offset is invalid!\n");
            else if (fstat(fileno(m_hfDisk), &st) == 0)
            {
                m_sGeometry.uTotalSectors = static_cast<UINT>((st.st_size - m_uDataOffset) / m_uSectorSize);

                // Update the identify data
                SetIdentifyData(&m_sIdentify);
            }

            return true;
        }
    }

    Close();
    return false;
}

void CHDFHardDisk::Close()
{
    if (IsOpen())
    {
        fclose(m_hfDisk);
        m_hfDisk = nullptr;
    }
}

bool CHDFHardDisk::ReadSector(UINT uSector_, BYTE* pb_)
{
    off_t lOffset = m_uDataOffset + static_cast<off_t>(uSector_)* m_uSectorSize;
    return m_hfDisk && !fseek(m_hfDisk, lOffset, SEEK_SET) && (fread(pb_, 1, m_uSectorSize, m_hfDisk) == m_uSectorSize);
}

bool CHDFHardDisk::WriteSector(UINT uSector_, BYTE* pb_)
{
    off_t lOffset = m_uDataOffset + static_cast<off_t>(uSector_)* m_uSectorSize;
    return m_hfDisk && !fseek(m_hfDisk, lOffset, SEEK_SET) && (fwrite(pb_, 1, m_uSectorSize, m_hfDisk) == m_uSectorSize);
}
