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
    Stream(const char* pcszPath_, bool fReadOnly_ = false);
    Stream(const Stream&) = delete;
    void operator= (const Stream&) = delete;
    virtual ~Stream();

public:
    static std::unique_ptr<Stream> Open(const char* pcszPath_, bool fReadOnly_ = false);

public:
    bool IsReadOnly() const { return m_fReadOnly; }
    const char* GetPath() const { return m_pszPath; }
    const char* GetFile() const { return m_pszFile ? m_pszFile : m_pszPath; }
    virtual size_t GetSize() { return m_uSize; }
    virtual bool IsOpen() const = 0;

    virtual void Close() = 0;
    virtual bool Rewind() = 0;
    virtual size_t Read(void* pvBuffer_, size_t uLen_) = 0;
    virtual size_t Write(void* pvBuffer_, size_t uLen_) = 0;

protected:
    enum { modeClosed, modeReading, modeWriting };
    int m_nMode = modeClosed;

    char* m_pszPath = nullptr;
    char* m_pszFile = nullptr;
    bool m_fReadOnly = false;
    size_t m_uSize = 0;
};

class FileStream final : public Stream
{
public:
    FileStream(FILE* hFile_, const char* pcszPath_, bool fReadOnly_ = false);
    FileStream(const FileStream&) = delete;
    void operator= (const FileStream&) = delete;
    ~FileStream() { Close(); }

public:
    bool IsOpen() const override { return m_hFile != nullptr; }

public:
    void Close() override;
    bool Rewind() override;
    size_t Read(void* pvBuffer_, size_t uLen_) override;
    size_t Write(void* pvBuffer_, size_t uLen_) override;

protected:
    FILE* m_hFile = nullptr;
};

class MemStream final : public Stream
{
public:
    MemStream(void* pv_, size_t uSize_, const char* pcszPath_);
    MemStream(const MemStream&) = delete;
    void operator= (const MemStream&) = delete;
    ~MemStream() { Close(); }

public:
    bool IsOpen() const override { return m_nMode != modeClosed; }

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

class ZLibStream final : public Stream
{
public:
    ZLibStream(gzFile hFile_, const char* pcszPath_, size_t uSize_ = 0, bool fReadOnly_ = false);
    ZLibStream(const ZLibStream&) = delete;
    void operator= (const ZLibStream&) = delete;
    ~ZLibStream() { Close(); }

public:
    bool IsOpen() const override { return m_hFile != nullptr; }
    size_t GetSize() override;

public:
    void Close() override;
    bool Rewind() override;
    size_t Read(void* pvBuffer_, size_t uLen_) override;
    size_t Write(void* pvBuffer_, size_t uLen_) override;

protected:
    gzFile m_hFile = nullptr;
    size_t m_uSize = 0;
};

class ZipStream final : public Stream
{
public:
    ZipStream(unzFile hFile_, const char* pcszPath_, bool fReadOnly_ = false);
    ZipStream(const ZipStream&) = delete;
    void operator= (const ZipStream&) = delete;
    ~ZipStream() { Close(); }

public:
    bool IsOpen() const  override { return m_hFile != nullptr; }

public:
    void Close() override;
    bool Rewind() override;
    size_t Read(void* pvBuffer_, size_t uLen_) override;
    size_t Write(void* pvBuffer_, size_t uLen_) override;

protected:
    unzFile m_hFile = nullptr;
};

#endif  // HAVE_LIBZ
