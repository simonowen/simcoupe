// Part of SimCoupe - A SAM Coupe emulator
//
// HardDisk.cpp: Hard disk abstraction layer
//
//  Copyright (c) 2004-2010 Simon Owen
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
//
// Notes:
//  - HDF spec: http://web.archive.org/web/20080615063233/http://www.ramsoft.bbk.org/tech/rs-hdf.txt

#include "SimCoupe.h"

#include "HardDisk.h"
#include "IDEDisk.h"


CHardDisk::CHardDisk (const char* pcszDisk_)
{
    m_pszDisk = strdup(pcszDisk_);
}

CHardDisk::~CHardDisk ()
{
    free(m_pszDisk);
}


bool CHardDisk::IsSDIDEDisk ()
{
    // Check for the HDOS free space file-info-block in sector 1
    BYTE ab[512];
    return ReadSector(1, ab) && !memcmp(ab+14, "Free_space", 10);
}

bool CHardDisk::IsBDOSDisk ()
{
    BYTE ab[512];

    // Read the MBR for a possible BDOS boot sector
    if (ReadSector(0, ab))
    {
        // Clear bits 7 and 5 (case) for the signature check
        for (int i = 0 ; i < 4 ; i++) { ab[i+0x000] &= ~0xa0; ab[i+0x100] &= ~0xa0; }

        // Check for byte-swapped (Atom) and normal (Atom Lite) boot signatures
        if (!memcmp(ab+0x000, "OBTO", 4) || !memcmp(ab+0x100, "BOOT", 4))
            return true;
    }

    // Calculate the number of base sectors (boot sector + record list)
    UINT uBase = 1 + ((m_sGeometry.uTotalSectors/1600 + 32) / 32);

    // Read the first directory sector from record 1, and check for the BDOS signature
    if (ReadSector(uBase, ab) && (!memcmp(ab+232, "DBSO", 4) || !memcmp(ab+232, "BDOS", 4)))
        return true;

    return false;
}


// Calculate a suitable CHS geometry covering the supplied number of sectors
/*static*/ void CHardDisk::CalculateGeometry (ATA_GEOMETRY* pg_)
{
    UINT uCylinders, uHeads, uSectors;

    // Start the head count to give balanced figures for smaller drives
    uHeads = (pg_->uTotalSectors >= 65536) ? 8 : (pg_->uTotalSectors >= 32768) ? 4 : 2;
    uSectors = 32;

    // Loop until we're (ideally) below 1024 cylinders
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

    // Update the supplied structure, capping the cylinder value if necessary
    pg_->uCylinders = (uCylinders > 16383) ? 16383 : uCylinders;
    pg_->uHeads = uHeads;
    pg_->uSectors = uSectors;
}

/*static*/ void CHardDisk::SetIdentityString (char* psz_, size_t uLen_, const char* pcszValue_)
{
    // Copy the string, padding out the extra length with spaces
    memset(psz_, ' ', uLen_);
    memcpy(psz_, pcszValue_, uLen_ = strlen(pcszValue_));

    // Byte-swap the string for the expected endian
    for (size_t i = 0 ; i < uLen_ ; i += 2)
        swap(psz_[i], psz_[i+1]);
}

////////////////////////////////////////////////////////////////////////////////

typedef struct
{
    char szSignature[6];            // RS-IDE
    BYTE bEOF;                      // 0x1a
    BYTE bRevision;                 // 0x10 for v1.0, 0x11 for v1.1
    BYTE bFlags;                    // b0 = halved sector data, b1 = ATAPI (HDF 1.1+)
    BYTE bOffsetLow, bOffsetHigh;   // Offset from start of file to HDD data
    BYTE abReserved[11];            // Must be zero
                                    // Identity data follows: 106 bytes for HDF 1.0, 512 for HDF 1.1+
}
RS_IDE;


/*static*/ CHardDisk* CHardDisk::OpenObject (const char* pcszDisk_)
{
    CHardDisk* pDisk;

    // Make sure we have a disk to try
    if (!pcszDisk_ || !*pcszDisk_)
        return NULL;

    // Try for device path first
    if ((pDisk = new CDeviceHardDisk(pcszDisk_)) && pDisk->Open())
        return pDisk;
    delete pDisk;

    // Try for HDF disk image
    if ((pDisk = new CHDFHardDisk(pcszDisk_)) && pDisk->Open())
        return pDisk;
    delete pDisk;

    // No match
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CHDFHardDisk::Create (const char* pcszDisk_, UINT uCylinders_, UINT uHeads_, UINT uSectors_)
{
    bool fRet = false;

    // Stored identity size is fixed for HDF 1.0, giving 128 bytes for the full header
    BYTE abIdentity[128-sizeof(RS_IDE)] = {0};
    DEVICEIDENTITY *pdi = reinterpret_cast<DEVICEIDENTITY*>(abIdentity);

    UINT uDataOffset = sizeof(RS_IDE) + sizeof(abIdentity);
    RS_IDE sHeader = { {'R','S','-','I','D','E'}, 0x1a, 0x10, 0x00,  uDataOffset%256, uDataOffset/256 };

    ATAPUT(pdi->wCaps, 0x2241);         // Fixed device, motor control, hard sectored, <= 5Mbps
    ATAPUT(pdi->wLogicalCylinders, uCylinders_);
    ATAPUT(pdi->wLogicalHeads, uHeads_);
    ATAPUT(pdi->wSectorsPerTrack, uSectors_);
    ATAPUT(pdi->wBytesPerSector, 512);
    ATAPUT(pdi->wBytesPerTrack, uSectors_*512);

    ATAPUT(pdi->wControllerType, 1);    // single port, single sector
    ATAPUT(pdi->wBufferSize512, 1);     // 512 bytes
    ATAPUT(pdi->wLongECCBytes, 4);

    ATAPUT(pdi->wReadWriteMulti, 0);	// no multi-sector handling
    ATAPUT(pdi->wCapabilities, 0x0200);	// LBA supported

    // The identity strings need to be padded with spaces and byte-swapped
    SetIdentityString(pdi->szSerialNumber, sizeof(pdi->szSerialNumber), "100");
    SetIdentityString(pdi->szFirmwareRev,  sizeof(pdi->szFirmwareRev), "1.0");
    SetIdentityString(pdi->szModelNumber,  sizeof(pdi->szModelNumber), "SimCoupe Disk");

    // Create the file in binary mode
    FILE* pFile = fopen(pcszDisk_, "wb");
    if (pFile)
    {
        off_t lDataSize = uCylinders_ * uHeads_ * uSectors_ * 512;
        BYTE bNull = 0;

        // Write the header, and extend the file up to the full size
        fRet = fwrite(&sHeader, sizeof(sHeader), 1, pFile) &&
               fwrite(abIdentity, sizeof(abIdentity), 1, pFile) &&
              !fseek(pFile, lDataSize - sizeof(bNull), SEEK_CUR) &&
               fwrite(&bNull, sizeof(bNull), 1, pFile);

        // Close the file (this may be slow)
        fclose(pFile);

        // Remove the file if unsuccessful
        if (!fRet)
            unlink(pcszDisk_);
    }

    return fRet;
}


bool CHDFHardDisk::Open ()
{
    Close();

    if (*m_pszDisk && (m_hfDisk = fopen(m_pszDisk, "r+b")))
    {
        RS_IDE sHeader;

        if (!fread(&sHeader, sizeof(sHeader), 1, m_hfDisk) || sHeader.bFlags & 3 ||
            memcmp(sHeader.szSignature, "RS-IDE", sizeof(sHeader.szSignature)))
            TRACE("!!! Invalid or incompatible HDF file\n");
        else
        {
            // Clear out any existing identity data
            memset(m_abIdentity, 0, sizeof(m_abIdentity));

            // Determine the offset to the device data and the sector size (fixed for now)
            m_uDataOffset = (sHeader.bOffsetHigh << 8) | sHeader.bOffsetLow;
            m_uSectorSize = 512;

            // Calculate how much of the header is identity data
            UINT uIdentity = m_uDataOffset - sizeof(sHeader);

            // Read the identity data
            if (m_uDataOffset < sizeof(sHeader) || !fread(m_abIdentity, min(uIdentity,sizeof(m_abIdentity)), 1, m_hfDisk))
                TRACE("HDF data offset is invalid!\n");
            else
            {
                // Extract the disk geometry from the identity structure
                DEVICEIDENTITY *pdi = reinterpret_cast<DEVICEIDENTITY*>(m_abIdentity);
                m_sGeometry.uCylinders = ATAGET(pdi->wLogicalCylinders);
                m_sGeometry.uHeads = ATAGET(pdi->wLogicalHeads);
                m_sGeometry.uSectors = ATAGET(pdi->wSectorsPerTrack);

                struct stat st;
                fstat(fileno(m_hfDisk), &st);
                m_sGeometry.uTotalSectors = static_cast<UINT>((st.st_size-m_uDataOffset)/m_uSectorSize);
            }

            return true;
        }
    }

    Close();
    return false;
}

void CHDFHardDisk::Close ()
{
    if (IsOpen())
    {
        fclose(m_hfDisk);
        m_hfDisk = NULL;
    }
}

bool CHDFHardDisk::ReadSector (UINT uSector_, BYTE* pb_)
{
    off_t lOffset = m_uDataOffset + (uSector_ * m_uSectorSize);
    return m_hfDisk && !fseek(m_hfDisk, lOffset, SEEK_SET) && fread(pb_, m_uSectorSize, 1, m_hfDisk);
}

bool CHDFHardDisk::WriteSector (UINT uSector_, BYTE* pb_)
{
    off_t lOffset = m_uDataOffset + (uSector_ * m_uSectorSize);
    return m_hfDisk && !fseek(m_hfDisk, lOffset, SEEK_SET) && fwrite(pb_, m_uSectorSize, 1, m_hfDisk);
}
