// Part of SimCoupe - A SAM Coupé emulator
//
// CStream.cpp: Data stream abstraction classes
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

// Notes:
//  Currently supports read-write access of uncompressed files, gzipped
//  files, and read-only zip archive access.
//
//  Access to real standard format disks is also supported where a
//  Floppy.cpp implementation exists.

// Todo:
//  - take out the naughty gzip hack used to test for compressed files
//  - remove 100K file test done on zip archives (add a container layer?)
//  - maybe add support for updating zip archives

#include "SimCoupe.h"
#include "CStream.h"

#include "Floppy.h"
#include "Util.h"

////////////////////////////////////////////////////////////////////////////////

CStream::CStream (const char* pcszStream_, bool fReadOnly_/*=false*/)
{
    // No file open initially
    m_nMode = modeClosed;

    // Save whether we're to treat the stream as read-only
    m_fReadOnly = fReadOnly_;

    // Keep a copy of the stream source as we'll need it for saving
    strcpy(m_pszStream = new char[1+strlen(pcszStream_)], pcszStream_);
}

/*virtual*/ CStream::~CStream ()
{
    delete m_pszStream;
}


// Identify the stream and create an object to supply data from it
/*static*/ CStream* CStream::Open (const char* pcszStream_, bool fReadOnly_/*=false*/)
{
    if (CFloppyStream::IsRecognised(pcszStream_))
        return new CFloppyStream(pcszStream_, fReadOnly_);
    else
    {
        // If the file is read-only, the stream will be read-only
        FILE* file = (pcszStream_ && *pcszStream_) ? fopen(pcszStream_, "r+b") : NULL;
        fReadOnly_ |= !file;
        if (file)
            fclose(file);

#ifndef NO_ZLIB
        // Try and open it as a zip file
        unzFile hfZip;
        if ((hfZip = unzOpen(pcszStream_)))
        {
            // Iterate through the contents of the zip looking for a file with a suitable size
            for (int nRet = unzGoToFirstFile(hfZip) ; nRet == UNZ_OK ; nRet = unzGoToNextFile(hfZip))
            {
                unz_file_info sInfo;
                char szFile[MAX_PATH];

                // Get details of the current file
                unzGetCurrentFileInfo(hfZip, &sInfo, szFile, sizeof szFile, NULL, 0, NULL, 0);

                // Continue looking if it's too small to be considered [strictly this shouldn't really be done here!]
                if (sInfo.uncompressed_size < 100000)
                    continue;

                // Ok, so open and use the first file in the zip and use that
                if (unzOpenCurrentFile(hfZip) == UNZ_OK)
                    return new CZipStream(hfZip, pcszStream_, true/*fReadOnly_*/);  // ZIPs are currently read-only
            }

            // Failed to open the first file, so close the zip
            unzClose(hfZip);
        }
        else
        {
            // Try gzipped or regular file
            gzFile hfGZip;
            if ((hfGZip = gzopen(pcszStream_, "rb")))
            {
                // Naughty direct access to a member of gz_stream to determine if the file is compressed
                bool fCompressed = !*reinterpret_cast<int*>(reinterpret_cast<BYTE*>(hfGZip)+88);

                // Only use a CZLibStream object for real compressed data
                if (fCompressed)
                    return new CZLibStream(hfGZip, pcszStream_, fReadOnly_);

                gzclose(hfGZip);
#endif  // !NO_ZLIB

                // Re-open the file using the regular file functions
                FILE* hf;
                if ((hf = fopen(pcszStream_, "rb")))
                    return new CFileStream(hf, pcszStream_, fReadOnly_);
#ifndef NO_ZLIB
            }
        }
#endif
    }

    // Couldn't handle what we were given :-/
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////

void CFileStream::Close ()
{
    if (m_hFile)
    {
        fclose(m_hFile);
        m_hFile = NULL;
        m_nMode = modeClosed;
    }
}

bool CFileStream::Rewind ()
{
    if (IsOpen())
    {
//      rewind(m_hFile);
        Close();
    }

    return true;
}

long CFileStream::Read (void* pvBuffer_, long lLen_)
{
    if (m_nMode != modeReading)
    {
        // Close the file, if open for writing
        Close();

        // Open the file for writing, using compression if the source file did
        if ((m_hFile = fopen(m_pszStream, "rb")))
            m_nMode = modeReading;
    }

    long lRead = m_hFile ? fread(pvBuffer_, 1, lLen_, m_hFile) : 0;
    return (lRead == -1) ? 0 : lRead;
}

long CFileStream::Write (void* pvBuffer_, long lLen_)
{
    if (m_nMode != modeWriting)
    {
        // Close the file, if open for reading
        Close();

        // Open the file for writing, using compression if the source file did
        if ((m_hFile = fopen(m_pszStream, "wb")))
            m_nMode = modeWriting;
    }

    return m_hFile ? fwrite(pvBuffer_, 1, lLen_, m_hFile) : 0;
}

////////////////////////////////////////////////////////////////////////////////

#ifndef NO_ZLIB

void CZLibStream::Close ()
{
    if (m_hFile)
    {
        gzclose(m_hFile);
        m_hFile = NULL;
        m_nMode = modeClosed;
    }
}

bool CZLibStream::Rewind ()
{
    return !IsOpen() || !gzrewind(m_hFile);
}

long CZLibStream::Read (void* pvBuffer_, long lLen_)
{
    if (m_nMode != modeReading)
    {
        // Close the file, if open for writing
        Close();

        // Open the file for writing, using compression if the source file did
        if ((m_hFile = gzopen(m_pszStream, "rb")))
            m_nMode = modeReading;
    }

    long lRead = m_hFile ? gzread(m_hFile, pvBuffer_, lLen_) : 0;
    return (lRead == -1) ? 0 : lRead;
}

long CZLibStream::Write (void* pvBuffer_, long lLen_)
{
    if (m_nMode != modeWriting)
    {
        // Close the file, if open for reading
        Close();

        // Open the file for writing, using compression if the source file did
        if ((m_hFile = gzopen(m_pszStream, "wb9")))
            m_nMode = modeWriting;
    }

    return m_hFile ? gzwrite(m_hFile, pvBuffer_, lLen_) : 0;
}

////////////////////////////////////////////////////////////////////////////////

void CZipStream::Close ()
{
    if (m_hFile)
    {
        unzCloseCurrentFile(m_hFile);
        unzClose(m_hFile);
        m_hFile = NULL;
        m_nMode = modeClosed;
    }
}

bool CZipStream::Rewind ()
{
    // There's no zip rewind so we close and re-open the current file
    if (IsOpen())
        unzCloseCurrentFile(m_hFile);

    return unzOpenCurrentFile(m_hFile) == UNZ_OK;
}

long CZipStream::Read (void* pvBuffer_, long lLen_)
{
    return unzReadCurrentFile(m_hFile, pvBuffer_, lLen_);
}

long CZipStream::Write (void* pvBuffer_, long lLen_)
{
    // Currently there's no support for zip writing (yet)
    return 0;
}

#endif  // !NO_ZLIB
