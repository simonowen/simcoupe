// Part of SimCoupe - A SAM Coupé emulator
//
// Atom.cpp: ATOM hard disk inteface
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

// For more information on Edwin Blink's ATOM interface, see:
//  http://www.designing.myweb.nl/samcoupe/hardware/atomhdinterface/atom.htm

// ToDo:
//  - a non-BDOS-specific implementation, for when/if other apps use the Atom?
//  - remove STL, to reduce possible porting problems?

#include "SimCoupe.h"
#include "Atom.h"

const unsigned int ATOM_LIGHT_DELAY = 2;    // Number of frames the hard disk LED remains on for after a command
const unsigned int ATOM_CACHE_SIZE = 2;     // Maximum number of disk objects to cache at one time

const char* const ATOM_SUBDIR = "Atom";             // Directory containing disk images for BDOS records
const char* const ATOM_HEADER_FILE = "Atom.dat";    // File containing boot sector and initial sectors holding BDOS record list


CAtomDiskDevice::CAtomDiskDevice ()
{
    // For now the ATOM disk relies on knowledge of the BDOS implementation, so make it easier to use
    m_pDisk = new CBDOSDevice;

    m_uLightDelay = 0;
}


CAtomDiskDevice::~CAtomDiskDevice ()
{
    delete m_pDisk;
}


BYTE CAtomDiskDevice::In (WORD wPort_)
{
    BYTE bRet = 0x00;

    switch (wPort_ & 0xff)
    {
        // Data high
        case 0xf6:
        {
            WORD wData;

            // Determine the latch being read from
            switch (~m_bAddressLatch & (ATOM_NCS1|ATOM_NCS3))
            {
                case ATOM_NCS1:
                    wData = m_pDisk->In(0x01f0 | (m_bAddressLatch & 0x7));
                    break;

                case ATOM_NCS3:
                    wData = m_pDisk->In(0x03f0 | (m_bAddressLatch & 0x7));
                    break;

                default:
                    TRACE("ATOM: Unrecognised read from %#04x\n", wPort_);
                    wData = 0x0000;
                    break;
            }

            // Store the low-byte in the latch and return the high-byte
            m_bDataLatch = wData & 0xff;
            bRet = wData >> 8;
            break;
        }

        // Data low
        case 0xf7:
            // Return the previously stored low-byte
            bRet = m_bDataLatch;
            break;

        default:
            TRACE("ATOM: Unrecognised read from %#04x\n", wPort_);
            break;
    }

    return bRet;
}

void CAtomDiskDevice::Out (WORD wPort_, BYTE bVal_)
{
    BYTE bRet = 0x00;

    switch (wPort_ & 0xff)
    {
        // Address select
        case 0xf5:
            // If the reset pin is low, reset the disk
            if (!(bVal_ & ATOM_NRESET))
                m_pDisk->Reset();

            m_bAddressLatch = bVal_;
            break;

        // Data high - store in the latch for later
        case 0xf6:
            m_bDataLatch = bVal_;
            break;

        // Data low
        case 0xf7:
            m_uLightDelay = ATOM_LIGHT_DELAY;

            // Return the previously stored low-byte
            bRet = m_bDataLatch;

            // Determine the latch being written to
            switch (~m_bAddressLatch & (ATOM_NCS1|ATOM_NCS3))
            {
                case ATOM_NCS1:
                    m_pDisk->Out(0x01f0 | (m_bAddressLatch & 0x7), (static_cast<WORD>(m_bDataLatch) << 8) | bVal_);
                    break;

                case ATOM_NCS3:
                    m_pDisk->Out(0x03f0 | (m_bAddressLatch & 0x7), (static_cast<WORD>(m_bDataLatch) << 8) | bVal_);
                    break;
            }
            break;

        default:
            TRACE("Atom: Unhandled write to %#04x with %#02x\n", wPort_, bVal_);
            break;
    }
}

void CAtomDiskDevice::FrameEnd ()
{
    // If the drive light is currently on, reduce the counter
    if (m_uLightDelay)
        m_uLightDelay--;
}

////////////////////////////////////////////////////////////////////////////////

CBDOSDevice::CBDOSDevice ()
{
    Init();
}

CBDOSDevice::~CBDOSDevice ()
{
    // Close the hard disk header file
    if (m_hfDisk)
        fclose(m_hfDisk);

    // Save and delete any cached disk objects
    for (CACHELIST::iterator it = m_lCached.begin() ; it != m_lCached.end() ; delete (*it).second, it = m_lCached.erase(it));
}


bool CBDOSDevice::Init ()
{
    // The default boot sector is empty, but if there's an existing boot sector we'll use that instead
    BYTE abBootSector[512];
    memset(abBootSector, 0, sizeof abBootSector);

    // Form the full path of the file used for boot sector and record list
    string sDir = OSD::GetFilePath(ATOM_SUBDIR), sFile = sDir + PATH_SEPARATOR + ATOM_HEADER_FILE;

    // Create the directory if it doesn't already exist
    if (mkdir(sDir.c_str(), S_IRWXU))
        TRACE("!!! Failed to create %s dir (%d)\n", sDir.c_str(), errno);

    // Try and open an existing file, or create a new one
    if ((m_hfDisk = fopen(sFile.c_str(), "rb")) != NULL)
    {
        // Read the existing boot sector, and close the file
        fread(abBootSector, 1, sizeof abBootSector, m_hfDisk);
        fclose(m_hfDisk);
    }

    // Create the new boot sector/record list file, and write the boot sector into it
    if (!(m_hfDisk = fopen(sFile.c_str(), "wb+")) || !fwrite(abBootSector, 1, sizeof abBootSector, m_hfDisk))
    {
        TRACE("!!! Error writing to %s!\n", sFile.c_str());
        return false;
    }

    // Open the directory for enumeration
    DIR* hDir = opendir(sDir.c_str());
    if (hDir)
    {
        // Add the full path of each directory entry to a list
        for (struct dirent* pDir ; (pDir = readdir(hDir)) ; )
            m_asRecords.push_back(sDir + PATH_SEPARATOR + pDir->d_name);

        closedir(hDir);
    }

    // Sort the directory list so the order is consistent/predictable
    sort(m_asRecords.begin(), m_asRecords.end());

    // Process each directory entry, look for valid disk image files
    for (STRINGARRAY::iterator it = m_asRecords.begin() ; it != m_asRecords.end() ;)
    {
        bool fValid = false;

        // Try to open the entry as a disk image
        CDisk* pDisk = CDisk::Open((*it).c_str());

        // Valid image?
        if (pDisk)
        {
            UINT uSize;

            // Don't bother with the image if the first sector is unreadable
            if (fValid = (pDisk->FindSector(0, 0, 1) && !pDisk->ReadData(m_abSectorData, &uSize)))
            {
                BYTE abLabel[16] = {0};

                // Check for a BDOS disk signature
                if (!memcmp(m_abSectorData + 0xe8, "BDOS", 4))
                {
                    // Assmeble the 16 character label from its two locations
                    memcpy(abLabel,     m_abSectorData + 0xd2, 10);
                    memcpy(abLabel+10,  m_abSectorData + 0xfa, 6);
                }

                // Is this MasterDOS (or rather, not SAMDOS)
                else if (m_abSectorData[0xd2] != 0x00 && m_abSectorData[0xd2] != 0xff)
                {
                    // Copy the label unless we find the special 'no label' marker
                    if (m_abSectorData[0xd2] != '*')
                        memcpy(abLabel, m_abSectorData + 0xd2, 10);
                }

                // Write out the record name to the list, and add the filename to the record list
                if (fwrite(abLabel, 1, 16, m_hfDisk))
                    m_asRecords.push_back(sFile);
            }

            // We're done with the disk (for now)
            delete pDisk;
        }

        // Discard invalid records
        if (!fValid)
            it = m_asRecords.erase(it);
        else
            it++;
    }

    // Find out how many records we have, and calculate the number of sectors for the boot sector and record list
    UINT uRecords = static_cast<UINT>(m_asRecords.size());
    m_uReservedSectors = 1 + ((uRecords + 31) / 32);    // Boot sector + record list sectors
    UINT uTotalSectors = m_uReservedSectors + (uRecords * (NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS * NORMAL_DISK_SECTORS));
    TRACE("%u records requires %u sectors (%u reserved)\n", uRecords, uTotalSectors, m_uReservedSectors);

    // Set up the device identity, mainly for the disk geometry (which is a perfect match to the the number of sectors needed)
    memset(&m_sIdentity, 0, sizeof m_sIdentity);
    m_sIdentity.wCaps = 0x0040;         // Fixed device
    m_sIdentity.wLongECCBytes = 4;      // 4 ECC bytes passed to host on READ LONG and WRITE LONG

    // To avoid problems with the cylinder getting too large with over 40 records, we'll do an LBA type trick
    // We can add up to 40 extra sectors without BDOS creating an additional small record on the end
    m_sIdentity.wLogicalHeads = 2;
    m_sIdentity.wSectorsPerTrack = 20;
    m_sIdentity.wLogicalCylinders = (uTotalSectors+39) / 40;

    return true;
}

bool CBDOSDevice::DiskReadWrite (bool fWrite_)
{
    bool fRet = false;

    WORD wCylinder = (static_cast<WORD>(m_sRegs.bCylinderHigh) << 8) | m_sRegs.bCylinderLow;
    BYTE bHead = (m_sRegs.bDriveAddress >> 2) & 0x0f, bSector = m_sRegs.bSector;
    TRACE("%s CHS %u:%u:%u\n", fWrite_ ? "Writing" : "Reading", wCylinder, bHead, bSector);

    // Calculate the logical block number from the CHS position
    UINT uBlock = (wCylinder * m_sIdentity.wLogicalHeads + bHead) * m_sIdentity.wSectorsPerTrack + (bSector - 1);

    if (uBlock < m_uReservedSectors)
    {
        // Locate the block
        if (m_hfDisk && !fseek(m_hfDisk, uBlock << 9, SEEK_SET))
        {
            // Write the data if we're writing
            if (fWrite_)
                fRet = fwrite(&m_abSectorData, 1, sizeof m_abSectorData, m_hfDisk) != 0;

            // Sector read
            else
            {
                // Clear the sector and read what we can (might be a part sector in the case of the record list)
                memset(m_abSectorData, 0, sizeof m_abSectorData);
                fread(&m_abSectorData, 1, sizeof m_abSectorData, m_hfDisk);

                fRet = true;
            }
        }
    }
    else
    {
        // Adjust the position to the start of the record data
        uBlock -= m_uReservedSectors;

        // Determine which record we're dealing with
        UINT uRecordBlocks = (NORMAL_DISK_SIDES * NORMAL_DISK_TRACKS * NORMAL_DISK_SECTORS);
        UINT uRecord = uBlock / uRecordBlocks;

        // Determine the position within the record/disk
        UINT uSector = (uBlock % NORMAL_DISK_SECTORS) + 1;
        UINT uTrack = (uBlock /= NORMAL_DISK_SECTORS) % NORMAL_DISK_TRACKS;
        UINT uSide = (uBlock /= NORMAL_DISK_TRACKS) & 1;


        CDisk* pDisk = NULL;

        // Look for a cached disk for the current record
        for (CACHELIST::iterator it = m_lCached.begin() ; it != m_lCached.end() ; it++)
        {
            // Found it?
            if ((*it).first == uRecord)
            {
                // Extract the disk object, and make the entry MRU
                pDisk = (*it).second;
                m_lCached.push_front(*it);
                m_lCached.erase(it);
                break;
            }
        }

        // If we didn't find a cached disk, load it now
        if (!pDisk && uRecord < (int)m_asRecords.size() && (pDisk = CDisk::Open(m_asRecords[uRecord].c_str())))
        {
            // Is the cache full?
            if (m_lCached.size() >= ATOM_CACHE_SIZE)
            {
                // Yep, so delete the LRU entry
                delete m_lCached.back().second;
                m_lCached.pop_back();
            }

            // Add the new entry as MRU
            m_lCached.push_front(CACHELIST::value_type(uRecord,pDisk));
        }

        // No disk, off the end of the disk, or sector not found?
        if (pDisk && pDisk->FindSector(uSide, uTrack, uSector))
        {
            UINT uSize;

            // Write the data if we're writing
            if (fWrite_ && !pDisk->WriteData(m_abSectorData, &uSize))
                fRet = true;

            // Read the data if we're reading
            else if (!fWrite_ && !pDisk->ReadData(m_abSectorData, &uSize))
            {
                fRet = true;

                // Add the BDOS signature to the data if this is the first directory sector, to keep BDOS happy
                if (!uSide && !uTrack && uSector == 1)
                {
                    // If there's no BDOS signature, clear the record/volume label to stop it displaying as junk
                    if (memcmp(m_abSectorData + 0xe8, "BDOS", 4))
                        m_abSectorData[0xd2] = '\0';

                    // If the floppy image is write-protected, mark the record as such too
                    if (pDisk->IsReadOnly())
                        m_abSectorData[0xd2] |= 0x80;

                    // Overlay the signature so BDOS accepts this record as valid
                    memcpy(m_abSectorData+0xe8, "BDOS", 4);
                }
            }
        }
    }

    return fRet;
}
