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

const UINT NORMAL_DISK_SIDES     = 2;    // Normally 2 sides per disk
const UINT NORMAL_DISK_TRACKS    = 80;   // Normally 80 tracks per side
const UINT NORMAL_DISK_SECTORS   = 10;   // Normally 10 sectors per track
const UINT NORMAL_SECTOR_SIZE    = 512;  // Normally 512 bytes per sector

const UINT NORMAL_DIRECTORY_TRACKS = 4;  // Normally 4 tracks in a SAMDOS directory

const UINT MSDOS_DISK_SECTORS = 9;   // Double-density MS-DOS disks are 9 sectors per track

const UINT SDF_TRACKSIZE = NORMAL_SECTOR_SIZE * 12;      // Large enough for any possible SAM disk format

const char SDF_SIGNATURE[] = "SDF"; // 4 bytes, including the terminating NULL


// The various disk format image sizes
#define DSK_IMAGE_SIZE          (NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS * NORMAL_DISK_SECTORS * NORMAL_SECTOR_SIZE)
#define MSDOS_IMAGE_SIZE        (NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS * MSDOS_DISK_SECTORS * NORMAL_SECTOR_SIZE)

const UINT DISK_FILE_HEADER_SIZE = 9;    // From SAM Technical Manual  (bType, wSize, wOffset, wUnused, bPages, bStartPage)

// Maximum size of a file that will fit on a SAM disk
const UINT MAX_SAM_FILE_SIZE = ((NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS) - NORMAL_DIRECTORY_TRACKS) *
                                NORMAL_DISK_SECTORS * (NORMAL_SECTOR_SIZE-2) - DISK_FILE_HEADER_SIZE;

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

#define FDI_SIGNATURE           "Formatted Disk Image file\r\n"

typedef struct
{
    char achSignature[27];      // FDI_SIGNATURE from above
    char achCreator[30];        // Who/what created the image, space padded
    char achCRLF[2];            // <cr><lf>
    char achComment[80];        // Comment, <eof> (0x1a) padded
    char chEOF;                 // <eof>

    BYTE abVersion[2];          // Image version number

    BYTE abLastTrack[2];        // Big endian word containing last track number
    BYTE bLastHead;             // Last head number

    BYTE bDiskType;             // Disk type
    BYTE bRotateSpeed;          // Base rotation speed in RPM
    BYTE bFlags;

    DWORD dwReserved;           // Reserved
}
FDI_HEADER;

typedef struct
{
    BYTE bType;                 // Track type
    BYTE bSize;                 // Track data size / 256
}
FDI_TRACK_DESC;

////////////////////////////////////////////////////////////////////////////////


// SDF chunk types
enum { SDF_CT_END, SDF_CT_HEADER, SDF_CT_DATA, SDF_CT_TEXT };


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


enum { dtUnknown, dtFloppy, dtFile, dtSDF, dtSAD, dtDSK, dtSBT, dtFDI };

class CDisk
{
    // Constructor and virtual destructor
    public:
        CDisk (CStream* pStream_, int nType_);
        virtual ~CDisk ();

    public:
        static int GetType (CStream* pStream_);
        static CDisk* Open (const char* pcszDisk_, bool fReadOnly_=false);
        void Close () { m_pStream->Close(); }

    // Public query functions
    public:
        const char* GetName () { return m_pStream->GetName(); }
        int GetType () const { return m_nType; }
        UINT GetSpinPos (bool fAdvance_=false);
        bool IsReadOnly () const { return m_pStream->IsReadOnly(); }
        bool IsModified () const { return m_fModified; }

    // Protected overrides
    public:
        virtual UINT FindInit (UINT uSide_, UINT uTrack_);
        virtual bool FindNext (IDFIELD* pIdField_=NULL, BYTE* pbStatus_=NULL);
        virtual bool FindSector (UINT uSide_, UINT uTrack_, UINT uSector_, IDFIELD* pID_=NULL);

        virtual BYTE ReadData (BYTE* pbData_, UINT* puSize_) = 0;
        virtual BYTE WriteData (BYTE* pbData_, UINT* puSize_) = 0;
        virtual bool Save () = 0;
        virtual BYTE FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, UINT uSectors_) = 0;
        virtual bool GetAsyncStatus (UINT* puSize_, BYTE* pbStatus_) { return false; }
        virtual bool WaitAsyncOp (UINT* puSize_, BYTE* pbStatus_) { return false; }
        virtual void AbortAsyncOp () { }

    protected:
        int     m_nType;
        UINT    m_uSides, m_uTracks, m_uSectors, m_uSectorSize;
        UINT    m_uSide, m_uTrack, m_uSector;
        bool    m_fModified;

        UINT    m_uSpinPos;
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
        BYTE FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, UINT uSectors_);
};


class CSADDisk : public CDisk
{
    public:
        CSADDisk (CStream* pStream_, UINT uSides_=NORMAL_DISK_SIDES, UINT uTracks_=NORMAL_DISK_TRACKS,
                    UINT uSectors_=NORMAL_DISK_SECTORS, UINT uSectorSize_=NORMAL_SECTOR_SIZE);
        virtual ~CSADDisk () { if (IsModified()) Save(); }

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        bool FindNext (IDFIELD* pIdField_, BYTE* pbStatus_);
        BYTE ReadData (BYTE* pbData_, UINT* puSize_);
        BYTE WriteData (BYTE* pbData_, UINT* puSize_);
        bool Save ();
        BYTE FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, UINT uSectors_);
};


class CSDFDisk : public CDisk
{
    public:
        CSDFDisk (CStream* pStream_, UINT uSides_=NORMAL_DISK_SIDES, UINT uTracks_=MAX_DISK_TRACKS);
        virtual ~CSDFDisk () { if (IsModified()) Save(); }

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        UINT FindInit (UINT uSide_, UINT uTrack_);
        bool FindNext (IDFIELD* pIdField_, BYTE* pbStatus_);
        BYTE ReadData (BYTE* pbData_, UINT* puSize_);
        BYTE WriteData (BYTE* pbData_, UINT* puSize_);
        bool Save ();
        BYTE FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, UINT uSectors_);

    protected:
        SDF_TRACK_HEADER* m_pTrack;     // Last track
        SDF_SECTOR_HEADER* m_pFind;     // Last sector found with FindNext()
};


class CFDIDisk : public CDisk
{
    public:
        CFDIDisk (CStream* pStream_, UINT uSides_=NORMAL_DISK_SIDES, UINT uTracks_=NORMAL_DISK_TRACKS);
        virtual ~CFDIDisk ();

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        UINT FindInit (UINT uSide_, UINT uTrack_);
        bool FindNext (IDFIELD* pIdField_, BYTE* pbStatus_);
        BYTE ReadData (BYTE* pbData_, UINT* puSize_);
        BYTE WriteData (BYTE* pbData_, UINT* puSize_);
        bool Save ();
        BYTE FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, UINT uSectors_);

    protected:
        FDI_HEADER m_fdih;
        FDI_TRACK_DESC* m_pTracks;
        int* m_pnOffsets;
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
        BYTE FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, UINT uSectors_);
        bool GetAsyncStatus (UINT* puSize_, BYTE* pbStatus_);
        bool WaitAsyncOp (UINT* puSize_, BYTE* pbStatus_);
        void AbortAsyncOp ();

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
        BYTE FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, UINT uSectors_);

    protected:
        UINT  m_uSize;
};

#endif  // CDISK_H
