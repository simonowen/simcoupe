// Part of SimCoupe - A SAM Coupé emulator
//
// CStream.h: Data stream abstraction classes
//
//  Copyright (c) 1999-2001  Simon Owen
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

#ifndef CSTREAM_H
#define CSTREAM_H

class CStream
{
    public:
        CStream (const char* pcszStream_, bool fReadOnly_=false);
        virtual ~CStream ();

    public:
        static CStream* Open (const char* pcszStream_, bool fReadOnly_=false);

    public:
        bool IsReadOnly () const { return this && m_fReadOnly; }
        const char* GetName () { return m_pszStream; }
        virtual bool IsOpen () const = 0;

        virtual bool Rewind () = 0;
        virtual long Read (void* pvBuffer_, long lLen_) = 0;
        virtual long Write (void* pvBuffer_, long lLen_) = 0;
        
    protected:
        virtual void Close () = 0;

////////////////////////////////////////////////////////////////////////////////

    protected:
        enum { modeClosed, modeReading, modeWriting };
        int     m_nMode;

        bool    m_fReadOnly;
        char*   m_pszStream;
};

class CFileStream : public CStream
{
    public:
        CFileStream (FILE* hFile_, const char* pcszStream_, bool fReadOnly_=false)
            : CStream(pcszStream_, fReadOnly_), m_hFile(hFile_) { }
        virtual ~CFileStream () { Close(); }

    public:
        bool IsOpen () const { return m_hFile != NULL; }
        bool Rewind ();
        long Read (void* pvBuffer_, long lLen_);
        long Write (void* pvBuffer_, long lLen_);

    protected:
        FILE* m_hFile;

    protected:
        void Close ();
};


#ifndef NO_ZLIB

class CZLibStream : public CStream
{
    public:
        CZLibStream (gzFile hFile_, const char* pcszStream_, bool fReadOnly_=false)
            : CStream(pcszStream_, fReadOnly_), m_hFile(hFile_) { }
        virtual ~CZLibStream () { Close(); }

    public:
        bool IsOpen () const { return m_hFile != NULL; }
        bool Rewind ();
        long Read (void* pvBuffer_, long lLen_);
        long Write (void* pvBuffer_, long lLen_);

    protected:
        gzFile  m_hFile;

    protected:
        void Close ();
};

class CZipStream : public CStream
{
    public:
        CZipStream (unzFile hFile_, const char* pcszFile_, bool fReadOnly_=false)
            : CStream(pcszFile_, fReadOnly_), m_hFile(hFile_) { }
        virtual ~CZipStream () { Close(); }

    public:
        bool IsOpen () const { return m_hFile != NULL; }
        bool Rewind ();
        long Read (void* pvBuffer_, long lLen_);
        long Write (void* pvBuffer_, long lLen_);

    protected:
        unzFile m_hFile;

    protected:
        void Close ();
};

#endif  // !NO_ZLIB

#endif  // CSTREAM_H
