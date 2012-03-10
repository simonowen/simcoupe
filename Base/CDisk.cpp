// Part of SimCoupe - A SAM Coupe emulator
//
// CDisk.cpp: C++ classes used for accessing all SAM disk image types
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

// Notes:
//  The CFloppyDisk implementation is OS-specific, and is in Floppy.cpp
//
//  Teledisk format details extracted from a document by Will Kranz,
//  with extra information from Sergey Erokhin.  See:
//    http://www.fpns.net/willy/wteledsk.htm

#include "SimCoupe.h"
#include "CDisk.h"

#include "CDrive.h"
#include "Floppy.h"
#include "Util.h"

////////////////////////////////////////////////////////////////////////////////

/*static*/ int CDisk::GetType (CStream* pStream_)
{
    if (pStream_)
    {
        // Try each type in turn to check for a match
        if (CFloppyDisk::IsRecognised(pStream_))
            return dtFloppy;
        else if (CEDSKDisk::IsRecognised(pStream_))
            return dtEDSK;
        else if (CTD0Disk::IsRecognised(pStream_))
            return dtTD0;
        else if (CSADDisk::IsRecognised(pStream_))
            return dtSAD;
        else if (CFileDisk::IsRecognised(pStream_))
        {
            // For now we'll only accept single files if they have a .sbt or .sbt.gz file extension
            const char* pcsz = pStream_->GetFile();
            if (strlen(pcsz) > 4 && (!strcasecmp(pcsz + strlen(pcsz) - 4, ".sbt") || !strcasecmp(pcsz + strlen(pcsz) - 4, ".sbt.gz")))
                return dtSBT;
        }

        // The MGT format has no signature, so we try it last
        if (CMGTDisk::IsRecognised(pStream_))
            return dtMGT;
    }

    // Not recognised
    return dtUnknown;
}

/*static*/ CDisk* CDisk::Open (const char* pcszDisk_, bool fReadOnly_/*=false*/)
{
    CDisk* pDisk = NULL;

    // Fetch stream for the disk source
    CStream* pStream = CStream::Open(pcszDisk_, fReadOnly_);

    // A disk will only be returned if the stream format is recognised
    if (pStream)
    {
        switch (GetType(pStream))
        {
            case dtFloppy:  pDisk = new CFloppyDisk(pStream);   break;      // Direct floppy access
            case dtEDSK:    pDisk = new CEDSKDisk(pStream);     break;      // .DSK
            case dtTD0:     pDisk = new CTD0Disk(pStream);      break;      // .TD0
            case dtSAD:     pDisk = new CSADDisk(pStream);      break;      // .SAD
            case dtMGT:     pDisk = new CMGTDisk(pStream);      break;      // .MGT
            case dtSBT:     pDisk = new CFileDisk(pStream);     break;      // .SBT (bootable SAM file on a floppy)
        }
    }

    // Unrecognised, so close the underlying stream
    if (!pDisk)
        delete pStream;

    // Return the disk pointer (or NULL)
    return pDisk;
}

/*static*/ CDisk* CDisk::Open (void* pv_, size_t uSize_, const char* pcszDisk_)
{
    CStream* pStream = new CMemStream(pv_, uSize_, pcszDisk_);
    return pStream ? new CFileDisk(pStream) : NULL;
}


CDisk::CDisk (CStream* pStream_, int nType_)
    : m_nType(nType_), m_uSides(0), m_uTracks(0), m_uSectors(0), m_uSectorSize(0),
      m_uSide(0), m_uTrack(0), m_uSector(0), m_fModified(!pStream_->IsOpen()), m_uSpinPos(0),
      m_pStream(pStream_), m_pbData(NULL)
{
}

CDisk::~CDisk ()
{
    // Delete the stream object and disk data memory we allocated
    delete m_pStream;
    delete[] m_pbData;
}


// Sector spin position on the spinning disk, as used by the READ_ADDRESS command
UINT CDisk::GetSpinPos (bool fAdvance_/*=false*/)
{
    // If required, move to the next spin position for next time
    if (fAdvance_)
        m_uSpinPos = (m_uSpinPos % (m_uSectors + !m_uSectors)) + 1;

    // Return the current/new position
    return m_uSpinPos;
}

// Initialise a sector enumeration, return the number of sectors on the track
UINT CDisk::FindInit (UINT uSide_, UINT uTrack_)
{
    // Remember the current side and track number
    m_uSide = uSide_;
    m_uTrack = uTrack_;

    // Set current sector to zero so the next sector is sector 1
    m_uSector = 0;

    // Return the number of sectors on the current track
    return (m_uSide < m_uSides && m_uTrack < m_uTracks) ? m_uSectors : 0;
}

bool CDisk::FindNext (IDFIELD* pIdField_/*=NULL*/, BYTE* pbStatus_/*=NULL*/)
{
    // Advance to the next sector
    m_uSector++;

    // Construct a normal ID field for the sector
    if (pIdField_)
    {
        pIdField_->bSide = m_uSide;
        pIdField_->bTrack = m_uTrack;
        pIdField_->bSector = m_uSector;
        pIdField_->bSize = 2;           // 128 << 2 = 512 bytes

        // Calculate and set the CRC for the ID field, including the 3 gap bytes and address mark
        WORD wCRC = CrcBlock("\xa1\xa1\xa1\xfe", 4);
        wCRC = CrcBlock(pIdField_, 4, wCRC);
        pIdField_->bCRC1 = wCRC >> 8;
        pIdField_->bCRC2 = wCRC & 0xff;
    }

    // Sector OK so reset all errors
    if (pbStatus_)
        *pbStatus_ = 0;

    // Return true if the sector exists
    return (m_uSide < m_uSides && m_uTrack < m_uTracks) && m_uSector <= m_uSectors;
}


// Locate the specific sector on the disk
bool CDisk::FindSector (UINT uSide_, UINT uTrack_, UINT uIdTrack_, UINT uSector_, IDFIELD* pID_/*=NULL*/)
{
    // Assume 'record not found' with zero bytes read, until we discover otherwise
    bool fFound = false;

    // Only check for sectors if there are some to check
    if (FindInit(uSide_, uTrack_))
    {
        IDFIELD id;
        BYTE bStatus;

        // Loop through all the sectors in the track
        while (FindNext(&id, &bStatus))
        {
            // Check to see if the track and sector numbers match and the CRC is correct
            // Note that the WD1772 doesn't perform any head comparison!
            if (id.bTrack == uIdTrack_ && id.bSector == uSector_ && !bStatus)
            {
                // Valid sector found
                fFound = true;

                // Copy the ID field details if required
                if (pID_)
                    *pID_ = id;

                break;
            }
        }
    }

    // Return whether or not we found it
    return fFound;
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CMGTDisk::IsRecognised (CStream* pStream_)
{
    size_t uSize;

    // If we don't have a size (gzipped) we have no choice but to read enough to find out
    if (!(uSize = pStream_->GetSize()))
    {
        BYTE* pb = new BYTE[MGT_IMAGE_SIZE+1];

        // Read 1 byte more than a full MGT image size, to check for larger files
        if (pb && pStream_->Rewind())
            uSize = pStream_->Read(pb, MGT_IMAGE_SIZE+1);

        delete[] pb;
    }

    // Accept 800K (10-sector) SAM disks and 720K (9-sector DOS) disks
    return uSize == MGT_IMAGE_SIZE || uSize == DOS_IMAGE_SIZE;
}

CMGTDisk::CMGTDisk (CStream* pStream_, UINT uSectors_/*=NORMAL_DISK_SECTORS*/)
    : CDisk(pStream_, dtMGT)
{
    // The MGT geometry is fixed
    m_uSides = NORMAL_DISK_SIDES;
    m_uTracks = NORMAL_DISK_TRACKS;
    m_uSectors = uSectors_;
    m_uSectorSize = NORMAL_SECTOR_SIZE;

    // Allocate some memory and clear it, just in case it's not a complete MGT image
    m_pbData = new BYTE[MGT_IMAGE_SIZE];
    memset(m_pbData, (uSectors_ == NORMAL_DISK_SECTORS) ? 0x00 : 0xe5, MGT_IMAGE_SIZE);

    // Read the data from any existing stream
    if (pStream_->IsOpen())
    {
        pStream_->Rewind();

        // If it's an MS-DOS image, treat as 9 sectors-per-track, otherwise 10 as normal for SAM
        m_uSectors = (pStream_->Read(m_pbData, MGT_IMAGE_SIZE) == DOS_IMAGE_SIZE) ? DOS_DISK_SECTORS : NORMAL_DISK_SECTORS;

        Close();
    }
}


// Read the data for the last sector found
BYTE CMGTDisk::ReadData (BYTE *pbData_, UINT* puSize_)
{
    // Work out the offset for the required data (MGT and IMG use different track interleaves)
    long lPos = m_uSide + NORMAL_DISK_SIDES * m_uTrack;
    lPos = lPos * (m_uSectors * m_uSectorSize) + ((m_uSector-1) * m_uSectorSize);

    // Copy the sector data from the image buffer
    memcpy(pbData_, m_pbData + lPos, *puSize_ = m_uSectorSize);

    // Data is always perfect on MGT images, so return OK
    return 0;
}

// Read the data for the last sector found
BYTE CMGTDisk::WriteData (BYTE *pbData_, UINT* puSize_)
{
    // Fail if read-only
    if (IsReadOnly())
        return WRITE_PROTECT;

    // Work out the offset for the required data (MGT and IMG use different track interleaves)
    long lPos = m_uSide + NORMAL_DISK_SIDES * m_uTrack;
    lPos = lPos * (m_uSectors * m_uSectorSize) + ((m_uSector-1) * m_uSectorSize);

    // Copy the sector data to the image buffer, and set the modified flag
    memcpy(m_pbData + lPos, pbData_, *puSize_ = m_uSectorSize);
    SetModified();

    // Data is always perfect on MGT images, so return OK
    return 0;
}

// Save the disk out to the stream
bool CMGTDisk::Save ()
{
    size_t uSize = m_uSides*m_uTracks*m_uSectors*m_uSectorSize;

    // Write the image out as a single block
    if (!m_pStream->Rewind() || m_pStream->Write(m_pbData, uSize) != uSize)
        return false;

    SetModified(false);
    m_pStream->Close();
    return true;
}

// Format a track using the specified format
BYTE CMGTDisk::FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_)
{
    DWORD dwSectors = 0;
    bool fNormal = true;
    UINT u;

    // Disk must be writable, same number of sectors, and within track limit
    if (IsReadOnly() || uSectors_ != m_uSectors || uTrack_ >= m_uTracks)
        return WRITE_PROTECT;

    // Make sure the remaining sectors are completely normal
    for (u = 0 ; u < uSectors_ ; u++)
    {
        // Side and track must match the ones it's being laid on
        fNormal &= (paID_[u].bSide == uSide_ && paID_[u].bTrack == uTrack_);

        // Sector size must be the same
        fNormal &= ((128U << paID_[u].bSize) == m_uSectorSize);

        // Remember we've seen this sector number
        dwSectors |= (1 << (paID_[u].bSector-1));
    }

    // There must be only 1 of each sector number from 1 to N (in any order though)
    fNormal &= (dwSectors == ((1UL << m_uSectors) - 1));

    // Reject tracks that are not completely normal
    if (!fNormal)
        return WRITE_PROTECT;

    // Work out the offset for the required track
    long lPos = (uSide_ + 2 * uTrack_) * (m_uSectors * m_uSectorSize);

    // Process each sector to write the supplied data
    for (u = 0 ; u < uSectors_ ; u++)
        memcpy(m_pbData + lPos + ((paID_[u].bSector-1) * m_uSectorSize), papbData_[u], m_uSectorSize);

    // Mark the disk stream as modified
    SetModified();

    return 0;
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CSADDisk::IsRecognised (CStream* pStream_)
{
    SAD_HEADER sh = {""};

    // Read the header, check for the signature, and make sure the disk geometry is sensible
    bool fValid = (pStream_->Rewind() && pStream_->Read(&sh, sizeof(sh)) == sizeof(sh) &&
            !memcmp(sh.abSignature, SAD_SIGNATURE, sizeof(sh.abSignature)) &&
            sh.bSides && sh.bSides <= MAX_DISK_SIDES && sh.bTracks && sh.bTracks <= 127 &&
            sh.bSectorSizeDiv64 && (sh.bSectorSizeDiv64 <= (MAX_SECTOR_SIZE >> 6)) &&
            (sh.bSectorSizeDiv64 & -sh.bSectorSizeDiv64) == sh.bSectorSizeDiv64);

    // If we know the stream size, validate the image size
    if (fValid && pStream_->GetSize())
    {
        UINT uDiskSize = sizeof(sh) + sh.bSides * sh.bTracks * sh.bSectors * (sh.bSectorSizeDiv64 << 6);
        fValid &= (pStream_->GetSize() == uDiskSize);
    }

    return fValid;
}

CSADDisk::CSADDisk (CStream* pStream_, UINT uSides_/*=NORMAL_DISK_SIDES*/, UINT uTracks_/*=NORMAL_DISK_TRACKS*/,
    UINT uSectors_/*=NORMAL_DISK_SECTORS*/, UINT uSectorSize_/*=NORMAL_SECTOR_SIZE*/)
    : CDisk(pStream_, dtSAD)
{
    SAD_HEADER sh = { "", uSides_, uTracks_, uSectors_, uSectorSize_ >> 6 };

    if (!pStream_->IsOpen())
        memcpy(sh.abSignature, SAD_SIGNATURE, sizeof(sh.abSignature));
    else
    {
        pStream_->Rewind();
        pStream_->Read(&sh, sizeof(sh));
    }

    m_uSides = sh.bSides;
    m_uTracks = sh.bTracks;
    m_uSectors = sh.bSectors;
    m_uSectorSize = sh.bSectorSizeDiv64 << 6;

    UINT uDiskSize = sizeof(sh) + m_uSides * m_uTracks * m_uSectors * m_uSectorSize;
    memcpy(m_pbData = new BYTE[uDiskSize], &sh, sizeof(sh));
    memset(m_pbData + sizeof(sh), 0, uDiskSize - sizeof(sh));

    if (pStream_->IsOpen())
    {
        pStream_->Read(m_pbData + sizeof(sh), uDiskSize - sizeof(sh));
        pStream_->Close();
    }
}


// Find the next sector in the current track
bool CSADDisk::FindNext (IDFIELD* pIdField_, BYTE* pbStatus_)
{
    bool fRet = CDisk::FindNext(pIdField_, pbStatus_);

    // Make sure there is a 'next' one
    if (fRet)
    {
        // Work out the sector size as needed for the IDFIELD
        pIdField_->bSize = 0;
        for (UINT uSize = m_uSectorSize/128 ; !(uSize & 1) ; (pIdField_->bSize)++, uSize >>= 1);
    }

    // Return true if we're returning a new item
    return fRet;
}

// Read the data for the last sector found
BYTE CSADDisk::ReadData (BYTE *pbData_, UINT* puSize_)
{
    // Work out the offset for the required data
    long lPos = sizeof(SAD_HEADER) + (m_uSide * m_uTracks + m_uTrack) * (m_uSectors * m_uSectorSize) + ((m_uSector-1) * m_uSectorSize) ;

    // Copy the sector data from the image buffer
    memcpy(pbData_, m_pbData + lPos, *puSize_ = m_uSectorSize);

    // Data is always perfect on SAD images, so return OK
    return 0;
}

// Read the data for the last sector found
BYTE CSADDisk::WriteData (BYTE *pbData_, UINT* puSize_)
{
    // Fail if read-only
    if (IsReadOnly())
        return WRITE_PROTECT;

    // Work out the offset for the required data
    long lPos = sizeof(SAD_HEADER) + (m_uSide * m_uTracks + m_uTrack) * (m_uSectors * m_uSectorSize) + ((m_uSector-1) * m_uSectorSize) ;

    // Copy the sector data to the image buffer, and set the modified flag
    memcpy(m_pbData + lPos, pbData_, *puSize_ = m_uSectorSize);
    SetModified();

    // Data is always perfect on SAD images, so return OK
    return 0;
}

// Save the disk out to the stream
bool CSADDisk::Save ()
{
    UINT uDiskSize = sizeof(SAD_HEADER) + m_uSides * m_uTracks * m_uSectors * m_uSectorSize;

    if (!m_pStream->Rewind() || m_pStream->Write(m_pbData, uDiskSize) != uDiskSize)
        return false;

    SetModified(false);
    m_pStream->Close();
    return true;
}

// Format a track using the specified format
BYTE CSADDisk::FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_)
{
    DWORD dwSectors = 0;
    bool fNormal = true;
    UINT u;

    // Disk must be writable, same number of sectors, and within track limit
    if (IsReadOnly() || uSectors_ != m_uSectors || uTrack_ >= m_uTracks)
        return WRITE_PROTECT;

    // Make sure the remaining sectors are completely normal
    for (u = 0 ; u < uSectors_ ; u++)
    {
        // Side and track must match the ones it's being laid on
        fNormal &= (paID_[u].bSide == uSide_ && paID_[u].bTrack == uTrack_);

        // Sector size must be the same
        fNormal &= ((128U << paID_[u].bSize) == m_uSectorSize);

        // Remember we've seen this sector number
        dwSectors |= (1 << (paID_[u].bSector-1));
    }

    // There must be only 1 of each sector number from 1 to N (in any order though)
    fNormal &= (dwSectors == ((1UL << m_uSectors) - 1));

    // Reject tracks that are not completely normal
    if (!fNormal)
        return WRITE_PROTECT;

    // Work out the offset for the required track
    long lPos = sizeof(SAD_HEADER) + (m_uSide * m_uTracks + m_uTrack) * (m_uSectors * NORMAL_SECTOR_SIZE);

    // Process each sector to write the supplied data
    for (u = 0 ; u < uSectors_ ; u++)
        memcpy(m_pbData + lPos + ((paID_[u].bSector-1) * m_uSectorSize), papbData_[u], m_uSectorSize);

    // Mark the disk stream as modified
    SetModified();

    return 0;
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CEDSKDisk::IsRecognised (CStream* pStream_)
{
    EDSK_HEADER eh;

    // Read the header, check the signature and basic geometry
    bool fValid = (pStream_->Rewind() && pStream_->Read(&eh, sizeof(eh)) == sizeof(eh) &&
            (!memcmp(eh.szSignature, EDSK_SIGNATURE, sizeof(EDSK_SIGNATURE)-1) ||
             !memcmp(eh.szSignature,  DSK_SIGNATURE, sizeof( DSK_SIGNATURE)-1)) &&
             eh.bSides >= 1 && eh.bSides <= 2 &&
             eh.bTracks && eh.bTracks <= MAX_DISK_TRACKS);

   return fValid;
}

CEDSKDisk::CEDSKDisk (CStream* pStream_, UINT uSides_/*=NORMAL_DISK_SIDES*/, UINT uTracks_/*=MAX_DISK_TRACKS*/)
    : CDisk(pStream_, dtEDSK), m_pTrack(NULL), m_pFind(NULL)
{
    m_uSides = uSides_;
    m_uTracks = uTracks_;

    // Unformatted, initially
    memset(m_apTracks, 0, sizeof(m_apTracks));
    memset(m_abSizes, 0, sizeof(m_abSizes));

    // There's nothing more to do if we don't have a stream
    if (!pStream_->IsOpen())
        return;

    BYTE ab[256];
    EDSK_HEADER* peh = reinterpret_cast<EDSK_HEADER*>(ab);
    BYTE* pbSizes = reinterpret_cast<BYTE*>(peh+1);

    pStream_->Rewind();
    pStream_->Read(ab, sizeof(ab));

    m_uSides = peh->bSides;
    m_uTracks = peh->bTracks;

    bool fEDSK = peh->szSignature[0] == EDSK_SIGNATURE[0];
    WORD wDSKTrackSize = peh->abTrackSize[0] | (peh->abTrackSize[1] << 8);  // DSK only

    for (BYTE cyl = 0 ; cyl < peh->bTracks ; cyl++)
    {
        for (BYTE head = 0 ; head < peh->bSides ; head++)
        {
            // Nothing to do for empty tracks
            UINT size = fEDSK ? (pbSizes[cyl*peh->bSides + head] << 8) : wDSKTrackSize;
            if (!size)
                continue;

            BYTE *pb = new BYTE[size];
            EDSK_TRACK* pt = reinterpret_cast<EDSK_TRACK*>(pb);

            // Read the track, rejecting anything but 250Kbps MFM
            if (pt && (pStream_->Read(pt, size) != size || (pt->bRate && pt->bRate != 1) || (pt->bEncoding && pt->bEncoding != 1)))
            {
                delete[] pb;
                pt = NULL;
                size = 0;
            }

            // Save the track (or NULL) and size MSB
            m_apTracks[head][cyl] = pt;
            m_abSizes[head][cyl] = size >> 8;
        }
    }

    pStream_->Close();
}

CEDSKDisk::~CEDSKDisk ()
{
    if (DiskModified())
        Save();

    // Free any allocated tracks
    for (BYTE cyl = 0 ; cyl < m_uTracks ; cyl++)
        for (BYTE head = 0 ; head < m_uSides ; head++)
            delete[] m_apTracks[head][cyl];
}


// Initialise the enumeration of all sectors in the track
UINT CEDSKDisk::FindInit (UINT uSide_, UINT uTrack_)
{
    if (uSide_ >= m_uSides || uTrack_ >= m_uTracks)
        return m_uSectors = 0;

    // Locate the track and determine the number of sectors available
    m_pTrack = m_apTracks[uSide_][uTrack_];
    m_uSectors = m_pTrack ? m_pTrack->bSectors : 0;
    m_pFind = NULL;

    // Call the base and return the number of sectors in the track
    return CDisk::FindInit(uSide_, uTrack_);
}

// Find the next sector in the current track
bool CEDSKDisk::FindNext (IDFIELD* pIdField_, BYTE* pbStatus_)
{
    bool fRet = CDisk::FindNext();

    // Make sure there is a 'next' one
    if (fRet)
    {
        // First sector required?
        if (!m_pFind)
        {
            m_pFind = reinterpret_cast<EDSK_SECTOR*>(m_pTrack+1);
            m_pbFind = reinterpret_cast<BYTE*>(m_pFind+EDSK_MAX_SECTORS);
        }
        else
        {
            // Advance to next sector
            m_pbFind += ((m_pFind->bDatahigh << 8) | m_pFind->bDatalow);
            m_pFind++;
        }

        // Complete the ID field
        pIdField_->bSide = m_pFind->bSide;
        pIdField_->bTrack = m_pFind->bTrack;
        pIdField_->bSector = m_pFind->bSector;
        pIdField_->bSize = m_pFind->bSize;
        pIdField_->bCRC1 = pIdField_->bCRC2 = 0;

        // Calculate and set the CRC for the ID field, including the 3 gap bytes and address mark
        WORD wCRC = CrcBlock("\xa1\xa1\xa1\xfe", 4);
        wCRC = CrcBlock(pIdField_, 4, wCRC);
        if (m_pFind->bStatus1 & ST1_765_CRC_ERROR) wCRC ^= 0x5555;  // Force an error if required
        pIdField_->bCRC1 = wCRC >> 8;
        pIdField_->bCRC2 = wCRC & 0xff;

        // Set the ID status
        *pbStatus_ = (m_pFind->bStatus1 & ST1_765_CRC_ERROR) ? CRC_ERROR : 0;
    }

    // Return true if we're returning a new item
    return fRet;
}

// Read the data for the last sector found
BYTE CEDSKDisk::ReadData (BYTE *pbData_, UINT* puSize_)
{
    UINT uRealSize = 128U << (m_pFind->bSize & 7);  // clip to a 3 bit size
    UINT uDataSize = (m_pFind->bDatahigh << 8) | m_pFind->bDatalow;

    // Single copy?
    if (uDataSize <= uRealSize)
    {
        memset(pbData_+uDataSize, 0, uRealSize-uDataSize);
        memcpy(pbData_, m_pbFind, uDataSize);
    }
    else
    {
        // Determine the number of copies stored
        UINT uCopies = uDataSize/uRealSize;

        // Rotate between the copies returned
        static UINT uCopy = 0U-1;
        uCopy = (uCopy+1) % uCopies;

        memcpy(pbData_, m_pbFind + (uCopy*uRealSize), uRealSize);
    }

    // Form the data status
    BYTE bStatus = 0;
    if (m_pFind->bStatus2 & ST2_765_DATA_NOT_FOUND) bStatus |= RECORD_NOT_FOUND;
    if (m_pFind->bStatus2 & ST2_765_CRC_ERROR)    { bStatus |= CRC_ERROR; }
    if (m_pFind->bStatus2 & ST2_765_CONTROL_MARK)   bStatus |= DELETED_DATA;

    // Set the data size and return the data status
    *puSize_ = uRealSize;
    return bStatus;
}

// Read the data for the last sector found
BYTE CEDSKDisk::WriteData (BYTE *pbData_, UINT* puSize_)
{
    if (IsReadOnly())
        return WRITE_PROTECT;

    *puSize_ = (128U << (m_pFind->bSize & 7));
    memcpy(m_pbFind, pbData_, *puSize_);

    SetModified();
    return 0;
}

// Save the disk out to the stream
bool CEDSKDisk::Save ()
{
    BYTE abHeader[256] = {0}, cyl, head;
    EDSK_HEADER *peh = reinterpret_cast<EDSK_HEADER*>(abHeader);
    BYTE *pbSizes = reinterpret_cast<BYTE*>(peh+1);

    // Complete the disk header
    memcpy(peh->szSignature, EDSK_SIGNATURE, sizeof(peh->szSignature));
    memcpy(peh->szCreator, "SimCoupe 1.0 ", sizeof(peh->szCreator));
    peh->bTracks = m_uTracks;
    peh->bSides = m_uSides;

    // Complete the MSB size table
    for (cyl = 0 ; cyl < m_uTracks ; cyl++)
        for (head = 0 ; head < m_uSides ; head++)
            *pbSizes++ = m_abSizes[head][cyl];

    // Write the disk header
    bool fSuccess = m_pStream->Rewind() && m_pStream->Write(&abHeader, sizeof(abHeader)) == sizeof(abHeader);

    // Write the track data
    for (cyl = 0 ; fSuccess && cyl < m_uTracks ; cyl++)
    {
        for (head = 0 ; head < m_uSides ; head++)
        {
            // Nothing to write?
            if (!m_apTracks[head][cyl])
                continue;

            UINT uSize = m_abSizes[head][cyl] << 8;
            fSuccess &= (m_pStream->Write(m_apTracks[head][cyl], uSize) == uSize);
        }
    }

    // Any problems?
    if (!fSuccess)
        return false;

    SetModified(false);
    m_pStream->Close();

    return true;
}

// Format a track using the specified format
BYTE CEDSKDisk::FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_)
{
    UINT u, uDataTotal = 0;

    // Disk must be writeable and within track limit
    if (IsReadOnly() || uTrack_ >= MAX_DISK_TRACKS)
        return WRITE_PROTECT;

    // The 256-byte track header limits the number of sectors it can hold
    if (uSectors_ > EDSK_MAX_SECTORS)
        return WRITE_PROTECT;

    // Add up the space needed for data fields
    for (u = 0 ; u < uSectors_ ; u++)
        uDataTotal += (128U << (paID_[u].bSize & 7));

    // Make sure the track contents are within the maximum size
    if ((uDataTotal + (62+1)*uSectors_) >= MAX_TRACK_SIZE)
        return WRITE_PROTECT;

    // Calculate the total track size, including header and padding up to the next 256-byte boundary
    uDataTotal = (sizeof(EDSK_TRACK) + uDataTotal + 0xff) & ~0xff;

    // EDSK limits the total track length to 0xff00, as only the high byte of the size is stored
    // This should always fit due to the above tests, but we may as well be sure
    if (uDataTotal > 0xff00)
        return WRITE_PROTECT;

    // Allocate space for the new track
    BYTE* pb = new BYTE[uDataTotal];
    memset(pb, 0, uDataTotal);

    // Set up the initial track/sector/data pointers
    EDSK_TRACK *pt = reinterpret_cast<EDSK_TRACK*>(pb);
    EDSK_SECTOR *ps = reinterpret_cast<EDSK_SECTOR*>(pt+1);
    pb += 256;

    // Complete a suitable track header
    memcpy(pt->szSignature, EDSK_TRACK_SIGNATURE, sizeof(pt->szSignature));
    pt->bRate = 0;              // default, which is 250Kbps
    pt->bEncoding = 0;          // default, which is MFM
    pt->bTrack = uTrack_;       // physical cylinder
    pt->bSide = uSide_;         // physical head
    pt->bSize = 2;              // dummy, we'll use 512-bytes
    pt->bSectors = uSectors_;   // sectors per track
    pt->bGap3 = 78;             // dummy, we'll use the normal EDSK value
    pt->bFill = 0x00;           // zero filler

    // Write each of the supplied sectors
    for (u = 0 ; u < uSectors_ ; u++)
    {
        // Complete the ID header, with no status conditions flagged
        ps[u].bTrack = paID_[u].bTrack;
        ps[u].bSide = paID_[u].bSide;
        ps[u].bSector = paID_[u].bSector;
        ps[u].bSize = paID_[u].bSize;
        ps[u].bStatus1 = ps[u].bStatus2 = 0;

        UINT uDataSize = (128U << (paID_[u].bSize & 7));
        ps[u].bDatalow = uDataSize & 0xff;
        ps[u].bDatahigh = uDataSize >> 8;

        // Copy the sector data, advancing the pointer for the next one
        memcpy(pb, papbData_[u], uDataSize);
        pb += uDataSize;
    }

    // Delete any old track, and assign the new one
    delete m_apTracks[uSide_][uTrack_];
    m_apTracks[uSide_][uTrack_] = pt;
    m_abSizes[uSide_][uTrack_] = uDataTotal >> 8;

    // Update the disk extents if required
    if (uTrack_ >= m_uTracks) m_uTracks = uTrack_+1;
    if (uSide_ >= m_uSides) m_uSides = uSide_+1;

    // Mark the disk stream as modified
    SetModified();

    return 0;
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CFloppyDisk::IsRecognised (CStream* pStream_)
{
    return CFloppyStream::IsRecognised(pStream_->GetPath());
}

CFloppyDisk::CFloppyDisk (CStream* pStream_)
    : CDisk(pStream_, dtFloppy), m_bStatus(0)
{
    // We can assume this as only CFloppyStream will recognise the real device, and we are always the disk used
    m_pFloppy = reinterpret_cast<CFloppyStream*>(pStream_);

    // Maximum supported geometry
    m_uSides = MAX_DISK_SIDES;
    m_uTracks = MAX_DISK_TRACKS;
    m_uSectors = m_uSectorSize = 0;

    m_pbData = new BYTE[MAX_TRACK_SIZE];
    m_uCacheTrack = 0U-1;

    m_pTrack = reinterpret_cast<PTRACK>(m_pbData);
    m_pSector = reinterpret_cast<PSECTOR>(m_pTrack+1);
}


UINT CFloppyDisk::FindInit (UINT uSide_, UINT uTrack_)
{
    if (uSide_ >= m_uSides || uTrack_ >= m_uTracks)
        return m_uSectors = 0;

    // LoadTrack will have been called, so the sector count will be correct
    m_uSectors = m_pTrack->sectors;

    // Call the base and return the number of sectors in the track
    return CDisk::FindInit(uSide_, uTrack_);
}


// Find the next sector in the current track
bool CFloppyDisk::FindNext (IDFIELD* pIdField_, BYTE* pbStatus_)
{
    bool fRet = CDisk::FindNext();

    // Make sure there is a 'next' one
    if (fRet)
    {
        PSECTOR ps = &m_pSector[m_uSector-1];

        // Construct a normal ID field for the sector
        pIdField_->bSide = ps->head;
        pIdField_->bTrack = ps->cyl;
        pIdField_->bSector = ps->sector;
        pIdField_->bSize = ps->size;

        // Calculate and set the CRC for the ID field, including the 3 gap bytes and address mark
        WORD wCRC = CrcBlock("\xa1\xa1\xa1\xfe", 4);
        wCRC = CrcBlock(pIdField_, 4, wCRC);
        if (ps->status & CRC_ERROR) wCRC ^= 0x5555; // Force an error if required
        pIdField_->bCRC1 = wCRC >> 8;
        pIdField_->bCRC2 = wCRC & 0xff;

        *pbStatus_ = ps->status;
    }

    // Return true if we're returning a new item
    return fRet;
}

// Read the data for the last sector found
BYTE CFloppyDisk::ReadData (BYTE *pbData_, UINT* puSize_)
{
    PSECTOR ps = &m_pSector[m_uSector-1];
    memcpy(pbData_, ps->pbData, *puSize_ = (128 << (ps->size & 7)));
    return ps->status;
}

// Write the data for the last sector found
BYTE CFloppyDisk::WriteData (BYTE *pbData_, UINT* puSize_)
{
    // Save the write details, but don't update the cache yet
    PSECTOR ps = &m_pSector[m_uSector-1];
    m_pbWrite = pbData_;
    *puSize_ = 128U << (ps->size & 7);

    // Start the write command
    m_bCommand = WRITE_1SECTOR;
    return m_bStatus = m_pFloppy->StartCommand(WRITE_1SECTOR, m_pTrack, m_uSector-1, pbData_);
}

// Save the disk out to the stream
bool CFloppyDisk::Save ()
{
    // Writes are live, so there's nothing to save
    return true;
}

// Format a track using the specified format
BYTE CFloppyDisk::FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_)
{
    UINT uSize = uSectors_ ? (128U << (paID_->bSize & 7)) : 0, u;

    // Disk must be writeable and within track limit
    if (IsReadOnly() || uTrack_ >= MAX_DISK_TRACKS)
        return WRITE_PROTECT;

    // For now, ensure all the sectors are the same size
    for (u = 1 ; u < uSectors_ ; u++)
        if (paID_[u].bSize != paID_->bSize)
            return WRITE_PROTECT;

    // Make sure the track contents aren't too big to format
    if (((62+uSize+1)*uSectors_) >= (MAX_TRACK_SIZE-50))
        return WRITE_PROTECT;

    // Set up the initial track/sector/data pointers
    PTRACK pt = m_pTrack;
    PSECTOR ps = reinterpret_cast<PSECTOR>(pt+1);
    BYTE *pb = reinterpret_cast<BYTE*>(ps+uSectors_);

    // Complete a suitable track header
    pt->sectors = uSectors_;
    pt->cyl = uTrack_;
    pt->head = uSide_;

    // Prepare each of the supplied sectors
    for (u = 0 ; u < uSectors_ ; u++)
    {
        // Complete the ID header, with no status conditions flagged
        ps[u].cyl = paID_[u].bTrack;
        ps[u].head = paID_[u].bSide;
        ps[u].sector = paID_[u].bSector;
        ps[u].size = paID_[u].bSize;
        ps[u].status = 0;
        ps[u].pbData = pb;

        // Copy the sector data, advancing the pointer for the next one
        memcpy(pb, papbData_[u], uSize);
        pb += uSize;
    }

    // Start the format command
    m_bCommand = WRITE_TRACK;
    return m_bStatus = m_pFloppy->StartCommand(WRITE_TRACK, m_pTrack);
}

BYTE CFloppyDisk::LoadTrack (UINT uSide_, UINT uTrack_)
{
    // Return if the track is already cached
    if (uTrack_ == m_uCacheTrack && uSide_ == m_uCacheSide)
        return 0;

    // Prepare a fresh track
    m_pTrack->sectors = 0;
    m_pTrack->cyl = uTrack_;
    m_pTrack->head = uSide_;

    // Read the track, setting busy if we've not finished yet
    m_bCommand = READ_MSECTOR;
    return m_bStatus = m_pFloppy->StartCommand(READ_MSECTOR, m_pTrack);
}

// Get the status of the current asynchronous operation, if any
bool CFloppyDisk::IsBusy (BYTE* pbStatus_, bool fWait_)
{
    bool fBusy = m_pFloppy->IsBusy(pbStatus_, fWait_);

    // If we've just finished a request, update the track cache
    if (!fBusy && m_bStatus == BUSY)
    {
        // Which command has just finished?
        switch (m_bCommand)
        {
            // Load track
            case READ_MSECTOR:
                m_uCacheSide = m_pTrack->head;
                m_uCacheTrack = m_pTrack->cyl;
                break;

            // Write sector
            case WRITE_1SECTOR:
                // Only write the data to the track cache if successful
                if (!*pbStatus_)
                {
                    PSECTOR ps = &m_pSector[m_uSector-1];
                    memcpy(ps->pbData, m_pbWrite, 128U << (ps->size & 7));
                    ps->status &= ~CRC_ERROR;
                }
                break;

            // Format track
            case WRITE_TRACK:
            {
                // Cache the track if successful
                m_uCacheTrack = *pbStatus_ ? 0U-1 : m_pTrack->cyl;
                m_uCacheSide = m_pTrack->head;
                break;
            }

            default:
                TRACE("!!! Finished unknown command (%u)\n", m_bCommand);
                break;
        }

        // Clear the status now we're done
        m_bStatus = 0;
    }

    return fBusy;
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CFileDisk::IsRecognised (CStream* pStream_)
{
    // Accept any file that isn't too big
    return pStream_->GetSize() <= MAX_SAM_FILE_SIZE;
}

CFileDisk::CFileDisk (CStream* pStream_)
    : CDisk(pStream_, dtFile)
{
    m_uSides = NORMAL_DISK_SIDES;
    m_uTracks = NORMAL_DISK_TRACKS;
    m_uSectors = NORMAL_DISK_SECTORS;
    m_uSectorSize = NORMAL_SECTOR_SIZE;

    // Work out the maximum file size, allocate some memory for it
    UINT uSize = MAX_SAM_FILE_SIZE + DISK_FILE_HEADER_SIZE;
    m_pbData = new BYTE[uSize];
    memset(m_pbData, 0, uSize);

    // Read the data from any existing stream, or show an empty disk as we don't support saving
    if (pStream_->IsOpen())
    {
        // Read in the file, leaving a 9-byte gap for the disk file header
        pStream_->Rewind();
        m_uSize = static_cast<UINT>(pStream_->Read(m_pbData + DISK_FILE_HEADER_SIZE, MAX_SAM_FILE_SIZE));
        pStream_->Close();

        // Create the disk file header
        m_pbData[0] = 19;                           // CODE file type
        m_pbData[1] = m_uSize & 0xff;               // LSB of size mod 16384
        m_pbData[2] = (m_uSize >> 8) & 0xff;        // MSB of size mod 16384
        m_pbData[3] = 0x00;                         // LSB of offset start
        m_pbData[4] = 0x80;                         // MSB of offset start
        m_pbData[5] = 0xff;                         // Unused
        m_pbData[6] = 0xff;                         // Unused
        m_pbData[7] = (m_uSize >> 14) & 0xff;       // Number of pages (size div 16384)
        m_pbData[8] = 0x01;                         // Starting page number

        m_uSize += DISK_FILE_HEADER_SIZE;
    }
}


// Read the data for the last sector found
BYTE CFileDisk::ReadData (BYTE *pbData_, UINT* puSize_)
{
    // Clear the sector out
    memset(pbData_, 0, *puSize_ = m_uSectorSize);

    // The first directory sector?
    if (m_uTrack == 0 && m_uSide == 0 && m_uSector == 1)
    {
        // CODE file type
        pbData_[0] = 19;

        // Strip any file extension and use up to the first 10 chars for the filename on the disk
        const char *pcszName = m_pStream->GetFile(), *pcszExt = strrchr(pcszName, '.'), *pcsz;
        for (pcsz = pcszName+strlen(pcszName)-1 ; pcsz[-1] != PATH_SEPARATOR && pcsz > pcszName ; pcsz--);
        size_t uLen = strlen(pcsz) - (pcszExt ? strlen(pcszExt) : 0);
        memset(pbData_+1, ' ', 10);
        memcpy(pbData_+1, pcsz, min(uLen, 10));

        // Number of sectors required
        WORD wSectors = (m_uSize + m_uSectorSize-3) / (m_uSectorSize-2);
        pbData_[11] = wSectors >> 8;
        pbData_[12] = wSectors & 0xff;

        // Starting track and sector
        pbData_[13] = NORMAL_DIRECTORY_TRACKS;
        pbData_[14] = 1;

        // Sector address map
        memset(pbData_+15, 0xff, wSectors >> 3);
        if (wSectors & 7)
            pbData_[15 + (wSectors >> 3)] = (1U << (wSectors & 7)) - 1;

        // Starting page number and offset
        pbData_[236] = m_pbData[8];
        pbData_[237] = m_pbData[3];
        pbData_[238] = m_pbData[4];

        // Size in pages and mod 16384
        pbData_[239] = m_pbData[7];
        pbData_[240] = m_pbData[1];
        pbData_[241] = m_pbData[2];

        // No auto-execute
        pbData_[242] = pbData_[243] = pbData_[244] = 0xff;
    }

    // Does the position fall within the file?
    else if (m_uTrack >= NORMAL_DIRECTORY_TRACKS)
    {
        // Work out the offset for the required data
        long lPos = (m_uSide * m_uTracks + m_uTrack - NORMAL_DIRECTORY_TRACKS) * (m_uSectors * (m_uSectorSize-2)) + ((m_uSector-1) * (m_uSectorSize-2));

        // Copy the file segment required
        memcpy(pbData_, m_pbData+lPos, min(m_uSectorSize-2, m_uSize - lPos));

        // If there are more sectors in the file, set the chain to the next sector
        if (lPos + m_uSectorSize < m_uSize)
        {
            // Work out the next sector
            UINT uSector = 1 + (m_uSector % m_uSectors);
            UINT uTrack = ((uSector == 1) ? m_uTrack+1 : m_uTrack) % m_uTracks;
            UINT uSide = (!uTrack ? m_uSide+1 : m_uSide) % m_uSides;

            pbData_[m_uSectorSize-2] = uTrack + (uSide ? 0x80 : 0x00);
            pbData_[m_uSectorSize-1] = uSector;
        }
    }

    // Data is always perfect
    return 0;
}

// Write the data for a sector
BYTE CFileDisk::WriteData (BYTE *pbData_, UINT* puSize_)
{
    // File disks are read-only
    return WRITE_PROTECT;
}

// Save the disk out to the stream
bool CFileDisk::Save ()
{
    // Saving not supported
    return false;
}

// Format a track using the specified format
BYTE CFileDisk::FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_)
{
    // Read-only, formatting not supported
    return WRITE_PROTECT;
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CTD0Disk::IsRecognised (CStream* pStream_)
{
    TD0_HEADER th;

    // Read the header, check the signature, Teledisk version and basic geometry
    bool fValid = (pStream_->Rewind() && pStream_->Read(&th, sizeof(th)) == sizeof(th) &&
            (!memcmp(th.abSignature, TD0_SIG_NORMAL, sizeof(th.abSignature)) ||
             !memcmp(th.abSignature, TD0_SIG_ADVANCED, sizeof(th.abSignature))) &&
             th.bTDVersion >= 10 && th.bTDVersion <= 21 &&
             th.bSurfaces >= 1 && th.bSurfaces <= 2);

    // Ensure the header CRC is correct before accepting the image
    fValid &= (CrcBlock(&th, sizeof(th)-2) == ((th.bCRCHigh << 8) | th.bCRCLow));

    return fValid;
}

CTD0Disk::CTD0Disk (CStream* pStream_, UINT uSides_/*=NORMAL_DISK_SIDES*/)
    : CDisk(pStream_, dtTD0), m_pTrack(NULL), m_pFind(NULL)
{
    // We can't create .td0 images yet
    if (!pStream_->IsOpen())
    {
        m_uSides = m_uTracks = 0;
        return;
    }

    // Read the file header, and set up the basic geometry
    pStream_->Rewind();
    pStream_->Read(&m_sHeader, sizeof(m_sHeader));
    m_uSides = m_sHeader.bSurfaces;
    m_uTracks = MAX_DISK_TRACKS;

    // Read in the rest of the file, which is still RLE-compressed
    size_t uSize = pStream_->GetSize()-sizeof(m_sHeader);
    pStream_->Read(m_pbData = new BYTE[uSize], uSize);
    pStream_->Close();

    // If the file is using advanced compression we have Huffman data to unpack too
    if (m_sHeader.abSignature[0] == 't')
    {
        // We don't know the unpacked size, so allocate a block large enough for any disk
        BYTE* pb = new BYTE[MAX_DISK_SIDES*MAX_DISK_TRACKS*MAX_TRACK_SIZE];
        uSize = LZSS::Unpack(m_pbData, uSize, pb);

        // Shrink the buffer to the used size, and clean up
        delete[] m_pbData;
        memcpy(m_pbData = new BYTE[uSize], pb, uSize);
        delete[] pb;
    }

    // We can now index the tracks in the image...
    memset(m_auIndex, 0, sizeof(m_auIndex));
    BYTE* pb = m_pbData;

    // Skip the comment field, if any
    if (m_sHeader.bTrackDensity & 0x80)
    {
        TD0_COMMENT* ptc = reinterpret_cast<TD0_COMMENT*>(pb);
        WORD wLen = (ptc->bLenHigh << 8) | ptc->bLenLow;
        pb += sizeof(TD0_COMMENT) + wLen;
    }

    // Process tracks until we hit the end marker
    for (TD0_TRACK* pt ; (pt = reinterpret_cast<TD0_TRACK*>(pb))->bSectors != 0xff ; )
    {
        // Store the track pointer in the index and advance by the header size
        m_auIndex[pt->bPhysSide][pt->bPhysTrack] = pt;
        pb += sizeof(TD0_TRACK);

        // Loop through the sectors in the track
        for (UINT uSector = 0 ; uSector < pt->bSectors ; uSector++)
        {
            TD0_SECTOR* ps = reinterpret_cast<TD0_SECTOR*>(pb);
            pb += sizeof(TD0_SECTOR);

            // Check if the sector has a data field
            if (!(ps->bFlags & 0x30))
            {
                // Read the data header to obtain the length, and skip over it
                TD0_DATA* pd = reinterpret_cast<TD0_DATA*>(pb);
                pb += sizeof(pd->bOffLow)+sizeof(pd->bOffHigh) + ((pd->bOffHigh << 8) | pd->bOffLow);
            }
        }
    }
}

// Unpack a possibly RLE-encoded data block
void CTD0Disk::UnpackData (TD0_SECTOR* ps_, BYTE* pbSector_)
{
    UINT uSize = (MIN_SECTOR_SIZE << ps_->bSize);
    TD0_DATA* pd = reinterpret_cast<TD0_DATA*>(ps_+1);
    BYTE *pb = reinterpret_cast<BYTE*>(pd+1), *pEnd = pbSector_+uSize;

    while (pbSector_ < pEnd)
    {
        switch (pd->bMethod)
        {
            case 0: // raw sector
                memcpy(pbSector_, pb, uSize);
                pbSector_ += uSize;
                break;

            case 1: // repeated 2-byte pattern
            {
                UINT uCount = (pb[1] << 8) | pb[0], uLen = uCount << 1;
                BYTE b1 = pb[2], b2 = pb[3];
                pb += 4;

                while (uCount--)
                {
                    *pbSector_++ = b1;
                    *pbSector_++ = b2;
                }

                pbSector_ += uLen;
                break;
            }

            case 2: // RLE block
            {
                // Zero count means a literal data block
                if (!pb[0])
                {
                    UINT uLen = pb[1];
                    pb += 2;

                    memcpy(pbSector_, pb, uLen);

                    pbSector_ += uLen;
                    pb += uLen;
                }
                else    // repeated fragment
                {
                    UINT uBlock = 1 << pb[0], uCount = pb[1];
                    pb += 2;

                    for ( ; uCount-- ; pbSector_ += uBlock)
                        memcpy(pbSector_, pb, uBlock);

                    pb += uBlock;
                }
                break;
            }

            default: // error!
                return;
        }
    }
}

// Initialise the enumeration of all sectors in the track
UINT CTD0Disk::FindInit (UINT uSide_, UINT uTrack_)
{
    // Validate the side, track and track pointer
    if (uSide_ >= m_uSides || uTrack_ >= m_uTracks || !(m_pTrack = m_auIndex[uSide_][uTrack_]))
        return m_uSectors = 0;

    // Read the number of sectors available and reset the found sector pointer
    m_uSectors = m_pTrack->bSectors;
    m_pFind = NULL;

    // Call the base and return the number of sectors in the track
    return CDisk::FindInit(uSide_, uTrack_);
}

// Find the next sector in the current track
bool CTD0Disk::FindNext (IDFIELD* pIdField_, BYTE* pbStatus_)
{
    bool fRet = CDisk::FindNext();

    // Make sure there is a 'next' one
    if (fRet)
    {
        if (!m_pFind)
            m_pFind = reinterpret_cast<TD0_SECTOR*>(m_pTrack+1);
        else
        {
            // We always have the sector header to skip
            UINT uSize = sizeof(TD0_SECTOR);

            // If data is present, adjust for the stored data size
            if (!(m_pFind->bFlags & 0x30))
            {
                TD0_DATA* pd = reinterpret_cast<TD0_DATA*>(m_pFind+1);
                uSize += sizeof(pd->bOffLow)+sizeof(pd->bOffHigh) + ((pd->bOffHigh << 8) | pd->bOffLow);
            }

            // Advance the pointer over the header and data to next header
            m_pFind = reinterpret_cast<TD0_SECTOR*>(reinterpret_cast<BYTE*>(m_pFind) + uSize);
        }

        // Copy the ID field to the supplied buffer
        pIdField_->bSide = m_pFind->bSide;
        pIdField_->bTrack = m_pFind->bTrack;
        pIdField_->bSector = m_pFind->bSector;
        pIdField_->bSize = m_pFind->bSize;

        // Set the ID field status
        *pbStatus_ = 0;//(m_pFind->bFlags & 0x04) ? DELETED_DATA : 0;
    }

    // Return true if we're returning a new item
    return fRet;
}

// Read the data for the last sector found
BYTE CTD0Disk::ReadData (BYTE *pbData_, UINT* puSize_)
{
    // If there's no data for this sector, return an error
    if (m_pFind->bFlags & 0x20)
        return RECORD_NOT_FOUND;

    // Fill the returned data size
    *puSize_ = MIN_SECTOR_SIZE << m_pFind->bSize;

    // If this is a not-allocated DOS sector, fill it with zeros, otherwise unpack the real sector data
    if (m_pFind->bFlags & 0x10)
        memset(pbData_, 0, *puSize_);
    else
        UnpackData(m_pFind, pbData_);

    // Fill in the data size and return the statuany data CRC errors, or success
    return (m_pFind->bFlags & 0x02) ? CRC_ERROR : 0;
}

// Read the data for the last sector found
BYTE CTD0Disk::WriteData (BYTE *pbData_, UINT* puSize_)
{
    // Read-only for now
    return WRITE_PROTECT;
}

// Save the disk out to the stream
bool CTD0Disk::Save ()
{
    // Not supported yet
    return false;
}

// Format a track using the specified format
BYTE CTD0Disk::FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_)
{
    // No formatting yet
    return WRITE_PROTECT;
}


// Generate/update a Teledisk CRC (only used for small headers so not worth a look-up table)
WORD CTD0Disk::CrcBlock (const void* pv_, UINT uLen_, WORD wCRC_/*=0*/)
{
    // Work through all bytes in the supplied block
    for (const BYTE* p = reinterpret_cast<const BYTE*>(pv_) ; uLen_-- ; p++)
    {
        // Merge in the input byte
        wCRC_ ^= (*p << 8);

        // Shift through all 8 bits, using the (CCITT?) polynomial 0xa097
        for (int i = 0 ; i < 8 ; i++)
            wCRC_ = (wCRC_ << 1) ^ ((wCRC_ & 0x8000) ? 0xa097 : 0);
    }

    return wCRC_;
}


////////////////////////////////////////////////////////////////////////////////
//
// LZSS Compression - adapted from the original C code by Haruhiko Okumura (1988)
//
// For algorithm/implementation details, as well as general compression info, see:
//   http://www.fadden.com/techmisc/hdc/  (chapter 10 covers LZSS)
// 
// Tweaked and reformatted to improve my own understanding, and wrapped in a class
// to avoid polluting the global namespace with the variables below.

#define N           4096                  // ring buffer size
#define F           60                    // lookahead buffer size
#define THRESHOLD   2                     // match needs to be longer than this for position/length coding

#define N_CHAR      (256 - THRESHOLD + F) // kinds of characters (character code = 0..N_CHAR-1)
#define T           (N_CHAR * 2 - 1)      // size of table
#define R           (T - 1)               // tree root position
#define MAX_FREQ    0x8000                // updates tree when root frequency reached this value


short LZSS::parent[T + N_CHAR];           // parent nodes (0..T-1) and leaf positions (rest)
short LZSS::son[T];                       // pointers to child nodes (son[], son[] + 1)
WORD LZSS::freq[T + 1];                   // frequency table

BYTE LZSS::ring_buff[N + F - 1];          // text buffer for match strings
UINT LZSS::r;                             // Ring buffer position

BYTE *LZSS::pIn, *LZSS::pEnd;             // current and end input pointers
UINT LZSS::uBits, LZSS::uBitBuff;         // buffered bit count and left-aligned bit buffer


BYTE LZSS::d_len[] = { 3,3,4,4,4,5,5,5,5,6,6,6,7,7,7,8 };

BYTE LZSS::d_code[256] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
    0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D, 0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F, 0x0F,
    0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11, 0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13,
    0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15, 0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
    0x18, 0x18, 0x19, 0x19, 0x1A, 0x1A, 0x1B, 0x1B, 0x1C, 0x1C, 0x1D, 0x1D, 0x1E, 0x1E, 0x1F, 0x1F,
    0x20, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23, 0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27,
    0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B, 0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
};


// Initialise the trees and state variables
void LZSS::Init ()
{
    UINT i;

    for (i = 0; i < N_CHAR; i++)
    {
        freq[i] = 1;
        son[i] = i + T;
        parent[i + T] = i;
    }

    i = 0;
    for (int j = N_CHAR ; j <= R ; i += 2, j++)
    {
        freq[j] = freq[i] + freq[i + 1];
        son[j] = i;
        parent[i] = parent[i + 1] = j;
    }

    uBitBuff = uBits = 0;
    memset(ring_buff, ' ', sizeof(ring_buff));

    freq[T] = 0xffff;
    parent[R] = 0;

    r = N - F;
}

// Rebuilt the tree
void LZSS::RebuildTree ()
{
    UINT i, j, k, f, l;

    // Collect leaf nodes in the first half of the table and replace the freq by (freq + 1) / 2
    for (i = j = 0; i < T; i++)
    {
        if (son[i] >= T)
        {
            freq[j] = (freq[i] + 1) / 2;
            son[j] = son[i];
            j++;
        }
    }

    // Begin constructing tree by connecting sons
    for (i = 0, j = N_CHAR; j < T; i += 2, j++)
    {
        k = i + 1;
        f = freq[j] = freq[i] + freq[k];
        for (k = j - 1; f < freq[k]; k--);
        k++;
        l = (j - k) * sizeof(*freq);

        memmove(&freq[k + 1], &freq[k], l);
        freq[k] = f;
        memmove(&son[k + 1], &son[k], l);
        son[k] = i;
    }

    // Connect parent
    for (i = 0 ; i < T ; i++)
        if ((k = son[i]) >= T)
            parent[k] = i;
        else
            parent[k] = parent[k + 1] = i;
}


// Increment frequency of given code by one, and update tree
void LZSS::UpdateTree (int c)
{
    UINT i, j, k, l;

    if (freq[R] == MAX_FREQ)
        RebuildTree();

    c = parent[c + T];

    do
    {
        k = ++freq[c];

        // If the order is disturbed, exchange nodes
        if (k > freq[l = c + 1])
        {
            while (k > freq[++l]);
            l--;
            freq[c] = freq[l];
            freq[l] = k;

            i = son[c];
            parent[i] = l;
            if (i < T)
                parent[i + 1] = l;

            j = son[l];
            son[l] = i;

            parent[j] = c;
            if (j < T)
                parent[j + 1] = c;
            son[c] = j;

            c = l;
        }
    }
    while ((c = parent[c]) != 0);  // Repeat up to root
}


// Get one bit
UINT LZSS::GetBit ()
{
    if (!uBits--)
    {
        uBitBuff |= GetChar() << 8;
        uBits = 7;
    }

    uBitBuff <<= 1;
    return (uBitBuff >> 16) & 1;
}

// Get one byte
UINT LZSS::GetByte ()
{
    if (uBits < 8)
        uBitBuff |= GetChar() << (8 - uBits);
    else
        uBits -= 8;

    uBitBuff <<= 8;
    return (uBitBuff >> 16) & 0xff;
}

UINT LZSS::DecodeChar ()
{
    UINT c = son[R];

    // Travel from root to leaf, choosing the smaller child node (son[]) if the
    // read bit is 0, the bigger (son[]+1} if 1
    while(c < T)
        c = son[c + GetBit()];

    c -= T;
    UpdateTree(c);
    return c;
}

UINT LZSS::DecodePosition ()
{
    UINT i, j, c;

    // Recover upper 6 bits from table
    i = GetByte();
    c = d_code[i] << 6;
    j = d_len[i >> 4];

    // Read lower 6 bits verbatim
    for (j -= 2 ; j-- ; i = (i << 1) | GetBit());

    return c | (i & 0x3f);
}


// Unpack a given block into the supplied output buffer
size_t LZSS::Unpack (BYTE* pIn_, size_t uSize_, BYTE* pOut_)
{
    UINT  i, j, c;
    size_t uCount = 0;

    // Store the input start/end positions and prepare to unpack
    pEnd = (pIn = pIn_) + uSize_;
    Init();

    // Loop until we've processed all the input
    while (pIn < pEnd)
    {
        c = DecodeChar();

        // Single output character?
        if (c < 256)
        {
            *pOut_++ = c;
            uCount++;

            // Update the ring buffer and position (wrapping if necessary)
            ring_buff[r++] = c;
            r &= (N - 1);
        }
        else
        {
            // Position in ring buffer and length
            i = (r - DecodePosition() - 1) & (N - 1);
            j = c - 255 + THRESHOLD;

            // Output the block
            for (UINT k = 0; k < j; k++)
            {
                c = ring_buff[(i + k) & (N - 1)];
                *pOut_++ = c;
                uCount++;

                ring_buff[r++] = c;
                r &= (N - 1);
            }
        }
    }

    // Return the unpacked size
    return uCount;
}
