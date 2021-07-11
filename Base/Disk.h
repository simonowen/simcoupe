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

#include "Stream.h"
#include "VL1772.h"

////////////////////////////////////////////////////////////////////////////////

constexpr auto MGT_DISK_HEADS = 2;
constexpr auto MGT_DISK_CYLS = 80;
constexpr auto MGT_DISK_SECTORS = 10;
constexpr auto MGT_FIRST_SECTOR = 1;
constexpr size_t NORMAL_SECTOR_SIZE = 512U;
constexpr uint8_t MGT_SECTOR_FILL = 0x00;

constexpr auto MGT_DIRECTORY_TRACKS = 4;
constexpr auto MGT_TRACK_SIZE = MGT_DISK_SECTORS * NORMAL_SECTOR_SIZE;
constexpr auto MGT_IMAGE_SIZE = MGT_DISK_HEADS * MGT_DISK_CYLS * MGT_TRACK_SIZE;

constexpr auto DOS_DISK_SECTORS = 9;
constexpr auto DOS_TRACK_SIZE = DOS_DISK_SECTORS * NORMAL_SECTOR_SIZE;
constexpr auto DOS_IMAGE_SIZE = MGT_DISK_HEADS * MGT_DISK_CYLS * DOS_TRACK_SIZE;
constexpr uint8_t DOS_SECTOR_FILL = 0xe5;

constexpr size_t DISK_FILE_HEADER_SIZE = 9;    // From SAM Technical Manual  (type, size16, offset16, unused16, pages, startpage)

constexpr auto MAX_SAM_FILE_SIZE = ((MGT_DISK_HEADS * MGT_DISK_CYLS) - MGT_DIRECTORY_TRACKS) *
    MGT_DISK_SECTORS * (NORMAL_SECTOR_SIZE - 2) - DISK_FILE_HEADER_SIZE;

////////////////////////////////////////////////////////////////////////////////

constexpr std::string_view SAD_SIGNATURE = "Aley's disk backup";

struct SAD_HEADER
{
    char signature[SAD_SIGNATURE.size()];
    uint8_t heads;
    uint8_t cyls;
    uint8_t sectors;
    uint8_t sector_size_div64;
};

////////////////////////////////////////////////////////////////////////////////

constexpr std::string_view DSK_SIGNATURE = "MV - CPC";
constexpr std::string_view EDSK_SIGNATURE = "EXTENDED CPC DSK File\r\nDisk-Info\r\n";
constexpr std::string_view EDSK_TRACK_SIGNATURE = "Track-Info\r\n";
constexpr std::string_view EDSK_SIMCOUPE_CREATOR = "SimCoupe";
constexpr auto EDSK_DATA_RATE_250K = 0;
constexpr auto EDSK_DATA_ENCODING_MFM = 0;

constexpr uint8_t ST1_765_CRC_ERROR = 0x20;
constexpr uint8_t ST2_765_DATA_NOT_FOUND = 0x01;
constexpr uint8_t ST2_765_CRC_ERROR = 0x20;
constexpr uint8_t ST2_765_CONTROL_MARK = 0x40;

struct EDSK_HEADER
{
    char signature[34];
    char creator[14];
    uint8_t cyls;
    uint8_t heads;
    uint8_t dsk_track_size[2];  // DSK only
    uint8_t size_msbs[204];
};
static_assert(sizeof(EDSK_HEADER) == 0x100);

struct EDSK_TRACK
{
    char signature[13];
    uint8_t data_rate;      // 0=unknown (default=250K), 1=250K/300K, 2=500K, 3=1M
    uint8_t data_encoding;  // 0=unknown (default=MFM), 1=FM, 2=MFM
    uint8_t unused;
    uint8_t cyl;
    uint8_t head;
    uint8_t unused2[2];
    uint8_t size;
    uint8_t sectors;
    uint8_t gap3;
    uint8_t fill;
};

struct EDSK_SECTOR
{
    uint8_t cyl;
    uint8_t head;
    uint8_t sector;
    uint8_t size;
    uint8_t status1;
    uint8_t status2;
    uint8_t data_low;
    uint8_t data_high;
};

constexpr size_t ESDK_MAX_TRACK_SIZE = 0xff00;
constexpr auto EDSK_MAX_SECTORS = ((0x100 - sizeof(EDSK_TRACK)) / sizeof(EDSK_SECTOR));

////////////////////////////////////////////////////////////////////////////////

enum class DiskType
{
    Unknown, Floppy, File, EDSK, SAD, MGT, SBT
};

// Stay BUSY for a few status reads after each command. Needed by Pro-Dos.
#define LOAD_DELAY  3

class Disk
{
    friend class Drive;

public:
    Disk(std::unique_ptr<Stream> stream, DiskType type);
    virtual ~Disk() = default;

    static DiskType GetType(Stream& pStream_);
    static std::unique_ptr<Disk> Open(const std::string& disk_path, bool read_only = false);
    static std::unique_ptr<Disk> Open(const std::vector<uint8_t>& mem_file, const std::string& file_desc);

    std::string GetPath() { return m_stream->GetPath(); }
    std::string GetFile() { return m_stream->GetName(); }
    bool WriteProtected() const { return m_stream->WriteProtected(); }
    bool StreamChanged() const { return m_stream->LastWriteTime() != m_last_write_time; }

    virtual void Close();
    virtual bool Save() = 0;
    virtual uint8_t FormatTrack(uint8_t cyl, uint8_t head,
        const std::vector<std::pair<IDFIELD, std::vector<uint8_t>>>& sectors) { return WRITE_PROTECT; }

protected:
    virtual uint8_t LoadTrack(uint8_t, uint8_t) { m_busy_frames = LOAD_DELAY; return 0; }
    virtual std::pair<uint8_t, IDFIELD> GetSector(uint8_t cyl, uint8_t head, uint8_t sector_index) = 0;
    virtual std::pair<uint8_t, std::vector<uint8_t>> ReadData(uint8_t cyl, uint8_t head, uint8_t sector_index) = 0;
    virtual uint8_t WriteData(uint8_t, uint8_t, uint8_t, const std::vector<uint8_t>&) = 0;
    virtual bool IsBusy(uint8_t& status, bool wait = false);

protected:
    DiskType m_type = DiskType::Unknown;
    int m_busy_frames = 0;
    bool m_modified = false;
    fs::file_time_type m_last_write_time{};

    std::unique_ptr<Stream> m_stream;
    std::vector<uint8_t> m_data;
};

class MGTDisk : public Disk
{
public:
    MGTDisk(std::unique_ptr<Stream> stream, int sectors = MGT_DISK_SECTORS);

    static bool IsRecognised(Stream& stream);

    bool Save() override;
    std::pair<uint8_t, IDFIELD> GetSector(uint8_t cyl, uint8_t head, uint8_t index) override;
    std::pair<uint8_t, std::vector<uint8_t>> ReadData(uint8_t cyl, uint8_t head, uint8_t index) override;
    uint8_t WriteData(uint8_t cyl, uint8_t head, uint8_t sector_index, const std::vector<uint8_t>& data) override;
    uint8_t FormatTrack(uint8_t cyl, uint8_t head, const std::vector<std::pair<IDFIELD, std::vector<uint8_t>>>& sectors) override;

protected:
    int m_sectors = 0;
};

class SADDisk : public Disk
{
public:
    SADDisk(std::unique_ptr<Stream> stream);

    static bool IsRecognised(Stream& stream);

    bool Save() override;
    std::pair<uint8_t, IDFIELD> GetSector(uint8_t cyl, uint8_t head, uint8_t sector_index) override;
    std::pair<uint8_t, std::vector<uint8_t>> ReadData(uint8_t cyl, uint8_t head, uint8_t sector_index) override;
    uint8_t WriteData(uint8_t cyl, uint8_t head, uint8_t sector_index, const std::vector<uint8_t>& data) override;
    uint8_t FormatTrack(uint8_t cyl, uint8_t head, const std::vector<std::pair<IDFIELD, std::vector<uint8_t>>>& sectors) override;

protected:
    int m_cyls = MGT_DISK_CYLS;
    int m_heads = MGT_DISK_HEADS;
    int m_sectors = MGT_DISK_SECTORS;
    int m_sector_size = NORMAL_SECTOR_SIZE;
};

class EDSKDisk final : public Disk
{
public:
    EDSKDisk(std::unique_ptr<Stream> stream, int cyls = MGT_DISK_CYLS, int heads = MGT_DISK_HEADS);

    static bool IsRecognised(Stream& pStream_);

    bool Save() override;
    std::pair<uint8_t, IDFIELD> GetSector(uint8_t cyl, uint8_t head, uint8_t sector_index) override;
    std::pair<uint8_t, std::vector<uint8_t>> ReadData(uint8_t cyl, uint8_t head, uint8_t sector_index) override;
    uint8_t WriteData(uint8_t cyl, uint8_t head, uint8_t sector_index, const std::vector<uint8_t>& data) override;
    uint8_t FormatTrack(uint8_t cyl, uint8_t head, const std::vector<std::pair<IDFIELD, std::vector<uint8_t>>>& sectors) override;

protected:
    int m_heads = 0;
    int m_cyls = 0;

    std::vector<std::vector<std::pair<EDSK_SECTOR, std::vector<uint8_t>>>> m_tracks;
};

class FileDisk final : public Disk
{
public:
    FileDisk(std::unique_ptr<Stream> stream);

    static bool IsRecognised(Stream& stream);

    bool Save() override;
    std::pair<uint8_t, IDFIELD> GetSector(uint8_t cyl, uint8_t head, uint8_t sector_index) override;
    std::pair<uint8_t, std::vector<uint8_t>> ReadData(uint8_t cyl, uint8_t head, uint8_t sector_index) override;
    uint8_t WriteData(uint8_t, uint8_t, uint8_t, const std::vector<uint8_t>&) override;
};

#ifdef _WIN32

#include "Floppy.h"

class FloppyDisk final : public Disk
{
public:
    FloppyDisk(std::unique_ptr<Stream>);

    static bool IsRecognised(Stream& pStream_);

    void Close() override;
    bool Save() override;
    uint8_t LoadTrack(uint8_t cyl, uint8_t head) override;
    std::pair<uint8_t, IDFIELD> GetSector(uint8_t cyl, uint8_t head, uint8_t sector_index) override;
    std::pair<uint8_t, std::vector<uint8_t>> ReadData(uint8_t cyl, uint8_t head, uint8_t sector_index) override;
    uint8_t WriteData(uint8_t cyl, uint8_t head, uint8_t sector_index, const std::vector<uint8_t>& data) override;
    uint8_t FormatTrack(uint8_t cyl, uint8_t head, const std::vector<std::pair<IDFIELD, std::vector<uint8_t>>>& sectors) override;

    bool IsBusy(uint8_t& status, bool wait) override;

protected:
    std::shared_ptr<TRACK> m_track;
};

#endif // _WIN32
