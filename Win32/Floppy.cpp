// Part of SimCoupe - A SAM Coupe emulator
//
// Floppy.cpp: Real floppy access (requires fdrawcmd.sys)
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "SimCoupe.h"
#include "Floppy.h"

#include "Disk.h"
#include "Options.h"
#include "Util.h"

////////////////////////////////////////////////////////////////////////////////

unsigned int __stdcall FloppyThreadProc(void* pv_)
{
    int nRet = reinterpret_cast<CFloppyStream*>(pv_)->ThreadProc();
    _endthreadex(nRet);
    return nRet;
}

bool Ioctl(HANDLE h_, DWORD dwCode_, LPVOID pIn_ = nullptr, DWORD cbIn_ = 0, LPVOID pOut_ = nullptr, DWORD cbOut_ = 0)
{
    if (DeviceIoControl(h_, dwCode_, pIn_, cbIn_, pOut_, cbOut_, &cbOut_, nullptr))
        return true;

    TRACE("!!! Ioctl %lu failed with %#08lx\n", dwCode_, GetLastError());
    return false;
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CFloppyStream::IsSupported()
{
    bool fSupported = false;

    // Open the Service Control manager
    SC_HANDLE hSCM = OpenSCManager(nullptr, nullptr, GENERIC_READ);
    if (hSCM)
    {
        // Open the FDC service, for fdc.sys (floppy disk controller driver)
        SC_HANDLE hService = OpenService(hSCM, "fdc", GENERIC_READ);
        if (hService)
        {
            SERVICE_STATUS ss;
            if (QueryServiceStatus(hService, &ss))
            {
                // If fdc.sys is running then fdrawcmd.sys is supported
                fSupported = ss.dwCurrentState == SERVICE_RUNNING;
            }

            CloseServiceHandle(hService);
        }

        CloseServiceHandle(hSCM);
    }

    return fSupported;
}

/*static*/ bool CFloppyStream::IsAvailable()
{
#ifdef HAVE_FDRAWCMD_H
    static DWORD dwVersion = 0x00000000, dwRet;

    // Open the global driver object
    HANDLE h = CreateFile("\\\\.\\fdrawcmd", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);

    if (h != INVALID_HANDLE_VALUE)
    {
        // Request the driver version number
        DeviceIoControl(h, IOCTL_FDRAWCMD_GET_VERSION, nullptr, 0, &dwVersion, sizeof(dwVersion), &dwRet, nullptr);
        CloseHandle(h);
    }

    // Ensure we're using fdrawcmd.sys >= 1.0.x.x
    return (dwVersion & 0xffff0000) >= (FDRAWCMD_VERSION & 0xffff0000);
#else
    return false;
#endif
}

/*static*/ bool CFloppyStream::IsRecognised(const char* pcszStream_)
{
    return !lstrcmpi(pcszStream_, "A:") || !lstrcmpi(pcszStream_, "B:");
}

CFloppyStream::CFloppyStream(const char* pcszDevice_, bool fReadOnly_)
    : CStream(pcszDevice_, fReadOnly_)
{
    if (IsAvailable())
    {
        const char* pcszDevice = !lstrcmpi(GetFile(), "A:") ? "\\\\.\\fdraw0" : "\\\\.\\fdraw1";
        m_hDevice = CreateFile(pcszDevice, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    }

    // Prepare defaults used when device is closed after use
    Close();
}

CFloppyStream::~CFloppyStream()
{
    BYTE bStatus;
    IsBusy(&bStatus, true);

    if (m_hDevice != INVALID_HANDLE_VALUE)
        CloseHandle(m_hDevice);
}


void CFloppyStream::Close()
{
    m_uSectors = GetOption(stdfloppy) ? NORMAL_DISK_SECTORS : 0;
}

// Start a command executing asynchronously
BYTE CFloppyStream::StartCommand(BYTE bCommand_, PTRACK pTrack_, UINT uSectorIndex_)
{
    UINT uThreadId;

    // Wait for any in-progress operation to complete
    BYTE bStatus;
    IsBusy(&bStatus, true);

    // Set up the command to perform
    m_bCommand = bCommand_;
    m_pTrack = pTrack_;
    m_uSectorIndex = uSectorIndex_;
    m_bStatus = 0;

    // Create a new thread to perform it
    m_hThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, FloppyThreadProc, this, 0, &uThreadId));
    return m_hThread ? BUSY : LOST_DATA;
}

// Get the status of the current asynchronous operation, if any
bool CFloppyStream::IsBusy(BYTE* pbStatus_, bool fWait_)
{
    // Is the worker thread active?
    if (m_hThread)
    {
        // Either wait until the thread completes, or simply check whether it's done
        if (WaitForSingleObject(m_hThread, fWait_ ? INFINITE : 0) == WAIT_TIMEOUT)
            return true;

        // Close the thread handle to delete it
        CloseHandle(m_hThread);
        m_hThread = nullptr;

        // Return the command status
        *pbStatus_ = m_bStatus;
    }

    // Not busy
    return false;
}


///////////////////////////////////////////////////////////////////////////////

// Read a single sector
static BYTE ReadSector(HANDLE hDevice_, BYTE phead_, PSECTOR ps_)
{
#ifdef HAVE_FDRAWCMD_H
    BYTE bStatus = 0;

    FD_READ_WRITE_PARAMS rwp;
    rwp.flags = FD_OPTION_MFM;
    rwp.phead = phead_;
    rwp.cyl = ps_->cyl;
    rwp.head = ps_->head;
    rwp.sector = ps_->sector;
    rwp.size = ps_->size;
    rwp.eot = ps_->sector + 1;
    rwp.gap = 0x0a;
    rwp.datalen = 0xff;

    if (!Ioctl(hDevice_, IOCTL_FDCMD_READ_DATA, &rwp, sizeof(rwp), ps_->pbData, 128 << (ps_->size & 7)))
        bStatus = (GetLastError() == ERROR_CRC) ? CRC_ERROR : RECORD_NOT_FOUND;

    FD_CMD_RESULT res;
    Ioctl(hDevice_, IOCTL_FD_GET_RESULT, nullptr, 0, &res, sizeof(res));
    if (res.st2 & 0x40) bStatus |= DELETED_DATA;

    return bStatus;
#else
    return RECORD_NOT_FOUND;
#endif
}

// Write a single sector
static BYTE WriteSector(HANDLE hDevice_, PTRACK pTrack_, UINT uSectorIndex_)
{
#ifdef HAVE_FDRAWCMD_H
    PSECTOR ps = reinterpret_cast<PSECTOR>(pTrack_ + 1) + uSectorIndex_;

    FD_READ_WRITE_PARAMS rwp;
    rwp.flags = FD_OPTION_MFM;
    rwp.phead = pTrack_->head;
    rwp.cyl = ps->cyl;
    rwp.head = ps->head;
    rwp.sector = ps->sector;
    rwp.size = ps->size;
    rwp.eot = ps->sector + 1;
    rwp.gap = 0x0a;
    rwp.datalen = 0xff;

    if (!Ioctl(hDevice_, IOCTL_FDCMD_WRITE_DATA, &rwp, sizeof(rwp), ps->pbData, 128 << (ps->size & 7)))
    {
        if (GetLastError() == ERROR_WRITE_PROTECT)
            return WRITE_PROTECT;
        else if (GetLastError() == ERROR_SECTOR_NOT_FOUND)
            return RECORD_NOT_FOUND;

        return WRITE_FAULT;
    }

    return 0;
#else
    return WRITE_FAULT;
#endif
}

// Format a track
static BYTE FormatTrack(HANDLE hDevice_, PTRACK pTrack_)
{
#ifdef HAVE_FDRAWCMD_H
    int i, step;
    BYTE bStatus = 0;

    // Prepare space for the variable-size structure - 64 sectors is plenty
    BYTE ab[sizeof(FD_FORMAT_PARAMS) + 64 * sizeof(FD_ID_HEADER)] = { 0 };
    PFD_FORMAT_PARAMS pfp = reinterpret_cast<PFD_FORMAT_PARAMS>(ab);

    PTRACK pt = pTrack_;
    PSECTOR ps = reinterpret_cast<PSECTOR>(pt + 1);
    UINT uSize = pt->sectors ? (128U << (ps->size & 7)) : 0;

    // Set up the format parameters
    pfp->flags = FD_OPTION_MFM;
    pfp->phead = pTrack_->head;
    pfp->size = 6;
    pfp->sectors = pt->sectors;
    pfp->gap = 1;
    pfp->fill = 0x00;

    // If the track contains any sectors, the size/gap/fill are tuned for the size/content
    if (pt->sectors)
    {
        UINT uGap = ((MAX_TRACK_SIZE - 50) - (pt->sectors * (62 + 1 + uSize))) / pt->sectors;
        if (uGap > 46) uGap = 46;

        pfp->size = ps->size;
        pfp->gap = uGap;
        pfp->fill = ps[pt->sectors - 1].pbData[uSize - 1];  // last byte of last sector used for fill byte
    }

    // Prepare the sector headers to write
    for (i = 0; i < pt->sectors; i++)
    {
        pfp->Header[i].cyl = ps[i].cyl;
        pfp->Header[i].head = ps[i].head;
        pfp->Header[i].sector = ps[i].sector;
        pfp->Header[i].size = ps[i].size;
    }

    // For blank tracks we still write a single long sector
    if (!pfp->sectors)
        pfp->sectors++;

    // Format the track
    if (!Ioctl(hDevice_, IOCTL_FDCMD_FORMAT_TRACK, pfp, sizeof(FD_FORMAT_PARAMS) + pfp->sectors * sizeof(FD_ID_HEADER)))
    {
        if (GetLastError() == ERROR_WRITE_PROTECT)
            return WRITE_PROTECT;

        return WRITE_FAULT;
    }

    // Write any in-place format data, as needed by Pro-Dos (and future mixed-sector sizes)
    // 2 interleaved passes over the track is better than risking missing the next sector each time
    for (i = 0, step = 2; i < step; i++)
    {
        for (int j = i; !bStatus && j < pt->sectors; j += step)
        {
            // Skip the write if the contents match the format filler
            if (ps[j].pbData[0] == pfp->fill && !memcmp(ps[j].pbData, ps[j].pbData + 1, uSize - 1))
                continue;

            // Write the sector
            bStatus = WriteSector(hDevice_, pt, j);
        }
    }

    return bStatus;
#else
    return WRITE_FAULT;
#endif
}

// Read a simple track of consecutive sectors, which is fast if it matches what's on the disk
static bool ReadSimpleTrack(HANDLE hDevice_, PTRACK pTrack_, UINT& ruSectors_)
{
#ifdef HAVE_FDRAWCMD_H
    int i;

    PTRACK pt = pTrack_;
    PSECTOR ps = (PSECTOR)(pt + 1);
    PBYTE pb = (PBYTE)(ps + (pt->sectors = ruSectors_));

    // Prepare the normal track container
    for (i = 0; i < pt->sectors; i++)
    {
        ps[i].cyl = pt->cyl;
        ps[i].head = pt->head;
        ps[i].sector = i + 1;
        ps[i].size = 2;
        ps[i].status = 0;
        ps[i].pbData = pb + i * NORMAL_SECTOR_SIZE;
    }

    // Attempt the full track in a single read
    FD_READ_WRITE_PARAMS rwp;
    rwp.flags = FD_OPTION_MFM;
    rwp.phead = pt->head;
    rwp.cyl = pt->cyl;
    rwp.head = pt->head;
    rwp.sector = 1;
    rwp.size = ps->size;
    rwp.eot = pt->sectors + 1;
    rwp.gap = 0x0a;
    rwp.datalen = 0xff;

    if (Ioctl(hDevice_, IOCTL_FDCMD_READ_DATA, &rwp, sizeof(rwp), ps->pbData, pt->sectors * NORMAL_SECTOR_SIZE))
        return true;

    // Accept blank tracks as normal
    if (GetLastError() == ERROR_FLOPPY_ID_MARK_NOT_FOUND)
    {
        pt->sectors = 0;
        return true;
    }

    // Failed to find one of the sectors?
    if (GetLastError() == ERROR_SECTOR_NOT_FOUND)
    {
        FD_CMD_RESULT res;

        // If it was the final sector that failed, it's probably a DOS disk
        if (Ioctl(hDevice_, IOCTL_FD_GET_RESULT, nullptr, 0, &res, sizeof(res)) && res.sector == NORMAL_DISK_SECTORS)
        {
            // Assume 9 sectors for the rest of this session
            pt->sectors = ruSectors_ = DOS_DISK_SECTORS;
            return true;
        }
    }

    // For any other failures, fall back on non-regular mode for a more thorough scan
    ruSectors_ = 0;
    return false;
#else
    return false;
#endif
}

// Read a custom track format, which is slower but more thorough
static bool ReadCustomTrack(HANDLE hDevice_, PTRACK pTrack_)
{
#ifdef HAVE_FDRAWCMD_H
    BYTE abScan[1 + 64 * sizeof(FD_SCAN_RESULT)];
    PFD_SCAN_RESULT psr = (PFD_SCAN_RESULT)abScan;

    // Scan the track layout
    FD_SCAN_PARAMS sp = { FD_OPTION_MFM, pTrack_->head };
    if (!Ioctl(hDevice_, IOCTL_FD_SCAN_TRACK, &sp, sizeof(sp), abScan, sizeof(abScan)))
        return false;

    PTRACK pt = pTrack_;
    PSECTOR ps = (PSECTOR)(pt + 1);
    PBYTE pb = (PBYTE)(ps + (pt->sectors = psr->count));

    // Read the sectors in 2 interleaved passes
    for (int i = 0, step = 2; i < step; i++)
    {
        for (int j = i; j < pt->sectors; j += step)
        {
            ps[j].cyl = psr->Header[j].cyl;
            ps[j].head = psr->Header[j].head;
            ps[j].sector = psr->Header[j].sector;
            ps[j].size = psr->Header[j].size;
            ps[j].status = 0;
            ps[j].pbData = pb;

            // Save individual status values for each sector
            ps[j].status = ReadSector(hDevice_, pt->head, &ps[j]);
            pb += (128U << (ps[j].size & 7));
        }
    }

    return true;
#else
    return false;
#endif
}

unsigned long CFloppyStream::ThreadProc()
{
#ifdef HAVE_FDRAWCMD_H
    FD_SEEK_PARAMS sp = { m_pTrack->cyl, m_pTrack->head };
    Ioctl(m_hDevice, IOCTL_FDCMD_SEEK, &sp, sizeof(sp));

    switch (m_bCommand)
    {
        // Load track contents
    case READ_MSECTOR:
        // If we're in regular mode, try a simple read for all sectors
        if (m_uSectors)
            ReadSimpleTrack(m_hDevice, m_pTrack, m_uSectors);

        // If we're not in regular mode, scan and read individual sectors (slower)
        if (!m_uSectors)
            ReadCustomTrack(m_hDevice, m_pTrack);

        break;

        // Write a sector
    case WRITE_1SECTOR:
        m_bStatus = WriteSector(m_hDevice, m_pTrack, m_uSectorIndex);
        break;

        // Format track
    case WRITE_TRACK:
        m_bStatus = FormatTrack(m_hDevice, m_pTrack);
        break;

    default:
        TRACE("!!! ThreadProc: unknown command (%u)\n", m_bCommand);
        m_bStatus = LOST_DATA;
        break;
    }
#endif

    return 0;
}
