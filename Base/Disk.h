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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#pragma once

#include "Floppy.h"     // native floppy support
#include "Stream.h"     // for the data stream abstraction
#include "VL1772.h"     // for the VL-1772 controller definitions

////////////////////////////////////////////////////////////////////////////////

const unsigned int NORMAL_DISK_SIDES = 2;       // Normally 2 sides per disk
const unsigned int NORMAL_DISK_TRACKS = 80;     // Normally 80 tracks per side
const unsigned int NORMAL_DISK_SECTORS = 10;    // Normally 10 sectors per track
const unsigned int NORMAL_SECTOR_SIZE = 512;    // Normally 512 bytes per sector

const unsigned int NORMAL_DIRECTORY_TRACKS = 4; // Normally 4 tracks in a SAMDOS directory

const unsigned int DOS_DISK_SECTORS = 9;        // Double-density MS-DOS disks are 9 sectors per track


// The various disk format image sizes
#define MGT_IMAGE_SIZE  (NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS * NORMAL_DISK_SECTORS * NORMAL_SECTOR_SIZE)
#define DOS_IMAGE_SIZE  (NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS * DOS_DISK_SECTORS * NORMAL_SECTOR_SIZE)

const unsigned int DISK_FILE_HEADER_SIZE = 9;    // From SAM Technical Manual  (bType, wSize, wOffset, wUnused, bPages, bStartPage)

// Maximum size of a file that will fit on a SAM disk
const unsigned int MAX_SAM_FILE_SIZE = ((NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS) - NORMAL_DIRECTORY_TRACKS) *
NORMAL_DISK_SECTORS * (NORMAL_SECTOR_SIZE - 2) - DISK_FILE_HEADER_SIZE;

////////////////////////////////////////////////////////////////////////////////

// The ID string for Aley Keprt's SAD disk images
#define SAD_SIGNATURE           "Aley's disk backup"

// SAD file header
struct SAD_HEADER
{
    uint8_t abSignature[sizeof(SAD_SIGNATURE) - 1];

    uint8_t bSides;             // Number of sides on the disk
    uint8_t bTracks;            // Number of tracks per side
    uint8_t bSectors;           // Number of sectors per track
    uint8_t bSectorSizeDiv64;   // Sector size divided by 64
};

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

struct EDSK_HEADER
{
    char szSignature[34];    // one of the signatures above, depending on DSK/EDSK
    char szCreator[14];      // name of creator (utility/emulator)
    uint8_t bTracks;
    uint8_t bSides;
    uint8_t abTrackSize[2];     // fixed track size (DSK only)
};

struct EDSK_TRACK
{
    char szSignature[13];    // Track-Info\r\n
    uint8_t bRate;              // 0=unknown (default=250K), 1=250K/300K, 2=500K, 3=1M
    uint8_t bEncoding;          // 0=unknown (default=MFM), 1=FM, 2=MFM
    uint8_t bUnused;
    uint8_t bTrack;
    uint8_t bSide;
    uint8_t abUnused[2];
    uint8_t bSize;
    uint8_t bSectors;
    uint8_t bGap3;
    uint8_t bFill;
};

struct EDSK_SECTOR
{
    uint8_t bTrack, bSide, bSector, bSize;
    uint8_t bStatus1, bStatus2;
    uint8_t bDatalow, bDatahigh;
};

////////////////////////////////////////////////////////////////////////////////

enum class DiskType
{
    None, Unknown, Floppy, File, EDSK, SAD, MGT, SBT, CAPS
};

#define LOAD_DELAY  3   // Number of status reads to artificially stay busy for image file track loads
// Pro-Dos relies on data not being available immediately a command is submitted

class CDisk
{
    friend class CDrive;

    // Constructor and virtual destructor
public:
    CDisk(std::unique_ptr<CStream> stream, DiskType type);

public:
    static DiskType GetType(CStream& pStream_);
    static std::unique_ptr<CDisk> Open(const char* pcszDisk_, bool fReadOnly_ = false);
    static std::unique_ptr<CDisk> Open(void* pv_, size_t uSize_, const char* pcszDisk_);

    virtual void Close() { m_stream->Close(); }
    virtual void Flush() { }
    virtual bool Save() { return false; };
    virtual uint8_t FormatTrack(uint8_t /*cyl_*/, uint8_t /*head_*/, IDFIELD* /*paID_*/, uint8_t* /*papbData_*/[], unsigned int /*uSectors_*/) { return WRITE_PROTECT; }


    // Public query functions
public:
    const char* GetPath() { return m_stream->GetPath(); }
    const char* GetFile() { return m_stream->GetFile(); }
    bool IsReadOnly() const { return m_stream->IsReadOnly(); }
    bool IsModified() const { return m_fModified; }

    void SetModified(bool fModified_ = true) { m_fModified = fModified_; }

    // Protected overrides
protected:
    virtual uint8_t LoadTrack(uint8_t /*cyl_*/, uint8_t /*head_*/) { m_nBusy = LOAD_DELAY; return 0; }
    virtual bool GetSector(uint8_t cyl_, uint8_t head_, uint8_t index_, IDFIELD* pID_, uint8_t* pbStatus_ = nullptr) = 0;
    virtual uint8_t ReadData(uint8_t cyl_, uint8_t head_, uint8_t index_, uint8_t* pbData_, unsigned int* puSize_) = 0;
    virtual uint8_t WriteData(uint8_t /*cyl_*/, uint8_t /*head_*/, uint8_t /*index_*/, uint8_t* /*pbData_*/, unsigned int* /*puSize_*/) { return WRITE_PROTECT; }

    virtual bool IsBusy(uint8_t* /*pbStatus_*/, bool /*fWait_*/ = false) { if (!m_nBusy) return false; m_nBusy--; return true; }

protected:
    DiskType m_nType;
    int m_nBusy;
    bool m_fModified;

    std::unique_ptr<CStream> m_stream;
    std::vector<uint8_t> m_data;
};


class CMGTDisk : public CDisk
{
public:
    CMGTDisk(std::unique_ptr<CStream> stream, unsigned int uSectors_ = NORMAL_DISK_SECTORS);

public:
    static bool IsRecognised(CStream& stream);

public:
    bool GetSector(uint8_t cyl_, uint8_t head_, uint8_t index_, IDFIELD* pID_, uint8_t* pbStatus_) override;
    uint8_t ReadData(uint8_t cyl_, uint8_t head_, uint8_t index_, uint8_t* pbData_, unsigned int* puSize_) override;
    uint8_t WriteData(uint8_t cyl_, uint8_t head_, uint8_t index_, uint8_t* pbData_, unsigned int* puSize_) override;
    bool Save() override;
    uint8_t FormatTrack(uint8_t cyl_, uint8_t head_, IDFIELD* paID_, uint8_t* papbData_[], unsigned int uSectors_) override;

protected:
    unsigned int m_uSectors = 0;
};


class CSADDisk : public CDisk
{
public:
    CSADDisk(std::unique_ptr<CStream> stream, unsigned int uSides_ = NORMAL_DISK_SIDES, unsigned int uTracks_ = NORMAL_DISK_TRACKS,
        unsigned int uSectors_ = NORMAL_DISK_SECTORS, unsigned int uSectorSize_ = NORMAL_SECTOR_SIZE);

public:
    static bool IsRecognised(CStream& stream);

public:
    bool GetSector(uint8_t cyl_, uint8_t head_, uint8_t index_, IDFIELD* pID_, uint8_t* pbStatus_) override;
    uint8_t ReadData(uint8_t cyl_, uint8_t head_, uint8_t index_, uint8_t* pbData_, unsigned int* puSize_) override;
    uint8_t WriteData(uint8_t cyl_, uint8_t head_, uint8_t index_, uint8_t* pbData_, unsigned int* puSize_) override;
    bool Save() override;
    uint8_t FormatTrack(uint8_t cyl_, uint8_t head_, IDFIELD* paID_, uint8_t* papbData_[], unsigned int uSectors_) override;

protected:
    unsigned int m_uSides = 0, m_uTracks = 0, m_uSectors = 0, m_uSectorSize = 0;
};


class CEDSKDisk final : public CDisk
{
public:
    CEDSKDisk(std::unique_ptr<CStream> stream, unsigned int uSides_ = NORMAL_DISK_SIDES, unsigned int uTracks_ = NORMAL_DISK_TRACKS);
    CEDSKDisk(const CEDSKDisk&) = delete;
    void operator= (const CEDSKDisk&) = delete;
    ~CEDSKDisk();

public:
    static bool IsRecognised(CStream& pStream_);

public:
    bool GetSector(uint8_t cyl_, uint8_t head_, uint8_t index_, IDFIELD* pID_, uint8_t* pbStatus_) override;
    uint8_t ReadData(uint8_t cyl_, uint8_t head_, uint8_t index_, uint8_t* pbData_, unsigned int* puSize_) override;
    uint8_t WriteData(uint8_t cyl_, uint8_t head_, uint8_t index_, uint8_t* pbData_, unsigned int* puSize_) override;
    bool Save() override;
    uint8_t FormatTrack(uint8_t cyl_, uint8_t head_, IDFIELD* paID_, uint8_t* papbData_[], unsigned int uSectors_) override;

protected:
    unsigned int m_uSides = 0, m_uTracks = 0;

    EDSK_TRACK* m_apTracks[MAX_DISK_SIDES][MAX_DISK_TRACKS];
    uint8_t m_abSizes[MAX_DISK_SIDES][MAX_DISK_TRACKS];

private:
    // These are for private class use and only valid immediately after calling GetSector()
    EDSK_SECTOR* m_pSector = nullptr;
    uint8_t* m_pbData = nullptr;
};


class CFloppyDisk final : public CDisk
{
public:
    CFloppyDisk(std::unique_ptr<CStream>);
    CFloppyDisk(const CFloppyDisk&) = delete;
    void operator= (const CFloppyDisk&) = delete;

public:
    static bool IsRecognised(CStream& pStream_);

public:
    void Close() override { m_stream->Close(); m_pTrack->head = 0xff; }
    void Flush() override { Close(); }

    uint8_t LoadTrack(uint8_t cyl_, uint8_t head_) override;
    bool GetSector(uint8_t cyl_, uint8_t head_, uint8_t index_, IDFIELD* pID_, uint8_t* pbStatus_) override;
    uint8_t ReadData(uint8_t cyl_, uint8_t head_, uint8_t index_, uint8_t* pbData_, unsigned int* puSize_) override;
    uint8_t WriteData(uint8_t cyl_, uint8_t head_, uint8_t index_, uint8_t* pbData_, unsigned int* puSize_) override;
    bool Save() override;

    uint8_t FormatTrack(uint8_t cyl_, uint8_t head_, IDFIELD* paID_, uint8_t* papbData_[], unsigned int uSectors_) override;

    bool IsBusy(uint8_t* pbStatus_, bool fWait_) override;

protected:
    uint8_t m_bCommand = 0, m_bStatus = 0;     // Current command and final status

    TRACK* m_pTrack = nullptr;              // Current track
    SECTOR* m_pSector = nullptr;            // Pointer to first sector on track
};


class CFileDisk final : public CDisk
{
public:
    CFileDisk(std::unique_ptr<CStream> stream);

public:
    static bool IsRecognised(CStream& stream);

public:
    bool GetSector(uint8_t cyl_, uint8_t head_, uint8_t index_, IDFIELD* pID_, uint8_t* pbStatus_) override;
    uint8_t ReadData(uint8_t cyl_, uint8_t head_, uint8_t index_, uint8_t* pbData_, unsigned int* puSize_) override;

protected:
    unsigned int m_uSize;
};
