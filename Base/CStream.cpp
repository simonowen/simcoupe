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
//  - remove 32K file test done on zip archives (add a container layer?)
//  - maybe add support for updating zip archives

#include "SimCoupe.h"
#include "CStream.h"
#include "CDisk.h"

#include "Floppy.h"
#include "Util.h"

////////////////////////////////////////////////////////////////////////////////

CStream::CStream (const char* pcszStream_, bool fReadOnly_/*=false*/)
    : m_nMode(modeClosed), m_nSize(0), m_fReadOnly(fReadOnly_)
{
    // Keep a copy of the stream source as we'll need it for saving
    m_pszStream = strdup(pcszStream_);
}

CStream::~CStream ()
{
    free(m_pszStream);
}


// Identify the stream and create an object to supply data from it
/*static*/ CStream* CStream::Open (const char* pcszStream_, bool fReadOnly_/*=false*/)
{
    struct stat st;

    // Give the OS-specific floppy driver first go at the path
    if (CFloppyStream::IsRecognised(pcszStream_))
        return new CFloppyStream(pcszStream_, fReadOnly_);

    // Check for a regular file that we have read access to
    else if (!::stat(pcszStream_, &st) && S_ISREG(st.st_mode) && !access(pcszStream_, R_OK))
    {
        // If the file is read-only, the stream will be read-only
        FILE* file = (pcszStream_ && *pcszStream_) ? fopen(pcszStream_, "r+b") : NULL;
        fReadOnly_ |= !file;
        if (file)
            fclose(file);

#ifdef USE_ZLIB
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
                if (sInfo.uncompressed_size < 32768)
                    continue;

                // Ok, so open and use the first file in the zip and use that
                if (unzOpenCurrentFile(hfZip) == UNZ_OK)
                    return new CZipStream(hfZip, pcszStream_, true/*fReadOnly_*/);  // ZIPs are currently read-only
            }

            // Failed to open the first file, so close the zip
            unzClose(hfZip);
        }
        else
#endif
        {
            // Open the file using the regular CRT file functions
            FILE* hf;
            if ((hf = fopen(pcszStream_, "rb")))
            {
#ifdef USE_ZLIB
                BYTE abSig[sizeof GZ_SIGNATURE];
                if ((fread(abSig, 1, sizeof abSig, hf) != sizeof abSig) || memcmp(abSig, GZ_SIGNATURE, sizeof abSig))
#endif
                    return new CFileStream(hf, pcszStream_, fReadOnly_);
#ifdef USE_ZLIB
                else
                {
                    // Close the file so we can open it thru ZLib
                    fclose(hf);
                    // Try to open it as a gzipped file
                    gzFile hfGZip;
                    if ((hfGZip = gzopen(pcszStream_, "rb")))
                        return new CZLibStream(hfGZip, pcszStream_, fReadOnly_);
                }
#endif  // USE_ZLIB
            }
        }
    }

    // Couldn't handle what we were given :-/
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////

CFileStream::CFileStream (FILE* hFile_, const char* pcszStream_, bool fReadOnly_/*=false*/)
    : CStream(pcszStream_, fReadOnly_), m_hFile(hFile_)
{
    struct stat st;
    if (!fstat(fileno(hFile_), &st))
        m_nSize = st.st_size;
}

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
        Close();

    return true;
}

size_t CFileStream::Read (void* pvBuffer_, size_t uLen_)
{
    if (m_nMode != modeReading)
    {
        // Close the file, if open for writing
        Close();

        // Open the file for writing, using compression if the source file did
        if ((m_hFile = fopen(m_pszStream, "rb")))
            m_nMode = modeReading;
    }

    size_t uRead = m_hFile ? fread(pvBuffer_, 1, uLen_, m_hFile) : 0;
    return uRead;
}

size_t CFileStream::Write (void* pvBuffer_, size_t uLen_)
{
    if (m_nMode != modeWriting)
    {
        // Close the file, if open for reading
        Close();

        // Open the file for writing, using compression if the source file did
        if ((m_hFile = fopen(m_pszStream, "wb")))
            m_nMode = modeWriting;
    }

    return m_hFile ? fwrite(pvBuffer_, 1, uLen_, m_hFile) : 0;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef USE_ZLIB

CZLibStream::CZLibStream (gzFile hFile_, const char* pcszStream_, bool fReadOnly_/*=false*/)
    : CStream(pcszStream_, fReadOnly_), m_hFile(hFile_)
{
    // We can't determine the size without an expensive seek, reading the whole file
    m_nSize = 0;
}

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

size_t CZLibStream::Read (void* pvBuffer_, size_t uLen_)
{
    if (m_nMode != modeReading)
    {
        // Close the file, if open for writing
        Close();

        // Open the file for writing, using compression if the source file did
        if ((m_hFile = gzopen(m_pszStream, "rb")))
            m_nMode = modeReading;
    }

    int nRead = m_hFile ? gzread(m_hFile, pvBuffer_, static_cast<unsigned>(uLen_)) : 0;
    return (nRead == -1) ? 0 : static_cast<size_t>(nRead);
}

size_t CZLibStream::Write (void* pvBuffer_, size_t uLen_)
{
    if (m_nMode != modeWriting)
    {
        // Close the file, if open for reading
        Close();

        // Open the file for writing, using compression if the source file did
        if ((m_hFile = gzopen(m_pszStream, "wb9")))
            m_nMode = modeWriting;
    }

    return m_hFile ? gzwrite(m_hFile, pvBuffer_, static_cast<unsigned>(uLen_)) : 0;
}

////////////////////////////////////////////////////////////////////////////////

CZipStream::CZipStream (unzFile hFile_, const char* pcszFile_, bool fReadOnly_/*=false*/)
    : CStream(pcszFile_, fReadOnly_), m_hFile(hFile_)
{
    unz_file_info sInfo;
    char szFile[MAX_PATH];

    // Get details of the current file
    if (unzGetCurrentFileInfo(hFile_, &sInfo, szFile, sizeof szFile, NULL, 0, NULL, 0) == UNZ_OK)
        m_nSize = sInfo.uncompressed_size;
}

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

size_t CZipStream::Read (void* pvBuffer_, size_t uLen_)
{
    return unzReadCurrentFile(m_hFile, pvBuffer_, static_cast<unsigned>(uLen_));
}

size_t CZipStream::Write (void* pvBuffer_, size_t uLen_)
{
    // Currently there's no support for zip writing (yet)
    return 0;
}

#endif  // USE_ZLIB
