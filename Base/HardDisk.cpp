// Part of SimCoupe - A SAM Coupe emulator
//
// HardDisk.cpp: Hard disk abstraction layer
//
//  Copyright (c) 2003 Simon Owen
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
//  - HDF spec: http://www.ramsoft.bbk.org/tech/rs-hdf.txt

#include "SimCoupe.h"

#include "HardDisk.h"
#include "IDEDisk.h"


bool CHardDisk::IsSDIDEDisk ()
{
    // Check for the HDOS free space file-info-block in sector 1
    BYTE ab[512];
    return ReadSector(1, ab) && !memcmp(ab+14, "Free_space", 10);
}

bool CHardDisk::IsBDOSDisk ()
{
    // Calculate the number of base sectors (boot sector + record list)
    UINT uBase = 1 + ((m_sGeometry.uTotalSectors/1600 + 32) / 32);

    // Check for the BDOS signature (byte-swapped for the Atom) in record 1
    BYTE ab[512];
    return ReadSector(uBase, ab) && !memcmp(ab+232, "DBSO", 4);
}


// Return a suitable CHS geometry covering the supplied number of sectors
bool CHardDisk::NormaliseGeometry (HARDDISK_GEOMETRY* pg_)
{
    // Return the supplied geometry if it's already valid for CHS
    if (pg_->uCylinders <= 16383 && pg_->uHeads <= 16 && pg_->uSectors <= 63)
        return false;

    // CHS can only handle up to 8GB, so truncate anything larger
    if (pg_->uTotalSectors > 16383*16*63)
        pg_->uTotalSectors = 16383*16*63;

    UINT uCylinders, uHeads, uSectors, uRound;

    for (uRound = 0 ; uRound < 512 ; uRound = (uRound << 1) | 1)
    {
        // A selection of small primes to use for factoring
        static int anPrimes[] = { 7,5,3,2, 0 };

        // Round the sector count down to the next power of 2 boundary
        uCylinders = pg_->uTotalSectors & ~uRound;
        uHeads = uSectors = 1;

        // Loop through the prime number list
        for (int i = 0 ; anPrimes[i] ; i++)
        {
            // Matched prime factor?
            if (!(uCylinders % anPrimes[i]))
            {
                // Use 2 for the head value if possible
                if (anPrimes[i] == 2 && uHeads <= 8)
                {
                    uHeads *= anPrimes[i];
                    uCylinders /= anPrimes[i--];    // Repeat factor
                }
                // Consider any remaining factor for the sector count
                else if (uSectors * anPrimes[i] <= 63)
                {
                    uSectors *= anPrimes[i];
                    uCylinders /= anPrimes[i--];    // Repeat factor
                }
            }
        }

        // Stop if the cylinder is now in CHS range
        if (uCylinders <= 16383)
            break;
    }

    // Did we fail to find a suitable match?
    if (uRound >= 512)
    {
        // Fall back on rounding up to the maximum track size (0-1007 extra sectors)
        uCylinders = (pg_->uTotalSectors + 16*63 - 1) / (16*63);
        uHeads = 16;
        uSectors = 63;
    }

    // Update the supplied structure
    pg_->uCylinders = uCylinders;
    pg_->uHeads = uHeads;
    pg_->uSectors = uSectors;
    pg_->uTotalSectors = uCylinders * uHeads * uSectors;

    return true;
}

////////////////////////////////////////////////////////////////////////////////

typedef struct
{
    char    szSignature[6];             // RS-IDE
    BYTE    bEOF;                       // 0x1a
    BYTE    bRevision;                  // 0x10 for v1.0
    BYTE    bFlags;                     // b0 = halved sector data
    BYTE    bOffsetLow, bOffsetHigh;    // Offset from start of file to HDD data
    BYTE    abReserved[11];             // Must be zero
    DEVICEIDENTITY sIdentity;           // ATA device identity
}
RS_IDE;


/*static*/ CHardDisk* CHardDisk::OpenObject (const char* pcszDisk_)
{
    CHardDisk* pDisk;

    // Try for device path first
    if ((pDisk = new CDeviceHardDisk) && pDisk->Open(pcszDisk_))
        return pDisk;
    delete pDisk;

    // Try for HDF disk image
    if ((pDisk = new CHDFHardDisk) && pDisk->Open(pcszDisk_))
        return pDisk;
    delete pDisk;

    // No match
    return NULL;
}


static void SetIdentityString (char* psz_, int nLen_, const char* pcszValue_)
{
    // Copy the string, padding out the extra length with spaces
    memset(psz_, ' ', nLen_);
    memcpy(psz_, pcszValue_, nLen_ = strlen(pcszValue_));

    // Byte-swap the string for the expected endian
    for (int i = 0 ; i < nLen_ ; i += 2)
        swap(psz_[i], psz_[i+1]);
}

/*static*/ bool CHDFHardDisk::Create (const char* pcszDisk_, UINT uCylinders_, UINT uHeads_, UINT uSectors_)
{
    bool fRet = false;

    UINT uSize = uCylinders_ * uHeads_ * uSectors_ * 512;

    RS_IDE sHeader = { {'R','S','-','I','D','E'}, 0x1a, 0x10, 0x00,  0x80, 0x00 };

    sHeader.sIdentity.wCaps = 0x2241;                   // Fixed device, motor control, hard sectored, <= 5Mbps
    sHeader.sIdentity.wLogicalCylinders = uCylinders_;
    sHeader.sIdentity.wLogicalHeads = uHeads_;
    sHeader.sIdentity.wBytesPerTrack = uSectors_ << 9;
    sHeader.sIdentity.wBytesPerSector = 1 << 9;
    sHeader.sIdentity.wSectorsPerTrack = uSectors_;

    sHeader.sIdentity.wControllerType = 1;  // single port, single sector
    sHeader.sIdentity.wBufferSize512 = 1;   // 512 bytes
    sHeader.sIdentity.wLongECCBytes = 4;

    sHeader.sIdentity.wReadWriteMulti = 0;  // no multi-sector handling

    // The identity strings need to be padded with spaces and byte-swapped
    SetIdentityString(sHeader.sIdentity.szSerialNumber, sizeof sHeader.sIdentity.szSerialNumber, "090");
    SetIdentityString(sHeader.sIdentity.szFirmwareRev,  sizeof sHeader.sIdentity.szFirmwareRev, "0.90");
    SetIdentityString(sHeader.sIdentity.szModelNumber,  sizeof sHeader.sIdentity.szModelNumber, "SimCoupe Disk");

    // Create the file in binary mode
    FILE* pFile = fopen(pcszDisk_, "wb");
    if (pFile)
    {
        BYTE bNull = 0;

        // Write the header, and extend the file up to the full size
        fRet = fwrite(&sHeader, sizeof sHeader, 1, pFile) &&
              !fseek(pFile, sizeof(sHeader) + uSize - 1, SEEK_SET) &&
               fwrite(&bNull, sizeof bNull, 1, pFile);

        // Close the file (this may be slow)
        fclose(pFile);

        // Remove the file if unsuccessful
        if (!fRet)
            unlink(pcszDisk_);
    }

    return fRet;
}


bool CHDFHardDisk::Open (const char* pcszDisk_)
{
    Close();

    if (*pcszDisk_ && (m_hfDisk = fopen(pcszDisk_, "r+b")))
    {
        RS_IDE sHeader;

        if (!fread(&sHeader, sizeof sHeader, 1, m_hfDisk) || sHeader.bRevision != 0x10 ||
            sHeader.bFlags & 1 || memcmp(sHeader.szSignature, "RS-IDE", sizeof sHeader.szSignature))
            TRACE("!!! Invalid or incompatible HDF file\n");
        else
        {
            // Use the identity structure from the header
            memcpy(&m_sIdentity, &sHeader.sIdentity, sizeof m_sIdentity);

            BYTE* pbCylinders = reinterpret_cast<BYTE*>(&m_sIdentity.wLogicalCylinders);
            BYTE* pbHeads = reinterpret_cast<BYTE*>(&m_sIdentity.wLogicalHeads);
            BYTE* pbSectors = reinterpret_cast<BYTE*>(&m_sIdentity.wSectorsPerTrack);

            // Fill the geometry, taking care of the source endian
            m_sGeometry.uCylinders = (pbCylinders[1] << 8) | pbCylinders[0];
            m_sGeometry.uHeads = (pbHeads[1] << 8) | pbHeads[0];
            m_sGeometry.uSectors = (pbSectors[1] << 8) | pbSectors[0];
            m_sGeometry.uTotalSectors = m_sGeometry.uCylinders * m_sGeometry.uHeads * m_sGeometry.uSectors;

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
    UINT uOffset = sizeof(RS_IDE) + (uSector_ << 9);
    return m_hfDisk && !fseek(m_hfDisk, uOffset, SEEK_SET) && fread(pb_, 1<<9, 1, m_hfDisk);
}

bool CHDFHardDisk::WriteSector (UINT uSector_, BYTE* pb_)
{
    UINT uOffset = sizeof(RS_IDE) + (uSector_ << 9);
    return m_hfDisk && !fseek(m_hfDisk, uOffset, SEEK_SET) && fwrite(pb_, 1<<9, 1, m_hfDisk);
}
