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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef STREAM_H
#define STREAM_H

class CStream
{
    public:
        CStream (const char* pcszPath_, bool fReadOnly_=false);
        virtual ~CStream ();

    public:
        static CStream* Open (const char* pcszPath_, bool fReadOnly_=false);

    public:
        bool IsReadOnly () const { return this && m_fReadOnly; }
        const char* GetPath () const { return m_pszPath; }
        const char* GetFile () const { return m_pszFile ? m_pszFile : m_pszPath; }
        size_t GetSize () const { return m_uSize; }
        virtual bool IsOpen () const = 0;

        virtual void Close () = 0;
        virtual bool Rewind () = 0;
        virtual size_t Read (void* pvBuffer_, size_t uLen_) = 0;
        virtual size_t Write (void* pvBuffer_, size_t uLen_) = 0;

    protected:
        enum { modeClosed, modeReading, modeWriting };
        int     m_nMode;

        char    *m_pszPath, *m_pszFile;
        bool    m_fReadOnly;
        size_t  m_uSize;
};

class CFileStream : public CStream
{
    public:
        CFileStream (FILE* hFile_, const char* pcszPath_, bool fReadOnly_=false);
        ~CFileStream () { Close(); }

    public:
        bool IsOpen () const { return m_hFile != NULL; }

    public:
        void Close ();
        bool Rewind ();
        size_t Read (void* pvBuffer_, size_t uLen_);
        size_t Write (void* pvBuffer_, size_t uLen_);

    protected:
        FILE* m_hFile;
};

class CMemStream : public CStream
{
    public:
        CMemStream (void* pv_, size_t uSize_, const char* pcszPath_);
        ~CMemStream () { Close(); }

    public:
        bool IsOpen () const { return m_nMode != modeClosed; }

    public:
        void Close ();
        bool Rewind ();
        size_t Read (void* pvBuffer_, size_t uLen_);
        size_t Write (void* pvBuffer_, size_t uLen_);

    protected:
        BYTE* m_pbData;
        size_t m_uPos;
};


#ifdef USE_ZLIB

const BYTE GZ_SIGNATURE[] = { 0x1f, 0x8b };

class CZLibStream : public CStream
{
    public:
        CZLibStream (gzFile hFile_, const char* pcszPath_, bool fReadOnly_=false);
        ~CZLibStream () { Close(); }

    public:
        bool IsOpen () const { return m_hFile != NULL; }

    public:
        void Close ();
        bool Rewind ();
        size_t Read (void* pvBuffer_, size_t uLen_);
        size_t Write (void* pvBuffer_, size_t uLen_);

    protected:
        gzFile  m_hFile;
};

class CZipStream : public CStream
{
    public:
        CZipStream (unzFile hFile_, const char* pcszPath_, bool fReadOnly_=false);
        ~CZipStream () { Close(); }

    public:
        bool IsOpen () const { return m_hFile != NULL; }

    public:
        void Close ();
        bool Rewind ();
        size_t Read (void* pvBuffer_, size_t uLen_);
        size_t Write (void* pvBuffer_, size_t uLen_);

    protected:
        unzFile m_hFile;
};

#endif  // USE_ZLIB

#endif  // STREAM_H
