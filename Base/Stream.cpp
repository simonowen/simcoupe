// Part of SimCoupe - A SAM Coupe emulator
//
// Stream.cpp: Data stream abstraction classes
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
#include "Stream.h"

#include "Disk.h"

////////////////////////////////////////////////////////////////////////////////

Stream::Stream(const std::string& filepath, bool read_only)
    : m_read_only(read_only)
{
    m_path = filepath;
    m_short_name = m_path.filename();
}

std::unique_ptr<Stream>
Stream::Open(const std::string& file_path, bool read_only)
{
    if (file_path.empty())
        return nullptr;

#ifdef _WIN32
    if (FloppyStream::IsRecognised(file_path))
        return std::make_unique<FloppyStream>(file_path, read_only);
#endif

    unique_FILE file = fopen(file_path.c_str(), "r+b");
    read_only |= !file;
    file.reset();

#ifdef HAVE_LIBZ
    if (auto hfZip = unzOpen(file_path.c_str()))
    {
        std::regex regex_types(R"(\.(dsk|sad|mgt|sbt|cpm)$)", std::regex::extended | std::regex::icase);

        for (auto ret = unzGoToFirstFile(hfZip); ret == UNZ_OK; ret = unzGoToNextFile(hfZip))
        {
            unz_file_info info{};
            std::array<char, MAX_PATH> file_name{};
            unzGetCurrentFileInfo(hfZip, &info, file_name.data(), MAX_PATH, nullptr, 0, nullptr, 0);

            if (std::regex_search(file_name.data(), regex_types) && unzOpenCurrentFile(hfZip) == UNZ_OK)
                return std::make_unique<ZipStream>(hfZip, file_path, file_name.data());
        }

        unzClose(hfZip);
    }
    else
#endif
    {
        if (unique_FILE file = fopen(file_path.c_str(), "rb"))
        {
#ifdef HAVE_LIBZ
            std::array<uint8_t, 2> sig;
            if ((fread(sig.data(), 1, sig.size(), file) != sig.size()) || sig != GZ_SIGNATURE)
#endif
                return std::make_unique<FileStream>(std::move(file), file_path, read_only);
#ifdef HAVE_LIBZ
            else
            {
                std::array<uint8_t, 4> size_le{};
                size_t file_size{};

                if (fseek(file, -4, SEEK_END) == 0 &&
                    fread(size_le.data(), 1, size_le.size(), file) == size_le.size())
                {
                    file_size = (size_le[3] << 24) | (size_le[2] << 16) | (size_le[1] << 8) | size_le[0];
                }

                file.reset();

                if (auto hfile = gzopen(file_path.c_str(), "rb"))
                    return std::make_unique<ZLibStream>(hfile, file_path, file_size, read_only);
            }
#endif  // HAVE_LIBZ
        }
    }

    return nullptr;
}

fs::file_time_type
Stream::LastWriteTime() const
{
    std::error_code ec{};
    auto file_time = fs::last_write_time(m_path, ec);
    return ec ? fs::file_time_type{} : file_time;
}

////////////////////////////////////////////////////////////////////////////////

FileStream::FileStream(unique_FILE&& file, const std::string& filepath, bool read_only)
    : Stream(filepath, read_only), m_file(std::move(file))
{
    if (!m_file)
        Write("", 0);
}

size_t FileStream::GetSize()
{
    std::error_code ec;
    auto file_size = static_cast<size_t>(fs::file_size(m_path, ec));
    return ec ? 0U : file_size;
}

void FileStream::Close()
{
    m_file.reset();
    m_mode = FileMode::Closed;
}

bool FileStream::Rewind()
{
    return m_file && fseek(m_file, 0, SEEK_SET) == 0;
}

size_t FileStream::Read(void* buffer, size_t len)
{
    if (m_mode != FileMode::Reading)
    {
        m_file.reset();
        m_file = fopen(m_path.c_str(), "rb");
        m_mode = FileMode::Reading;
    }

    return m_file ? fread(buffer, 1, len, m_file) : 0U;
}

size_t FileStream::Write(const void* buffer, size_t len)
{
    if (m_mode != FileMode::Writing)
    {
        m_file.reset();
        m_file = fopen(m_path.c_str(), "wb");
        m_mode = FileMode::Writing;
    }

    return m_file ? fwrite(buffer, 1, len, m_file) : 0U;
}

////////////////////////////////////////////////////////////////////////////////

MemStream::MemStream(const std::vector<uint8_t>& file_data)
    : Stream("<memory>", true)
{
    m_data = file_data;
}

void MemStream::Close()
{
    m_mode = FileMode::Closed;
}

size_t MemStream::GetSize()
{
    return m_data.size();
}

bool MemStream::Rewind()
{
    m_pos = 0;
    return true;
}

size_t MemStream::Read(void* buffer, size_t len)
{
    if (m_mode != FileMode::Reading)
    {
        m_mode = FileMode::Reading;
        m_pos = 0;
    }

    auto avail = std::min(m_data.size() - m_pos, len);
    memcpy(buffer, m_data.data() + m_pos, avail);
    m_pos += avail;
    return avail;
}

size_t MemStream::Write(const void*, size_t)
{
    m_mode = FileMode::Writing;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef HAVE_LIBZ

ZLibStream::ZLibStream(gzFile file, const std::string& filepath, size_t file_size, bool read_only)
    : Stream(filepath, read_only), m_file(file), m_size(file_size)
{
    if (!m_file)
        Write("", 0);

    m_short_name += " (gzip)";
}

void ZLibStream::Close()
{
    m_file.reset();
}

size_t ZLibStream::GetSize()
{
    return m_file ? m_size : 0U;
}

bool ZLibStream::Rewind()
{
    if (m_file && m_mode == FileMode::Reading)
        return !gzrewind(m_file);

    return m_file != nullptr;
}

size_t ZLibStream::Read(void* buffer, size_t len)
{
    if (m_mode != FileMode::Reading)
    {
        Close();
        m_file = gzopen(m_path.c_str(), "rb");
        m_mode = FileMode::Reading;
    }

    auto avail = m_file ? gzread(m_file, buffer, static_cast<unsigned>(len)) : 0;
    return (avail < 0) ? 0 : static_cast<size_t>(avail);
}

size_t ZLibStream::Write(const void* buffer, size_t len)
{
    if (m_mode != FileMode::Writing)
    {
        Close();
        m_file = gzopen(m_path.c_str(), "wb9");
        m_mode = FileMode::Writing;
    }

    auto written = m_file ? gzwrite(m_file, buffer, static_cast<unsigned>(len)) : 0;
    return (written < 0) ? 0U : static_cast<size_t>(written);
}

////////////////////////////////////////////////////////////////////////////////

ZipStream::ZipStream(unzFile file, const std::string& filepath, const std::string& filename)
    : Stream(filepath, true), m_file(file)
{
    m_short_name = filename + " (zip)";

    unz_file_info info{};
    unzGetCurrentFileInfo(m_file, &info, nullptr, 0, nullptr, 0, nullptr, 0);
    m_size = info.uncompressed_size;
}

void ZipStream::Close()
{
    // We can't easily re-open the zip container so leave the current file open.
}

size_t ZipStream::GetSize()
{
    return m_size;
}

bool ZipStream::Rewind()
{
    return unzSetOffset(m_file, 0) == UNZ_OK;
}

size_t ZipStream::Read(void* buffer, size_t len)
{
    return unzReadCurrentFile(m_file, buffer, static_cast<unsigned>(len));
}

size_t ZipStream::Write(const void*, size_t)
{
    return 0;
}

#endif  // HAVE_LIBZ
