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


/*static*/ bool CFloppyStream::IsRecognised (const char* pcszStream_)
{
    return (GetDriveType(pcszStream_) == DRIVE_REMOVABLE);
}

namespace Floppy
{

bool Init ()
{
    CFloppyStream::LoadDriver();
    return true;
}

void Exit (bool fReInit_/*=true*/)
{
    if (!fReInit_)
        CFloppyStream::UnloadDriver();
}

}   // namespace Floppy



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
    if (!(hManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)))
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





/*
    HANDLE hDevice = CreateFile("\\\\.\\VWIN32", 0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL);

    BYTE* pb = new BYTE[102400];
    memset(pb, 0, 102400);

    for (int i = 0 ; i < 4 ; i++)
    {
        DIOC_REGISTERS regs;
        regs.reg_EAX = 0x0201;       // Read sectors, 10 sectors
        regs.reg_EBX = (DWORD)pb;   // 
        regs.reg_ECX = 0x0001;      // cyl 0, sector 1
        regs.reg_EDX = 0x0000;      // head 0, A:
        regs.reg_Flags = 0x0001;     // assume error (carry flag is set) 

        if (!DIOC_Int13(&reg) || (reg.reg_Flags & 0x0001))
        {
            TRACE("Error reading sectors!\n");  // error if carry flag is set

            reg.reg_EAX = 0x0000;       // Reset disk, <none>
            reg.reg_EDX = 0x0000;       // <none>, A:

            if (!DIOC_Int13(&regs) || (regs.reg_Flags & 0x0001))
            {
                TRACE("Error resetting drive!\n");
                break;
            }
        }
        else
            TRACE("Read %d successful\n");
    }

    delete pb;
    CloseHandle(hDevice);

    return false;   //(GetDriveType(pcszStream_) == DRIVE_REMOVABLE);
}
*/

/*
    {
        m_hDevice = CreateFile("\\\\.\\VWIN32", GENERIC_READ | (fReadOnly_ ? 0 : GENERIC_WRITE),
                                    0,  NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);

        if (m_hDevice != INVALID_HANDLE_VALUE)
            return new CFloppyStream(hDevice, pcszStream_, fReadOnly_);
    }
    else
*/
















CFloppyStream::CFloppyStream (const char* pcszDrive_, bool fReadOnly_)
    : CStream(pcszDrive_, fReadOnly_), m_hDevice(INVALID_HANDLE_VALUE)
{
    // WinNT or W2K?
    if (GetOSType() != osWin9x)
    {
        // Ensure the device driver is loaded
        char szDevice[32] = "\\\\.\\";
        lstrcat(szDevice, pcszDrive_);

        if ((m_hDevice = CreateFile(szDevice, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE)
            Message(msgWarning, "Failed to open floppy device!");
        else
        {
            // This is what we want the disk to look like
            DISK_GEOMETRY dg = { { 80 }, F3_720_512, 2, 10, 512 };

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
/*
    else
    {
        m_hDevice = CreateFile("\\\\.\\VWIN32", 0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL);

        Reset();

        // Check whether the drive supports the change-line, and remember for later use
        DIOC_REGISTERS regs;
        regs.reg_EAX = 0x1500;       // Get disk type, <none>
        regs.reg_EDX = 0x0000;      // <none>, A:
        regs.reg_Flags = 0x0001;     // assume error (carry flag is set)
        m_fChangeSupported = (DIOC_Int13(&regs) && !(regs.reg_Flags & 0x0001) && ((regs.reg_EAX & 0xff00) == 0x0200));

        Lock();
    }
*/
}


void CFloppyStream::Close ()
{
    if (GetOSType() != osWin9x)
    {
        if (m_hDevice && (m_hDevice!= INVALID_HANDLE_VALUE))
            CloseHandle(m_hDevice);
    }
/*
    else
    {
        if (m_hDevice != INVALID_HANDLE_VALUE)
        {
//          Unlock();
            CloseHandle(m_hDevice);
            m_hDevice = INVALID_HANDLE_VALUE;
        }
    }
*/
}


BYTE CFloppyStream::Read (int nSide_, int nTrack_, int nSector_, BYTE* pbData_, UINT* puSize_)
{
    if (GetOSType() != osWin9x)
    {
        long lPos = (nSide_ + 2 * nTrack_) * (10 * 512) + ((nSector_-1) * 512);

        if (SetFilePointer(m_hDevice, lPos, NULL, FILE_BEGIN) == static_cast<DWORD>(lPos))
        {
//          TRACE("Reading sector from %d:%d:%d\n", nSide_, nTrack_, nSector_);

            DWORD dwRead = 0;
            if (ReadFile(m_hDevice, pbData_, *puSize_ = 512, &dwRead, NULL) && (dwRead == 512))
                return 0;
            else
                TRACE("!!! Failed to read side %d, track %d (%#08lx)\n", nSide_, nTrack_, GetLastError());
        }
        else
            TRACE("!!! Failed to seek to side %d, track %d (%#08lx)\n", nSide_, nTrack_, GetLastError());

        *puSize_ = 0;
        return RECORD_NOT_FOUND;
    }

    *puSize_ = 0;
    return RECORD_NOT_FOUND;
}


BYTE CFloppyStream::Write (int nSide_, int nTrack_, int nSector_, BYTE* pbData_, UINT* puSize_)
{
    if (GetOSType() != osWin9x)
    {
        long lPos = (nSide_ + 2 * nTrack_) * (10 * 512) + ((nSector_-1) * 512);

        if (SetFilePointer(m_hDevice, lPos, NULL, FILE_BEGIN) == static_cast<DWORD>(lPos))
        {
//          TRACE("Writing sector to %d:%d:%d\n", nSide_, nTrack_, nSector_);

            DWORD dwWritten = 0;
            if (WriteFile(m_hDevice, pbData_, *puSize_ = 512, &dwWritten, NULL) && (dwWritten == 512))
                return 0;
            else
                TRACE("!!! Failed to write side %d, track %d (%#08lx)\n", nSide_, nTrack_, GetLastError());
        }
        else
            TRACE("!!! Failed to seek to side %d, track %d (%#08lx)\n", nSide_, nTrack_, GetLastError());

    }

    *puSize_ = 0;
    return RECORD_NOT_FOUND;
}
