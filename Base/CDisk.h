// Part of SimCoupe - A SAM Coupé emulator
//
// CDisk.h: C++ classes used for accessing all SAM disk image types
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

#ifndef CDISK_H
#define CDISK_H

#include "CStream.h"    // for the data stream abstraction
#include "Floppy.h"
#include "VL1772.h"     // for the VL-1772 controller definitions

////////////////////////////////////////////////////////////////////////////////

const int NORMAL_DISK_SIDES     = 2;    // Normally 2 sides per disk
const int NORMAL_DISK_TRACKS    = 80;   // Normally 80 tracks per side
const int NORMAL_DISK_SECTORS   = 10;   // Normally 10 sectors per track
const int NORMAL_SECTOR_SIZE    = 512;  // Normally 512 bytes per sector

const int NORMAL_DIRECTORY_TRACKS = 4;  // Normally 4 tracks in a SAMDOS directory

const int MSDOS_DISK_SECTORS = 9;   // Double-density MS-DOS disks are 9 sectors per track

const int SDF_TRACKSIZE = NORMAL_SECTOR_SIZE * 12;      // Large enough for any possible SAM disk format

const char SDF_SIGNATURE[] = "SDF"; // 4 bytes, including the terminating NULL


// The various disk format image sizes
#define DSK_IMAGE_SIZE          (NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS * NORMAL_DISK_SECTORS * NORMAL_SECTOR_SIZE)
#define MSDOS_IMAGE_SIZE        (NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS * MSDOS_DISK_SECTORS * NORMAL_SECTOR_SIZE)
#define OLD_SDF_IMAGE_SIZE      (NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS * SDF_TRACKSIZE)

const int DISK_FILE_HEADER_SIZE = 9;    // From SAM Technical Manual  (bType, wSize, wOffset, wUnused, bPages, bStartPage)

////////////////////////////////////////////////////////////////////////////////

// The ID string for Aley Keprt's SAD disk image format (heh!)
#define SAD_SIGNATURE           "Aley's disk backup"

// Format of a SAD image header
typedef struct
{
    BYTE    abSignature[sizeof SAD_SIGNATURE - 1];

    BYTE    bSides;             // Number of sides on the disk
    BYTE    bTracks;            // Number of tracks per side
    BYTE    bSectors;           // Number of sectors per track
    BYTE    bSectorSizeDiv64;   // Sector size divided by 64
}
SAD_HEADER;


////////////////////////////////////////////////////////////////////////////////


// SDF chunk types
enum { SDF_CT_END, SDF_CT_HEADER, SDF_CT_DATA, SDF_CT_TEXT };


typedef struct
{
    BYTE    bType;
    DWORD   dwLength;

    // dwLength bytes follow...
}
SDF_CHUNK;


// SDF file header
typedef struct
{
    BYTE    bTracks;            // Tracks per side (usually 80)
    BYTE    bSides;             // Sides (usually 2)
}
SDF_HEADER;


// Track header on each track in the SDF disk image
typedef struct
{
    BYTE    bSectors;       // Number of sectors on this track

    // Block of 'bSectors' SECTOR_HEADER structures follow...
}
SDF_TRACK_HEADER;


// Sector header on each sector in the SDF disk image
typedef struct
{
    BYTE    bIdStatus;      // Status error bits for after READ_ADDRESS command
    BYTE    bDataStatus;    // Status error bits for after READ_XSECTOR commands

    IDFIELD sIdField;       // ID field containing sector details

    // Sector data follows here unless bIdStatus indicates an error (size is MIN_SECTOR_SIZE << sIdField.bSize)
}
SDF_SECTOR_HEADER;


class CDisk
{
    public:
        enum { dtUnknown, dtFloppy, dtSDF, dtSAD, dtDSK, dtSBT };

    // Constructor and virtual destructor
    public:
        CDisk (CStream* pStream_);
        virtual ~CDisk ();

    public:
        static int GetType (const char* pcszDisk_);
        static CDisk* Open (const char* pcszDisk_, bool fReadOnly_=false);

    // Public query functions
    public:
        const char* GetName () { return m_pStream->GetName(); }
        int GetSpinPos (bool fAdvance_=false);
        bool IsReadOnly () const { return m_pStream->IsReadOnly(); }
        bool IsModified () const { return m_fModified; }

    // Protected overrides
    public:
        virtual int FindInit (int nSide_, int nTrack_);
        virtual bool FindNext (IDFIELD* pIdField_=NULL, BYTE* pbStatus_=NULL);
        virtual bool FindSector (int nSide_, int nTrack_, int nSector_, IDFIELD* pID_=NULL);

        virtual BYTE ReadData (BYTE* pbData_, UINT* puSize_) = 0;
        virtual BYTE WriteData (BYTE* pbData_, UINT* puSize_) = 0;
        virtual bool Save () = 0;
        virtual BYTE FormatTrack (int nSide_, int nTrack_, IDFIELD* paID_, int nSectors_) = 0;

    protected:
        int     m_nSides, m_nTracks, m_nSectors, m_nSectorSize;
        int     m_nSide, m_nTrack, m_nSector;
        bool    m_fModified;

        int     m_nSpinPos;
        CStream*m_pStream;
        BYTE*   m_pbData;

    protected:
        void SetModified (bool fModified_=true) { m_fModified = fModified_; }
        
};


class CDSKDisk : public CDisk
{
    public:
        CDSKDisk (CStream* pStream_);
        virtual ~CDSKDisk () { if (IsModified()) Save(); }

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        BYTE ReadData (BYTE* pbData_, UINT* puSize_);
        BYTE WriteData (BYTE* pbData_, UINT* puSize_);
        bool Save ();
        BYTE FormatTrack (int nSide_, int nTrack_, IDFIELD* paID_, int nSectors_);
};


class CSADDisk : public CDisk
{
    public:
        CSADDisk (CStream* pStream_, int nSides_=NORMAL_DISK_SIDES, int nTracks_=NORMAL_DISK_TRACKS,
                    int nSectors_=NORMAL_DISK_SECTORS, int nSectorSize_=NORMAL_SECTOR_SIZE);
        virtual ~CSADDisk () { if (IsModified()) Save(); }

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        bool FindNext (IDFIELD* pIdField_, BYTE* pbStatus_);
        BYTE ReadData (BYTE* pbData_, UINT* puSize_);
        BYTE WriteData (BYTE* pbData_, UINT* puSize_);
        bool Save ();
        BYTE FormatTrack (int nSide_, int nTrack_, IDFIELD* paID_, int nSectors_);
};


class CSDFDisk : public CDisk
{
    public:
        CSDFDisk (CStream* pStream_, int nSides_=NORMAL_DISK_SIDES, int nTracks_=MAX_DISK_TRACKS);
        virtual ~CSDFDisk () { if (IsModified()) Save(); }

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        int FindInit (int nSide_, int nTrack_);
        bool FindNext (IDFIELD* pIdField_, BYTE* pbStatus_);
        BYTE ReadData (BYTE* pbData_, UINT* puSize_);
        BYTE WriteData (BYTE* pbData_, UINT* puSize_);
        bool Save ();
        BYTE FormatTrack (int nSide_, int nTrack_, IDFIELD* paID_, int nSectors_);

    protected:
        SDF_TRACK_HEADER* m_pTrack;     // Last track 
        SDF_SECTOR_HEADER* m_pFind;     // Last sector found with FindNext()
};


class CFloppyDisk : public CDisk
{
    public:
        CFloppyDisk (CStream* pStream_);

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        bool FindNext (IDFIELD* pIdField_, BYTE* pbStatus_);
        BYTE ReadData (BYTE* pbData_, UINT* puSize_);
        BYTE WriteData (BYTE* pbData_, UINT* puSize_);
        bool Save ();
        BYTE FormatTrack (int nSide_, int nTrack_, IDFIELD* paID_, int nSectors_);

    protected:
        CFloppyStream* m_pFloppy;
};


class CFileDisk : public CDisk
{
    public:
        CFileDisk (CStream* pStream_);
        virtual ~CFileDisk () { if (IsModified()) Save(); }

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        BYTE ReadData (BYTE* pbData_, UINT* puSize_);
        BYTE WriteData (BYTE* pbData_, UINT* puSize_);
        bool Save ();
        BYTE FormatTrack (int nSide_, int nTrack_, IDFIELD* paID_, int nSectors_);

    protected:
        long    m_lSize;
};

#endif  // CDISK_H
