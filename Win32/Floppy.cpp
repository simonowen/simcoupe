// Part of SimCoupe - A SAM Coupé emulator
//
// Floppy.cpp: Win32 direct floppy access
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

// ToDo:
//  - complete the Win9x support, including caching
//  - maybe release SAMDISK.SYS under GPL after tidying it?

#include "SimCoupe.h"
#include <winioctl.h>
#include "Floppy.h"

#include "CDisk.h"
#include "Util.h"


#define IOCTL_DISK_SET_DRIVE_GEOMETRY   CTL_CODE(IOCTL_DISK_BASE, 0x0849, METHOD_BUFFERED, FILE_READ_DATA|FILE_WRITE_DATA)

#define NT_DRIVER_NAME      "SAMDISK"
#define NT_DRIVER_DESC      "SAM Coupé Disk Filter"


////////////////////////////////////////////////////////////////////////////////

enum { osWin9x, osWinNT, osWin2K };

int GetOSType ()
{
    OSVERSIONINFO osvi;
    osvi.dwOSVersionInfoSize = sizeof osvi;
    GetVersionEx(&osvi);

    if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
        return osWin9x;
    else if (osvi.dwMajorVersion < 5)
        return osWinNT;
    else
        return osWin2K;
}

////////////////////////////////////////////////////////////////////////////////


bool Floppy::Init ()
{
    CFloppyStream::LoadDriver();
    return true;
}

void Floppy::Exit (bool fReInit_/*=true*/)
{
    if (!fReInit_)
        CFloppyStream::UnloadDriver();
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CFloppyStream::LoadDriver ()
{
    bool fRet = false;
    SC_HANDLE hManager, hService = NULL;

    // Form the full path of the driver in the same location that SimCoupe.exe is running from
    const char* pcszDriver = OSD::GetFilePath((GetOSType() == osWin2K) ? "SAMDISK.SYS" : "SAMDISKL.SYS");


    // We can't do anything if the driver file is missing
    if (GetFileAttributes(pcszDriver) == 0xffffffff)
        TRACE("!!! Floppy driver (%s) not found\n", pcszDriver);

    // Open the with the Service Control Manager - requires administrative NT/W2K rights to do what we need!
    else if (!(hManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)))
        TRACE("!!! Failed to open Service Control Manager (%#08lx)\n", GetLastError());
    else
    {
        // Try and open an existing driver
        if (!(hService = OpenService(hManager, NT_DRIVER_NAME, SERVICE_ALL_ACCESS)) && (GetLastError() != ERROR_SERVICE_DOES_NOT_EXIST))
            TRACE("!!! Failed to open %s driver (%#08lx)\n", NT_DRIVER_NAME, GetLastError());

        // If we couldn't find it, create it
        else if (!hService && !(hService = CreateService(hManager, NT_DRIVER_NAME, NT_DRIVER_DESC, SERVICE_ALL_ACCESS,
                    SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, pcszDriver, NULL, NULL, NULL, NULL, NULL)))
            TRACE("!!! Failed to create %s driver (%#08lx)\n", NT_DRIVER_NAME, GetLastError());
        else
        {
            SERVICE_STATUS ss = { 0 };

            // Driver not running?
            if (QueryServiceStatus(hService, &ss) && (ss.dwCurrentState != SERVICE_RUNNING))
            {
                // Start the driver
                if (!StartService(hService, 0, NULL))
                    TRACE("!!! Failed to start %s driver (%#08lx)\n", NT_DRIVER_NAME, GetLastError());
                else
                {
                    DWORD dwStartTime = GetTickCount(), dwWait = ss.dwWaitHint ? ss.dwWaitHint : 3000;

                    // Loop for as long as the wait hint time says
                    do
                    {
                        // Stop looping if the query fails or the device has started
                        if (!QueryServiceStatus(hService, &ss) || (ss.dwCurrentState == SERVICE_RUNNING))
                            break;

                        // Give it 1/4 of a second before checking again
                        Sleep(250);
                    }
                    while ((GetTickCount() - dwStartTime) < dwWait);
                }
            }

            // Driver now running?
            if (!(fRet = (ss.dwCurrentState == SERVICE_RUNNING)))
                TRACE("!!! Failed to start %s driver (%#08lx)\n", NT_DRIVER_NAME, GetLastError());

            // Done with the driver handle
            CloseServiceHandle(hService);
        }

        // Done with the Service Control Manager
        CloseServiceHandle(hManager);
    }

    // Return true if the driver is ready to use
    return fRet;
}

/*static*/ bool CFloppyStream::UnloadDriver ()
{
    SC_HANDLE hManager, hService;
    bool fRet = true;               // Assume success until we learn otherwise

    // Open the with the Service Control Manager - requires administrative NT/W2K rights to do what we need!
    if (!(hManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)))
        TRACE("!!! Failed to open service control manager (%#08lx)\n", GetLastError());
    else
    {
        const char* pcszDriver = OSD::GetFilePath((GetOSType() == osWin2K) ? "SAMDISK.SYS" : "SAMDISKL.SYS");

        // Open the existing service, create it if it's not already there
        if (!(hService = OpenService(hManager, NT_DRIVER_NAME, SERVICE_ALL_ACCESS)))
            TRACE("!!! Failed to open existing %s driver (%#08lx)\n", NT_DRIVER_NAME, GetLastError());
        else
        {
            SERVICE_STATUS ss;

            // From here we know the service exists, so any failures will mean it'll stay in the registry
            fRet = false;

            // Driver not stopped?
            if (QueryServiceStatus(hService, &ss) && (ss.dwCurrentState != SERVICE_STOPPED))
            {
                // Stop the driver
                SERVICE_STATUS ss;
                if (!ControlService(hService, SERVICE_CONTROL_STOP, &ss))
                    TRACE("!!! Failed to stop %s driver (%#08lx)\n", NT_DRIVER_NAME, GetLastError());
                else
                {
                    DWORD dwStartTime = GetTickCount(), dwWait = ss.dwWaitHint ? ss.dwWaitHint : 3000;

                    // Loop for as long as the wait hint time says
                    do
                    {
                        // Stop looping if the query fails or the device has started
                        if (!QueryServiceStatus(hService, &ss) || (ss.dwCurrentState == SERVICE_STOPPED))
                            break;

                        // Give it 1/4 of a second before checking again
                        Sleep(250);
                    }
                    while ((GetTickCount() - dwStartTime) < dwWait);
                }
            }

            // Flag the service for deletion
            if (!(fRet = (DeleteService(hService) == TRUE)))
                TRACE("!!! Failed to delete %s driver (%#08lx)\n", NT_DRIVER_NAME, GetLastError());

            // Close the service - it'll get deleted at this point if all references have gone
            CloseServiceHandle(hService);
        }

        // Done with the Service Control Manager
        CloseServiceHandle(hManager);
    }

    // Return true if the driver is gone
    return fRet;
}

////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CFloppyStream::IsRecognised (const char* pcszStream_)
{
    return (GetDriveType(pcszStream_) == DRIVE_REMOVABLE);
}

CFloppyStream::CFloppyStream (const char* pcszDrive_, bool fReadOnly_)
    : CStream(pcszDrive_, fReadOnly_), m_hDevice(INVALID_HANDLE_VALUE), m_dwResult(ERROR_SUCCESS)
{
    // WinNT or W2K?
    if (GetOSType() != osWin9x)
    {
        // Ensure the device driver is loaded
        char szDevice[32] = "\\\\.\\";
        lstrcat(szDevice, pcszDrive_);

        if ((m_hDevice = CreateFile(szDevice, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                FILE_FLAG_OVERLAPPED, NULL)) != INVALID_HANDLE_VALUE)
        {
            // This is what we want the disk to look like
            DISK_GEOMETRY dg = { { NORMAL_DISK_TRACKS }, F3_720_512, NORMAL_DISK_SIDES, NORMAL_DISK_SECTORS, NORMAL_SECTOR_SIZE };

            DWORD dwRet;
            if (DeviceIoControl(m_hDevice, IOCTL_DISK_SET_DRIVE_GEOMETRY, &dg, sizeof dg, NULL, 0, &dwRet, NULL))
                TRACE("New Geometry set: C=%ld, H=%ld, S=%ld\n", *(DWORD*)&dg.Cylinders, dg.TracksPerCylinder, dg.SectorsPerTrack);
            else
            {
                Message(msgWarning, "Error accessing SAMDISK driver, direct access disabled!");
                CloseHandle(m_hDevice);
                m_hDevice = INVALID_HANDLE_VALUE;
            }
        }
    }
}

void CFloppyStream::RealClose ()
{
    if (GetOSType() != osWin9x)
    {
        if (m_hDevice && (m_hDevice != INVALID_HANDLE_VALUE))
        {
            AbortAsyncOp();
            CloseHandle(m_hDevice);
        }
        m_hDevice = INVALID_HANDLE_VALUE;
    }
}

// Translate a Windows error to an appropriate FDC status code
inline BYTE CFloppyStream::TranslateError () const
{
    switch (m_dwResult) {
        case ERROR_SUCCESS:         return 0;
        case ERROR_WRITE_PROTECT:   return WRITE_PROTECT;
        case ERROR_CRC:             return CRC_ERROR;
        case ERROR_IO_INCOMPLETE:
        case ERROR_IO_PENDING:      return BUSY;
        default:                    return RECORD_NOT_FOUND;
    }
}

BYTE CFloppyStream::Read (UINT uSide_, UINT uTrack_, UINT uSector_, BYTE* pbData_, UINT* puSize_)
{
//  TRACE("Reading sector from %d:%d:%d\n", uSide_, uTrack_, uSector_);
    *puSize_ = 0;

    if ((GetOSType() != osWin9x) && (m_hDevice != INVALID_HANDLE_VALUE))
    {
        AbortAsyncOp();
        ZeroMemory(&m_sOverlapped, sizeof(m_sOverlapped));
        m_sOverlapped.Offset = (uSide_ + NORMAL_DISK_SIDES * uTrack_) * (NORMAL_DISK_SECTORS * NORMAL_SECTOR_SIZE) + ((uSector_-1) * NORMAL_SECTOR_SIZE);
        DWORD dwProcessed = 0;
        if (ReadFile(m_hDevice, pbData_, NORMAL_SECTOR_SIZE, &dwProcessed, &m_sOverlapped))
        {
            if (dwProcessed == NORMAL_SECTOR_SIZE)
            {
                m_dwResult = ERROR_SUCCESS;
                *puSize_ = dwProcessed;
            }
            else
            {
                m_dwResult = ERROR_HANDLE_EOF;
                TRACE("!!! Read (immediate) only %d bytes from side %d, track %d\n", dwProcessed, uSide_, uTrack_);
            }
        }
        else
        {
            m_dwResult = GetLastError();
            if (m_dwResult != ERROR_IO_PENDING)
                TRACE("!!! Failed to read (immediate) from side %d, track %d (%#08lx)\n", uSide_, uTrack_, m_dwResult);
        }
    }
    else
        m_dwResult = ERROR_HANDLE_EOF;

    return TranslateError();
}

BYTE CFloppyStream::Write (UINT uSide_, UINT uTrack_, UINT uSector_, BYTE* pbData_, UINT* puSize_)
{
//  TRACE("Writing sector to %d:%d:%d\n", uSide_, uTrack_, uSector_);
    *puSize_ = 0;

    if ((GetOSType() != osWin9x) && (m_hDevice != INVALID_HANDLE_VALUE))
    {
        AbortAsyncOp();
        ZeroMemory(&m_sOverlapped, sizeof(m_sOverlapped));
        m_sOverlapped.Offset = (uSide_ + NORMAL_DISK_SIDES * uTrack_) * (NORMAL_DISK_SECTORS * NORMAL_SECTOR_SIZE) + ((uSector_-1) * NORMAL_SECTOR_SIZE);
        DWORD dwProcessed = 0;
        if (WriteFile(m_hDevice, pbData_, NORMAL_SECTOR_SIZE, &dwProcessed, &m_sOverlapped))
        {
            if (dwProcessed == NORMAL_SECTOR_SIZE)
            {
                m_dwResult = ERROR_SUCCESS;
                *puSize_ = dwProcessed;
            }
            else
            {
                m_dwResult = ERROR_HANDLE_EOF;
                TRACE("!!! Written (immediate) only %d bytes from side %d, track %d\n", dwProcessed, uSide_, uTrack_);
            }
        }
        else
        {
            m_dwResult = GetLastError();
            if (m_dwResult != ERROR_IO_PENDING)
                TRACE("!!! Failed to write (immediate) to side %d, track %d (%#08lx)\n", uSide_, uTrack_, m_dwResult);
        }
    }
    else
        m_dwResult = ERROR_HANDLE_EOF;

    return TranslateError();
}

// Get the status of the current asynchronous operation, if any
bool CFloppyStream::GetAsyncStatus (UINT* puSize_, BYTE* pbStatus_)
{
    // Only continue if the current operation is asynchronous
    if (m_dwResult == ERROR_IO_PENDING)
    {
        if (HasOverlappedIoCompleted(&m_sOverlapped))
            // Operation has completed
            WaitAsyncOp(puSize_, pbStatus_);
        else
            // Operation is still in progress
            *pbStatus_ = BUSY;
        return true;
    }
    else
        return false;
}

// Wait for the current asynchronous operation to complete, if any
bool CFloppyStream::WaitAsyncOp (UINT* puSize_, BYTE* pbStatus_)
{
    // Only continue if the current operation is asynchronous
    if (m_dwResult == ERROR_IO_PENDING)
    {
        *puSize_ = 0;

        DWORD dwProcessed = 0;
        if (GetOverlappedResult(m_hDevice, &m_sOverlapped, &dwProcessed, TRUE))
        {
            if (dwProcessed == NORMAL_SECTOR_SIZE)
            {
                m_dwResult = ERROR_SUCCESS;
                *puSize_ = dwProcessed;
            }
            else
            {
                m_dwResult = ERROR_HANDLE_EOF;
                TRACE("!!! Processed (asynchronous) only %d bytes\n", dwProcessed);
            }
        }
        else
        {
            m_dwResult = GetLastError();
            TRACE("!!! Failed to complete asynchronous operation (%#08lx)\n", m_dwResult);
        }

        *pbStatus_ = TranslateError();
        return true;
    }

    return false;
}

// Abort the current asynchronous operation, if any
void CFloppyStream::AbortAsyncOp ()
{
    // Only continue if the current operation is asynchronous
    if (m_dwResult == ERROR_IO_PENDING)
    {
        if (CancelIo(m_hDevice))
            m_dwResult = ERROR_SUCCESS;
        else
        {
            m_dwResult = GetLastError();
            TRACE("!!! Failed to abort asynchronous operation (%#08lx)\n", m_dwResult);
        }
    }
}
