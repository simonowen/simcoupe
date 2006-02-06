// Part of SimCoupe - A SAM Coupe emulator
//
// Floppy.cpp: SDL direct floppy access
//
//  Copyright (c) 1999-2006  Simon Owen
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

// ToDo:
//  - some copy protected disk support

#include "SimCoupe.h"

#include "Floppy.h"
#include "CDisk.h"

////////////////////////////////////////////////////////////////////////////////

#ifdef __linux__

#include <linux/fd.h>
#include <linux/fdreg.h>


/*static*/ bool CFloppyStream::IsRecognised (const char* pcszStream_)
{
    struct stat st;
    char sz[MAX_PATH];
    int nMaxFollow = 10;

    // Work with a copy of the path as it may be updated to follow links
    strncpy(sz, pcszStream_, sizeof sz);

    // Loop examining, in case there are links to follow
    while (!lstat(sz, &st))
    {
        // If it's a block device it must have a major of 2 (floppy)
        if (S_ISBLK(st.st_mode) && (st.st_rdev >> 8) == 2)
            return true;

        // Check for a link, and not too deep (or circular)
        if (!S_ISLNK(st.st_mode) || !--nMaxFollow)
            break;

        // Read the link target, failing if there's an error (no access or dangling link)
        char szLink[MAX_PATH];
        int nLen = readlink(sz, szLink, sizeof szLink);
        if (nLen < 0)
            break;

        // Terminate the target string, as readlink doesn't do it
        szLink[nLen] = '\0';

        // Use absolute targets, or form the full path from the relative link
        if (szLink[0] == '/')
            strcpy(sz, szLink);
        else
        {
            strrchr(sz, '/')[1] = '\0';
            strcat(sz, szLink);
        }
    }

    // Not recognised
    return false;
}


bool CFloppyStream::Open ()
{
    if (!IsOpen())
    {
        // Open the floppy in the appropriate mode, falling back to read-only if necessary
        if (m_fReadOnly || (m_nFloppy = open(m_pszPath, O_EXCL|O_RDWR)) == -1)
        {
            m_nFloppy = open(m_pszPath, O_EXCL|O_RDONLY);
            m_fReadOnly = true;
        }

        // If we've opened it, set the SAM disk geometry
        if (IsOpen())
        {
            struct floppy_struct fs = { 1600, 10, 2, 80, 0, 0x0c, 2, 0xdf, 0x17 };
            if (ioctl(m_nFloppy, FDSETPRM, &fs))
                Close();
        }
    }

    // Possibly reset/recalibrate here?

    return IsOpen();
}

void CFloppyStream::Close ()
{
    if (IsOpen())
    {
        close(m_nFloppy);
        m_nFloppy = -1;
    }
}


BYTE CFloppyStream::ReadTrack (BYTE cyl_, BYTE head_, BYTE *pbData_)
{
    int i;

    // Open the device, if not already open
    if (!Open())
        return false;

    PTRACK pt = (PTRACK)pbData_;
    PSECTOR ps = (PSECTOR)(pt+1);
    BYTE *pb = (BYTE*)(ps+(pt->sectors=NORMAL_DISK_SECTORS));

    // Prepare the track container
    for (i = 0 ; i < pt->sectors ; i++)
    {
        ps[i].cyl = cyl_;
        ps[i].head = head_;
        ps[i].sector = i+1;
        ps[i].size = 2;
        ps[i].status = 0;
        ps[i].pbData = pb + i*NORMAL_SECTOR_SIZE;
    }

    // Clear out the command structure
    struct floppy_raw_cmd rc;
    memset(&rc, 0, sizeof rc);

    // Set up details for the command
    rc.flags = FD_RAW_READ  | FD_RAW_INTR | FD_RAW_NEED_SEEK;
    rc.data  = pb;
    rc.length = NORMAL_SECTOR_SIZE*NORMAL_DISK_SECTORS;
    rc.rate = 2;
    rc.track = cyl_;

    // Set up the command and its parameters
    BYTE abCommands[] = { FD_READ, head_ << 2, cyl_, head_, 1, 2, 1+NORMAL_DISK_SECTORS, 0x0a, 0xff };
    memcpy(rc.cmd, abCommands, sizeof abCommands);
    rc.cmd_count = sizeof(abCommands) / sizeof(abCommands[0]);

    // Call failed?
    if (ioctl(m_nFloppy, FDRAWCMD, &rc))
        return RECORD_NOT_FOUND;
 
    // Command successful?
    if (!(rc.reply[0] & 0x40))
        return BUSY;

    // Missing address mark? (blank track)
    if (rc.reply[1] & 0x01)
    {
        pt->sectors = 0;
        return BUSY;
    }

    // Other errors are fatal
    return RECORD_NOT_FOUND;
}

BYTE CFloppyStream::ReadWrite (bool fRead_, BYTE bSide_, BYTE bTrack_, BYTE* pbData_)
{
    return RECORD_NOT_FOUND;
}

bool CFloppyStream::ReadCustomTrack (BYTE cyl_, BYTE head_, BYTE *pbData_)
{
    return false;
}

bool CFloppyStream::ReadMGTTrack (BYTE cyl_, BYTE head_, BYTE *pbData_)
{
    return false;
}

bool CFloppyStream::IsBusy (BYTE* pbStatus_, bool fWait_)
{
    return false;
}

#else
// Dummy implementation for non-Linux SDL versions

/*static*/ bool CFloppyStream::IsRecognised (const char* pcszStream_)
{
    return false;
}

bool CFloppyStream::Open ()
{
    return false;
}

void CFloppyStream::Close ()
{
}

BYTE CFloppyStream::ReadTrack (BYTE cyl_, BYTE head_, BYTE *pbData_)
{
    return RECORD_NOT_FOUND;
}

BYTE CFloppyStream::ReadWrite (bool fRead_, BYTE bSide_, BYTE bTrack_, BYTE* pbData_)
{
    return RECORD_NOT_FOUND;
}

bool CFloppyStream::ReadCustomTrack (BYTE cyl_, BYTE head_, BYTE *pbData_)
{
    return false;
}

bool CFloppyStream::ReadMGTTrack (BYTE cyl_, BYTE head_, BYTE *pbData_)
{
    return false;
}

bool CFloppyStream::IsBusy (BYTE* pbStatus_, bool fWait_)
{
    return false;
}

#endif
