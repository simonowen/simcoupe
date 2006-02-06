// Part of SimCoupe - A SAM Coupe emulator
//
// Floppy.cpp: W2K/XP/W2K3 direct floppy access using fdrawcmd.sys
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

#include "SimCoupe.h"
#include <process.h>    // for _beginthreadex/_endthreadex
#include <fdrawcmd.h>   // fdrawcmd.sys definitions

#include "Floppy.h"
#include "CDisk.h"
#include "Util.h"

////////////////////////////////////////////////////////////////////////////////

unsigned int __stdcall FloppyThreadProc (void *pv_)
{
    int nRet = reinterpret_cast<CFloppyStream*>(pv_)->ThreadProc();
    _endthreadex(nRet);
    return nRet;
}

bool Ioctl (HANDLE h_, DWORD dwCode_, LPVOID pIn_=NULL, DWORD cbIn_=0, LPVOID pOut_=NULL, DWORD cbOut_=0)
{
    DWORD dwRet;
    bool f = !!DeviceIoControl(h_, dwCode_, pIn_,cbIn_, pOut_,cbOut_, &dwRet, NULL);

    if (!f)
        TRACE("!!! Ioctl failed  with %#08lx\n", GetLastError());

    return f;
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CFloppyStream::IsAvailable ()
{
    static DWORD dwVersion = 0x00000000;

    // Open the global driver object
    HANDLE h = CreateFile("\\\\.\\fdrawcmd", GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD dwRet;

        // Request the driver version number
        DeviceIoControl(h, IOCTL_FDRAWCMD_GET_VERSION, NULL, 0, &dwVersion, sizeof(dwVersion), &dwRet, NULL);
        CloseHandle(h);
    }

    // Ensure we're using fdrawcmd.sys >= 1.0.x.x
    return (dwVersion & 0xffff0000) >= (FDRAWCMD_VERSION & 0xffff0000);
}

/*static*/ bool CFloppyStream::IsRecognised (const char* pcszStream_)
{
    return !lstrcmpi(pcszStream_, "A:") || !lstrcmpi(pcszStream_, "B:");
}

CFloppyStream::CFloppyStream (const char* pcszDevice_, bool fReadOnly_)
    : CStream(pcszDevice_, fReadOnly_), m_hDevice(INVALID_HANDLE_VALUE), m_hThread(NULL), m_fMGT(true)
{
    OSVERSIONINFO osvi = { sizeof osvi };
    GetVersionEx(&osvi);

    // W2K or higher?
    if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT && osvi.dwMajorVersion >= 5)
    {
        const char* pcszDevice = !lstrcmpi(GetFile(), "A:") ? "\\\\.\\fdraw0" : "\\\\.\\fdraw1";
        m_hDevice = CreateFile(pcszDevice, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    }
}

CFloppyStream::~CFloppyStream ()
{
    BYTE bStatus;
    IsBusy(&bStatus, true);

    if (m_hDevice != INVALID_HANDLE_VALUE)
        CloseHandle(m_hDevice);
}


void CFloppyStream::Close ()
{
}

bool ReadSector (HANDLE h_, BYTE phead_, PSECTOR ps_)
{
    FD_READ_WRITE_PARAMS rwp = { FD_OPTION_MFM, phead_, ps_->cyl,ps_->head,ps_->sector,ps_->size, ps_->sector+1,0x0a,0xff };
    if (!Ioctl(h_, IOCTL_FDCMD_READ_DATA , &rwp, sizeof(rwp), ps_->pbData, 128 << (ps_->size & 3)))
    {
        if (GetLastError() == ERROR_CRC)
            ps_->status = CRC_ERROR;
        else
            return false;
    }

    return true;
}

bool WriteSector (HANDLE h_, BYTE phead_, PSECTOR ps_)
{
    FD_READ_WRITE_PARAMS rwp = { FD_OPTION_MFM, phead_, ps_->cyl,ps_->head,ps_->sector,ps_->size, ps_->sector+1,0x0a,0xff };
    if (!Ioctl(h_, IOCTL_FDCMD_WRITE_DATA , &rwp, sizeof(rwp), ps_->pbData, 128 << (ps_->size & 3)))
    {
        if (GetLastError() == ERROR_WRITE_PROTECT)
            ps_->status = WRITE_PROTECT;
        else if (GetLastError() == ERROR_SECTOR_NOT_FOUND)
            ps_->status = RECORD_NOT_FOUND;
            return false;
    }

    return true;
}

bool CFloppyStream::ReadCustomTrack (BYTE cyl_, BYTE head_, PBYTE pbData_)
{
    BYTE abScan[1+128*sizeof(FD_SCAN_RESULT)];
    PFD_SCAN_RESULT psr = (PFD_SCAN_RESULT)abScan;

    FD_SCAN_PARAMS sp = { FD_OPTION_MFM, head_ };
    if (!Ioctl(m_hDevice, IOCTL_FD_SCAN_TRACK, &sp, sizeof(sp), abScan, sizeof(abScan)))
        return false;

    PTRACK pt = (PTRACK)pbData_;
    PSECTOR ps = (PSECTOR)(pt+1);
    PBYTE pb = (PBYTE)(ps+(pt->sectors = psr->count));

    // Read the track in 2 interleaved passes
    for (int i = 0, step = 2 ; i < step ; i++)
    {
        for (int j = i ; j < pt->sectors ; j += step)
        {
            ps[j].cyl = psr->Header[j].cyl;
            ps[j].head = psr->Header[j].head;
            ps[j].sector = psr->Header[j].sector;
            ps[j].size = psr->Header[j].size;
            ps[j].status = 0;
            ps[j].pbData = pb;

            ReadSector(m_hDevice, head_, &ps[j]);
            pb += 128 << (ps[j].size & 3);
        }
    }

    return true;
}

bool CFloppyStream::ReadMGTTrack (BYTE cyl_, BYTE head_, PBYTE pbData_)
{
    int i;

    PTRACK pt = (PTRACK)pbData_;
    PSECTOR ps = (PSECTOR)(pt+1);
    PBYTE pb = (PBYTE)(ps+(pt->sectors=NORMAL_DISK_SECTORS));

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

    for (BYTE sector = 1 ; sector <= pt->sectors ; sector++)
    {
        FD_READ_WRITE_PARAMS rwp = { FD_OPTION_MFM, head_, cyl_,head_,sector,2, pt->sectors+1,0x0a,0xff };

        // Read the sector(s)
        if (Ioctl(m_hDevice, IOCTL_FDCMD_READ_DATA , &rwp, sizeof(rwp), ps[sector-1].pbData, (pt->sectors-sector+1)*NORMAL_SECTOR_SIZE))
            break;

        DWORD dwError = GetLastError();

        // Accept blank tracks as normal
        if (dwError == ERROR_FLOPPY_ID_MARK_NOT_FOUND)
        {
            pt->sectors = 0;
            break;
        }

        // Any other error is treated as a failure to read the MGT track
        pt->sectors = 0;
        return false;
    }

    // Process the sector list to check for irregularities
    for (i = 0 ; i < pt->sectors ; i++)
    {
        // Sector missing?
        if (ps[i].status & RECORD_NOT_FOUND)
        {
            // Remove it from the list
            memcpy(&ps[i], &ps[i+1], (pt->sectors-i-1)*sizeof(*ps));
            pt->sectors--;
            i--;
        }
    }

    return true;
}

unsigned long CFloppyStream::ThreadProc ()
{
    FD_SEEK_PARAMS sp = { m_bTrack, m_bSide };
    Ioctl(m_hDevice, IOCTL_FDCMD_SEEK, &sp, sizeof(sp));

    if (m_bCommand == READ_MSECTOR)
    {
        if (m_fMGT)
            m_fMGT = ReadMGTTrack(m_bTrack, m_bSide, m_pbData);

        if (!m_fMGT)
            ReadCustomTrack(m_bTrack, m_bSide, m_pbData);
    }
    else if (m_bCommand == WRITE_1SECTOR)
    {
        // ToDo: write the sector
    }

    return 0;
}

BYTE CFloppyStream::ReadTrack (BYTE cyl_, BYTE head_, PBYTE pbData_)
{
    BYTE bStatus;

    // Wait for any in-progress operation to complete
    IsBusy(&bStatus, true);

    // Set up the command to perform
    m_bCommand = READ_MSECTOR;
    m_bTrack = cyl_;
    m_bSide = head_;
    m_pbData = pbData_;
    m_bStatus = 0;

    TRACE("### ReadTrack: cyl=%u head=%u\n", cyl_, head_);

    // Create a new thread to perform it
    UINT uThreadId;
    m_hThread = reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, FloppyThreadProc, this, 0, &uThreadId));

    return m_hThread ? BUSY : 0;
}


BYTE CFloppyStream::ReadWrite (bool fRead_, BYTE head_, BYTE cyl_, BYTE* pbData_)
{
    BYTE bStatus;

    // Wait for any in-progress operation to complete
    IsBusy(&bStatus, true);

    // Set up the command to perform
    m_bCommand = WRITE_1SECTOR;
    m_bTrack = cyl_;
    m_bSide = head_;
    m_pbData = pbData_;
    m_bStatus = 0;

    // Create a new thread to perform it
    UINT uThreadId;
    m_hThread = reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, FloppyThreadProc, this, 0, &uThreadId));

    return m_hThread ? BUSY : 0;
}

// Get the status of the current asynchronous operation, if any
bool CFloppyStream::IsBusy (BYTE* pbStatus_, bool fWait_)
{
    // Is the worker thread active?
    if (m_hThread)
    {
        // Either wait until the thread completes, or simply check whether it's done
        if (WaitForSingleObject(m_hThread, fWait_ ? INFINITE : 0) == WAIT_TIMEOUT)
            return true;

        // Close the thread handle to delete it
        CloseHandle(m_hThread);
        m_hThread = NULL;

        // Return the command status
        *pbStatus_ = m_bStatus;
    }

    // Not busy
    return false;
}
