// Part of SimCoupe - A SAM Coupe emulator
//
// Stream.h: Data stream abstraction classes
//
//  Copyright (c) 1999-2012  Simon Owen
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

class Stream
{
public:
    Stream(const std::string& filepath, bool read_only = false);
    virtual ~Stream() = default;

    static std::unique_ptr<Stream> Open(const std::string& filepath, bool read_only = false);

    bool WriteProtected() const { return m_read_only; }
    std::string GetPath() const { return m_path; }
    std::string GetName() const { return !m_short_name.empty() ? m_short_name : fs::path(m_path).filename().string(); }
    virtual fs::file_time_type LastWriteTime() const;

    virtual size_t GetSize() = 0;
    virtual void Close() = 0;
    virtual void Rewind() = 0;
    virtual size_t Read(void* buffer, size_t len) = 0;
    virtual size_t Write(const void* buffer, size_t len) = 0;

protected:
    enum class FileMode { Closed, Reading, Writing };
    FileMode m_mode = FileMode::Reading;

    std::string m_path;
    std::string m_short_name;
    bool m_read_only = false;
};

class FileStream final : public Stream
{
public:
    FileStream(unique_FILE&& file, const std::string& filepath, bool read_only = false);

    size_t GetSize() override;
    void Close() override;
    void Rewind() override;
    size_t Read(void* buffer, size_t len) override;
    size_t Write(const void* buffer, size_t len) override;

protected:
    unique_FILE m_file;
};

class MemStream final : public Stream
{
public:
    MemStream(const std::vector<uint8_t>& mem_file);

    fs::file_time_type LastWriteTime() const override { return {}; }

    size_t GetSize() override;
    void Close() override;
    void Rewind() override;
    size_t Read(void* buffer, size_t len) override;
    size_t Write(const void* buffer, size_t len) override;

protected:
    std::vector<uint8_t> m_data;
    size_t m_pos = 0;
};

#ifdef HAVE_LIBZ

const std::array<uint8_t, 2> GZ_SIGNATURE{ 0x1f, 0x8b };

struct gzFileCloser { void operator()(gzFile file) { gzclose(file); } };
using unique_gzFile = unique_resource<gzFile, nullptr, gzFileCloser>;

class ZLibStream final : public Stream
{
public:
    ZLibStream(gzFile file, const std::string& filepath, size_t file_size = 0, bool read_only = false);

    size_t GetSize() override;
    void Close() override;
    void Rewind() override;
    size_t Read(void* buffer, size_t len) override;
    size_t Write(const void* buffer, size_t len) override;

protected:
    unique_gzFile m_file;
    size_t m_size = 0;
};

struct unzFileCloser { void operator()(unzFile file) { unzCloseCurrentFile(file); unzClose(file); } };
using unique_unzFile = unique_resource<unzFile, nullptr, unzFileCloser>;

class ZipStream final : public Stream
{
public:
    ZipStream(unzFile file, const std::string& filepath, const std::string& filename);

    size_t GetSize() override;
    void Close() override;
    void Rewind() override;
    size_t Read(void* buffer, size_t len) override;
    size_t Write(const void* buffer, size_t len) override;

protected:
    unique_unzFile m_file;
};

#endif  // HAVE_LIBZ
