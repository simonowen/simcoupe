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

#ifndef STREAM_H
#define STREAM_H

class CStream
{
    public:
        CStream (const char* pcszPath_, bool fReadOnly_=false);
        CStream (const CStream &) = delete;
        void operator= (const CStream &) = delete;
        virtual ~CStream ();

    public:
        static CStream* Open (const char* pcszPath_, bool fReadOnly_=false);

    public:
        bool IsReadOnly () const { return m_fReadOnly; }
        const char* GetPath () const { return m_pszPath; }
        const char* GetFile () const { return m_pszFile ? m_pszFile : m_pszPath; }
        virtual size_t GetSize () { return m_uSize; }
        virtual bool IsOpen () const = 0;

        virtual void Close () = 0;
        virtual bool Rewind () = 0;
        virtual size_t Read (void* pvBuffer_, size_t uLen_) = 0;
        virtual size_t Write (void* pvBuffer_, size_t uLen_) = 0;

    protected:
        enum { modeClosed, modeReading, modeWriting };
        int m_nMode = modeClosed;

        char *m_pszPath = nullptr;
        char *m_pszFile = nullptr;
        bool m_fReadOnly = false;
        size_t m_uSize = 0;
};

class CFileStream final : public CStream
{
    public:
        CFileStream (FILE* hFile_, const char* pcszPath_, bool fReadOnly_=false);
        CFileStream (const CFileStream &) = delete;
        void operator= (const CFileStream &) = delete;
        ~CFileStream () { Close(); }

    public:
        bool IsOpen () const override { return m_hFile != nullptr; }

    public:
        void Close () override;
        bool Rewind () override;
        size_t Read (void* pvBuffer_, size_t uLen_) override;
        size_t Write (void* pvBuffer_, size_t uLen_) override;

    protected:
        FILE *m_hFile = nullptr;
};

class CMemStream final : public CStream
{
    public:
        CMemStream (void* pv_, size_t uSize_, const char* pcszPath_);
        CMemStream (const CMemStream &) = delete;
        void operator= (const CMemStream &) = delete;
        ~CMemStream () { Close(); }

    public:
        bool IsOpen () const override { return m_nMode != modeClosed; }

    public:
        void Close () override;
        bool Rewind () override;
        size_t Read (void* pvBuffer_, size_t uLen_) override;
        size_t Write (void* pvBuffer_, size_t uLen_) override;

    protected:
        BYTE *m_pbData = nullptr;
        size_t m_uPos = 0;
};


#ifdef HAVE_LIBZ

const BYTE GZ_SIGNATURE[] = { 0x1f, 0x8b };

class CZLibStream final : public CStream
{
    public:
        CZLibStream (gzFile hFile_, const char* pcszPath_, size_t uSize_=0, bool fReadOnly_=false);
        CZLibStream (const CZLibStream &) = delete;
        void operator= (const CZLibStream &) = delete;
        ~CZLibStream () { Close(); }

    public:
        bool IsOpen () const override { return m_hFile != nullptr; }
        size_t GetSize () override;

    public:
        void Close () override;
        bool Rewind () override;
        size_t Read (void* pvBuffer_, size_t uLen_) override;
        size_t Write (void* pvBuffer_, size_t uLen_) override;

    protected:
        gzFile m_hFile = nullptr;
        size_t m_uSize = 0;
};

class CZipStream final : public CStream
{
    public:
        CZipStream (unzFile hFile_, const char* pcszPath_, bool fReadOnly_=false);
        CZipStream (const CZipStream &) = delete;
        void operator= (const CZipStream &) = delete;
        ~CZipStream () { Close(); }

    public:
        bool IsOpen () const  override { return m_hFile != nullptr; }

    public:
        void Close () override;
        bool Rewind () override;
        size_t Read (void* pvBuffer_, size_t uLen_) override;
        size_t Write (void* pvBuffer_, size_t uLen_) override;

    protected:
        unzFile m_hFile = nullptr;
};

#endif  // HAVE_LIBZ

#endif  // STREAM_H
