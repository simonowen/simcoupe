// Part of SimCoupe - A SAM Coupe emulator
//
// Drive.cpp: VL1772-02 floppy disk controller emulation
//
//  Copyright (c) 1999-2012 Simon Owen
//  Copyright (c) 1996-2001 Allan Skillman
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
//  - real delayed spin-up (including 'hang' when command sent and no disk present)
//  - data timeouts for type 2 commands

#include "SimCoupe.h"
#include "Drive.h"

#include "CPU.h"

////////////////////////////////////////////////////////////////////////////////

CDrive::CDrive (CDisk* pDisk_/*=NULL*/)
    : m_pDisk(pDisk_), m_pbBuffer(NULL)
{
    Reset ();

    // Head starts over track 0, motor off
    m_bHeadCyl = 0;
    m_nMotorDelay = 0;
}

// Reset the controller back to default settings
void CDrive::Reset ()
{
    // Initialise registers
    m_sRegs.bCommand = 0;
    m_sRegs.bStatus = 0;
    m_sRegs.bTrack = 0xff;
    m_sRegs.bSector = 1;
    m_sRegs.bData = 0;
    m_sRegs.fDir = false;

    m_uBuffer = 0;
    m_bDataStatus = 0;
}

// Insert a new disk from the named source (usually a file)
bool CDrive::Insert (const char* pcszSource_, bool fAutoLoad_)
{
    Eject();

    // Open the new disk image
    m_pDisk = CDisk::Open(pcszSource_);
    if (!m_pDisk)
        return false;

    // Check for auto-booting with drive 1
    if (this == pFloppy1 && fAutoLoad_)
        IO::AutoLoad(AUTOLOAD_DISK);

    return true;
}

// Eject any inserted disk
void CDrive::Eject ()
{
    if (m_pDisk && m_pDisk->IsModified())
        m_pDisk->Save();

    delete m_pDisk, m_pDisk = NULL;
}

void CDrive::FrameEnd ()
{
    // Base implementation includes default activity handling
    CDiskDevice::FrameEnd();

    // If the motor hasn't been used for 2 seconds, switch it off
    if (m_nMotorDelay && !--m_nMotorDelay)
    {
        // Clear the motor-on bit
        m_sRegs.bStatus &= ~MOTOR_ON;

        // Close any real floppy device to ensure any changes are flushed
        if (m_pDisk)
            m_pDisk->Flush();
    }
}

////////////////////////////////////////////////////////////////////////////////


inline void CDrive::ModifyStatus (BYTE bSet_, BYTE bReset_)
{
    // Reset then set the specified bits
    m_sRegs.bStatus &= ~bReset_;
    m_sRegs.bStatus |= bSet_;

    // If the motor enable bit is set, update the last used time
    if (bSet_ & MOTOR_ON)
        m_nMotorDelay = FLOPPY_MOTOR_TIMEOUT;
}

// Set the status of a read operation before the data has been read by the CPU
inline void CDrive::ModifyReadStatus ()
{
    // Report errors (other than CRC errors) and busy status (from asynchronous operations)
    if (m_bDataStatus & ~CRC_ERROR)
        ModifyStatus(m_bDataStatus, BUSY);

    // Otherwise signal that data is available for reading
    else
        ModifyStatus(DRQ, 0);
}


void CDrive::ExecuteNext ()
{
    BYTE bStatus = m_sRegs.bStatus;

    // Nothing to do if there's no disk in the drive
    if (!m_pDisk)
        return;

    // Continue processing the background
    if (m_pDisk->IsBusy(&bStatus))
    {
        // Keep the drive motor on as we're busy
        ModifyStatus(MOTOR_ON, 0);
        return;
    }

    // Some commands require additional handling
    switch (m_sRegs.bCommand & FDC_COMMAND_MASK)
    {
        case READ_1SECTOR:
        case READ_MSECTOR:
        {
            IDFIELD id;

            if (!FindSector(&id))
                ModifyStatus(RECORD_NOT_FOUND, BUSY);
            else
            {
                // Read the data, reporting anything but CRC errors now, as we can't check the CRC until we reach it at the end of the data on the disk
                m_bDataStatus = ReadSector(m_pbBuffer = m_abBuffer, &m_uBuffer);
                ModifyReadStatus();

                // Just for fun ;-)
                if (m_sRegs.bTrack == 4 && m_sRegs.bSector == 1 && m_abBuffer[0x016] == 0xC3 && CrcBlock(m_abBuffer, m_uBuffer) == 0x6c54)
                    m_abBuffer[0x016] -= 0x37;
            }
            break;
        }

        case WRITE_1SECTOR:
        case WRITE_MSECTOR:
        {
            if (m_nState == 0)
            {
                IDFIELD id;

                // Locate the sector, reset busy and signal record not found if we couldn't find it
                if (!FindSector(&id))
                    ModifyStatus(RECORD_NOT_FOUND, BUSY);
                else if (m_pDisk->IsReadOnly())
                    ModifyStatus(WRITE_PROTECT, BUSY);
                else
                {
                    // Prepare data pointer to receive data, and the amount we're expecting
                    m_pbBuffer = m_abBuffer;
                    m_uBuffer = 128U << (id.bSize & 3);

                    // Signal that data is now requested for writing
                    ModifyStatus(DRQ, 0);
                    m_nState++;
                }
            }
            else
            {
                // Write complete, so set its status and clear busy
                ModifyStatus(bStatus, BUSY);
            }

            break;
        }

        case READ_ADDRESS:
        {
            // Read an ID field into our general buffer
            IDFIELD* pId = reinterpret_cast<IDFIELD*>(m_pbBuffer = m_abBuffer);
            BYTE bStatus = ReadAddress(pId);

            // If successful set up the number of bytes available to read
            if (!(bStatus & TYPE23_ERROR_MASK))
            {
                m_sRegs.bSector = pId->bTrack;

                m_uBuffer = sizeof(IDFIELD);
                ModifyStatus(bStatus|DRQ, 0);   // Don't clear BUSY yet!
            }

            // Set the error status, resetting BUSY so the client sees the error
            else
            {
                ModifyStatus(bStatus, BUSY);
                m_uBuffer = 0;
            }

            break;
        }

        case READ_TRACK:
        {
            // Prepare a semi-convincig raw track
            ReadTrack(m_pbBuffer = m_abBuffer, m_uBuffer = sizeof(m_abBuffer));
            ModifyStatus(DRQ, 0);
            break;
        }

        case WRITE_TRACK:
        {
            ModifyStatus(bStatus, BUSY);
            break;
        }
    }
}


BYTE CDrive::In (WORD wPort_)
{
    BYTE bRet = 0x00;

    // Continue command execution if we're busy but not transferring data
    if ((m_sRegs.bStatus & (BUSY|DRQ)) == BUSY)
        ExecuteNext();

    // Register to read from is the bottom 3 bits of the port
    switch (wPort_ & 0x03)
    {
        case regStatus:
        {
            // Return value is the status byte
            bRet = m_sRegs.bStatus;

            // Type 1 command mode uses more status bits
            if ((m_sRegs.bCommand & FDC_COMMAND_MASK) <= STEP_OUT_UPD)
            {
                // Set the track 0 bit state
                if (!m_bHeadCyl)
                {
                    bRet |= TRACK00;
                    m_sRegs.bTrack = 0;         // this is updated even in non-update mode!
                }

                // The following only apply if there's a disk in the drive
                if (m_pDisk)
                {
                    // Set the write protect bit if the disk is read-only
                    if (m_pDisk->IsReadOnly())
                        bRet |= WRITE_PROTECT;

                    // If spin-up wasn't disabled, flag it complete
                    if (!(m_sRegs.bCommand & CMD_FLAG_SPINUP))
                        bRet |= SPIN_UP;

                    // Toggle the index pulse status bit periodically to show the disk is spinning
                    static int n = 0;
                    if (IsMotorOn() && !(++n % 1024))   // FIXME: use an event for the correct index timing
                        bRet |= INDEX_PULSE;
                }
            }

            // SAM DICE relies on a strange error condition, which requires special handling
            else if ((m_sRegs.bCommand & FDC_COMMAND_MASK) == READ_ADDRESS)
            {
                static int nBusyTimeout = 0;

                // Clear busy after 16 polls of the status port
                if (!(bRet & BUSY))
                    nBusyTimeout = 0;
                else if (!(++nBusyTimeout & 0x0f))
                {
                    ModifyStatus(0, BUSY);
                    m_bSectorIndex = 0;
                }
            }

            break;
        }

        case regTrack:
            // Return the current track register value (may not match the current physical head position)
            bRet = m_sRegs.bTrack;
            TRACE("Disk track: returning %#02x\n", bRet);
            break;

        case regSector:
            // Return the current sector register value
            bRet = m_sRegs.bSector;
//          TRACE("Disk sector: returning %#02x\n", byte);
            break;

        case regData:
        {
            // Data available?
            if (m_uBuffer)
            {
                // Read the next byte into the data register
                m_sRegs.bData = *m_pbBuffer++;
                m_uBuffer--;

                // Has all the data been read?
                if (!m_uBuffer)
                {
                    // Reset BUSY and DRQ to show we're done
                    ModifyStatus(0, BUSY|DRQ);

                    // Some commands require additional handling
                    switch (m_sRegs.bCommand & FDC_COMMAND_MASK)
                    {
                        case READ_ADDRESS:
                            break;

                        case READ_TRACK:
                            // No more data available
                            ModifyStatus(RECORD_NOT_FOUND, 0);
                            break;

                        case READ_1SECTOR:
                            // Set the data read status to include data CRC errors
                            ModifyStatus(m_bDataStatus, 0);
                            break;

                        case READ_MSECTOR:
                            // Set the data read status to include data CRC errors, and only continue if ok
                            ModifyStatus(m_bDataStatus, 0);
                            if (!m_bDataStatus)
                            {
                                IDFIELD id;

                                // Advance the sector number
                                m_sRegs.bSector++;

                                // Are there any more sectors to return?
                                if (FindSector(&id))
                                {
                                    TRACE("FDC: Multiple-sector read moving to sector %d\n", id.bSector);

                                    // Read the data, reporting anything but CRC errors now
                                    m_bDataStatus = ReadSector(m_pbBuffer = m_abBuffer, &m_uBuffer);
                                    ModifyReadStatus();
                                }
                            }
                            break;

                        default:
                            TRACE("Data requested for unknown command (%d)!\n", m_sRegs.bCommand);
                    }
                }
            }

            // Return the data register value
            bRet = m_sRegs.bData;
        }
    }

    return bRet;
}


void CDrive::Out (WORD wPort_, BYTE bVal_)
{
    // Extract side from port address
    m_bSide = ((wPort_) >> 2) & 1;

    // Register to write to is the bottom 3 bits of the port
    switch (wPort_ & 0x03)
    {
        case regCommand:
        {
            m_sRegs.bCommand = bVal_;

            // If we're busy, accept only the FORCE_INTERRUPT command
            if ((m_sRegs.bStatus & BUSY) && (m_sRegs.bCommand & FDC_COMMAND_MASK) != FORCE_INTERRUPT)
                return;

            // Reset drive activity counter
            m_uActive = FLOPPY_ACTIVE_FRAMES;

            // Reset the status (except motor state) as we're starting a new command
            ModifyStatus(m_sRegs.bStatus = MOTOR_ON, 0);
            m_nState = 0;

            // The main command is taken from the top 2
            switch (m_sRegs.bCommand & FDC_COMMAND_MASK)
            {
                // Type I commands

                // Restore disk head to track 0
                case RESTORE:
                {
                    TRACE("FDC: RESTORE\n");

                    // Move to track 0
                    m_sRegs.bTrack = m_bHeadCyl = 0;
                    break;
                }

                // Seek the track in the data register
                case SEEK:
                {
                    TRACE("FDC: SEEK to track %d\n", m_sRegs.bData);

                    // Move the head and update the direction flag
                    m_sRegs.fDir = (m_sRegs.bData > m_sRegs.bTrack);
                    m_sRegs.bTrack = m_bHeadCyl = m_sRegs.bData;

                    break;
                }

                // Step in/out with/without update
                case STEP_UPD:
                case STEP_NUPD:
                case STEP_IN_UPD:
                case STEP_IN_NUPD:
                case STEP_OUT_UPD:
                case STEP_OUT_NUPD:
                {
                    TRACE("FDC: STEP to cyl %d\n", m_bHeadCyl);

                    // Step in/out commands update the direction flag
                    if (m_sRegs.bCommand & CMD_FLAG_STEPDIR)
                        m_sRegs.fDir = (m_sRegs.bCommand & CMD_FLAG_DIR) != 0;

                    // Step the head according to the direction flag
                    if (!m_sRegs.fDir)
                        m_bHeadCyl++;
                    else if (m_bHeadCyl > 0)
                        m_bHeadCyl--;

                    // Update the track register if required
                    if (m_sRegs.bCommand & CMD_FLAG_UPDATE)
                        m_sRegs.bTrack = m_bHeadCyl;

                    break;
                }


                // Type II Commands

                // Read one or multiple sectors
                case READ_1SECTOR:
                case READ_MSECTOR:
                {
                    TRACE("FDC: READ_xSECTOR (from cyl %d, head %d, sector %d)\n", m_bHeadCyl, m_bSide, m_sRegs.bSector);

                    ModifyStatus(BUSY, 0);
                    if (m_pDisk) m_pDisk->LoadTrack(m_bHeadCyl, m_bSide);
                    break;
                }

                // Write one or multiple sectors
                case WRITE_1SECTOR:
                case WRITE_MSECTOR:
                {
                    TRACE("FDC: WRITE_xSECTOR (to cyl %d, head %d, sector %d)\n", m_bHeadCyl, m_bSide, m_sRegs.bSector);

                    ModifyStatus(BUSY, 0);
                    if (m_pDisk) m_pDisk->LoadTrack(m_bHeadCyl, m_bSide);
                    break;
                }

                // Type III Commands

                // Read address, read track, write track
                case READ_ADDRESS:
                {
                    TRACE("FDC: READ_ADDRESS on cyl %d head %d\n", m_bHeadCyl, m_bSide);

                    ModifyStatus(BUSY, 0);
                    if (m_pDisk) m_pDisk->LoadTrack(m_bHeadCyl, m_bSide);
                    break;
                }
                break;

                case READ_TRACK:
                {
                    TRACE("FDC: READ_TRACK\n");

                    ModifyStatus(BUSY, 0);
                    if (m_pDisk) m_pDisk->LoadTrack(m_bHeadCyl, m_bSide);
                    break;
                }

                case WRITE_TRACK:
                    TRACE("FDC: WRITE_TRACK\n");

                    if (m_pDisk)
                    {
                        // Fail if read-only
                        if (m_pDisk->IsReadOnly())
                            ModifyStatus(WRITE_PROTECT, 0);
                        else
                        {
                            // Set buffer pointer and count ready to write
                            m_pbBuffer = m_abBuffer;
                            m_uBuffer = sizeof(m_abBuffer);
                            ModifyStatus(BUSY|DRQ, 0);
                        }
                    }
                    break;


                // Type IV Commands

                // Force interrupt
                case FORCE_INTERRUPT:
                {
                    TRACE("FDC: FORCE_INTERRUPT\n");

                    BYTE bStatus;
                    if (m_pDisk) m_pDisk->IsBusy(&bStatus, true);   // Wait until any active command is complete

                    ModifyStatus(m_sRegs.bStatus &= MOTOR_ON, ~MOTOR_ON & 0xff);   // Leave motor on but reset everything else

                    // Return to type 1 mode, no data available/required
                    m_sRegs.bCommand = 0;
                    m_uBuffer = 0;
                    break;
                }
            }
        }
        break;

        case regTrack:
            TRACE("FDC: Set TRACK to %d\n", bVal_);

            // Only allow register write if we're not busy
            if (!(m_sRegs.bStatus & BUSY))
                m_sRegs.bTrack = bVal_;
            break;

        case regSector:
            TRACE("FDC: Set SECTOR to %d\n", bVal_);

            // Only allow register write if we're not busy
            if (!(m_sRegs.bStatus & BUSY))
                m_sRegs.bSector = bVal_;
            break;

        case regData:
        {
            // Store the data value
            m_sRegs.bData = bVal_;

            // Are we expecting any data?
            if (m_uBuffer)
            {
                // Store the byte
                *m_pbBuffer++ = bVal_;

                // Got all the data we need?
                if (!--m_uBuffer)
                {
                    // Reset BUSY and DRQ to show we're done
                    ModifyStatus(0, BUSY|DRQ);

                    // Some commands require additional handling
                    switch (m_sRegs.bCommand & FDC_COMMAND_MASK)
                    {
                        case WRITE_1SECTOR:
                        {
                            UINT uWritten;
                            BYTE bStatus = WriteSector(m_abBuffer, &uWritten);
                            ModifyStatus(bStatus, 0);
                            break;
                        }

                        case WRITE_MSECTOR:
                        {
                            UINT uWritten;
                            BYTE bStatus = WriteSector(m_abBuffer, &uWritten);
                            ModifyStatus(bStatus, 0);

                            // Add multi-sector writing here?
                            break;
                        }
                        break;

                        case WRITE_TRACK:
                        {
                            // Examine and perform the format
                            BYTE bStatus = WriteTrack(m_abBuffer, sizeof(m_abBuffer));
                            ModifyStatus(bStatus, 0);
                        }
                        break;

                        default:
                            TRACE("!!! Unexpected data arrived!\n");
                    }
                }
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

bool CDrive::GetSector (BYTE index_, IDFIELD *pID_, BYTE *pbStatus_)
{
    return m_pDisk->GetSector(m_bHeadCyl, m_bSide, index_, pID_, pbStatus_);
}

// Locate the sector matching the current register details
bool CDrive::FindSector (IDFIELD* pID_)
{
    IDFIELD id;
    BYTE bStatus;

    int nIndexCount = 0;
    m_bSectorIndex = 0;

    // Search until we've seen the index hole twice
    while (nIndexCount < 2)
    {
        // Fetch the next sector details
        if (!m_pDisk->GetSector(m_bHeadCyl, m_bSide, m_bSectorIndex, &id, &bStatus))
        {
            // If we've run out of sectors, loop back to the start of the track
            nIndexCount++;
            m_bSectorIndex = 0;
            continue;
        }

        // Check to see if the track and sector numbers match
        if (id.bTrack == m_sRegs.bTrack && id.bSector == m_sRegs.bSector)
        {
            *pID_ = id;
            return true;
        }

        m_bSectorIndex++;
    }

    // Not found
    return false;
}


BYTE CDrive::ReadSector (BYTE* pbData_, UINT* puSize_)
{
    return m_pDisk->ReadData(m_bHeadCyl, m_bSide, m_bSectorIndex, pbData_, puSize_);
}

BYTE CDrive::WriteSector (BYTE* pbData_, UINT* puSize_)
{
    return m_pDisk->WriteData(m_bHeadCyl, m_bSide, m_bSectorIndex, pbData_, puSize_);
}

// Find and return the data for the next ID field seen on the spinning disk
BYTE CDrive::ReadAddress (IDFIELD* pID_)
{
    BYTE bStatus = RECORD_NOT_FOUND;

    // Fetch a sector, wrapping if necessary
    if (!m_pDisk->GetSector(m_bHeadCyl, m_bSide, m_bSectorIndex, pID_, &bStatus))
        m_pDisk->GetSector(m_bHeadCyl, m_bSide, m_bSectorIndex = 0, pID_, &bStatus);

    // Advance to next sector
    m_bSectorIndex++;

    return bStatus;
}


// Helper macro for function below
static void PutBlock (BYTE*& rpb_, BYTE bVal_, int nCount_=1)
{
    while (nCount_--)
        *rpb_++ = bVal_;
}

// Construct the raw track from the information of each sector on the track to make it look real
void CDrive::ReadTrack (BYTE* pbTrack_, UINT uSize_)
{
    IDFIELD id;
    BYTE bStatus;

    // Initialise track with 4E gap filler
    memset(pbTrack_, 0x4e, uSize_);

    // Start of track
    BYTE *pb = pbTrack_;
    m_bSectorIndex = 0;

    // Gap 1 and track header (min 32)
    PutBlock(pb, 0x4e, 32);

    // Only check for sectors if there are some to check
    while (GetSector(m_bSectorIndex, &id, &bStatus))
    {
        PutBlock(pb, 0x4e, 22);         // Gap 2: min 22 bytes of 0x4e
        PutBlock(pb, 0x00, 12);         // Gap 2: exactly 12 bytes of 0x00

        PutBlock(pb, 0xa1, 3);          // Gap 2: exactly 3 bytes of 0xf5 (written as 0xa1)
        PutBlock(pb, 0xfe);             // ID address mark: 1 byte of 0xfe

        PutBlock(pb, id.bTrack);        // Track number
        PutBlock(pb, id.bSide);         // Disk side
        PutBlock(pb, id.bSector);       // Sector number
        PutBlock(pb, id.bSize);         // Sector size

        PutBlock(pb, id.bCRC1);         // CRC MSB
        PutBlock(pb, id.bCRC2);         // CRC LSB

        PutBlock(pb, 0x4e, 22);         // Gap 3: min 22 (spec says 24?) bytes of 0x4e
        PutBlock(pb, 0x00, 8);          // Gap 3: min 8 bytes of 0x00

        // The data block only really exists if the ID field is valid
        if (!(bStatus & CRC_ERROR))
        {
            BYTE* pbData = pb;              // Data CRC begins here
            PutBlock(pb, 0xa1, 3);          // Gap 3: exactly 3 bytes of 0xa1

            // Read the sector contents, leaving a gap for the address mark
            UINT uSize;
            bStatus = ReadSector(pb+1, &uSize);

            // Write the appropriate data address mark: 1 byte of 0xfb (normal) or 0xf8 (deleted)
            PutBlock(pb, (bStatus & DELETED_DATA) ? 0xf8 : 0xfb);

            // Advance past the data block
            pb += uSize;

            // CRC the entire data area, ensuring it's invalid for data CRC errors
            WORD wCRC = CrcBlock(pbData, pb-pbData) ^ (bStatus & CRC_ERROR);
            PutBlock(pb, wCRC >> 8);    // CRC MSB
            PutBlock(pb, wCRC & 0xff);  // CRC LSB
        }

        m_bSectorIndex++;
    }

    PutBlock(pb, 0x4e, 16);         // Gap 4: min 16 bytes of 0x4e
}


// Verify the track position on the disk by looking for a sector with the correct track number and a valid CRC
BYTE CDrive::VerifyTrack ()
{
    IDFIELD id;

    // Sniff the next passing header
    BYTE bStatus = ReadAddress(&id);

    // Check the header track matches our location
    if (id.bTrack != m_bHeadCyl)
        bStatus |= RECORD_NOT_FOUND;

    return bStatus;
}


// Helper function for WriteTrack below, to check for ranges of marker bytes
static bool ExpectBlock (BYTE*& rpb_, BYTE* pbEnd_, BYTE bVal_, int nMin_, int nMax_=INT_MAX)
{
    // Find the end of the block of bytes
    for ( ; rpb_ < pbEnd_ && *rpb_ == bVal_ && nMax_ ; rpb_++, nMin_--, nMax_--);

    // Return true if the number found is in range
    return (nMin_ <= 0 && nMax_ >= 0);
}

// Scan the raw track information for disk formatting
BYTE CDrive::WriteTrack (BYTE* pbTrack_, UINT uSize_)
{
    BYTE *pb = pbTrack_, *pbEnd = pb + uSize_;

    int nSectors = 0, nMaxSectors = MAX_TRACK_SECTORS;
    IDFIELD* paID = new IDFIELD[nMaxSectors];
    BYTE** papbData = new BYTE*[nMaxSectors];


    // Note: the spec mentions that some things could be as small as 2 bytes for the 1772-02
    // If this is true the minimum values below could be reduced to accept even tighter formats

    // Look for Gap 1 and track header (min 32 bytes of 0x4e)
    if ((pb = (reinterpret_cast<BYTE*>(memchr(pb, 0x4e, pbEnd - pb)))) && ExpectBlock(pb, pbEnd, 0x4e, 32))
    {
        // Loop looking for sectors now
        while (pb < pbEnd)
        {
            // Assume the sector is valid until we discover otherwise
            bool fValid = true;

            fValid &= ExpectBlock(pb, pbEnd, 0x00, 12, 12); // Gap 2: exactly 12 bytes of 0x00
            fValid &= ExpectBlock(pb, pbEnd, 0xf5, 3, 3);   // Gap 2: exactly 3 bytes of 0xf5 (written as 0xa1)

            fValid &= ExpectBlock(pb, pbEnd, 0xfe, 1, 1);   // ID address mark: 1 byte of 0xfe


            // If there's enough data copy the IDFIELD info (CRC not included as the FDC generates it, below)
            if (pb + sizeof(*paID) <= pbEnd)
            {
                memcpy(&paID[nSectors], pb, sizeof(*paID));
                paID[nSectors].bCRC1 = paID[nSectors].bCRC2 = 0;
                pb += (sizeof(*paID) - sizeof(paID->bCRC1) - sizeof(paID->bCRC2));
            }

            fValid &= ExpectBlock(pb, pbEnd, 0xf7, 1, 1);   // CRC: 1 byte of 0xf7 (writes 2 CRC bytes)

            fValid &= ExpectBlock(pb, pbEnd, 0x4e, 22);     // Gap 3: min 22 (spec says 24?) bytes of 0x4e
            fValid &= ExpectBlock(pb, pbEnd, 0x00, 8);      // Gap 3: min 8 bytes of 0x00
            fValid &= ExpectBlock(pb, pbEnd, 0xf5, 3, 3);   // Gap 3: exactly 3 bytes of 0xf5 (written as 0xa1)

            // Data or Deleted Data address mark: 1 byte of 0xfb or 0xf8
            fValid &= (ExpectBlock(pb, pbEnd, 0xfb, 1, 1) || ExpectBlock(pb, pbEnd, 0xf8, 1, 1));

            // Store a pointer to the data, and skip it in the source block
            papbData[nSectors] = pb;
            pb += 128 << (paID[nSectors].bSize & 3);
            fValid &= (pb < pbEnd);

            fValid &= ExpectBlock(pb, pbEnd, 0xf7, 1, 1);   // CRC: 1 byte of 0xf7

            fValid &= ExpectBlock(pb, pbEnd, 0x4e, 16);     // Gap 4: min 16 bytes of 0x4e

            // Only count the sector if it was valid
            if (fValid)
                nSectors++;
        }
    }

    // Present the format to the disk for laying out
    BYTE bStatus = m_pDisk ? m_pDisk->FormatTrack(m_bHeadCyl, m_bSide, paID, papbData, nSectors) : WRITE_PROTECT;

    delete[] paID;
    delete[] papbData;

    return bStatus;
}
