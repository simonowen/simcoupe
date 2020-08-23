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

public:
    static std::unique_ptr<Stream> Open(const std::string& filepath, bool read_only = false);

public:
    bool IsReadOnly() const { return m_fReadOnly; }
    std::string GetPath() const { return m_path.string(); }
    std::string GetName() const { return !m_shortname.empty() ? m_shortname : m_path.filename().string(); }
    virtual size_t GetSize() { return m_uSize; }
    virtual bool IsOpen() const = 0;

    virtual void Close() = 0;
    virtual bool Rewind() = 0;
    virtual size_t Read(void* pvBuffer_, size_t uLen_) = 0;
    virtual size_t Write(void* pvBuffer_, size_t uLen_) = 0;

protected:
    enum class Mode { Closed, Reading, Writing };
    Mode m_mode = Mode::Closed;

    fs::path m_path;
    std::string m_shortname;
    bool m_fReadOnly = false;
    size_t m_uSize = 0;
};

class FileStream final : public Stream
{
public:
    FileStream(unique_FILE&& file, const std::string& filepath, bool read_only = false);

public:
    bool IsOpen() const override { return m_file; }

public:
    void Close() override;
    bool Rewind() override;
    size_t Read(void* pvBuffer_, size_t uLen_) override;
    size_t Write(void* pvBuffer_, size_t uLen_) override;

protected:
    unique_FILE m_file;
};

class MemStream final : public Stream
{
public:
    MemStream(void* pv_, size_t uSize_, const std::string& filepath);
    MemStream(const MemStream&) = delete;
    void operator= (const MemStream&) = delete;

public:
    bool IsOpen() const override { return m_mode != Mode::Closed; }

public:
    void Close() override;
    bool Rewind() override;
    size_t Read(void* pvBuffer_, size_t uLen_) override;
    size_t Write(void* pvBuffer_, size_t uLen_) override;

protected:
    uint8_t* m_pbData = nullptr;
    size_t m_uPos = 0;
};


#ifdef HAVE_LIBZ

const uint8_t GZ_SIGNATURE[] = { 0x1f, 0x8b };

struct gzFileCloser { void operator()(gzFile file) { gzclose(file); } };
using unique_gzFile = unique_resource<gzFile, nullptr, gzFileCloser>;

class ZLibStream final : public Stream
{
public:
    ZLibStream(gzFile hFile_, const std::string& filepath, size_t uSize_ = 0, bool read_only = false);

public:
    bool IsOpen() const override { return m_file; }
    size_t GetSize() override;

public:
    void Close() override;
    bool Rewind() override;
    size_t Read(void* pvBuffer_, size_t uLen_) override;
    size_t Write(void* pvBuffer_, size_t uLen_) override;

protected:
    unique_gzFile m_file;
    size_t m_uSize = 0;
};


struct unzFileCloser { void operator()(unzFile file) { unzCloseCurrentFile(file);  unzClose(file); } };
using unique_unzFile = unique_resource<unzFile, nullptr, unzFileCloser>;

class ZipStream final : public Stream
{
public:
    ZipStream(unzFile hFile_, const std::string& filepath, bool read_only = false);

public:
    bool IsOpen() const  override { return m_file; }

public:
    void Close() override;
    bool Rewind() override;
    size_t Read(void* pvBuffer_, size_t uLen_) override;
    size_t Write(void* pvBuffer_, size_t uLen_) override;

protected:
    unique_unzFile m_file;
};

#endif  // HAVE_LIBZ
