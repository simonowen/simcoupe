// Part of SimCoupe - A SAM Coupe emulator
//
// Disk.h: C++ classes used for accessing all SAM disk image types
//
//  Copyright (c) 1999-2014 Simon Owen
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

#ifndef DISK_H
#define DISK_H

#include "Floppy.h"     // native floppy support
#include "Stream.h"     // for the data stream abstraction
#include "VL1772.h"     // for the VL-1772 controller definitions

////////////////////////////////////////////////////////////////////////////////

const UINT NORMAL_DISK_SIDES     = 2;    // Normally 2 sides per disk
const UINT NORMAL_DISK_TRACKS    = 80;   // Normally 80 tracks per side
const UINT NORMAL_DISK_SECTORS   = 10;   // Normally 10 sectors per track
const UINT NORMAL_SECTOR_SIZE    = 512;  // Normally 512 bytes per sector

const UINT NORMAL_DIRECTORY_TRACKS = 4;  // Normally 4 tracks in a SAMDOS directory

const UINT DOS_DISK_SECTORS = 9;         // Double-density MS-DOS disks are 9 sectors per track


// The various disk format image sizes
#define MGT_IMAGE_SIZE  (NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS * NORMAL_DISK_SECTORS * NORMAL_SECTOR_SIZE)
#define DOS_IMAGE_SIZE  (NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS * DOS_DISK_SECTORS * NORMAL_SECTOR_SIZE)

const UINT DISK_FILE_HEADER_SIZE = 9;    // From SAM Technical Manual  (bType, wSize, wOffset, wUnused, bPages, bStartPage)

// Maximum size of a file that will fit on a SAM disk
const UINT MAX_SAM_FILE_SIZE = ((NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS) - NORMAL_DIRECTORY_TRACKS) *
                                NORMAL_DISK_SECTORS * (NORMAL_SECTOR_SIZE-2) - DISK_FILE_HEADER_SIZE;

////////////////////////////////////////////////////////////////////////////////

// The ID string for Aley Keprt's SAD disk images
#define SAD_SIGNATURE           "Aley's disk backup"

// SAD file header
typedef struct
{
    BYTE abSignature[sizeof(SAD_SIGNATURE) - 1];

    BYTE bSides;             // Number of sides on the disk
    BYTE bTracks;            // Number of tracks per side
    BYTE bSectors;           // Number of sectors per track
    BYTE bSectorSizeDiv64;   // Sector size divided by 64
}
SAD_HEADER;

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
    char szSignature[34];    // one of the signatures above, depending on DSK/EDSK
    char szCreator[14];      // name of creator (utility/emulator)
    BYTE bTracks;
    BYTE bSides;
    BYTE abTrackSize[2];     // fixed track size (DSK only)
} EDSK_HEADER;

typedef struct
{
    char szSignature[13];    // Track-Info\r\n
    BYTE bRate;              // 0=unknown (default=250K), 1=250K/300K, 2=500K, 3=1M
    BYTE bEncoding;          // 0=unknown (default=MFM), 1=FM, 2=MFM
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

enum { dtNone, dtUnknown, dtFloppy, dtFile, dtEDSK, dtSAD, dtMGT, dtSBT, dtCAPS };

#define LOAD_DELAY  3   // Number of status reads to artificially stay busy for image file track loads
                        // Pro-Dos relies on data not being available immediately a command is submitted

class CDisk
{
    friend class CDrive;

    // Constructor and virtual destructor
    public:
        CDisk (CStream* pStream_, int nType_);
        CDisk (const CDisk &) = delete;
        void operator= (const CDisk &) = delete;
        virtual ~CDisk ();

    public:
        static int GetType (CStream* pStream_);
        static CDisk* Open (const char* pcszDisk_, bool fReadOnly_=false);
        static CDisk* Open (void* pv_, size_t uSize_, const char* pcszDisk_);

        virtual void Close () { m_pStream->Close(); }
        virtual void Flush () { }
        virtual bool Save () { return false; };
        virtual BYTE FormatTrack (BYTE /*cyl_*/, BYTE /*head_*/, IDFIELD* /*paID_*/, BYTE* /*papbData_*/[], UINT /*uSectors_*/) { return WRITE_PROTECT; }


    // Public query functions
    public:
        const char* GetPath () { return m_pStream->GetPath(); }
        const char* GetFile () { return m_pStream->GetFile(); }
        bool IsReadOnly () const { return m_pStream->IsReadOnly(); }
        bool IsModified () const { return m_fModified; }

        void SetModified (bool fModified_=true) { m_fModified = fModified_; }

    // Protected overrides
    protected:
        virtual BYTE LoadTrack (BYTE /*cyl_*/, BYTE /*head_*/) { m_nBusy = LOAD_DELAY; return 0; }
        virtual bool GetSector (BYTE cyl_, BYTE head_, BYTE index_, IDFIELD* pID_, BYTE* pbStatus_=nullptr) = 0;
        virtual BYTE ReadData (BYTE cyl_, BYTE head_, BYTE index_, BYTE* pbData_, UINT* puSize_) = 0;
        virtual BYTE WriteData (BYTE /*cyl_*/, BYTE /*head_*/, BYTE /*index_*/, BYTE* /*pbData_*/, UINT* /*puSize_*/) { return WRITE_PROTECT; }

        virtual bool IsBusy (BYTE* /*pbStatus_*/, bool /*fWait_*/=false) { if (!m_nBusy) return false; m_nBusy--; return true; }

    protected:
        int m_nType;
        int m_nBusy;
        bool m_fModified;

        CStream *m_pStream;
        BYTE *m_pbData;
};


class CMGTDisk : public CDisk
{
    public:
        CMGTDisk (CStream* pStream_, UINT uSectors_=NORMAL_DISK_SECTORS);

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        bool GetSector (BYTE cyl_, BYTE head_, BYTE index_, IDFIELD* pID_, BYTE* pbStatus_) override;
        BYTE ReadData (BYTE cyl_, BYTE head_, BYTE index_, BYTE* pbData_, UINT* puSize_) override;
        BYTE WriteData (BYTE cyl_, BYTE head_, BYTE index_, BYTE* pbData_, UINT* puSize_) override;
        bool Save () override;
        BYTE FormatTrack (BYTE cyl_, BYTE head_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_) override;

    protected:
        UINT m_uSectors = 0;
};


class CSADDisk : public CDisk
{
    public:
        CSADDisk (CStream* pStream_, UINT uSides_=NORMAL_DISK_SIDES, UINT uTracks_=NORMAL_DISK_TRACKS,
                    UINT uSectors_=NORMAL_DISK_SECTORS, UINT uSectorSize_=NORMAL_SECTOR_SIZE);

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        bool GetSector (BYTE cyl_, BYTE head_, BYTE index_, IDFIELD* pID_, BYTE* pbStatus_) override;
        BYTE ReadData (BYTE cyl_, BYTE head_, BYTE index_, BYTE* pbData_, UINT* puSize_) override;
        BYTE WriteData (BYTE cyl_, BYTE head_, BYTE index_, BYTE* pbData_, UINT* puSize_) override;
        bool Save () override;
        BYTE FormatTrack (BYTE cyl_, BYTE head_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_) override;

    protected:
        UINT m_uSides = 0, m_uTracks = 0, m_uSectors = 0, m_uSectorSize = 0;
};


class CEDSKDisk final : public CDisk
{
    public:
        CEDSKDisk (CStream* pStream_, UINT uSides_=NORMAL_DISK_SIDES, UINT uTracks_=NORMAL_DISK_TRACKS);
        CEDSKDisk (const CEDSKDisk &) = delete;
        void operator= (const CEDSKDisk &) = delete;
        ~CEDSKDisk ();

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        bool GetSector (BYTE cyl_, BYTE head_, BYTE index_, IDFIELD* pID_, BYTE* pbStatus_) override;
        BYTE ReadData (BYTE cyl_, BYTE head_, BYTE index_, BYTE* pbData_, UINT* puSize_) override;
        BYTE WriteData (BYTE cyl_, BYTE head_, BYTE index_, BYTE* pbData_, UINT* puSize_) override;
        bool Save () override;
        BYTE FormatTrack (BYTE cyl_, BYTE head_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_) override;

    protected:
        UINT m_uSides = 0, m_uTracks = 0;

        EDSK_TRACK* m_apTracks[MAX_DISK_SIDES][MAX_DISK_TRACKS];
        BYTE m_abSizes[MAX_DISK_SIDES][MAX_DISK_TRACKS];

    private:
        // These are for private class use and only valid immediately after calling GetSector()
        EDSK_SECTOR *m_pSector = nullptr;
        BYTE *m_pbData = nullptr;
};


class CFloppyDisk final : public CDisk
{
    public:
        CFloppyDisk (CStream* pStream_);
        CFloppyDisk (const CFloppyDisk &) = delete;
        void operator= (const CFloppyDisk &) = delete;

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        void Close () override { m_pFloppy->Close(); m_pTrack->head = 0xff; }
        void Flush () override { Close(); }

        BYTE LoadTrack (BYTE cyl_, BYTE head_) override;
        bool GetSector (BYTE cyl_, BYTE head_, BYTE index_, IDFIELD* pID_, BYTE* pbStatus_) override;
        BYTE ReadData (BYTE cyl_, BYTE head_, BYTE index_, BYTE* pbData_, UINT* puSize_) override;
        BYTE WriteData (BYTE cyl_, BYTE head_, BYTE index_, BYTE* pbData_, UINT* puSize_) override;
        bool Save () override;

        BYTE FormatTrack (BYTE cyl_, BYTE head_, IDFIELD* paID_, BYTE* papbData_[], UINT uSectors_) override;

        bool IsBusy (BYTE* pbStatus_, bool fWait_) override;

    protected:
        CFloppyStream* m_pFloppy = nullptr;

        BYTE m_bCommand = 0, m_bStatus = 0;     // Current command and final status

        PTRACK  m_pTrack = nullptr;             // Current track
        PSECTOR m_pSector = nullptr;            // Pointer to first sector on track
};


class CFileDisk final : public CDisk
{
    public:
        CFileDisk (CStream* pStream_);

    public:
        static bool IsRecognised (CStream* pStream_);

    public:
        bool GetSector (BYTE cyl_, BYTE head_, BYTE index_, IDFIELD* pID_, BYTE* pbStatus_) override;
        BYTE ReadData (BYTE cyl_, BYTE head_, BYTE index_, BYTE* pbData_, UINT* puSize_) override;

    protected:
        UINT m_uSize;
};

#endif  // DISK_H
