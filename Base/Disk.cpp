// Part of SimCoupe - A SAM Coupe emulator
//
// Disk.cpp: C++ classes used for accessing all SAM disk image types
//
//  Copyright (c) 1999-2015 Simon Owen
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

#include "SimCoupe.h"
#include "Disk.h"

#include "Drive.h"

DiskType Disk::GetType(Stream& stream)
{
#ifdef _WIN32
    if (FloppyDisk::IsRecognised(stream))
        return DiskType::Floppy;
    else
#endif
    if (EDSKDisk::IsRecognised(stream))
        return DiskType::EDSK;
    else if (SADDisk::IsRecognised(stream))
        return DiskType::SAD;
    else if (MGTDisk::IsRecognised(stream))
        return DiskType::MGT;
    else if (FileDisk::IsRecognised(stream))
    {
        fs::path file = fs::path(stream.GetName());
        if (tolower(file.extension().string()) == ".sbt")
            return DiskType::SBT;
    }

    return DiskType::Unknown;
}

std::unique_ptr<Disk>
Disk::Open(const std::string& disk_path, bool read_only)
{
    if (auto stream = Stream::Open(disk_path, read_only))
    {
        switch (GetType(*stream))
        {
#ifdef _WIN32
        case DiskType::Floppy:  return std::make_unique<FloppyDisk>(std::move(stream));
#endif
        case DiskType::EDSK:    return std::make_unique<EDSKDisk>(std::move(stream));
        case DiskType::SAD:     return std::make_unique<SADDisk>(std::move(stream));
        case DiskType::MGT:     return std::make_unique<MGTDisk>(std::move(stream));
        case DiskType::SBT:     return std::make_unique<FileDisk>(std::move(stream));
        default: break;
        }
    }

    return nullptr;
}

std::unique_ptr<Disk>
Disk::Open(const std::vector<uint8_t>& mem_file, const std::string& file_desc)
{
    if (auto stream = std::make_unique<MemStream>(mem_file))
        return std::make_unique<FileDisk>(std::move(stream));

    return nullptr;
}

Disk::Disk(std::unique_ptr<Stream> stream, DiskType type)
    : m_type(type), m_busy_frames(0), m_stream(std::move(stream))
{
    m_last_write_time = m_stream->LastWriteTime();
}

void Disk::Close()
{
    if (m_modified)
        Save();

    m_stream->Close();
}

std::pair<uint8_t, IDFIELD>
Disk::GetSector(uint8_t cyl, uint8_t head, uint8_t sector_index)
{
    IDFIELD id{};
    id.cyl = cyl;
    id.head = head;
    id.sector = sector_index + 1;
    id.size = 2;

    auto crc = CrcBlock("\xa1\xa1\xa1\xfe", 4);
    crc = CrcBlock(&id, 4, crc);
    id.crc1 = crc >> 8;
    id.crc2 = crc & 0xff;

    return std::make_pair(0, id);
}

bool Disk::IsBusy(uint8_t& status, bool wait)
{
    status = 0;

    if (wait || !m_busy_frames)
    {
        m_busy_frames = 0;
        return false;
    }

    m_busy_frames--;
    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool MGTDisk::IsRecognised(Stream& stream)
{
    auto size = stream.GetSize();
    return size == MGT_IMAGE_SIZE || size == DOS_IMAGE_SIZE;
}

MGTDisk::MGTDisk(std::unique_ptr<Stream> stream, int num_sectors)
    : Disk(std::move(stream), DiskType::MGT), m_sectors(num_sectors)
{
    m_data.resize((num_sectors == DOS_DISK_SECTORS) ? DOS_IMAGE_SIZE : MGT_IMAGE_SIZE);

    if (m_stream->GetSize())
    {
        m_stream->Rewind();
        m_stream->Read(m_data.data(), m_data.size());
        m_stream->Close();
    }
}

std::pair<uint8_t, IDFIELD>
MGTDisk::GetSector(uint8_t cyl, uint8_t head, uint8_t sector_index)
{
    if (cyl >= MGT_DISK_CYLS || head >= MGT_DISK_HEADS || sector_index >= m_sectors)
        return std::make_pair(RECORD_NOT_FOUND, IDFIELD{});

    return Disk::GetSector(cyl, head, sector_index);
}

std::pair<uint8_t, std::vector<uint8_t>>
MGTDisk::ReadData(uint8_t cyl, uint8_t head, uint8_t sector_index)
{
    if (sector_index >= m_sectors)
        return std::make_pair(RECORD_NOT_FOUND, std::vector<uint8_t>());

    auto offset = ((static_cast<size_t>(cyl) * MGT_DISK_HEADS + head) * m_sectors + sector_index) * NORMAL_SECTOR_SIZE;
    auto data = std::vector<uint8_t>(m_data.begin() + offset, m_data.begin() + offset + NORMAL_SECTOR_SIZE);

    return std::make_pair(0, std::move(data));
}

uint8_t MGTDisk::WriteData(uint8_t cyl, uint8_t head, uint8_t sector_index, const std::vector<uint8_t>& data)
{
    if (sector_index >= m_sectors || data.size() != NORMAL_SECTOR_SIZE)
        return RECORD_NOT_FOUND;

    if (WriteProtected())
        return WRITE_PROTECT;

    auto offset = ((static_cast<size_t>(cyl) * MGT_DISK_HEADS + head) * m_sectors + sector_index) * NORMAL_SECTOR_SIZE;
    std::copy(data.begin(), data.end(), m_data.begin() + offset);
    m_modified = true;

    return 0;
}

bool MGTDisk::Save()
{
    m_stream->Rewind();
    auto saved = m_stream->Write(m_data.data(), m_data.size()) == m_data.size();
    m_stream->Close();

    m_last_write_time = m_stream->LastWriteTime();
    m_modified = false;
    return saved;
}

uint8_t MGTDisk::FormatTrack(uint8_t cyl, uint8_t head,
    const std::vector<std::pair<IDFIELD, std::vector<uint8_t>>>& sectors)
{
    if (WriteProtected())
        return WRITE_PROTECT;
    else if (cyl >= MGT_DISK_CYLS || head >= MGT_DISK_HEADS || sectors.size() != m_sectors)
        return WRITE_FAULT;

    bool normal = true;
    int sector_mask = 0;
    for (auto& [id, data] : sectors)
    {
        normal &= (id.head == head && id.cyl == cyl);
        normal &= (SizeFromSizeCode(id.size) == NORMAL_SECTOR_SIZE);
        normal &= (data.size() == NORMAL_SECTOR_SIZE);
        sector_mask |= (1 << (id.sector - 1));
    }

    if (!normal || (sector_mask != ((1 << m_sectors) - 1)))
        return WRITE_FAULT;

    auto track_offset = (static_cast<size_t>(cyl) * MGT_DISK_HEADS + head) * (m_sectors * NORMAL_SECTOR_SIZE);

    for (auto& [id, data] : sectors)
    {
        auto sector_offset = (id.sector - 1) * NORMAL_SECTOR_SIZE;
        std::copy(data.begin(), data.end(), m_data.begin() + track_offset + sector_offset);
    }

    m_modified = true;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

bool SADDisk::IsRecognised(Stream& stream)
{
    SAD_HEADER sh{};

    auto valid = stream.Rewind() &&
        stream.Read(&sh, sizeof(sh)) == sizeof(sh) &&
        std::string_view(sh.signature, SAD_SIGNATURE.size()) == SAD_SIGNATURE &&
        sh.heads > 0 && sh.heads <= MAX_DISK_HEADS &&
        sh.cyls > 0 && sh.cyls <= MAX_DISK_CYLS &&
        (sh.sector_size_div64 << 6) >= MIN_SECTOR_SIZE &&
        (sh.sector_size_div64 << 6) <= MAX_SECTOR_SIZE &&
        (sh.sector_size_div64 & -sh.sector_size_div64) == sh.sector_size_div64;

    return valid;
}

SADDisk::SADDisk(std::unique_ptr<Stream> stream)
    : Disk(std::move(stream), DiskType::SAD)
{
    if (m_stream->GetSize())
    {
        SAD_HEADER sh{};
        m_stream->Rewind();
        m_stream->Read(&sh, sizeof(sh));

        m_heads = sh.heads;
        m_cyls = sh.cyls;
        m_sectors = sh.sectors;
        m_sector_size = sh.sector_size_div64 << 6;
        m_data.resize(static_cast<size_t>(m_cyls) * m_heads * m_sectors * m_sector_size);

        m_stream->Read(m_data.data(), m_data.size());
        m_stream->Close();
    }
    else
    {
        m_data.resize(m_cyls * m_heads * m_sectors * m_sector_size);
    }
}

std::pair<uint8_t, IDFIELD>
SADDisk::GetSector(uint8_t cyl, uint8_t head, uint8_t sector_index)
{
    if (cyl >= m_cyls || head >= m_heads || sector_index >= m_sectors)
        return std::make_pair(RECORD_NOT_FOUND, IDFIELD{});

    auto [status, id] = Disk::GetSector(cyl, head, sector_index);
    id.size = GetSizeCode(m_sector_size);
    return std::make_pair(status, id);
}

std::pair<uint8_t, std::vector<uint8_t>>
SADDisk::ReadData(uint8_t cyl, uint8_t head, uint8_t sector_index)
{
    size_t offset = (head * m_cyls + cyl) * (m_sectors * m_sector_size) +
        (sector_index * m_sector_size);

    return std::make_pair(0, std::vector<uint8_t>(
        m_data.begin() + offset,
        m_data.begin() + offset + m_sector_size));
}

uint8_t SADDisk::WriteData(uint8_t cyl, uint8_t head, uint8_t sector_index, const std::vector<uint8_t>& data)
{
    if (sector_index >= m_sectors || data.size() != m_sector_size)
        return RECORD_NOT_FOUND;

    if (WriteProtected())
        return WRITE_PROTECT;

    auto offset = (head * m_cyls + cyl) * (m_sectors * m_sector_size) + (sector_index * m_sector_size);
    std::copy(data.begin(), data.end(), m_data.begin() + offset);
    m_modified = true;

    return 0;
}

bool SADDisk::Save()
{
    SAD_HEADER sh{};
    SAD_SIGNATURE.copy(sh.signature, sizeof(sh.signature));
    sh.cyls = static_cast<uint8_t>(m_cyls);
    sh.heads = static_cast<uint8_t>(m_heads);
    sh.sectors = static_cast<uint8_t>(m_sectors);
    sh.sector_size_div64 = static_cast<uint8_t>(m_sector_size >> 6);

    m_stream->Rewind();
    auto saved = m_stream->Write(&sh, sizeof(sh)) == sizeof(sh) &&
        m_stream->Write(m_data.data(), m_data.size()) == m_data.size();
    m_stream->Close();

    m_last_write_time = m_stream->LastWriteTime();
    m_modified = false;
    return saved;
}

uint8_t SADDisk::FormatTrack(uint8_t cyl, uint8_t head,
    const std::vector<std::pair<IDFIELD, std::vector<uint8_t>>>& sectors)
{
    if (WriteProtected() || sectors.size() != m_sectors || cyl >= m_cyls)
        return WRITE_PROTECT;

    bool normal = true;
    uint32_t sector_mask = 0;
    for (auto& [id, data] : sectors)
    {
        normal &= (id.head == head && id.cyl == cyl);
        normal &= (SizeFromSizeCode(id.size) == m_sector_size);
        sector_mask |= (1 << (id.sector - 1));
    }

    normal &= (sector_mask == ((1UL << m_sectors) - 1));
    if (!normal)
        return WRITE_PROTECT;

    auto track_offset = sizeof(SAD_HEADER) + (head * m_cyls + cyl) * (m_sectors * NORMAL_SECTOR_SIZE);

    for (auto& [id, data] : sectors)
    {
        auto sector_offset = (id.sector - 1) * m_sector_size;
        std::copy(data.begin(), data.end(), m_data.begin() + track_offset + sector_offset);
    }

    m_modified = true;
    return 0;
}
////////////////////////////////////////////////////////////////////////////////

bool EDSKDisk::IsRecognised(Stream& stream)
{
    EDSK_HEADER eh{};

    return stream.Rewind() &&
        stream.Read(&eh, sizeof(eh)) == sizeof(eh) &&
        (std::string_view(eh.signature, EDSK_SIGNATURE.size()) == EDSK_SIGNATURE ||
            std::string_view(eh.signature, DSK_SIGNATURE.size()) == DSK_SIGNATURE);
}

EDSKDisk::EDSKDisk(std::unique_ptr<Stream> stream, int cyls, int heads)
    : Disk(std::move(stream), DiskType::EDSK), m_cyls(cyls), m_heads(heads)
{
    m_tracks.resize(MAX_DISK_CYLS * MAX_DISK_HEADS);

    if (!m_stream->GetSize())
        return;

    EDSK_HEADER eh{};
    m_stream->Rewind();
    m_stream->Read(&eh, sizeof(eh));

    m_cyls = std::min(eh.cyls, static_cast<uint8_t>(MAX_DISK_CYLS));
    m_heads = std::min(eh.heads, static_cast<uint8_t>(MAX_DISK_HEADS));

    auto is_dsk = std::string_view(eh.signature, DSK_SIGNATURE.size()) == DSK_SIGNATURE;
    size_t dsk_track_size = eh.dsk_track_size[0] | (eh.dsk_track_size[1] << 8);

    for (uint8_t cyl = 0; cyl < m_cyls; cyl++)
    {
        for (uint8_t head = 0; head < m_heads; head++)
        {
            auto edsk_track_size = eh.size_msbs[cyl * m_heads + head] << 8;
            auto track_size = is_dsk ? dsk_track_size : edsk_track_size;
            if (!track_size)
                continue;

            EDSK_TRACK et{};
            if (m_stream->Read(&et, sizeof(et)) != sizeof(et) ||
                std::string_view(et.signature, EDSK_TRACK_SIGNATURE.size()) != EDSK_TRACK_SIGNATURE)
            {
                return;
            }

            std::vector<std::pair<EDSK_SECTOR, std::vector<uint8_t>>> track;
            track.resize(et.sectors);

            for (auto& s : track)
            {
                auto& [sector, data] = s;
                if (m_stream->Read(&sector, sizeof(sector)) != sizeof(sector))
                    return;
            }

            auto slack_size = (0x100 - sizeof(EDSK_TRACK) - sizeof(EDSK_SECTOR) * et.sectors) & 0xff;
            std::vector<uint8_t> slack(slack_size);
            m_stream->Read(slack.data(), slack.size());

            for (auto& [sector, data] : track)
            {
                size_t data_size = (sector.data_high << 8) | sector.data_low;
                data.resize(data_size);
                if (m_stream->Read(data.data(), data.size()) != data.size())
                    return;

                data_size = SizeFromSizeCode(sector.size);
                data.resize(data_size);
            }

            if ((et.data_rate != 0 && et.data_rate != 1) || (et.data_encoding != 0 && et.data_encoding != 1))
                track.clear();

            m_tracks[static_cast<size_t>(cyl) * m_heads + head] = std::move(track);
        }
    }

    m_stream->Close();
}

std::pair<uint8_t, IDFIELD>
EDSKDisk::GetSector(uint8_t cyl, uint8_t head, uint8_t sector_index)
{
    auto entry = static_cast<size_t>(cyl) * m_heads + head;
    if (entry >= m_tracks.size() || sector_index >= m_tracks[entry].size())
        return std::make_pair(RECORD_NOT_FOUND, IDFIELD{});

    auto& [sector, data] = m_tracks[entry][sector_index];

    IDFIELD id{};
    id.cyl = sector.cyl;
    id.head = sector.head;
    id.sector = sector.sector;
    id.size = sector.size;

    auto crc = CrcBlock("\xa1\xa1\xa1\xfe", 4);
    crc = CrcBlock(&id, 4, crc);
    id.crc1 = crc >> 8;
    id.crc2 = crc & 0xff;

    auto status = 0;
    if (sector.status1 & ST1_765_CRC_ERROR)
    {
        status = CRC_ERROR;
        id.crc1 ^= 0x55;
    }
    return std::make_pair(status, id);
}

std::pair<uint8_t, std::vector<uint8_t>>
EDSKDisk::ReadData(uint8_t cyl, uint8_t head, uint8_t sector_index)
{
    auto [status, id] = GetSector(cyl, head, sector_index);
    if (status & RECORD_NOT_FOUND)
        return std::make_pair(RECORD_NOT_FOUND, std::vector<uint8_t>());

    auto entry = static_cast<size_t>(cyl) * m_heads + head;
    auto [sector, data] = m_tracks[entry][sector_index];

    auto data_size = SizeFromSizeCode(sector.size);
    data.resize(data_size);

    status = 0;
    if (sector.status2 & ST2_765_DATA_NOT_FOUND) status |= RECORD_NOT_FOUND;
    if (sector.status2 & ST2_765_CRC_ERROR) { status |= CRC_ERROR; }
    if (sector.status2 & ST2_765_CONTROL_MARK)   status |= DELETED_DATA;

    return std::make_pair(status, std::move(data));
}

uint8_t EDSKDisk::WriteData(uint8_t cyl, uint8_t head, uint8_t sector_index, const std::vector<uint8_t>& data)
{
    auto [status, id] = GetSector(cyl, head, sector_index);
    if (status & RECORD_NOT_FOUND)
        return RECORD_NOT_FOUND;

    if (WriteProtected())
        return WRITE_PROTECT;

    auto entry = static_cast<size_t>(cyl) * m_heads + head;
    auto& [sector, sector_data] = m_tracks[entry][sector_index];

    auto data_size = SizeFromSizeCode(sector.size);
    if (data.size() != data_size)
        return RECORD_NOT_FOUND;

    sector_data = data;
    m_modified = true;
    sector.status1 &= ~ST1_765_CRC_ERROR;
    sector.status2 &= ~ST2_765_CRC_ERROR;

    return 0;
}

bool EDSKDisk::Save()
{
    EDSK_HEADER eh{};
    EDSK_SIGNATURE.copy(&eh.signature[0], sizeof(eh.signature));
    EDSK_SIMCOUPE_CREATOR.copy(&eh.creator[0], sizeof(eh.creator));
    eh.cyls = m_cyls;
    eh.heads = m_heads;

    for (auto cyl = 0; cyl < m_cyls; cyl++)
    {
        for (auto head = 0; head < m_heads; head++)
        {
            auto entry = static_cast<size_t>(cyl) * m_heads + head;
            auto& track = m_tracks[entry];
            auto track_size = sizeof(EDSK_TRACK);
            track_size += std::accumulate(track.begin(), track.end(), size_t{ 0 },
                [](size_t val, auto& s) { return val + s.second.size(); });
            eh.size_msbs[entry] = static_cast<uint8_t>((track_size + 0xff) >> 8);
        }
    }

    bool saved = m_stream->Rewind() && m_stream->Write(&eh, sizeof(eh)) == sizeof(eh);

    for (auto cyl = 0; cyl < m_cyls; cyl++)
    {
        for (auto head = 0; head < m_heads; head++)
        {
            auto entry = static_cast<size_t>(cyl) * m_heads + head;
            auto& track = m_tracks[entry];

            EDSK_TRACK et{};
            EDSK_TRACK_SIGNATURE.copy(et.signature, EDSK_TRACK_SIGNATURE.size());
            et.cyl = cyl;
            et.head = head;
            et.size = 2;
            et.sectors = static_cast<uint8_t>(track.size());
            et.size = 2;
            et.gap3 = 0x4e;
            et.fill = 0x00;

            saved &= m_stream->Write(&et, sizeof(et)) == sizeof(et);

            for (auto& [sector, data] : track)
                saved &= m_stream->Write(&sector, sizeof(sector)) == sizeof(sector);

            auto slack_size = (0x100 - sizeof(EDSK_TRACK) - sizeof(EDSK_SECTOR) * et.sectors) & 0xff;
            std::vector<uint8_t> slack(slack_size);
            saved &= m_stream->Write(slack.data(), slack.size()) == slack.size();

            for (auto& [sector, data] : track)
                saved &= m_stream->Write(data.data(), data.size()) == data.size();
        }
    }

    m_stream->Close();
    m_last_write_time = m_stream->LastWriteTime();
    m_modified = false;
    return saved;
}

uint8_t EDSKDisk::FormatTrack(uint8_t cyl, uint8_t head,
    const std::vector<std::pair<IDFIELD, std::vector<uint8_t>>>& sectors)
{
    if (WriteProtected() || sectors.size() > EDSK_MAX_SECTORS)
        return WRITE_PROTECT;

    auto entry = static_cast<size_t>(cyl) * m_heads + head;
    if (entry >= m_tracks.size())
        return WRITE_PROTECT;

    auto& track = m_tracks[entry];
    track.clear();

    for (auto& [id, data] : sectors)
    {
        EDSK_SECTOR es{};
        es.cyl = id.cyl;
        es.head = id.head;
        es.sector = id.sector;
        es.size = id.size;
        es.data_low = static_cast<uint8_t>(data.size() & 0xff);
        es.data_high = static_cast<uint8_t>(data.size() >> 8);

        track.push_back(std::make_pair(es, data));
    }

    m_modified = true;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

bool FileDisk::IsRecognised(Stream& stream)
{
    return stream.GetSize() <= MAX_SAM_FILE_SIZE;
}

FileDisk::FileDisk(std::unique_ptr<Stream> stream)
    : Disk(std::move(stream), DiskType::File)
{
    if (auto file_size = m_stream->GetSize())
    {
        m_data.resize(DISK_FILE_HEADER_SIZE + file_size);

        m_data[0] = 19;                         // CODE file type
        m_data[1] = file_size & 0xff;           // LSB of size mod 16384
        m_data[2] = (file_size >> 8) & 0x3f;    // MSB of size mod 16384
        m_data[3] = 0x00;                       // LSB of offset start
        m_data[4] = 0x80;                       // MSB of offset start
        m_data[5] = 0xff;                       // unused
        m_data[6] = 0xff;                       // unused
        m_data[7] = (file_size >> 14) & 0x1f;   // page count (size div 16384)
        m_data[8] = 0x01;                       // first page

        m_stream->Rewind();
        m_stream->Read(m_data.data() + DISK_FILE_HEADER_SIZE, file_size);
        m_stream->Close();
    }
}

std::pair<uint8_t, IDFIELD>
FileDisk::GetSector(uint8_t cyl, uint8_t head, uint8_t sector_index)
{
    if (cyl >= MGT_DISK_CYLS || head >= MGT_DISK_HEADS || sector_index >= MGT_DISK_SECTORS)
        return std::make_pair(RECORD_NOT_FOUND, IDFIELD{});

    return Disk::GetSector(cyl, head, sector_index);
}

std::pair<uint8_t, std::vector<uint8_t>>
FileDisk::ReadData(uint8_t cyl, uint8_t head, uint8_t sector_index)
{
    std::vector<uint8_t> data(NORMAL_SECTOR_SIZE);

    // The first directory sector?
    if (cyl == 0 && head == 0 && sector_index == 0)
    {
        // file type
        data[0] = m_data[0];

        // Use a fixed filename, starting with "auto" so SimCoupe's embedded DOS boots it
        const std::string filename = "autoExec  ";
        std::copy(filename.begin(), filename.end(), data.begin() + 1);

        auto num_sectors = (m_data.size() + NORMAL_SECTOR_SIZE - 3) / (NORMAL_SECTOR_SIZE - 2);
        data[11] = static_cast<uint8_t>(num_sectors >> 8);
        data[12] = static_cast<uint8_t>(num_sectors & 0xff);

        // Starting track and sector
        data[13] = MGT_DIRECTORY_TRACKS;
        data[14] = 1;

        // Sector address map
        std::fill(data.begin() + 15, data.begin() + 15 + num_sectors / 8, 0xff);
        if (num_sectors & 7)
            data[15 + (num_sectors / 8)] = (1U << (num_sectors & 7)) - 1;

        // Starting page number and offset
        data[236] = m_data[8];
        data[237] = m_data[3];
        data[238] = m_data[4];

        // Size in pages and mod 16384
        data[239] = m_data[7];
        data[240] = m_data[1];
        data[241] = m_data[2];

        // Auto-execute code with normal paging (see PDPSUBR in ROM0 for details)
        data[242] = 2;
        data[243] = m_data[3];
        data[244] = m_data[4];
    }
    else if (cyl >= MGT_DIRECTORY_TRACKS)
    {
        auto offset = (head * MGT_DISK_CYLS + cyl - MGT_DIRECTORY_TRACKS) *
            (MGT_DISK_SECTORS * (NORMAL_SECTOR_SIZE - 2)) +
            (sector_index * (NORMAL_SECTOR_SIZE - 2));
        auto size = std::min(NORMAL_SECTOR_SIZE - 2, m_data.size() - offset);
        std::copy(m_data.begin() + offset, m_data.begin() + offset + size, data.begin());

        if (offset + NORMAL_SECTOR_SIZE < m_data.size())
        {
            auto next_sector = 1 + ((sector_index + 1) % MGT_DISK_SECTORS);
            auto next_cyl = ((next_sector == 1) ? cyl + 1 : cyl) % MGT_DISK_CYLS;
            auto next_head = ((next_cyl == 0) ? head + 1 : head) % MGT_DISK_HEADS;

            data[NORMAL_SECTOR_SIZE - 2] = next_cyl + (next_head ? 0x80 : 0x00);
            data[NORMAL_SECTOR_SIZE - 1] = next_sector;
        }
    }

    return std::make_pair(0, std::move(data));
}

uint8_t FileDisk::WriteData(uint8_t, uint8_t, uint8_t, const std::vector<uint8_t>&)
{
    return WRITE_PROTECT;
}

bool FileDisk::Save()
{
    return false;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

bool FloppyDisk::IsRecognised(Stream& stream)
{
    return FloppyStream::IsRecognised(stream.GetPath());
}

FloppyDisk::FloppyDisk(std::unique_ptr<Stream> stream)
    : Disk(std::move(stream), DiskType::Floppy),
    m_track(std::make_shared<TRACK>())
{
    m_track->head = 0xff;   // invalidate cache
}

std::pair<uint8_t, IDFIELD>
FloppyDisk::GetSector(uint8_t cyl, uint8_t head, uint8_t sector_index)
{
    if (cyl != m_track->cyl || head != m_track->head || sector_index >= m_track->sectors.size())
        return std::make_pair(RECORD_NOT_FOUND, IDFIELD{});

    auto& sector = m_track->sectors[sector_index];

    IDFIELD id{};
    id.head = sector.head;
    id.cyl = sector.cyl;
    id.sector = sector.sector;
    id.size = sector.size;

    auto crc = CrcBlock("\xa1\xa1\xa1\xfe", 4);
    crc = CrcBlock(&id, 4, crc);
    if (sector.status & CRC_ERROR)
        crc ^= 0x5555;
    id.crc1 = crc >> 8;
    id.crc2 = crc & 0xff;

    return std::make_pair(sector.status, id);
}

std::pair<uint8_t, std::vector<uint8_t>>
FloppyDisk::ReadData(uint8_t cyl, uint8_t head, uint8_t sector_index)
{
    if (cyl != m_track->cyl || head != m_track->head || sector_index >= m_track->sectors.size())
        return std::make_pair(RECORD_NOT_FOUND, std::vector<uint8_t>());

    auto& sector = m_track->sectors[sector_index];
    return std::make_pair(sector.status, sector.data);
}

uint8_t FloppyDisk::WriteData(uint8_t cyl, uint8_t head, uint8_t sector_index, const std::vector<uint8_t>& data)
{
    if (cyl != m_track->cyl || head != m_track->head || sector_index >= m_track->sectors.size())
        return RECORD_NOT_FOUND;

    auto& sector = m_track->sectors[sector_index];
    auto data_size = SizeFromSizeCode(sector.size);
    if (data.size() != data_size)
        return RECORD_NOT_FOUND;

    sector.data = data;
    m_modified = true;
    sector.status &= ~CRC_ERROR;

    auto& floppy = reinterpret_cast<FloppyStream&>(*m_stream);
    floppy.StartCommand(WRITE_1SECTOR, m_track, sector_index);
    return BUSY;
}

void FloppyDisk::Close()
{
    Disk::Close();
    m_track->head = 0xff;   // invalidate cache
}

bool FloppyDisk::Save()
{
    m_last_write_time = m_stream->LastWriteTime();
    m_modified = false;
    return true;
}

uint8_t FloppyDisk::FormatTrack(uint8_t cyl, uint8_t head,
    const std::vector<std::pair<IDFIELD, std::vector<uint8_t>>>& sectors)
{
    if (WriteProtected() || cyl >= MAX_DISK_TRACKS)
        return WRITE_PROTECT;

    // For now, require all sectors to be the same size
    auto size0 = sectors.empty() ? 0 : sectors[0].first.size;
    if (std::any_of(sectors.begin(), sectors.end(),
        [=](auto& s) { return s.first.size != size0; }))
    {
        return WRITE_PROTECT;
    }

    m_track->cyl = cyl;
    m_track->head = head;
    m_track->sectors.clear();

    for (auto& [id, data] : sectors)
    {
        SECTOR sector{};
        sector.cyl = id.cyl;
        sector.head = id.head;
        sector.sector = id.sector;
        sector.size = id.size;
        sector.status = 0;
        sector.data = data;

        m_track->sectors.push_back(std::move(sector));
    }

    auto& floppy = reinterpret_cast<FloppyStream&>(*m_stream);
    floppy.StartCommand(WRITE_TRACK, m_track);
    return BUSY;
}

uint8_t FloppyDisk::LoadTrack(uint8_t cyl, uint8_t head)
{
    if (cyl == m_track->cyl && head == m_track->head)
        return 0;

    m_track->sectors.clear();
    m_track->cyl = cyl;
    m_track->head = head;

    auto& floppy = reinterpret_cast<FloppyStream&>(*m_stream);
    floppy.StartCommand(READ_MSECTOR, m_track);
    return BUSY;
}

bool FloppyDisk::IsBusy(uint8_t& status, bool wait)
{
    auto& floppy = reinterpret_cast<FloppyStream&>(*m_stream);
    auto busy = floppy.IsBusy(status, wait);

    if (!busy && status)
        m_track->head = 0xff;   // invalidate cache after errors

    return busy;
}

#endif // _WIN32
