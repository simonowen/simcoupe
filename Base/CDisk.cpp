// Part of SimCoupe - A SAM Coupe emulator
//
// CDisk.cpp: C++ classes used for accessing all SAM disk image types
//
//  Copyright (c) 1999-2002  Simon Owen
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
//  - Complete FDI image support, once a reliable Disk2FDI is released.

// Notes:
//  SDF images lack a proper header, are read-only, and fixed at 80
//  tracks per side.  The format is to be replaced by FDI, though
//  read-only SDF support will remain for the forseeable future.
//
//  The CFloppyDisk implementation is OS-specific, and is in Floppy.cpp

#include "SimCoupe.h"
#include "CDisk.h"

#include "Floppy.h"

////////////////////////////////////////////////////////////////////////////////

/*static*/ int CDisk::GetType (CStream* pStream_)
{
    if (pStream_)
    {
        // Try each type in turn to check for a match
        if (CFloppyDisk::IsRecognised(pStream_))
            return dtFloppy;
/*
        else if (CFDIDisk::IsRecognised(pStream_))  // Not supported until Disk2FDI 1.0 is released
            return dtFDI;
*/
        else if (CSDFDisk::IsRecognised(pStream_))
            return dtSDF;
        else if (CSADDisk::IsRecognised(pStream_))
            return dtSAD;
        else if (CFileDisk::IsRecognised(pStream_))
        {
            // For now we'll only accept single files if they have a .SBT extension on the filename
            const char* pcszDisk = pStream_->GetName();
            if (strlen(pcszDisk) > 4 && !strcasecmp(pcszDisk + strlen(pcszDisk) - 4, ".sbt"))
                return dtSBT;
        }

        // The original DSK format has no signature, so we try it last
        if (CDSKDisk::IsRecognised(pStream_))
            return dtDSK;
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
        switch (GetType(pStream))
        {
            case dtFloppy:  pDisk = new CFloppyDisk(pStream);   break;      // Direct floppy access
//          case dtFDI:     pDisk = new CFDIDisk(pStream);      break;      // .FDI
            case dtSDF:     pDisk = new CSDFDisk(pStream);      break;      // .SDF
            case dtSAD:     pDisk = new CSADDisk(pStream);      break;      // .SAD
            case dtDSK:     pDisk = new CDSKDisk(pStream);      break;      // .DSK
            case dtSBT:     pDisk = new CFileDisk(pStream);     break;      // .SBT (bootable SAM file on a floppy)
        }
    }

    // Return the disk pointer, or NULL if we didn't recognise it
    return pDisk;
}


CDisk::CDisk (CStream* pStream_, int nType_)
    : m_nType(nType_), m_uSides(0), m_uTracks(0), m_uSectors(0), m_uSectorSize(0),
      m_uSide(0), m_uTrack(0), m_uSector(0), m_fModified(false), m_uSpinPos(1),
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
        pIdField_->bSize = 2;                       // 128 << 2 = 512 bytes
        pIdField_->bCRC1 = pIdField_->bCRC2 = 0;
    }

    // Sector OK so reset all errors
    if (pbStatus_)
        *pbStatus_ = 0;

    // Return true if the sector exists
    return (m_uSide < m_uSides && m_uTrack < m_uTracks) && m_uSector <= m_uSectors;
}


// Locate the specific sector on the disk
bool CDisk::FindSector (UINT uSide_, UINT uTrack_, UINT uSector_, IDFIELD* pID_/*=NULL*/)
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
            // Check to see if the side, track and sector numbers match, and the CRC is correct
            if (id.bSide == uSide_ && id.bTrack == uTrack_ && id.bSector == uSector_ && !bStatus)
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

/*static*/ bool CDSKDisk::IsRecognised (CStream* pStream_)
{
    UINT uSize;

    // If we don't have a size (gzipped) we have no choice but to read enough to find out
    if (!(uSize = pStream_->GetSize()))
    {
        BYTE* pb = new BYTE[DSK_IMAGE_SIZE+1];

        // Read 1 byte more than a full DSK image size, to check for larger files
        if (pb && pStream_->Rewind())
            uSize = pStream_->Read(pb, DSK_IMAGE_SIZE+1);

        delete pb;
    }

    // Accept files that are a multiple of the track size, but no larger than a full disk
    return uSize && uSize <= DSK_IMAGE_SIZE && !(uSize % (NORMAL_DISK_SECTORS*NORMAL_SECTOR_SIZE));
}

CDSKDisk::CDSKDisk (CStream* pStream_)
    : CDisk(pStream_, dtDSK)
{
    // The DSK geometry is fixed
    m_uSides = NORMAL_DISK_SIDES;
    m_uTracks = NORMAL_DISK_TRACKS;
    m_uSectors = NORMAL_DISK_SECTORS;
    m_uSectorSize = NORMAL_SECTOR_SIZE;

    // Allocate some memory and clear it, just in case it's not a complete DSK image
    m_pbData = new BYTE[DSK_IMAGE_SIZE];
    memset(m_pbData, 0, sizeof DSK_IMAGE_SIZE);

    // Read the data from any existing stream, or create and save a new disk
    if (!pStream_->IsOpen())
        SetModified();
    else
    {
        pStream_->Rewind();

        // If it's an MS-DOS image, treat as 9 sectors-per-track, otherwise 10 as normal for SAM
        m_uSectors = (pStream_->Read(m_pbData, DSK_IMAGE_SIZE) == MSDOS_IMAGE_SIZE) ?
                        MSDOS_DISK_SECTORS : NORMAL_DISK_SECTORS;
    }
}


// Read the data for the last sector found
BYTE CDSKDisk::ReadData (BYTE *pbData_, UINT* puSize_)
{
    // Work out the offset for the required data
    long lPos = (m_uSide + 2 * m_uTrack) * (m_uSectors * m_uSectorSize) + ((m_uSector-1) * m_uSectorSize);

    // Copy the sector data from the image buffer
    memcpy(pbData_, m_pbData + lPos, *puSize_ = m_uSectorSize);

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
    long lPos = (m_uSide + 2 * m_uTrack) * (m_uSectors * m_uSectorSize) + ((m_uSector-1) * m_uSectorSize);

    // Copy the sector data to the image buffer, and set the modified flag
    memcpy(m_pbData + lPos, pbData_, *puSize_ = m_uSectorSize);
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
BYTE CDSKDisk::FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, UINT uSectors_)
{
    // Disk must be writable and must be the same number of sectors
    if (!IsReadOnly() && uSectors_ == m_uSectors)
    {
        DWORD dwSectors = 0;
        bool fNormal = true;

        // Make sure the remaining sectors are completely normal
        for (UINT u = 0 ; u < uSectors_ ; u++)
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


        // All all the above checks out it's a normal sector
        if (fNormal)
        {
            // Work out the offset for the required track
            long lPos = (uSide_ + 2 * uTrack_) * (m_uSectors * m_uSectorSize);

            // Formatting it requires simply clearing the existing data
            memset(m_pbData + lPos, 0, m_uSectors * m_uSectorSize);
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
    bool fValid = (pStream_->Rewind() && pStream_->Read(&sh, sizeof sh) == sizeof(sh) &&
            !memcmp(sh.abSignature, SAD_SIGNATURE, sizeof sh.abSignature) &&
            sh.bSides && sh.bSides <= MAX_DISK_SIDES && sh.bTracks && sh.bTracks <= 127 &&
            sh.bSectorSizeDiv64 && (sh.bSectorSizeDiv64 <= (MAX_SECTOR_SIZE >> 6)) &&
            (sh.bSectorSizeDiv64 & -sh.bSectorSizeDiv64) == sh.bSectorSizeDiv64);

    // If we know the stream size, validate the image size
    if (fValid && pStream_->GetSize())
    {
        UINT uDiskSize = sizeof sh + sh.bSides * sh.bTracks * sh.bSectors * (sh.bSectorSizeDiv64 << 6);
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
        memcpy(sh.abSignature, SAD_SIGNATURE, sizeof sh.abSignature);
    else
    {
        // Read the header and extract the disk geometry
        pStream_->Rewind();
        pStream_->Read(&sh, sizeof sh);
    }

    m_uSides = sh.bSides;
    m_uTracks = sh.bTracks;
    m_uSectors = sh.bSectors;
    m_uSectorSize = sh.bSectorSizeDiv64 << 6;

    // Work out the disk size, allocate some memory for it and read it in
    UINT uDiskSize = sizeof sh + m_uSides * m_uTracks * m_uSectors * m_uSectorSize;
    memcpy(m_pbData = new BYTE[uDiskSize], &sh, sizeof sh);

    // Read the contents if we've got a stream, or create and save a new one
    if (pStream_->IsOpen())
        pStream_->Read(m_pbData + sizeof sh, uDiskSize - sizeof sh);
    else
    {
        memset(m_pbData + sizeof sh, 0, uDiskSize - sizeof sh);
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
        for (UINT uSize = m_uSectorSize/128 ; !(uSize & 1) ; (pIdField_->bSize)++, uSize >>= 1);
    }

    // Return true if we're returning a new item
    return fRet;
}

// Read the data for the last sector found
BYTE CSADDisk::ReadData (BYTE *pbData_, UINT* puSize_)
{
    // Work out the offset for the required data
    long lPos = sizeof(SAD_HEADER) + (m_uSide * m_uTracks + m_uTrack) * (m_uSectors * NORMAL_SECTOR_SIZE) + ((m_uSector-1) * NORMAL_SECTOR_SIZE) ;

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
    long lPos = sizeof(SAD_HEADER) + (m_uSide * m_uTracks + m_uTrack) * (m_uSectors * NORMAL_SECTOR_SIZE) + ((m_uSector-1) * NORMAL_SECTOR_SIZE) ;

    // Copy the sector data to the image buffer, and set the modified flag
    memcpy(m_pbData + lPos, pbData_, *puSize_ = NORMAL_SECTOR_SIZE);
    SetModified();

    // Data is always perfect on SAD images, so return OK
    return 0;
}

// Save the disk out to the stream
bool CSADDisk::Save ()
{
    UINT uDiskSize = sizeof(SAD_HEADER) + m_uSides * m_uTracks * m_uSectors * m_uSectorSize;

    if (m_pStream->Rewind() && m_pStream->Write(m_pbData, uDiskSize) == uDiskSize)
    {
        SetModified(false);
        return true;
    }

    TRACE("!!! SADDisk::Save() failed to write modified disk contents!\n");
    return false;
}

// Format a track using the specified format
BYTE CSADDisk::FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, UINT uSectors_)
{
    // Disk must be writable and must be the same number of sectors
    if (!IsReadOnly() && uSectors_ == m_uSectors)
    {
        DWORD dwSectors = 0;
        bool fNormal = true;

        // Make sure the remaining sectors are completely normal
        for (UINT u = 0 ; u < uSectors_ ; u++)
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


        // All all the above checks out it's a normal sector
        if (fNormal)
        {
            // Work out the offset for the required track
            long lPos = sizeof(SAD_HEADER) + (m_uSide * m_uTracks + m_uTrack) * (m_uSectors * NORMAL_SECTOR_SIZE);

            // Formatting it requires simply clearing the existing data
            memset(m_pbData + lPos, 0, m_uSectors * m_uSectorSize);
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
    UINT uSize;

    // If we don't have a size (gzipped) we have no choice but to read enough to find out
    if (!(uSize = pStream_->GetSize()))
    {
        BYTE* pb = new BYTE[SDF_IMAGE_SIZE+1];

        // Read 1 byte more than a full DSK image size, to check for larger files
        if (pb && pStream_->Rewind())
            uSize = pStream_->Read(pb, SDF_IMAGE_SIZE+1);

        delete pb;
    }

    // Accept files that are a multiple of the track size, but no larger than a full disk
    return uSize == SDF_IMAGE_SIZE;
}

CSDFDisk::CSDFDisk (CStream* pStream_, UINT uSides_/*=NORMAL_DISK_SIDES*/, UINT uTracks_/*=MAX_DISK_TRACKS*/)
    : CDisk(pStream_, dtSDF)
{
    // Set up the fixed geometry used by old version
    m_uSides = NORMAL_DISK_SIDES;
    m_uTracks = NORMAL_DISK_TRACKS;

    // SDF images don't have a fixed number of sectors per track or sector size
    m_uSectors = m_uSectorSize = 0;

    // Work out the disk size, allocate some memory for it
    UINT uDiskSize = m_uSides * m_uTracks * SDF_TRACKSIZE;
    m_pbData = new BYTE[uDiskSize];

    // Read the data from any existing stream, or create and save a new disk
    if (pStream_->IsOpen())
    {
        pStream_->Rewind();
        pStream_->Read(m_pbData, uDiskSize);
    }
    else
    {
        memset(m_pbData, 0, uDiskSize);
        SetModified();
    }
}


// Initialise the enumeration of all sectors in the track
UINT CSDFDisk::FindInit (UINT uSide_, UINT uTrack_)
{
    if (uSide_ >= m_uSides || uTrack_ >= m_uTracks)
        return m_uSectors = 0;

    // Locate the track in the buffer to determine the number of sectors available
    UINT uPos = (uSide_*m_uTracks + uTrack_) * SDF_TRACKSIZE;
    m_pTrack = reinterpret_cast<SDF_TRACK_HEADER*>(m_pbData+uPos);
    m_uSectors = m_pTrack->bSectors;

    m_pFind = NULL;

    // Call the base and return the number of sectors in the track
    return CDisk::FindInit(uSide_, uTrack_);
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
            UINT uSize = m_pFind->bIdStatus ? 0 : (MIN_SECTOR_SIZE << m_pFind->sIdField.bSize);

            // Advance the pointer over the header and data to next header
            m_pFind = reinterpret_cast<SDF_SECTOR_HEADER*>(reinterpret_cast<BYTE*>(m_pFind) + uSize + sizeof(SDF_SECTOR_HEADER));
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
BYTE CSDFDisk::FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, UINT uSectors_)
{
    // Failed :-/
    return WRITE_PROTECT;
}


////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CFDIDisk::IsRecognised (CStream* pStream_)
{
    FDI_HEADER fdih;

    return (pStream_->Rewind() && (pStream_->Read(&fdih, sizeof fdih) == sizeof fdih) &&
            !memcmp(fdih.achSignature, FDI_SIGNATURE, sizeof fdih.achSignature));
}

CFDIDisk::CFDIDisk (CStream* pStream_, UINT uSides_/*=NORMAL_DISK_SIDES*/, UINT uTracks_/*=MAX_DISK_TRACKS*/)
    : CDisk(pStream_, dtFDI)
{
    // SDF images don't have a fixed number of sectors per track or sector size
    m_uSectors = 0;
    m_uSectorSize = 0;

    // Work out the disk size, allocate some memory for it
    UINT uDiskSize = m_uSides * m_uTracks * SDF_TRACKSIZE;
    m_pbData = new BYTE[uDiskSize];

    // Read the data from any existing stream, or create and save a new disk
    if (!pStream_->IsOpen())
    {
        m_uSides = uSides_;
        m_uTracks = uTracks_;

        SetModified();
    }
    else
    {
        pStream_->Rewind();
        pStream_->Read(&m_fdih, sizeof m_fdih);

        m_uSides = m_fdih.bLastHead + 1;
        m_uTracks = ((m_fdih.abLastTrack[0] << 8) + m_fdih.abLastTrack[1]) + 1;

        int nTotalTracks = m_uSides * m_uTracks;
        int nTrackDescSize = nTotalTracks * sizeof(FDI_TRACK_DESC);
        int nDescSlack = (nTotalTracks > 180) ? 0 : 512 - (sizeof(FDI_HEADER) + nTrackDescSize);

        m_pTracks = new FDI_TRACK_DESC[nTotalTracks];
        m_pnOffsets = new int[nTotalTracks];
        pStream_->Read(m_pTracks, nTrackDescSize);

        int nPos = 0;
        for (int i = 0 ; i < nTotalTracks ; i++)
        {
            m_pnOffsets[i] = nPos;
            nPos += (m_pTracks[i].bType == 1) ? (m_pTracks[i].bSize & 0x0f) << 9 : m_pTracks[i].bSize << 8;
        }

        m_pbData = new BYTE[nPos];
        pStream_->Read(m_pbData, nDescSlack);   // Discard any slack between the track descs and track data
        pStream_->Read(m_pbData, nPos);
    }
}

/*virtual*/ CFDIDisk::~CFDIDisk ()
{
    if (IsModified())
        Save();

    delete m_pTracks;
    delete m_pnOffsets;
}


// Initialise the enumeration of all sectors in the track
UINT CFDIDisk::FindInit (UINT uSide_, UINT uTrack_)
{
    m_uSectors = 0;

    if (uSide_ >= m_uSides || uTrack_ >= m_uTracks)
        return 0;

    int uTrack = (uTrack_ * m_uSides) + uSide_;
    BYTE* pb = m_pbData + m_pnOffsets[uTrack];

    BYTE bType = m_pTracks[uTrack].bType;

    if ((bType & 0xf0) == 0xe0)
    {
        TRACE("Sector-described track\n");

//      BYTE bEncoding = pb[0];
//      UINT uOffset = pb[3] << 16 | pb[2] << 8 | pb[1];
        pb += 4;

        for (BYTE b ; (b = *pb++) != 0xff ; )
        {
            switch (b)
            {
                case 0x00:  TRACE("one \"0\" bit\n");   break;
                case 0x01:  TRACE("one \"1\" bit\n");   break;
                case 0x02:  TRACE("standard MFM sync\n");   break;
                case 0x03:  TRACE("standard Index sync\n"); break;
                case 0x04:  TRACE("one MFM sync bit\n");    break;

                case 0x05:
                case 0x06:
                case 0x07:
                    TRACE("!!! Unknown data type! (%#02x)\n", b);
                    break;

                case 0x08:  // RLE MFM-encoded data
                    TRACE("RLE MFM-encoded data: %d bytes of %#02x\n", pb[0], pb[1]);
                    pb += 2;
                    break;

                case 0x09:  // RLE MFM-decoded data
                    TRACE("RLE MFM-decoded data: %d bytes of %#02x\n", pb[0], pb[1]);
                    pb += 2;
                    break;

                case 0x0b:  // MFM-encoded data
                case 0x0d:  // MFM-decoded data
                    pb += 2;
                    break;

                case 0x0a:  // MFM-encoded data
                case 0x0c:  // MFM-decoded data
                {
                    int n = (((pb[0] << 8 | pb[1]) + 7) >> 3);
                    pb += 2 + n;
                    break;
                }

                case 0x10:  // standard IBM index address mark
                    TRACE("standard IBM index address mark\n");
                    break;

                case 0x11:  // standard IBM pre-gap
                    TRACE("standard IBM pre-gap\n");
                    break;

                case 0x12:  // standard ST pre-gap
                    TRACE("standard ST pre-gap\n");
                    break;

                case 0x13:  // standard extended IBM sector header
                    TRACE("standard extended IBM sector header: %02X %02X %02X %02X\n", pb[0], pb[1], pb[2], pb[3]);
                    pb += 4;
                    break;

                case 0x14:  // standard mini-extended IBM sector header
                    TRACE("standard mini-extended IBM sector header: %02X %02X %02X %02X\n", pb[0], pb[1], pb[2], pb[3]);
                    pb += 4;
                    break;

                case 0x15:  // standard short IBM sector header
                    TRACE("standard short IBM sector header: %02X\n", pb[0]);
                    pb++;
                    break;

                case 0x16:  // standard mini-short IBM sector header
                    TRACE("standard mini-short IBM sector header: %02X\n", pb[0]);
                    pb++;
                    break;

                case 0x17:  // standard CRC-incorrect mini-extended IBM sector header
                    TRACE("standard CRC-incorrect mini-extended IBM sector header: %02X %02X %02X %02X %02X %02X\n", pb[0], pb[1], pb[2], pb[3], pb[4], pb[5]);
                    pb += 4 + 2;
                    break;

                case 0x18:  // standard CRC-incorrect mini-short IBM sector header
                    TRACE("standard CRC-incorrect mini-short IBM sector header: %02X %02X %02X\n", pb[0], pb[1], pb[2]);
                    pb += 1 + 2;
                    break;

                case 0x19:  // standard 512-byte CRC-correct IBM data
                    TRACE("standard 512-byte CRC-correct IBM data\n");
                    break;

                case 0x1a:  // standard 128*2^x-byte CRC-correct IBM data
                case 0x1b:  // standard 128*2^x-byte CRC-incorrect IBM data
                    pb += 0;
                    break;

                case 0x1c:  // standard extended IBM sector
                    TRACE("standard extended IBM sector: %02X %02X %02X %02X\n", pb[0], pb[1], pb[2], pb[3]);
                    pb += 0;
                    break;

                case 0x1d:  // standard short IBM sector
                    TRACE("standard short IBM sector: %02X\n", pb[0]);
                    pb += 1 + 512;
                    break;

                // Amiga stuff
                case 0x20:  pb += 20;   break;
                case 0x21:  pb += 4;    break;
                case 0x22:  pb += 2;    break;
                case 0x23:  pb += 512;  break;
                case 0x24:  pb += 0;    break;
                case 0x25:  pb += 0;    break;
                case 0x26:  pb += 516;  break;
                case 0x27:  pb += 514;  break;

                default:
                    break;
            }
        }
    }
    else
        TRACE("Unknown track type: %u\n", m_pTracks[uTrack].bType);
/*
    // Locate the track in the buffer to determine the number of sectors available
    UINT uPos = 0;//(uSide_*m_uTracks + uTrack_) * FDI_TRACKSIZE;
    m_pTrack = reinterpret_cast<FDI_TRACK_HEADER*>(m_pbData+uPos);
    m_uSectors = m_pTrack->bSectors;

    m_pFind = NULL;
*/
    // Call the base and return the number of sectors in the track
    return CDisk::FindInit(uSide_, uTrack_);
}

// Find the next sector in the current track
bool CFDIDisk::FindNext (IDFIELD* pIdField_, BYTE* pbStatus_)
{
    bool fRet = false;
/*
    // Make sure there is a 'next' one
    if (fRet = CDisk::FindNext())
    {
        if (!m_pFind)
            m_pFind = reinterpret_cast<FDI_SECTOR_HEADER*>(&m_pTrack[1]);
        else
        {
            // The data length is zero if there is a problem with the ID header
            UINT uSize = m_pFind->bIdStatus ? 0 : (MIN_SECTOR_SIZE << m_pFind->sIdField.bSize);

            // Advance the pointer over the header and data to next header
            m_pFind = reinterpret_cast<FDI_SECTOR_HEADER*>(reinterpret_cast<BYTE*>(m_pFind) + uSize + sizeof(FDI_SECTOR_HEADER));
        }

        // Copy the ID field to the supplied buffer, and store the ID status
        memcpy(pIdField_, &m_pFind->sIdField, sizeof(*pIdField_));
        *pbStatus_ = m_pFind->bIdStatus;
    }
*/
    // Return true if we're returning a new item
    return fRet;
}

// Read the data for the last sector found
BYTE CFDIDisk::ReadData (BYTE *pbData_, UINT* puSize_)
{
    // Copy the sector data
//  memcpy(pbData_, reinterpret_cast<BYTE*>(m_pFind+1), *puSize_ = MIN_SECTOR_SIZE << m_pFind->sIdField.bSize);

    // Return the data status to include a possible CRC error
    return CRC_ERROR;
//  return m_pFind->bDataStatus;
}

// Read the data for the last sector found
BYTE CFDIDisk::WriteData (BYTE *pbData_, UINT* puSize_)
{
    // Fail if read-only
//  if (IsReadOnly())
        return WRITE_PROTECT;

    // Copy the sector data
//  memcpy(reinterpret_cast<BYTE*>(m_pFind+1), pbData_, *puSize_ = MIN_SECTOR_SIZE << m_pFind->sIdField.bSize);
    SetModified();

    // Written ok
    return 0;
}

// Save the disk out to the stream
bool CFDIDisk::Save ()
{
    if (IsModified())
    {
    }

    return true;
}

// Format a track using the specified format
BYTE CFDIDisk::FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, UINT uSectors_)
{
    // Failed :-/
    return WRITE_PROTECT;
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CFloppyDisk::IsRecognised (CStream* pStream_)
{
    return CFloppyStream::IsRecognised(pStream_->GetName());
}

CFloppyDisk::CFloppyDisk (CStream* pStream_)
    : CDisk(pStream_, dtFloppy)
{
    // We can assume this as only CFloppyStream will recognise the real device, and we are always the disk used
    m_pFloppy = reinterpret_cast<CFloppyStream*>(pStream_);

    // Maximum supported geometry
    m_uSides = 2;
    m_uTracks = 83;
    m_uSectors = 10;
    m_uSectorSize = 512;

    // Work out the disk size, allocate some memory for it and read it in
    UINT uDiskSize = m_uSides * m_uTracks * m_uSectors * m_uSectorSize;
    m_pbData = new BYTE[uDiskSize];
}


// Find the next sector in the current track
bool CFloppyDisk::FindNext (IDFIELD* pIdField_, BYTE* pbStatus_)
{
    bool fRet = CDisk::FindNext();

    // Make sure there is a 'next' one
    if (fRet)
    {
        // Construct a normal ID field for the sector
        pIdField_->bSide = m_uSide;
        pIdField_->bTrack = m_uTrack;
        pIdField_->bSector = m_uSector;
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
    return m_pFloppy->Read(m_uSide, m_uTrack, m_uSector, pbData_, puSize_);
}

// Write the data for a sector
BYTE CFloppyDisk::WriteData (BYTE *pbData_, UINT* puSize_)
{
    return m_pFloppy->Write(m_uSide, m_uTrack, m_uSector, pbData_, puSize_);
}

// Save the disk out to the stream
bool CFloppyDisk::Save ()
{
    // Eveything's currently written immediately, but we'll need to do more if/when writes are cached
    return true;
}

// Format a track using the specified format
BYTE CFloppyDisk::FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, UINT uSectors_)
{
    // Not supported at present, though regular formats should be possible in the
    // future, even if just implemented as writing zeros (as with DSK images)
    return WRITE_PROTECT;
}

// Get the status of the current asynchronous operation, if any
bool CFloppyDisk::GetAsyncStatus (UINT* puSize_, BYTE* pbStatus_)
{
    return m_pFloppy->GetAsyncStatus(puSize_, pbStatus_);
}

// Wait for the current asynchronous operation to complete, if any
bool CFloppyDisk::WaitAsyncOp (UINT* puSize_, BYTE* pbStatus_)
{
    return m_pFloppy->WaitAsyncOp(puSize_, pbStatus_);
}

// Abort the current asynchronous operation, if any
void CFloppyDisk::AbortAsyncOp ()
{
    m_pFloppy->AbortAsyncOp();
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
    // The DSK geometry is fixed
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

    // The first sector is the directory entry we need to fill
    if (!m_uTrack && m_uSector == 1)
    {
        // CODE file type
        pbData_[0] = 19;

        // Strip any file extension and use up to the first 10 chars for the filename on the disk
        const char *pcszName = m_pStream->GetName(), *pcszExt = strrchr(pcszName, '.'), *pcsz;
        for (pcsz = pcszName+strlen(pcszName)-1 ; pcsz[-1] != '/' && pcsz[-1] != '\\' && pcsz > pcszName ; pcsz--);
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
BYTE CFileDisk::FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, UINT uSectors_)
{
    // Read-only, formatting not supported
    return WRITE_PROTECT;
}
