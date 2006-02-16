// Part of SimCoupe - A SAM Coupe emulator
//
// CDisk.h: C++ classes used for accessing all SAM disk image types
//
//  Copyright (c) 1999-2006  Simon Owen
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
#include "Floppy.h"     // native floppy support
#include "VL1772.h"     // for the VL-1772 controller definitions

////////////////////////////////////////////////////////////////////////////////

const UINT NORMAL_DISK_SIDES     = 2;    // Normally 2 sides per disk
const UINT NORMAL_DISK_TRACKS    = 80;   // Normally 80 tracks per side
const UINT NORMAL_DISK_SECTORS   = 10;   // Normally 10 sectors per track
const UINT NORMAL_SECTOR_SIZE    = 512;  // Normally 512 bytes per sector

const UINT NORMAL_DIRECTORY_TRACKS = 4;  // Normally 4 tracks in a SAMDOS directory

const UINT DOS_DISK_SECTORS = 9;         // Double-density MS-DOS disks are 9 sectors per track

const UINT SDF_TRACKSIZE = NORMAL_SECTOR_SIZE * 12;      // Large enough for any possible SAM disk format


// The various disk format image sizes
#define MGT_IMAGE_SIZE  (NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS * NORMAL_DISK_SECTORS * NORMAL_SECTOR_SIZE)
#define DOS_IMAGE_SIZE  (NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS * DOS_DISK_SECTORS * NORMAL_SECTOR_SIZE)

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

#define TD0_SIG_NORMAL          "TD"    // Normal compression (RLE)
#define TD0_SIG_ADVANCED        "td"    // Huffman compression also used for everything after TD0_HEADER

// Overall file header, always uncompressed
typedef struct
{
    BYTE    abSignature[2];
    BYTE    bVolSequence;       // Volume sequence (zero for the first)
    BYTE    bCheckSig;          // Check signature for multi-volume sets (all must match)
    BYTE    bTDVersion;         // Teledisk version used to create the file (11 = v1.1)
    BYTE    bSourceDensity;     // Source disk density (0 = 250K bps,  1 = 300K bps,  2 = 500K bps ; +128 = single-density FM)
    BYTE    bDriveType;         // Source drive type (1 = 360K, 2 = 1.2M, 3 = 720K, 4 = 1.44M)
    BYTE    bTrackDensity;      // 0 = source matches media density, 1 = double density, 2 = quad density)
    BYTE    bDOSMode;           // Non-zero if disk was analysed according to DOS allocation
    BYTE    bSurfaces;          // Disk sides stored in the image
    BYTE    bCRCLow, bCRCHigh;  // 16-bit CRC for this header
}
TD0_HEADER;

// Optional comment block, present if bit 7 is set in bTrackDensity above
typedef struct
{
    BYTE    bCRCLow, bCRCHigh;  // 16-bit CRC covering the comment block
    BYTE    bLenLow, bLenHigh;  // Comment block length
    BYTE    bYear, bMon, bDay;  // Date of disk creation
    BYTE    bHour, bMin, bSec;  // Time of disk creation
//  BYTE    abData[];           // Comment data, in null-terminated blocks
}
TD0_COMMENT;

// Track header
typedef struct
{
    BYTE    bSectors;           // Number of sectors in track
    BYTE    bPhysTrack;         // Physical track we read from
    BYTE    bPhysSide;          // Physical side we read from
    BYTE    bCRC;               // Low 8-bits of track header CRC
}
TD0_TRACK;

// Sector header
typedef struct
{
    BYTE    bTrack;             // Track number in ID field
    BYTE    bSide;              // Side number in ID field
    BYTE    bSector;            // Sector number in ID field
    BYTE    bSize;              // Sector size indicator:  (128 << bSize) gives the real size
    BYTE    bFlags;             // Flags detailing special sector conditions
    BYTE    bCRC;               // Low 8-bits of sector header CRC
}
TD0_SECTOR;

// Data header, only present if a data block is available
typedef struct
{
    BYTE    bOffLow, bOffHigh;  // Offset to next sector, from after this offset value
    BYTE    bMethod;            // Storage method used for sector data (0 = raw, 1 = repeated 2-byte pattern, 2 = RLE block)
}
TD0_DATA;

////////////////////////////////////////////////////////////////////////////////

// Track header on each track in the SDF disk image
typedef struct
{
    BYTE    bSectors;           // Number of sectors on this track

    // Block of 'bSectors' SECTOR_HEADER structures follow...
}
SDF_TRACK_HEADER;

// Sector header on each sector in the SDF disk image
typedef struct
{
    BYTE    bIdStatus;          // Status error bits for after READ_ADDRESS command
    BYTE    bDataStatus;        // Status error bits for after READ_XSECTOR commands

    IDFIELD sIdField;           // ID field containing sector details

    // Sector data follows here unless bIdStatus indicates an error (size is MIN_SECTOR_SIZE << sIdField.bSize)
}
SDF_SECTOR_HEADER;

////////////////////////////////////////////////////////////////////////////////

#define DSK_SIGNATURE           "MV - CPC"
#define EDSK_SIGNATURE          "EXTENDED CPC DSK File\r\nDisk-Info\r\n"
#define EDSK_TRACK_SIGNATURE    "Track-Info\r\n"
#define ESDK_MAX_TRACK_SIZE     0xff00
#define EDSK_MAX_SECTORS        ((256 - sizeof(EDSK_TRACK)) / sizeof(EDSK_SECTOR))  // = 29

#define ST1_765_CRC_ERROR       0x20
#define ST2_765_DATA_NOT_FOUND  0x01
#define ST2_765_CRC_ERROR       0x20
#define ST2_765_CONTROL_MARK    0x40

typedef struct
{
    char szSignature[34];       // one of the signatures above, depending on DSK/EDSK
    char szCreator[14];         // name of creator (utility/emulator)
    BYTE bTracks;
    BYTE bSides;
    BYTE abTrackSize[2];        // fixed track size (DSK only)
}
EDSK_HEADER;

typedef struct
{
    char szSignature[13];       // Track-Info\r\n
    BYTE bRate;                 // 0=unknown (default=250K), 1=250K/300K, 2=500K, 3=1M
    BYTE bEncoding;             // 0=unknown (default=MFM), 1=FM, 2=MFM
    BYTE bUnused;
    BYTE bTrack;
    BYTE bSide;
    BYTE abUnused[2];
    BYTE bSize;
    BYTE bSectors;
    BYTE bGap3;
    BYTE bFill;
}
EDSK_TRACK;

typedef struct
{
    BYTE bTrack, bSide, bSector, bSize;
    BYTE bStatus1, bStatus2;
    BYTE bDatalow, bDatahigh;
}
EDSK_SECTOR;

////////////////////////////////////////////////////////////////////////////////

enum { dtNone, dtUnknown, dtFloppy, dtFile, dtEDSK, dtSDF, dtTD0, dtSAD, dtMGT, dtSBT };

class CDisk
{
    // Constructor and virtual destructor
    public:
        CDisk (CStream* pStream_, int nType_);
        virtual ~CDisk ();

    public:
        static int GetType (CStream* pStream_);
        static CDisk* Open (const char* pcszDisk_, bool fReadOnly_=false);
        static CDisk* Open (void* pv_, size_t uSize_, const char* pcszDisk_);
        virtual void Close () { m_pStream->Close(); }

    // Public query functions
    public:
        const char* GetPath () { return m_pStream->GetPath(); }
        const char* GetFile () { return m_pStream->GetFile(); }
        int GetType () const { return m_nType; }
        UINT GetSpinPos (bool fAdvance_=false);
        bool IsReadOnly () const { return m_pStream->IsReadOnly(); }
        bool IsModified () const { return m_fModified; }

        void SetModified (bool fModified_=true) { m_fModified = fModified_; }

    // Protected overrides
    public:
        virtual UINT FindInit (UINT uSide_, UINT uTrack_);
        virtual bool FindNext (IDFIELD* pIdField_=NULL, BYTE* pbStatus_=NULL);
        virtual bool FindSector (UINT uSide_, UINT uTrack_, UINT uIdTrack_, UINT uSector_, IDFIELD* pID_=NULL);

        virtual BYTE LoadTrack (UINT uSide_, UINT uTrack_) { return 0; }
        virtual BYTE ReadData (BYTE* pbData_, UINT* puSize_) = 0;
        virtual BYTE WriteData (BYTE* pbData_, UINT* puSize_) = 0;
        virtual bool Save () = 0;

        virtual BYTE FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_) = 0;

        virtual bool IsBusy (BYTE* pbStatus_, bool fWait_=false) { return false; }

    protected:
        int     m_nType;
        UINT    m_uSides, m_uTracks, m_uSectors, m_uSectorSize;
        UINT    m_uSide, m_uTrack, m_uSector, m_uSize;
        bool    m_fModified;

        UINT    m_uSpinPos;
        CStream*m_pStream;
        BYTE*   m_pbData;
};


class CMGTDisk : public CDisk
{
    public:
        CMGTDisk (CStream* pStream_, UINT uSectors_=NORMAL_DISK_SECTORS);
        virtual ~CMGTDisk () { if (IsModified()) Save(); }

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        BYTE ReadData (BYTE* pbData_, UINT* puSize_);
        BYTE WriteData (BYTE* pbData_, UINT* puSize_);
        bool Save ();
        BYTE FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_);
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
        BYTE FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_);
};


class CTD0Disk : public CDisk
{
    public:
        CTD0Disk (CStream* pStream_, UINT uSides_=NORMAL_DISK_SIDES);
        virtual ~CTD0Disk () { if (IsModified()) Save(); }

    public:
        static bool IsRecognised (CStream* pStream_);
        static WORD CrcBlock (const void* pv_, UINT uLen_, WORD wCRC_=0);

    public:
        UINT FindInit (UINT uSide_, UINT uTrack_);
        bool FindNext (IDFIELD* pIdField_, BYTE* pbStatus_);
        BYTE ReadData (BYTE* pbData_, UINT* puSize_);
        BYTE WriteData (BYTE* pbData_, UINT* puSize_);
        bool Save ();
        BYTE FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_);

    protected:
        void UnpackData (TD0_SECTOR* ps_, BYTE* pbSector_);

    protected:
        TD0_HEADER m_sHeader;   // File header
        TD0_TRACK* m_pTrack;    // Last track
        TD0_SECTOR* m_pFind;    // Last sector found with FindNext()

        TD0_TRACK* m_auIndex[MAX_DISK_SIDES][MAX_DISK_TRACKS];  // Track pointers in m_pData block
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
        BYTE FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_);

    protected:
        SDF_TRACK_HEADER* m_pTrack;     // Last track
        SDF_SECTOR_HEADER* m_pFind;     // Last sector found with FindNext()
};


class CEDSKDisk : public CDisk
{
    public:
        CEDSKDisk (CStream* pStream_, UINT uSides_=NORMAL_DISK_SIDES, UINT uTracks_=NORMAL_DISK_TRACKS);
        virtual ~CEDSKDisk ();

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        UINT FindInit (UINT uSide_, UINT uTrack_);
        bool FindNext (IDFIELD* pIdField_, BYTE* pbStatus_);
        BYTE ReadData (BYTE* pbData_, UINT* puSize_);
        BYTE WriteData (BYTE* pbData_, UINT* puSize_);
        bool Save ();
        BYTE FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_);

    protected:
        EDSK_TRACK* m_apTracks[MAX_DISK_SIDES][MAX_DISK_TRACKS];
        BYTE m_abSizes[MAX_DISK_SIDES][MAX_DISK_TRACKS];

        EDSK_TRACK* m_pTrack;     // Last track
        EDSK_SECTOR* m_pFind;     // Last sector found with FindNext()
        BYTE* m_pbFind;           // Last sector data
};


class CFloppyDisk : public CDisk
{
    public:
        CFloppyDisk (CStream* pStream_);

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        UINT FindInit (UINT uSide_, UINT uTrack_);
        bool FindNext (IDFIELD* pIdField_, BYTE* pbStatus_);
        void Close () { m_pFloppy->Close(); m_uCacheTrack = 0U-1; }

        BYTE LoadTrack (UINT uSide_, UINT uTrack_);
        BYTE ReadData (BYTE* pbData_, UINT* puSize_);
        BYTE WriteData (BYTE* pbData_, UINT* puSize_);
        bool Save ();

        BYTE FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_);

        bool IsBusy (BYTE* pbStatus_, bool fWait_);

    protected:
        CFloppyStream* m_pFloppy;

        BYTE m_bCommand, m_bStatus;     // Current command and final status

        PTRACK  m_pTrack;               // Current track
        PSECTOR m_pSector;              // Pointer to first sector on track
        BYTE   *m_pbWrite;              // Data for in-progress write, to copy if successful

        UINT m_uCacheSide, m_uCacheTrack;
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
        BYTE FormatTrack (UINT uSide_, UINT uTrack_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_);

    protected:
        UINT  m_uSize;
};


// Namespace wrapper for the Huffman decompression code, to keep the global namespace clean
class LZSS
{
    public:
        static size_t Unpack (BYTE* pIn_, size_t uSize_, BYTE* pOut_);

    protected:
        static void Init ();
        static void RebuildTree ();
        static void UpdateTree (int c);

        static UINT GetChar () { return (pIn < pEnd) ? *pIn++ : 0; }
        static UINT GetBit ();
        static UINT GetByte ();
        static UINT DecodeChar ();
        static UINT DecodePosition ();

    protected:
        static BYTE ring_buff[], d_code[], d_len[];
        static WORD freq[];
        static short parent[], son[];

        static BYTE *pIn, *pEnd;
        static UINT uBits, uBitBuff, r;
};

#endif  // CDISK_H
