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
#include "Stream.h"

#include "Disk.h"
#include "Floppy.h"
#include "Util.h"

////////////////////////////////////////////////////////////////////////////////

CStream::CStream (const char* pcszPath_, bool fReadOnly_/*=false*/)
    : m_nMode(modeClosed), m_pszFile(NULL), m_fReadOnly(fReadOnly_), m_uSize(0)
{
    // Keep a copy of the stream source as we'll need it for saving
    m_pszPath = strdup(pcszPath_);
}

CStream::~CStream ()
{
    free(m_pszPath);
    if (m_pszFile) free(m_pszFile);
}


// Identify the stream and create an object to supply data from it
/*static*/ CStream* CStream::Open (const char* pcszPath_, bool fReadOnly_/*=false*/)
{
    // Reject empty strings immediately
    if (!pcszPath_ || !*pcszPath_)
        return NULL;

    // Give the OS-specific floppy driver first go at the path
    if (CFloppyStream::IsRecognised(pcszPath_))
        return new CFloppyStream(pcszPath_, fReadOnly_);

    // If the file is read-only, the stream will be read-only
    FILE* file = fopen(pcszPath_, "r+b");
    fReadOnly_ |= !file;
    if (file)
        fclose(file);

#ifdef USE_ZLIB
    // Try and open it as a zip file
    unzFile hfZip;
    if ((hfZip = unzOpen(pcszPath_)))
    {
        // Iterate through the contents of the zip looking for a file with a suitable size
        for (int nRet = unzGoToFirstFile(hfZip) ; nRet == UNZ_OK ; nRet = unzGoToNextFile(hfZip))
        {
            unz_file_info sInfo;

            // Get details of the current file
            unzGetCurrentFileInfo(hfZip, &sInfo, NULL, 0, NULL, 0, NULL, 0);

            // Continue looking if it's too small to be considered [strictly this shouldn't really be done here!]
            if (sInfo.uncompressed_size < 32768)
                continue;

            // Ok, so open and use the first file in the zip and use that
            if (unzOpenCurrentFile(hfZip) == UNZ_OK)
                return new CZipStream(hfZip, pcszPath_, true/*fReadOnly_*/);  // ZIPs are currently read-only
        }

        // Failed to open the first file, so close the zip
        unzClose(hfZip);
    }
    else
#endif
    {
        // Open the file using the regular CRT file functions
        FILE* hf;
        if ((hf = fopen(pcszPath_, "rb")))
        {
#ifdef USE_ZLIB
            BYTE abSig[sizeof(GZ_SIGNATURE)];
            if ((fread(abSig, 1, sizeof(abSig), hf) != sizeof(abSig)) || memcmp(abSig, GZ_SIGNATURE, sizeof(abSig)))
#endif
                return new CFileStream(hf, pcszPath_, fReadOnly_);
#ifdef USE_ZLIB
            else
            {
                BYTE ab[4] = {};
                size_t uSize = 0;

                // Read the uncompressed size from the end of the file (if under 4GiB)
                if (fseek(hf, -4, SEEK_END) == 0 && fread(ab, 1, sizeof(ab), hf) == sizeof(ab))
                    uSize = (ab[3] << 24) | (ab[2] << 16) | (ab[1] << 8) | ab[0];

                // Close the file so we can open it through ZLib
                fclose(hf);

                // Try to open it as a gzipped file
                gzFile hfGZip;
                if ((hfGZip = gzopen(pcszPath_, "rb")))
                    return new CZLibStream(hfGZip, pcszPath_, uSize, fReadOnly_);
            }
#endif  // USE_ZLIB
        }
    }

    // Couldn't handle what we were given :-/
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////

CFileStream::CFileStream (FILE* hFile_, const char* pcszPath_, bool fReadOnly_/*=false*/)
    : CStream(pcszPath_, fReadOnly_), m_hFile(hFile_)
{
    struct stat st;

    if (hFile_ && !stat(pcszPath_, &st))
        m_uSize = static_cast<size_t>(st.st_size);

    for (const char* p = pcszPath_ ; *p ; p++)
    {
        if (*p == PATH_SEPARATOR)
            pcszPath_ = p+1;
    }

    m_pszFile = strdup(pcszPath_);
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
        if ((m_hFile = fopen(m_pszPath, "rb")))
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
        if ((m_hFile = fopen(m_pszPath, "wb")))
            m_nMode = modeWriting;
    }

    return m_hFile ? fwrite(pvBuffer_, 1, uLen_, m_hFile) : 0;
}

////////////////////////////////////////////////////////////////////////////////

CMemStream::CMemStream (void* pv_, size_t uLen_, const char* pcszPath_)
    : CStream(pcszPath_, true), m_uPos(0)
{
    m_nMode = modeReading;
    m_uSize = uLen_;
    m_pbData = reinterpret_cast<BYTE*>(pv_),
    m_pszFile = strdup(pcszPath_);
}

void CMemStream::Close ()
{
    m_nMode = modeClosed;
}

bool CMemStream::Rewind ()
{
    m_uPos = 0;
    return true;
}

size_t CMemStream::Read (void* pvBuffer_, size_t uLen_)
{
    if (m_nMode != modeReading)
    {
        m_nMode = modeReading;
        m_uPos = 0;
    }

    size_t uRead = std::min(m_uSize-m_uPos, uLen_);
    memcpy(pvBuffer_, m_pbData+m_uPos, uRead);
    m_uPos += uRead;
    return uRead;
}

size_t CMemStream::Write (void* pvBuffer_, size_t uLen_)
{
    m_nMode = modeWriting;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef USE_ZLIB

CZLibStream::CZLibStream (gzFile hFile_, const char* pcszPath_, size_t uSize_, bool fReadOnly_/*=false*/)
    : CStream(pcszPath_, fReadOnly_), m_hFile(hFile_), m_uSize(uSize_)
{
    for (const char* p = pcszPath_ ; *p ; p++)
    {
        if (*p == PATH_SEPARATOR)
            pcszPath_ = p+1;
    }

    char szFile[MAX_PATH+7];
    strncpy(szFile, pcszPath_, MAX_PATH);
    strcat(szFile, " (gzip)");
    m_pszFile = strdup(szFile);
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


size_t CZLibStream::GetSize ()
{
    // Do we need to determine the size?
    if (!m_uSize && IsOpen())
    {
        long lPos = gztell(m_hFile);
        gzrewind(m_hFile);

        BYTE ab[512];
        int nRead;

        do {
            m_uSize += (nRead = gzread(m_hFile, ab, sizeof(ab)));
        } while (nRead > 0);

        gzseek(m_hFile, lPos, SEEK_SET);
    }

    return m_uSize;
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
        if ((m_hFile = gzopen(m_pszPath, "rb")))
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
        if ((m_hFile = gzopen(m_pszPath, "wb9")))
            m_nMode = modeWriting;
    }

    return m_hFile ? gzwrite(m_hFile, pvBuffer_, static_cast<unsigned>(uLen_)) : 0;
}

////////////////////////////////////////////////////////////////////////////////

CZipStream::CZipStream (unzFile hFile_, const char* pcszPath_, bool fReadOnly_/*=false*/)
    : CStream(pcszPath_, fReadOnly_), m_hFile(hFile_)
{
    unz_file_info sInfo;
    char szFile[MAX_PATH+6];

    // Get details of the current file
    if (unzGetCurrentFileInfo(hFile_, &sInfo, szFile, MAX_PATH, NULL, 0, NULL, 0) == UNZ_OK)
    {
        m_uSize = sInfo.uncompressed_size;
        strcat(szFile, " (zip)");
        m_pszFile = strdup(szFile);
    }
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
