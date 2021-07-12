// Part of SimCoupe - A SAM Coupe emulator
//
// Drive.cpp: VL1772-02 floppy disk controller emulation
//
//  Copyright (c) 1999-2014 Simon Owen
//  Copyright (c) 1996-2001 Allan Skillman
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

// ToDo:
//  - real delayed spin-up (including 'hang' when command sent and no disk present)
//  - data timeouts for type 2 commands

#include "SimCoupe.h"
#include "Drive.h"

////////////////////////////////////////////////////////////////////////////////

Drive::Drive()
{
    Reset();
}

void Drive::Reset()
{
    m_regs.command = 0;
    m_regs.status = 0;
    m_regs.cyl = 0xff;
    m_regs.sector = 1;
    m_regs.data = 0;
    m_regs.dir_out = false;

    m_buffer_pos = 0;
    m_data_status = 0;
    m_sector_index = 0;
    m_head = 0;
}

bool Drive::Insert(const std::string& disk_path)
{
    Eject();

    if (disk_path.empty())
        return true;

    m_disk = Disk::Open(disk_path);
    return m_disk != nullptr;
}

bool Drive::Insert(const std::vector<uint8_t>& mem_file)
{
    Eject();
    m_disk = Disk::Open(mem_file, "<internal>");
    return m_disk != nullptr;
}

void Drive::Eject()
{
    if (m_disk)
    {
        m_disk->Close();
        m_disk.reset();
    }
}

void Drive::Flush()
{
    if (m_disk)
        m_disk->Close();
}

void Drive::FrameEnd()
{
    DiskDevice::FrameEnd();

    if (m_motor_off_frames && !--m_motor_off_frames)
    {
        m_regs.status &= ~MOTOR_ON;
        Flush();
    }
}

void Drive::ModifyStatus(uint8_t set_bits, uint8_t reset_bits)
{
    if (set_bits & MOTOR_ON)
    {
        m_motor_off_frames = FLOPPY_MOTOR_TIMEOUT;

        if (!(m_regs.status & MOTOR_ON) && m_disk && m_disk->StreamChanged())
            Insert(m_disk->GetPath());
    }

    m_regs.status |= set_bits;
    m_regs.status &= ~reset_bits;
}

void Drive::ModifyReadStatus()
{
    if (m_data_status & ~CRC_ERROR)
        ModifyStatus(m_data_status, BUSY);
    else
        ModifyStatus(DRQ, 0);
}

void Drive::ExecuteNext()
{
    if (!m_disk)
        return;

    uint8_t status{};
    if (m_disk->IsBusy(status))
    {
        ModifyStatus(MOTOR_ON, 0);
        return;
    }

    switch (m_regs.command & FDC_COMMAND_MASK)
    {
    case READ_1SECTOR:
    case READ_MSECTOR:
    {
        if (auto id = FindSector())
        {
            auto [status, data] = ReadSector();
            m_data_status = status;
            m_buffer = std::move(data);
            m_buffer_pos = 0;
            ModifyReadStatus();

            // Tweak MNEMOdemo1 boot sector to remove SimCoupe warning.
            if (m_regs.cyl == 4 && m_regs.sector == 1 &&
                m_buffer[0x016] == 0xC3 && CrcBlock(m_buffer.data(), m_buffer.size()) == 0x6c54)
            {
                m_buffer[0x016] -= 0x37;
            }
        }
        else
        {
            ModifyStatus(RECORD_NOT_FOUND, BUSY);
        }
        break;
    }

    case WRITE_1SECTOR:
    case WRITE_MSECTOR:
    {
        if (m_state == 0)
        {
            if (auto id = FindSector())
            {
                if (m_disk->WriteProtected())
                {
                    ModifyStatus(WRITE_PROTECT, BUSY);
                }
                else
                {
                    m_buffer.resize(SizeFromSizeCode(id->size));
                    m_buffer_pos = 0;

                    ModifyStatus(DRQ, 0);
                    m_state++;
                }
            }
            else
            {
                ModifyStatus(RECORD_NOT_FOUND, BUSY);
            }
        }
        else
        {
            ModifyStatus(status, BUSY);
        }

        break;
    }

    case READ_ADDRESS:
    {
        auto [status, id] = ReadAddress();

        if (!(status & TYPE23_ERROR_MASK))
        {
            m_regs.sector = id.cyl;

            m_buffer = { id.cyl, id.head, id.sector, id.size, id.crc1, id.crc2 };
            m_buffer_pos = 0;
            ModifyStatus(DRQ, 0);
        }
        else
        {
            ModifyStatus(status, BUSY);
            m_buffer_pos = 0;
        }
        break;
    }

    case READ_TRACK:
        m_buffer = ReadTrack();
        m_buffer_pos = 0;
        ModifyStatus(DRQ, 0);
        break;

    case WRITE_TRACK:
        ModifyStatus(status, BUSY);
        break;
    }
}

uint8_t Drive::In(uint16_t port)
{
    if ((m_regs.status & (BUSY | DRQ)) == BUSY)
        ExecuteNext();

    switch (static_cast<VLReg>(port & 0x03))
    {
    default:
    case VLReg::status:
    {
        auto status = m_regs.status;

        if ((m_regs.command & FDC_COMMAND_MASK) <= STEP_OUT_UPD)
        {
            if (m_cyl == 0)
            {
                status |= TRACK00;
                m_regs.cyl = 0;
            }

            if (m_disk)
            {
                if (m_disk->WriteProtected())
                    status |= WRITE_PROTECT;

                if (!(m_regs.command & CMD_FLAG_SPINUP))
                    status |= SPIN_UP;

                // Toggle the index pulse periodically to show the disk is spinning.
                // TODO: convert to an event?
                static auto status_reads = 0U;
                if ((m_regs.status & MOTOR_ON) && !(++status_reads % 1024U))
                    status |= INDEX_PULSE;
            }

            return status;
        }

        // Fail after 16 polls of the status port with no data reads.
        // SAM DICE relies uses this timeout as a synchronisation mechanism.
        if ((m_regs.status & DRQ) && ++m_status_reads_with_data == 0x10)
        {
            ModifyStatus(LOST_DATA, BUSY | DRQ);
            m_sector_index = 0;
        }

        return m_regs.status;
    }

    case VLReg::track:
        return m_regs.cyl;

    case VLReg::sector:
        return m_regs.sector;

    case VLReg::data:
        if ((m_regs.status & DRQ) && m_buffer_pos < m_buffer.size())
        {
            m_regs.data = m_buffer[m_buffer_pos++];
            m_status_reads_with_data = 0;

            if (m_buffer_pos == m_buffer.size())
            {
                ModifyStatus(0, BUSY | DRQ);

                switch (m_regs.command & FDC_COMMAND_MASK)
                {
                case READ_ADDRESS:
                    break;

                case READ_TRACK:
                    ModifyStatus(RECORD_NOT_FOUND, 0);
                    break;

                case READ_1SECTOR:
                    ModifyStatus(m_data_status, 0);
                    break;

                case READ_MSECTOR:
                    ModifyStatus(m_data_status, 0);
                    if (!m_data_status)
                    {
                        m_regs.sector++;

                        if (auto id = FindSector())
                        {
                            TRACE("FDC: Multiple-sector read moving to sector {}\n", id->sector);

                            auto [status, data] = ReadSector();
                            m_data_status = status;
                            m_buffer_pos = 0;
                            ModifyReadStatus();
                        }
                    }
                    break;

                default:
                    TRACE("Data requested for unknown command ({})!\n", m_regs.command);
                    break;
                }
            }
        }

        return m_regs.data;
    }
}

void Drive::Out(uint16_t port, uint8_t val)
{
    m_head = ((port) >> 2) & 1;

    switch (static_cast<VLReg>(port & 0x03))
    {
    case VLReg::command:
    {
        m_regs.command = val;
        auto command = m_regs.command & FDC_COMMAND_MASK;

        if ((m_regs.status & BUSY) && (command != FORCE_INTERRUPT))
            return;

        m_uActive = FLOPPY_ACTIVE_FRAMES;

        m_regs.status &= MOTOR_ON;
        ModifyStatus(MOTOR_ON, 0);
        m_state = 0;

        switch (command)
        {
        case RESTORE:
            TRACE("FDC: RESTORE\n");
            m_regs.cyl = m_cyl = 0;
            break;

        case SEEK:
            TRACE("FDC: SEEK to track {}\n", m_regs.data);
            m_regs.dir_out = (m_regs.data > m_regs.cyl);
            m_regs.cyl = m_cyl = m_regs.data;
            break;

        case STEP_UPD:
        case STEP_NUPD:
        case STEP_IN_UPD:
        case STEP_IN_NUPD:
        case STEP_OUT_UPD:
        case STEP_OUT_NUPD:
            if (m_regs.command & CMD_FLAG_STEPDIR)
                m_regs.dir_out = (m_regs.command & CMD_FLAG_DIR) != 0;

            if (!m_regs.dir_out)
                m_cyl++;
            else if (m_cyl > 0)
                m_cyl--;

            if (m_regs.command & CMD_FLAG_UPDATE)
                m_regs.cyl = m_cyl;
            break;

        case READ_1SECTOR:
        case READ_MSECTOR:
            TRACE("FDC: READ_xSECTOR (cyl {} head {} sector {})\n", m_cyl, m_head, m_regs.sector);
            ModifyStatus(BUSY, 0);
            if (m_disk)
                m_disk->LoadTrack(m_cyl, m_head);
            break;

        case WRITE_1SECTOR:
        case WRITE_MSECTOR:
            TRACE("FDC: WRITE_xSECTOR (cyl {} head {} sector {})\n", m_cyl, m_head, m_regs.sector);
            ModifyStatus(BUSY, 0);
            if (m_disk)
                m_disk->LoadTrack(m_cyl, m_head);
            break;

        case READ_ADDRESS:
            TRACE("FDC: READ_ADDRESS (cyl {} head {})\n", m_cyl, m_head);
            ModifyStatus(BUSY, 0);
            if (m_disk)
                m_disk->LoadTrack(m_cyl, m_head);
            break;

        case READ_TRACK:
            TRACE("FDC: READ_TRACK\n");
            ModifyStatus(BUSY, 0);
            if (m_disk)
                m_disk->LoadTrack(m_cyl, m_head);
            break;

        case WRITE_TRACK:
            TRACE("FDC: WRITE_TRACK\n");
            if (m_disk)
            {
                if (m_disk->WriteProtected())
                    ModifyStatus(WRITE_PROTECT, 0);
                else
                {
                    m_buffer.resize(MAX_TRACK_SIZE);
                    m_buffer_pos = 0;
                    ModifyStatus(BUSY | DRQ, 0);
                }
            }
            break;

        case FORCE_INTERRUPT:
            TRACE("FDC: FORCE_INTERRUPT\n");

            if (m_disk)
            {
                uint8_t status{};
                m_disk->IsBusy(status, true);
            }

            m_regs.status &= MOTOR_ON;
            ModifyStatus(MOTOR_ON, 0);

            m_regs.command = 0;
            m_buffer_pos = 0;
            break;
        }
    }
    break;

    case VLReg::track:
        if (!(m_regs.status & BUSY))
            m_regs.cyl = val;
        break;

    case VLReg::sector:
        if (!(m_regs.status & BUSY))
            m_regs.sector = val;
        break;

    case VLReg::data:
    {
        m_regs.data = val;

        if ((m_regs.status & DRQ) && m_buffer_pos < m_buffer.size())
        {
            m_buffer[m_buffer_pos++] = val;

            if (m_buffer_pos == m_buffer.size())
            {
                ModifyStatus(0, BUSY | DRQ);

                switch (m_regs.command & FDC_COMMAND_MASK)
                {
                case WRITE_1SECTOR:
                case WRITE_MSECTOR:
                {
                    auto status = WriteSector(m_buffer);
                    ModifyStatus(status, 0);

                    if (m_regs.command & CMD_FLAG_MULTIPLE)
                    {
                        m_regs.sector++;

                        if (auto id = FindSector())
                        {
                            TRACE("FDC: Multiple-sector writing moving to sector {}\n", id->sector);

                            m_buffer.resize(SizeFromSizeCode(id->size));
                            m_buffer_pos = 0;
                            ModifyStatus(DRQ, 0);
                        }
                    }
                    break;
                }
                break;

                case WRITE_TRACK:
                {
                    auto status = WriteTrack(m_buffer);
                    ModifyStatus(status, 0);
                }
                break;

                default:
                    TRACE("!!! Unexpected data arrived!\n");
                    break;
                }
            }
        }
    }
    }
}

std::pair<uint8_t, IDFIELD>
Drive::GetSector(uint8_t index)
{
    if (m_disk)
        return m_disk->GetSector(m_cyl, m_head, index);

    return std::make_pair(RECORD_NOT_FOUND, IDFIELD{});
}

std::optional<IDFIELD>
Drive::FindSector()
{
    m_sector_index = 0;

    for (auto index_count = 0; index_count < 2; )
    {
        auto [status, id] = m_disk->GetSector(m_cyl, m_head, m_sector_index);
        if (status & RECORD_NOT_FOUND)
        {
            index_count++;
            m_sector_index = 0;
            continue;
        }

        if (id.cyl == m_regs.cyl && id.sector == m_regs.sector)
            return id;

        m_sector_index++;
    }

    return std::nullopt;
}

std::pair<uint8_t, std::vector<uint8_t>>
Drive::ReadSector()
{
    return m_disk->ReadData(m_cyl, m_head, m_sector_index);
}

uint8_t Drive::WriteSector(const std::vector<uint8_t>& data)
{
    return m_disk->WriteData(m_cyl, m_head, m_sector_index, data);
}

std::pair<uint8_t, IDFIELD>
Drive::ReadAddress()
{
    auto [status, id] = m_disk->GetSector(m_cyl, m_head, m_sector_index++);
    if (!(status & RECORD_NOT_FOUND))
        return std::make_pair(status, id);

    m_sector_index = 0;
    auto [status2, id2] = m_disk->GetSector(m_cyl, m_head, m_sector_index++);
    return std::make_pair(status2, id2);
}

static void AddBytes(std::vector<uint8_t>& data, uint8_t val, int count = 1)
{
    if (count > 0)
        data.insert(data.end(), count, val);
}

std::vector<uint8_t> Drive::ReadTrack()
{
    std::vector<uint8_t> track_data;
    track_data.reserve(MAX_TRACK_SIZE);

    m_sector_index = 0;

    // Gap 4a
    AddBytes(track_data, 0x4e, 32);

    for (;;)
    {
        auto [status, id] = GetSector(m_sector_index++);
        if (status & RECORD_NOT_FOUND)
            break;

        // Gap 1/3 and sync
        AddBytes(track_data, 0x4e, 22);
        AddBytes(track_data, 0x00, 12);

        // IDAM
        AddBytes(track_data, 0xa1, 3);
        AddBytes(track_data, 0xfe);

        // Sector ID header
        AddBytes(track_data, id.cyl);
        AddBytes(track_data, id.head);
        AddBytes(track_data, id.sector);
        AddBytes(track_data, id.size);
        AddBytes(track_data, id.crc1);
        AddBytes(track_data, id.crc2);

        // Gap 2 and sync
        AddBytes(track_data, 0x4e, 22);
        AddBytes(track_data, 0x00, 8);

        if (!(status & CRC_ERROR))
        {
            auto data_start = track_data.size();
            auto [status, sector_data] = ReadSector();

            // DAM
            AddBytes(track_data, 0xa1, 3);
            AddBytes(track_data, (status & DELETED_DATA) ? 0xf8 : 0xfb);

            track_data.insert(track_data.end(), sector_data.begin(), sector_data.end());

            auto crc = CrcBlock(track_data.data() + data_start, track_data.size() - data_start);
            crc ^= (status & CRC_ERROR);
            AddBytes(track_data, crc >> 8);
            AddBytes(track_data, crc & 0xff);
        }
    }

    // Gap 4b
    track_data.resize(MAX_TRACK_SIZE, 0x4e);

    return track_data;
}

uint8_t Drive::VerifyTrack()
{
    auto [status, id] = ReadAddress();

    if (id.cyl != m_cyl)
        status |= RECORD_NOT_FOUND;

    return status;
}

static bool ExpectBlock(const uint8_t*& pb, const uint8_t* end_ptr, uint8_t val, int min_count, int max_count = INT_MAX)
{
    for (; pb < end_ptr && *pb == val && max_count; pb++, min_count--, max_count--);
    return (min_count <= 0 && max_count >= 0);
}

uint8_t Drive::WriteTrack(const std::vector<uint8_t>& data)
{
    if (!m_disk)
        return WRITE_PROTECT;

    std::vector<std::pair<IDFIELD, std::vector<uint8_t>>> sectors;

    auto end_ptr = data.data() + data.size();
    auto pb = reinterpret_cast<const uint8_t*>(memchr(data.data(), 0x4e, data.size()));

    // Look for Gap 1 and track header (min 32 bytes of 0x4e)
    if (pb && ExpectBlock(pb, end_ptr, 0x4e, 32))
    {
        while (pb < end_ptr)
        {
            bool valid = true;
            IDFIELD id{};
            std::vector<uint8_t> data;

            // Gap 1 sync
            valid &= ExpectBlock(pb, end_ptr, 0x00, 12, 12);

            // IDAM
            valid &= ExpectBlock(pb, end_ptr, 0xf5, 3, 3);
            valid &= ExpectBlock(pb, end_ptr, 0xfe, 1, 1);

            valid &= (pb + sizeof(IDFIELD) <= end_ptr);
            if (valid)
            {
                id.cyl = pb[0];
                id.head = pb[1];
                id.sector = pb[2];
                id.size = pb[3];
                pb += 4;
            }

            // CRC generation
            valid &= ExpectBlock(pb, end_ptr, 0xf7, 1, 1);

            // Gap 2
            valid &= ExpectBlock(pb, end_ptr, 0x4e, 22);
            valid &= ExpectBlock(pb, end_ptr, 0x00, 8);

            // DAM
            valid &= ExpectBlock(pb, end_ptr, 0xf5, 3, 3);
            valid &= (ExpectBlock(pb, end_ptr, 0xfb, 1, 1) || ExpectBlock(pb, end_ptr, 0xf8, 1, 1));

            auto sector_size = SizeFromSizeCode(id.size);
            valid &= (pb + sector_size < end_ptr);
            if (valid)
            {
                data = std::vector<uint8_t>(pb, pb + sector_size);
                pb += sector_size;
            }

            // CRC generation
            valid &= ExpectBlock(pb, end_ptr, 0xf7, 1, 1);

            // Gap 4
            valid &= ExpectBlock(pb, end_ptr, 0x4e, 16);

            sectors.push_back(std::make_pair(id, std::move(data)));
        }
    }

    return m_disk->FormatTrack(m_cyl, m_head, sectors);
}
