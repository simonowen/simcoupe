// Part of SimCoupe - A SAM Coupé emulator
//
// CDisk.cpp: C++ classes used for accessing all SAM disk image types
//
//  Copyright (c) 1999-2001  Simon Owen
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

// ToDo:
//  - finish the new SDF disk format (tagged structure, etc.)

// Notes:
//  The current SDF images all lack a proper header, are read-only and fixed
//  at 80 tracks per side.  They will continue to be supported once the new
//  version is complete, but writeable images will be upgraded automatically.
//
//  The CFloppyDisk implementation is OS-specific, and is in Floppy.cpp

#include "SimCoupe.h"
#include "CDisk.h"

#include "Floppy.h"

////////////////////////////////////////////////////////////////////////////////

/*static*/ int CDisk::GetType (const char* pcszDisk_)
{
    // Open a read-only stream to examine
    CStream* pStream = CStream::Open(pcszDisk_, true);

    if (pStream)
    {
        // Try each type in turn to check for a match
        if (CFloppyDisk::IsRecognised(pStream))
            return dtFloppy;
        else if (CSDFDisk::IsRecognised(pStream))
            return dtSDF;
        else if (CSADDisk::IsRecognised(pStream))
            return dtSAD;
        else if (CDSKDisk::IsRecognised(pStream))
            return dtDSK;
        else if (CFileDisk::IsRecognised(pStream) && strlen(pcszDisk_) > 4 && !strcasecmp(pcszDisk_ + strlen(pcszDisk_) - 4, ".sbt"))
            return dtSBT;
    }

    // Not recognised
    return dtUnknown;
}

/*static*/ CDisk* CDisk::Open (const char* pcszDisk_, bool fReadOnly_/*=false*/)
{
    // First of all try and get a data stream for the disk
    CStream* pStream = CStream::Open(pcszDisk_, fReadOnly_);

    // A disk will only be returned if the stream format is recognised
    CDisk* pDisk = NULL;

    if (pStream)
    {
        switch (GetType(pcszDisk_))
        {
            case dtFloppy:  pDisk = new CFloppyDisk(pStream);   break;      // Direct floppy access
            case dtSDF:     pDisk = new CSDFDisk(pStream);      break;      // .SDF
            case dtSAD:     pDisk = new CSADDisk(pStream);      break;      // .SAD
            case dtDSK:     pDisk = new CDSKDisk(pStream);      break;      // .DSK
            case dtSBT:     pDisk = new CFileDisk(pStream);     break;      // .SBT (bootable SAM file on a floppy)
        }
    }

    // Return the disk pointer, or NULL if we didn't recognise it
    return pDisk;
}


CDisk::CDisk (CStream* pStream_)
{
    // Save the supplied stream
    m_pStream = pStream_;

    // No geometry information, current track/size or spin position
    m_nSides = m_nTracks = m_nSectors = m_nSector = 0;
    m_nSide = m_nTrack = -1;
    m_nSpinPos = 1;

    // No sector data available
    m_pbData = NULL;

    // Disk not modified, yet
    SetModified(false);

}

/*virtual*/ CDisk::~CDisk ()
{
    // Delete the stream object and disk data memory we allocated
    if (m_pStream) { delete m_pStream; m_pStream = NULL; }
    if (m_pbData) { delete m_pbData; m_pbData = NULL; }
}


// Sector spin position on the spinning disk, as used by the READ_ADDRESS command
int CDisk::GetSpinPos (bool fAdvance_/*=false*/)
{
    // If required, move to the next spin position for next time
    if (fAdvance_)
        m_nSpinPos = (m_nSpinPos % (m_nSectors + !m_nSectors)) + 1;

    // Return the current/new position
    return m_nSpinPos;
}

// Initialise a sector enumeration, return the number of sectors on the track
int CDisk::FindInit (int nSide_, int nTrack_)
{
    // Remember the current side and track number
    m_nSide = nSide_;
    m_nTrack = nTrack_;

    // Set current sector to zero so the next sector is sector 1
    m_nSector = 0;

    // Return the number of sectors on the current track
    return (m_nSide < m_nSides && m_nTrack < m_nTracks) ? m_nSectors : 0;
}

bool CDisk::FindNext (IDFIELD* pIdField_/*=NULL*/, BYTE* pbStatus_/*=NULL*/)
{
    // Advance to the next sector
    m_nSector++;

    // Construct a normal ID field for the sector
    if (pIdField_)
    {
        pIdField_->bSide = m_nSide;
        pIdField_->bTrack = m_nTrack;
        pIdField_->bSector = m_nSector;
        pIdField_->bSize = 2;                       // 128 << 2 = 512 bytes
        pIdField_->bCRC1 = pIdField_->bCRC2 = 0;
    }

    // Sector OK so reset all errors
    if (pbStatus_)
        *pbStatus_ = 0;

    // Return true if the sector exists
    return (m_nSide < m_nSides && m_nTrack < m_nTracks) && m_nSector <= m_nSectors;
}


// Locate and read the specific sector from the disk
bool CDisk::FindSector (int nSide_, int nTrack_, int nSector_, IDFIELD* pID_/*=NULL*/)
{
    // Assume 'record not found' with zero bytes read, until we discover otherwise
    bool fFound = false;

    // Only check for sectors if there are some to check
    if (FindInit(nSide_, nTrack_))
    {
        IDFIELD id;
        BYTE bStatus;

        // Loop through all the sectors in the track
        while (FindNext(&id, &bStatus))
        {
            // Check to see if the side, track and sector numbers match, and the CRC is correct
            if (id.bSide == nSide_ && id.bTrack == nTrack_ && id.bSector == nSector_ && !bStatus)
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

CDSKDisk::CDSKDisk (CStream* pStream_) : CDisk(pStream_)
{
    // The DSK geometry is fixed
    m_nSides = NORMAL_DISK_SIDES;
    m_nTracks = NORMAL_DISK_TRACKS;
    m_nSectors = NORMAL_DISK_SECTORS;
    m_nSectorSize = NORMAL_SECTOR_SIZE;

    // Work out the disk size, allocate some memory for it
    m_pbData = new BYTE[DSK_IMAGE_SIZE];

    // Read the data from any existing stream, or create and save a new disk
    if (pStream_->IsOpen())
    {
        pStream_->Rewind();
        m_nSectors = pStream_->Read(m_pbData, DSK_IMAGE_SIZE) / (NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS * NORMAL_SECTOR_SIZE);
    }
    else
    {
        memset(m_pbData, 0, DSK_IMAGE_SIZE);
        SetModified();
    }
}

/*static*/ bool CDSKDisk::IsRecognised (CStream* pStream_)
{
    bool fValid = false;
    BYTE* pb = new BYTE[DSK_IMAGE_SIZE];

    // It's only a DSK image if it's exactly the right size, since there's no signature to check for
    if (pb)
    {
        fValid = (pStream_->Rewind() && pStream_->Read(pb, DSK_IMAGE_SIZE) == DSK_IMAGE_SIZE && !pStream_->Read(pb, 1)) ||
                 (pStream_->Rewind() && pStream_->Read(pb, MSDOS_IMAGE_SIZE) == MSDOS_IMAGE_SIZE && !pStream_->Read(pb, 1));
        delete pb;
    }

    return fValid;
}


// Read the data for the last sector found
BYTE CDSKDisk::ReadData (BYTE *pbData_, UINT* puSize_)
{
    // Work out the offset for the required data
    long lPos = (m_nSide + 2 * m_nTrack) * (m_nSectors * m_nSectorSize) + ((m_nSector-1) * m_nSectorSize);

    // Copy the sector data from the image buffer
    memcpy(pbData_, m_pbData + lPos, *puSize_ = m_nSectorSize);

    // Data is always perfect on DSK images, so return OK
    return 0;
}

// Read the data for the last sector found
BYTE CDSKDisk::WriteData (BYTE *pbData_, UINT* puSize_)
{
    // Fail if read-only
    if (IsReadOnly())
        return WRITE_PROTECT;

    // Work out the offset for the required data
    long lPos = (m_nSide + 2 * m_nTrack) * (m_nSectors * m_nSectorSize) + ((m_nSector-1) * m_nSectorSize);

    // Copy the sector data to the image buffer, and set the modified flag
    memcpy(m_pbData + lPos, pbData_, *puSize_ = m_nSectorSize);
    SetModified();

    // Data is always perfect on DSK images, so return OK
    return 0;
}

// Save the disk out to the stream
bool CDSKDisk::Save ()
{
    // Write the image out as a single block
    if (m_pStream->Rewind() && m_pStream->Write(m_pbData, DSK_IMAGE_SIZE) == DSK_IMAGE_SIZE)
    {
        SetModified(false);
        return true;
    }

    TRACE("!!! CDSKDisk::Save() failed to write modified disk contents!\n");
    return false;
}

// Format a track using the specified format
BYTE CDSKDisk::FormatTrack (int nSide_, int nTrack_, IDFIELD* paID_, int nSectors_)
{
    // Disk must be writable and must be the same number of sectors
    if (!IsReadOnly() && nSectors_ == m_nSectors)
    {
        DWORD dwSectors = 0;
        bool fNormal = true;

        // Make sure the remaining sectors are completely normal
        for (int i = 0 ; i < nSectors_ ; i++)
        {
            // Side and track must match the ones it's being laid on
            fNormal &= (paID_[i].bSide == nSide_ && paID_[i].bTrack == nTrack_);

            // Sector size must be the same
            fNormal &= ((128 << paID_[i].bSize) == m_nSectorSize);

            // Remember we've seen this sector number
            dwSectors |= (1 << (paID_[i].bSector-1));
        }

        // There must be only 1 of each sector number from 1 to N (in any order though)
        fNormal &= (dwSectors == ((1UL << m_nSectors) - 1));


        // All all the above checks out it's a normal sector
        if (fNormal)
        {
            // Work out the offset for the required track
            long lPos = (nSide_ + 2 * nTrack_) * (m_nSectors * m_nSectorSize);

            // Formatting it requires simply clearing the existing data
            memset(m_pbData + lPos, 0, m_nSectors * m_nSectorSize);
            SetModified();

            // No error
            return 0;
        }
    }

    return WRITE_PROTECT;
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CSADDisk::IsRecognised (CStream* pStream_)
{
    SAD_HEADER sh;

    // Read the header, check for the signature, and make sure the disk geometry is sensible
    bool fRecognised = (pStream_->Rewind() && pStream_->Read(&sh, sizeof sh) == sizeof sh &&
            !memcmp(sh.abSignature, SAD_SIGNATURE, sizeof sh.abSignature) &&
            sh.bSides && sh.bSides <= MAX_DISK_SIDES && sh.bTracks && sh.bTracks <= 127 &&
            sh.bSectorSizeDiv64 && (sh.bSectorSizeDiv64 <= (MAX_SECTOR_SIZE >> 6)) &&
            (sh.bSectorSizeDiv64 & -sh.bSectorSizeDiv64) == sh.bSectorSizeDiv64);

    return fRecognised;
}

CSADDisk::CSADDisk (CStream* pStream_, int nSides_/*=NORMAL_DISK_SIDES*/, int nTracks_/*=NORMAL_DISK_TRACKS*/,
    int nSectors_/*=NORMAL_DISK_SECTORS*/, int nSectorSize_/*=NORMAL_SECTOR_SIZE*/) : CDisk(pStream_)
{
    SAD_HEADER sh = { "", nSides_, nTracks_, nSectors_, nSectorSize_ >> 6 };

    if (!pStream_->IsOpen())
        memcpy(sh.abSignature, SAD_SIGNATURE, sizeof sh.abSignature);
    else
    {
        // Read the header and extract the disk geometry
        pStream_->Rewind();
        pStream_->Read(&sh, sizeof sh);
    }

    m_nSides = sh.bSides;
    m_nTracks = sh.bTracks;
    m_nSectors = sh.bSectors;
    m_nSectorSize = sh.bSectorSizeDiv64 << 6;

    // Work out the disk size, allocate some memory for it and read it in
    int nDiskSize = sizeof sh + m_nSides * m_nTracks * m_nSectors * m_nSectorSize;
    memcpy(m_pbData = new BYTE[nDiskSize], &sh, sizeof sh);

    // Read the contents if we've got a stream, or create and save a new one
    if (pStream_->IsOpen())
        pStream_->Read(m_pbData + sizeof sh, nDiskSize - sizeof sh);
    else
    {
        memset(m_pbData + sizeof sh, 0, nDiskSize - sizeof sh);
        SetModified();
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
        for (UINT uSize = m_nSectorSize/128 ; !(uSize & 1) ; (pIdField_->bSize)++, uSize >>= 1);
    }

    // Return true if we're returning a new item
    return fRet;
}

// Read the data for the last sector found
BYTE CSADDisk::ReadData (BYTE *pbData_, UINT* puSize_)
{
    // Work out the offset for the required data
    long lPos = sizeof(SAD_HEADER) + (m_nSide * m_nTracks + m_nTrack) * (m_nSectors * NORMAL_SECTOR_SIZE) + ((m_nSector-1) * NORMAL_SECTOR_SIZE) ;

    // Copy the sector data from the image buffer
    memcpy(pbData_, m_pbData + lPos, *puSize_ = NORMAL_SECTOR_SIZE);

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
    long lPos = sizeof(SAD_HEADER) + (m_nSide * m_nTracks + m_nTrack) * (m_nSectors * NORMAL_SECTOR_SIZE) + ((m_nSector-1) * NORMAL_SECTOR_SIZE) ;

    // Copy the sector data to the image buffer, and set the modified flag
    memcpy(m_pbData + lPos, pbData_, *puSize_ = NORMAL_SECTOR_SIZE);
    SetModified();

    // Data is always perfect on SAD images, so return OK
    return 0;
}

// Save the disk out to the stream
bool CSADDisk::Save ()
{
    int nDiskSize = sizeof(SAD_HEADER) + m_nSides * m_nTracks * m_nSectors * m_nSectorSize;

    if (m_pStream->Rewind() && m_pStream->Write(m_pbData, nDiskSize) == nDiskSize)
    {
        SetModified(false);
        return true;
    }

    TRACE("!!! SADDisk::Save() failed to write modified disk contents!\n");
    return false;
}

// Format a track using the specified format
BYTE CSADDisk::FormatTrack (int nSide_, int nTrack_, IDFIELD* paID_, int nSectors_)
{
    // Disk must be writable and must be the same number of sectors
    if (!IsReadOnly() && nSectors_ == m_nSectors)
    {
        DWORD dwSectors = 0;
        bool fNormal = true;

        // Make sure the remaining sectors are completely normal
        for (int i = 0 ; i < nSectors_ ; i++)
        {
            // Side and track must match the ones it's being laid on
            fNormal &= (paID_[i].bSide == nSide_ && paID_[i].bTrack == nTrack_);

            // Sector size must be the same
            fNormal &= ((128 << paID_[i].bSize) == m_nSectorSize);

            // Remember we've seen this sector number
            dwSectors |= (1 << (paID_[i].bSector-1));
        }

        // There must be only 1 of each sector number from 1 to N (in any order though)
        fNormal &= (dwSectors == ((1UL << m_nSectors) - 1));


        // All all the above checks out it's a normal sector
        if (fNormal)
        {
            // Work out the offset for the required track
            long lPos = sizeof(SAD_HEADER) + (m_nSide * m_nTracks + m_nTrack) * (m_nSectors * NORMAL_SECTOR_SIZE);

            // Formatting it requires simply clearing the existing data
            memset(m_pbData + lPos, 0, m_nSectors * m_nSectorSize);
            SetModified();

            // No error
            return 0;
        }
    }

    return WRITE_PROTECT;
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CSDFDisk::IsRecognised (CStream* pStream_)
{
    bool fValid = false;

    BYTE* pb = new BYTE [OLD_SDF_IMAGE_SIZE];
    if (pb)
    {
        // Old files have no header and are a fixed size
        if (pStream_->Rewind() && (pStream_->Read(pb, OLD_SDF_IMAGE_SIZE) == OLD_SDF_IMAGE_SIZE) && !pStream_->Read(pb, 1))
            fValid = true;

        // Read the header
        else
        {
            BYTE abSignature[sizeof SDF_SIGNATURE];

            if (pStream_->Rewind() && pStream_->Read(&abSignature, sizeof abSignature) == sizeof abSignature &&
                !memcmp(abSignature, SDF_SIGNATURE, sizeof abSignature))
            {
                // Check for the signature, and make sure the geometry is sensible
//              if (!memcmp(abSignature, SDF_SIGNATURE, sizeof abSignature) &&
//                  sh.bSides && sh.bSides <= MAX_DISK_SIDES && sh.bTracks && sh.bTracks <= MAX_DISK_TRACKS)
                    fValid = true;
            }
        }

        delete pb;
    }

    return fValid;
}

CSDFDisk::CSDFDisk (CStream* pStream_, int nSides_/*=NORMAL_DISK_SIDES*/, int nTracks_/*=MAX_DISK_TRACKS*/)
    : CDisk(pStream_)
{
    pStream_->Rewind();

    // Read the signature
    BYTE abSignature[4];
    pStream_->Read(&abSignature, sizeof abSignature);

    // Read the header (if any)
    SDF_HEADER sh;
    pStream_->Read(&sh, sizeof sh);
/*
    // If we find the disk signature, extract the disk geometry from it
    if (!memcmp(&abSignature, SDF_SIGNATURE, sizeof abSignature))
    {
        m_nSides = sh.bSides;
        m_nTracks = sh.bTracks;
    }

    // Else we have an old format without a header
    else
*/
    {
        // Set up the fixed geometry used by old version
        m_nSides = NORMAL_DISK_SIDES;
        m_nTracks = NORMAL_DISK_TRACKS;

        // Rewind to the start to the data again
        pStream_->Rewind();

        // If the stream isn't read-only, mark the disk as modified so it gets written back in the new format
//      if (!pStream_->IsReadOnly())
//          SetModified();
    }

    // SDF images don't have a fixed number of sectors per track or sector size
    m_nSectors = 0;
    m_nSectorSize = 0;

    // Work out the disk size, allocate some memory for it and read it in
    int nDiskSize = m_nSides * m_nTracks * SDF_TRACKSIZE;
    pStream_->Read(m_pbData = new BYTE[nDiskSize], nDiskSize);
}


// Initialise the enumeration of all sectors in the track
int CSDFDisk::FindInit (int nSide_, int nTrack_)
{
    if (nSide_ >= m_nSides || nTrack_ >= m_nTracks)
        return m_nSectors = 0;

    // Locate the track in the buffer to determine the number of sectors available
    int nPos = /*sizeof SDF_HEADER*/ + (SDF_TRACKSIZE)*(nSide_*m_nTracks + nTrack_);
    m_pTrack = reinterpret_cast<SDF_TRACK_HEADER*>(m_pbData+nPos);
    m_nSectors = m_pTrack->bSectors;

    m_pFind = NULL;

    // Call the base and return the number of sectors in the track
    return CDisk::FindInit(nSide_, nTrack_);
}

// Find the next sector in the current track
bool CSDFDisk::FindNext (IDFIELD* pIdField_, BYTE* pbStatus_)
{
    bool fRet;

    // Make sure there is a 'next' one
    if (fRet = CDisk::FindNext())
    {
        if (!m_pFind)
            m_pFind = reinterpret_cast<SDF_SECTOR_HEADER*>(&m_pTrack[1]);
        else
        {
            // The data length is zero if there is a problem with the ID header
            int nSize = m_pFind->bIdStatus ? 0 : (MIN_SECTOR_SIZE << m_pFind->sIdField.bSize);

            // Advance the pointer over the header and data to next header
            m_pFind = reinterpret_cast<SDF_SECTOR_HEADER*>(reinterpret_cast<BYTE*>(m_pFind) + nSize + sizeof(SDF_SECTOR_HEADER));
        }

        // Copy the ID field to the supplied buffer, and store the ID status
        memcpy(pIdField_, &m_pFind->sIdField, sizeof(*pIdField_));
        *pbStatus_ = m_pFind->bIdStatus;
    }

    // Return true if we're returning a new item
    return fRet;
}

// Read the data for the last sector found
BYTE CSDFDisk::ReadData (BYTE *pbData_, UINT* puSize_)
{
    // Copy the sector data
    memcpy(pbData_, reinterpret_cast<BYTE*>(m_pFind+1), *puSize_ = MIN_SECTOR_SIZE << m_pFind->sIdField.bSize);

    // Return the data status to include a possible CRC error
    return m_pFind->bDataStatus;
}

// Read the data for the last sector found
BYTE CSDFDisk::WriteData (BYTE *pbData_, UINT* puSize_)
{
    // Fail if read-only
//  if (IsReadOnly())
        return WRITE_PROTECT;

    // Copy the sector data
    memcpy(reinterpret_cast<BYTE*>(m_pFind+1), pbData_, *puSize_ = MIN_SECTOR_SIZE << m_pFind->sIdField.bSize);
    SetModified();

    // Written ok
    return 0;
}

// Save the disk out to the stream
bool CSDFDisk::Save ()
{
    if (IsModified())
    {
    }

    return true;
}

// Format a track using the specified format
BYTE CSDFDisk::FormatTrack (int nSide_, int nTrack_, IDFIELD* paID_, int nSectors_)
{
    // Failed :-/
    return WRITE_PROTECT;
}

////////////////////////////////////////////////////////////////////////////////


CFloppyDisk::CFloppyDisk (CStream* pStream_) : CDisk(pStream_)
{
    // We can assume this as only CFloppyStream will recognise the real device, and we are always the disk used
    m_pFloppy = reinterpret_cast<CFloppyStream*>(pStream_);

    // Maximum supported geometry
    m_nSides = 2;
    m_nTracks = 83;
    m_nSectors = 10;
    m_nSectorSize = 512;

    // Work out the disk size, allocate some memory for it and read it in
    int nDiskSize = m_nSides * m_nTracks * m_nSectors * m_nSectorSize;
    m_pbData = new BYTE[nDiskSize];
}

/*static*/ bool CFloppyDisk::IsRecognised (CStream* pStream_)
{
    return CFloppyStream::IsRecognised(pStream_->GetName());
}


// Find the next sector in the current track
bool CFloppyDisk::FindNext (IDFIELD* pIdField_, BYTE* pbStatus_)
{
    bool fRet = CDisk::FindNext();

    // Make sure there is a 'next' one
    if (fRet)
    {
        // Construct a normal ID field for the sector
        pIdField_->bSide = m_nSide;
        pIdField_->bTrack = m_nTrack;
        pIdField_->bSector = m_nSector;
        pIdField_->bSize = 2;                       // 128 << 2 = 512 bytes
        pIdField_->bCRC1 = pIdField_->bCRC2 = 0;

        // Sector OK so reset all errors
        *pbStatus_ = 0;
    }

    // Return true if we're returning a new item
    return fRet;
}

// Read the data for the last sector found
BYTE CFloppyDisk::ReadData (BYTE *pbData_, UINT* puSize_)
{
    return m_pFloppy->Read(m_nSide, m_nTrack, m_nSector, pbData_, puSize_);
}

// Write the data for a sector
BYTE CFloppyDisk::WriteData (BYTE *pbData_, UINT* puSize_)
{
    return m_pFloppy->Write(m_nSide, m_nTrack, m_nSector, pbData_, puSize_);
}

// Save the disk out to the stream
bool CFloppyDisk::Save ()
{
    // Eveything's currently written immediately, but we'll need to do more if/when writes are cached
    return true;
}

// Format a track using the specified format
BYTE CFloppyDisk::FormatTrack (int nSide_, int nTrack_, IDFIELD* paID_, int nSectors_)
{
    // Failed :-/
    return WRITE_PROTECT;
}

////////////////////////////////////////////////////////////////////////////////


CFileDisk::CFileDisk (CStream* pStream_) : CDisk(pStream_)
{
    // The DSK geometry is fixed
    m_nSides = NORMAL_DISK_SIDES;
    m_nTracks = NORMAL_DISK_TRACKS;
    m_nSectors = NORMAL_DISK_SECTORS;
    m_nSectorSize = NORMAL_SECTOR_SIZE;

    // Work out the maximum file size, allocate some memory for it
    int nMaxSize = ((NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS) - NORMAL_DIRECTORY_TRACKS) * NORMAL_DISK_SECTORS * (NORMAL_SECTOR_SIZE-2) - DISK_FILE_HEADER_SIZE;
    m_pbData = new BYTE[nMaxSize];

    // Read the data from any existing stream, or show an empty disk as we don't support saving
    if (!pStream_->IsOpen())
        memset(m_pbData, 0, nMaxSize);
    else
    {
        // Read in the file, leaving a 9-byte gap for the disk file header
        pStream_->Rewind();
        m_lSize = (pStream_->Read(m_pbData + DISK_FILE_HEADER_SIZE, nMaxSize));

        // Create the disk file header
        m_pbData[0] = 19;                           // CODE file type
        m_pbData[1] = m_lSize & 0xff;               // LSB of size mod 16384
        m_pbData[2] = (m_lSize >> 8) & 0xff;        // MSB of size mod 16384
        m_pbData[3] = 0x00;                         // LSB of offset start
        m_pbData[4] = 0x80;                         // MSB of offset start
        m_pbData[5] = 0xff;                         // Unused
        m_pbData[6] = 0xff;                         // Unused
        m_pbData[7] = (m_lSize >> 14) & 0xff;       // Number of pages (size div 16384)
        m_pbData[8] = 0x01;                         // Starting page number

        m_lSize += DISK_FILE_HEADER_SIZE;
    }
}

/*static*/ bool CFileDisk::IsRecognised (CStream* pStream_)
{
    bool fValid = false;

    int nMaxSize = ((NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS) - NORMAL_DIRECTORY_TRACKS) * NORMAL_DISK_SECTORS * (NORMAL_SECTOR_SIZE-2);
    BYTE* pb = new BYTE[DSK_IMAGE_SIZE];

    // It's only a DSK image if it's exactly the right size, since there's no signature to check for
    if (pb)
    {
        fValid = pStream_->Rewind() && (pStream_->Read(pb, nMaxSize) <= nMaxSize || !pStream_->Read(pb, 1));
        delete pb;
    }

    return fValid;
}


// Read the data for the last sector found
BYTE CFileDisk::ReadData (BYTE *pbData_, UINT* puSize_)
{
    // Clear the sector out
    memset(pbData_, 0, *puSize_ = m_nSectorSize);

    // The first sector is the directory entry we need to fill
    if (!m_nTrack && m_nSector == 1)
    {
        // CODE file type
        pbData_[0] = 19;

        // Strip any file extension and use up to the first 10 chars for the filename on the disk
        const char *pcszName = m_pStream->GetName(), *pcszExt = strrchr(pcszName, '.'), *pcsz;
        for (pcsz = pcszName+strlen(pcszName)-1 ; pcsz[-1] != '/' && pcsz[-1] != '\\' && pcsz > pcszName ; pcsz--);
        int nLen = strlen(pcsz) - (pcszExt ? strlen(pcszExt) : 0);
        memset(pbData_+1, ' ', 10);
        memcpy(pbData_+1, pcsz, min(nLen, 10));

        // Number of sectors required
        WORD wSectors = (m_lSize + m_nSectorSize-3) / (m_nSectorSize-2);
        pbData_[11] = wSectors >> 8;
        pbData_[12] = wSectors & 0xff;

        // Starting track and sector
        pbData_[13] = NORMAL_DIRECTORY_TRACKS;
        pbData_[14] = 1;

        // Sector address map
        memset(pbData_+15, 0xff, wSectors >> 3);
        if (wSectors & 7)
            pbData_[15 + (wSectors >> 3)] = (1L << (wSectors & 7)) - 1;

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
    else if (m_nTrack >= NORMAL_DIRECTORY_TRACKS)
    {
        // Work out the offset for the required data
        long lPos = (m_nSide * m_nTracks + m_nTrack - NORMAL_DIRECTORY_TRACKS) * (m_nSectors * (m_nSectorSize-2)) + ((m_nSector-1) * (m_nSectorSize-2));

        // Copy the file segment required
        memcpy(pbData_, m_pbData+lPos, min(m_nSectorSize-2, m_lSize - lPos));

        // If there are more sectors in the file, set the chain to the next sector
        if (lPos + m_nSectorSize < m_lSize)
        {
            // Work out the next sector
            int nSector = 1 + (m_nSector % m_nSectors);
            int nTrack = ((nSector == 1) ? m_nTrack+1 : m_nTrack) % m_nTracks;
            int nSide = (!nTrack ? m_nSide+1 : m_nSide) % m_nSides;

            pbData_[m_nSectorSize-2] = nTrack + (nSide ? 0x80 : 0x00);
            pbData_[m_nSectorSize-1] = nSector;
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
BYTE CFileDisk::FormatTrack (int nSide_, int nTrack_, IDFIELD* paID_, int nSectors_)
{
    // Read-only, formatting not supported
    return WRITE_PROTECT;
}
