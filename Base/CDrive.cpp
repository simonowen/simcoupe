// Part of SimCoupe - A SAM Coupe emulator
//
// CDrive.cpp: VL1772-02 floppy disk controller emulation
//
//  Copyright (c) 1999-2004  Simon Owen
//  Copyright (c) 1996-2001  Allan Skillman
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

// Changes 1999-2001 by Simon Owen:
//  - implemented READ_ADDRESS, READ_TRACK and WRITE_TRACK
//  - tightened up motor and spin-up flags, and a couple of BUSY bugs
//  - added write-protection and direction flags
//  - changed index flag to only pulse when disk inserted (for missing disk error)
//  - real head position is now tracked properly for step without update
//  - changed to use one set of registers per drive, instead of per disk side

// ToDo:
//  - real delayed spin-up (including 'hang' when command sent and no disk present)
//  - data timeouts for type 2 commands
//  - t-state based index pulses (current value seems tuned for MasterDOS)

#include "SimCoupe.h"

#include "CDrive.h"
#include "CPU.h"

////////////////////////////////////////////////////////////////////////////////

CDrive::CDrive (CDisk* pDisk_/*=NULL*/)
    : CDiskDevice(dskImage),
    m_pDisk(pDisk_), m_pbBuffer(NULL)
{
    Reset ();
}

// Reset the controller back to default settings
void CDrive::Reset ()
{
    // Track 0, sector 1 and head over track 0
    memset(&m_sRegs, 0, sizeof m_sRegs);
    m_sRegs.bSector = 1;
    m_sRegs.bData = 0xff;

    m_uBuffer = 0;
    m_bDataStatus = 0;
    m_nHeadPos = m_nMotorDelay = 0;
}

// Insert a new disk from the named source (usually a file)
bool CDrive::Insert (const char* pcszSource_, bool fReadOnly_/*=false*/)
{
    // If no image was supplied, simply eject the current disk
    if (!pcszSource_ || !*pcszSource_)
        return Eject();

    // Open the new disk image, save+close the previous one if successful
    CDisk* pNew = CDisk::Open(pcszSource_, fReadOnly_);
    if (pNew)
    {
        delete m_pDisk;
        m_pDisk = pNew;
    }

    // Return whether a new disk was inserted
    return pNew != NULL;
}

// Eject any inserted disk
bool CDrive::Eject ()
{
    delete m_pDisk;
    m_pDisk = NULL;

    return true;
}

bool CDrive::Flush ()
{
    return (m_pDisk && (!m_pDisk->IsModified() || m_pDisk->Save()));
}

void CDrive::FrameEnd ()
{
    // If the motor hasn't been used for 2 seconds, switch it off
    if (m_nMotorDelay && !--m_nMotorDelay)
    {
        // Clear the motor-on bit
        m_sRegs.bStatus &= ~MOTOR_ON;

        // Close any real floppy device to ensure any changes are flushed
        if (m_pDisk && m_pDisk->GetType() == dtFloppy)
            m_pDisk->Close();
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
        m_nMotorDelay = FLOPPY_MOTOR_ACTIVE_TIME;
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

BYTE CDrive::In (WORD wPort_)
{
    BYTE bRet = 0x00;

    // Update status from current asynchronous operation, if any
    if (m_pDisk && m_pDisk->GetAsyncStatus(&m_uBuffer, &m_bDataStatus))
    {
        // Some commands require additional handling
        switch (m_sRegs.bCommand)
        {
            case READ_1SECTOR:
            case READ_MSECTOR:
                ModifyReadStatus();
                break;

            case WRITE_1SECTOR:
            case WRITE_MSECTOR:
                ModifyStatus(m_bDataStatus, BUSY);
                break;

            default:
                TRACE("!!! Unexpected command type behaving asynchronously (%d)!\n", m_sRegs.bCommand);
        }
    }

    // Register to read from is the bottom 3 bits of the port
    switch (wPort_ & 0x03)
    {
        case regStatus:
        {
            // Return value is the status byte
            bRet = m_sRegs.bStatus;

            // Read address work-around for a SAM DICE bug, which relies on unimplemented controller timeouts
            if (m_sRegs.bCommand == READ_ADDRESS)
            {
                static int n = 0;

                // If the ID field data has been read, and busy cleared, reset the counter
                if (!(bRet & BUSY))
                    n = 0;
                // If busy has been set for 16 reads, reset it to allow a stuck caller to continue
                else if (!(++n & 0xf))
                    m_sRegs.bStatus &= ~BUSY;
            }

            // Type 1 command mode uses more status bits
            if (m_pDisk && !(m_sRegs.bCommand & 0x80))
            {
                // Set the write protect bit if the disk is read-only
                if (m_pDisk->IsReadOnly())
                    ModifyStatus(WRITE_PROTECT, 0);

                // Toggle the index pulse status bit periodically to show the disk is spinning
                if (IsMotorOn() && (g_dwCycleCounter % (REAL_TSTATES_PER_SECOND / (FLOPPY_RPM/60))) < TSTATES_PER_FRAME)
                    bRet |= INDEX_PULSE;
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
            // ToDo:  Use a real SAM to try booting BDOS without an ATOM connected, to see whether data written to the data
            // register can be read back, when DRQ is not active.  BDOS hangs in SimCoupé as the 0xa0 (master select) is written,
            // and happens to be read back from the data port.  BDOS think the drive is BUSY bit is set in the ATA status, and
            // waits forever for it to clear!

            // Only consider returning data if some is available
            if (m_uBuffer)
            {
                // Get the next byte
                m_sRegs.bData = *m_pbBuffer++;

                // Has all the data been read?
                if (!--m_uBuffer)
                {
                    // Reset BUSY and DRQ to show we're done
                    ModifyStatus(0, BUSY | DRQ);

                    // Some commands require additional handling
                    switch (m_sRegs.bCommand)
                    {
                        case READ_ADDRESS:
                        case READ_TRACK:
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
                                BYTE bStatus;

                                // Find the next sector on the track
                                if (m_pDisk->FindNext(&id, &bStatus))
                                {
                                    // If there's an error, set the error and reset busy so it's seen
                                    if (bStatus)
                                        ModifyStatus(bStatus, BUSY);
                                    else
                                    {
                                        TRACE("FDC: Multiple-sector read moving to sector %d\n", id.bSector);

                                        // Read the data, reporting anything but CRC errors now
                                        // NB - ReadData may now be asynchronous, in which case BUSY will be returned
                                        m_bDataStatus = m_pDisk->ReadData(m_pbBuffer = m_abBuffer, &m_uBuffer);
                                        ModifyReadStatus();
                                    }
                                }
                            }
                            break;

                        default:
                            TRACE("Data requested for unknown command type (%d)!\n", m_sRegs.bCommand);
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
    // Extract the side number the port refers to (0 or 1)
    int nSide = ((wPort_) >> 2) & 1;

    // Register to write to is the bottom 3 bits of the port
    switch (wPort_ & 0x03)
    {
        case regCommand:
        {
            // Reset the status as we're starting a new command
            m_sRegs.bStatus = 0;

            // Motor on
            ModifyStatus(MOTOR_ON, 0);

            // Type 1 commands have additional status flags set
            if (m_pDisk && !(m_sRegs.bCommand & 0x80))
            {
                // Flag spin-up complete if the spin-up bit is set in a type 1 command
                if (!(bVal_ & FLAG_SPINUP))
                    ModifyStatus(SPIN_UP, 0);
            }

            // The main command is taken from the top 2
            switch (m_sRegs.bCommand = bVal_ & 0xf0)
            {
                // Type I commands

                // Restore disk head to track 0
                case RESTORE:
                    ModifyStatus(TRACK00, SPIN_UP);
                    m_sRegs.bTrack = m_nHeadPos = 0;

                    TRACE("FDC: RESTORE\n");
                    break;

                // Seek a track (send the track number to the DATA reg)
                case SEEK:
                    // The direction flag is affected by the direction we need to seek
                    m_sRegs.fDir = (m_sRegs.bData > m_sRegs.bTrack);

                    // Move to the required track
                    m_sRegs.bTrack = m_nHeadPos = m_sRegs.bData;

                    // If we're now at track zero, set the TRACK00 bit in the status
                    if (!m_nHeadPos)
                        ModifyStatus(TRACK00, 0);

                    TRACE("FDC: SEEK to track %d\n", m_sRegs.bData);
                    break;

                // Step using current dir without updating track register
                case STEP_NUPD:
                    // If the direction flag is zero, step in
                    if (!m_sRegs.fDir)
                        m_nHeadPos++;

                    // Only step out if we're not already at zero
                    else if (m_nHeadPos)
                        m_nHeadPos--;

                    // The TRACK00 status bit reflects whether we're at track zero
                    if (!m_nHeadPos)
                    {
                        ModifyStatus(TRACK00, 0);

                        // Note: the track register is still zeroed, even tho we're in non-update mode!
                        m_sRegs.bTrack = 0;
                    }

                    TRACE("FDC: STEP_NUPD (%s to track %d (%d))\n", m_sRegs.fDir ? "out" : "in", m_nHeadPos, m_sRegs.bTrack);
                    break;

                // Step drive using current direction flags
                case STEP_UPD:
                    // If the direction flag is zero, step in
                    if (!m_sRegs.fDir)
                        m_sRegs.bTrack = ++m_nHeadPos;

                    // Only step out if we're not already at zero
                    else if (m_nHeadPos)
                        m_sRegs.bTrack = --m_nHeadPos;

                    // The TRACK00 status bit reflects whether we're at track zero
                    if (!m_nHeadPos)
                        ModifyStatus(TRACK00, 0);

                    TRACE("FDC: STEP_UPD (%s to track %d)\n", m_sRegs.fDir ? "out" : "in", m_nHeadPos);
                    break;

                // Step in without updating track register
                case STEP_IN_NUPD:
                    // Step the real head in
                    m_nHeadPos++;

                    // Remember the direction we last moved
                    m_sRegs.fDir = false;
                    TRACE("FDC: STEP_IN_NUPD (to track %d (%d)\n", m_nHeadPos, m_sRegs.bTrack);
                    break;

                // Step in and increment track register
                case STEP_IN_UPD:
                    // Move in a track and remember the direction we last moved
                    m_sRegs.bTrack = ++m_nHeadPos;
                    m_sRegs.fDir = false;
                    TRACE("FDC: STEP_IN_UPD (to track %d)\n", m_nHeadPos);
                    break;

                // Step out without updating track register
                case STEP_OUT_NUPD:
                    // Only decrement the track register if it's not already zero
                    if (m_nHeadPos)
                        m_nHeadPos--;

                    // If we're now at track zero, set the TRACK00 bit in the status and zero the track register
                    if (!m_nHeadPos)
                    {
                        m_sRegs.bTrack = 0;         // Note: the FDC does clear this in non-update mode!
                        ModifyStatus(TRACK00, 0);
                    }

                    // Remember the direction we last moved
                    m_sRegs.fDir = true;
                    TRACE("FDC: STEP_OUT_NUPD (to track %d (%d)\n", m_nHeadPos, m_sRegs.bTrack);
                    break;

                // Step out and decrement track register
                case STEP_OUT_UPD:
                    // Only decrement the track register if it's not already zero
                    if (m_nHeadPos)
                        m_sRegs.bTrack = --m_nHeadPos;

                    // If we're now at track zero, set the TRACK00 bit in the status
                    if (!m_nHeadPos)
                        ModifyStatus(TRACK00, 0);
                    else
                        ModifyStatus(0, TRACK00);

                    // Move in a track and remember the direction we last moved
                    m_sRegs.fDir = true;
                    TRACE("FDC: STEP_OUT_UPD (to track %d)\n", m_nHeadPos);
                    break;


                // Type II Commands

                // Read one or multiple sectors
                case READ_1SECTOR:
                case READ_MSECTOR:
                {
                    TRACE("FDC: READ_xSECTOR (from side %d, track %d, sector %d)\n", nSide, m_sRegs.bTrack, m_sRegs.bSector);
                    ModifyStatus(BUSY, TRACK00 | DELETED_DATA);

                    // Locate the sector, reset busy and signal record not found if we couldn't find it
                    if (!m_pDisk || !m_pDisk->FindSector(nSide, m_nHeadPos, m_sRegs.bTrack, m_sRegs.bSector))
                        ModifyStatus(RECORD_NOT_FOUND, BUSY);
                    else
                    {
                        // Read the data, reporting anything but CRC errors now, as we can't check the CRC until we reach it at the end of the data on the disk
                        // NB - ReadData may now be asynchronous, in which case BUSY will be returned
                        m_bDataStatus = m_pDisk->ReadData(m_pbBuffer = m_abBuffer, &m_uBuffer);
                        ModifyReadStatus();

                        // Just for fun ;-)
                        if (m_sRegs.bTrack == 4 && m_sRegs.bSector == 1 && m_abBuffer[0x1c2] == 't' && CrcBlock(m_abBuffer, m_uBuffer) == 0x6c54)
                            m_abBuffer[0x1c2] = 'w';
                    }
                }
                break;

                // Write one or multiple sectors
                case WRITE_1SECTOR:
                case WRITE_MSECTOR:
                    TRACE("FDC: WRITE_xSECTOR\n");
                    ModifyStatus(BUSY, TRACK00 | DELETED_DATA);

                    // Locate the sector, reset busy and signal record not found if we couldn't find it
                    IDFIELD sID;
                    if (!m_pDisk || !m_pDisk->FindSector(nSide, m_nHeadPos, m_sRegs.bTrack, m_sRegs.bSector, &sID))
                        ModifyStatus(RECORD_NOT_FOUND, BUSY);
                    else if (m_pDisk->IsReadOnly())
                        ModifyStatus(WRITE_PROTECT, BUSY);
                    else
                    {
                        // Prepare data pointer to receive data, and the amount we're expecting
                        m_pbBuffer = m_abBuffer;
                        m_uBuffer = 128 << sID.bSize;

                        // Signal that data is now requested for writing
                        ModifyStatus(DRQ, 0);
                    }
                    break;


                // Type III Commands

                // Read address, read track, write track
                case READ_ADDRESS:
                {
                    ModifyStatus(BUSY, TRACK00 | DELETED_DATA);

                    // Read an ID field into our general buffer
                    IDFIELD* pId = reinterpret_cast<IDFIELD*>(m_pbBuffer = m_abBuffer);
                    BYTE bStatus = ReadAddress(nSide, m_nHeadPos, pId);

                    // If successful set up the number of bytes available to read
                    if (!(bStatus & TYPE23_ERROR_MASK))
                    {
                        m_sRegs.bSector = pId->bTrack;

                        m_uBuffer = sizeof(IDFIELD);
                        ModifyStatus(bStatus|DRQ, 0);
                    }

                    // Set the error status, resetting BUSY so the client sees the error
                    else
                    {
                        ModifyStatus(bStatus, BUSY);
                        m_uBuffer = 0;
                    }

                    TRACE("FDC: READ_ADDRESS (Track:%d Side:%d Sector:%d Size:%d) returned %#02x\n", pId->bSide, pId->bTrack, pId->bSector, 128 << pId->bSize, bStatus);
                }
                break;

                case READ_TRACK:
                    TRACE("FDC: READ_TRACK\n");
                    ModifyStatus(BUSY, TRACK00 | DELETED_DATA);

                    // See if the disk object has a real track for us
                    if (m_pDisk && m_pDisk->ReadTrack(nSide, m_nHeadPos, m_pbBuffer = m_abBuffer, m_uBuffer = sizeof m_abBuffer))
                        ModifyStatus(DRQ, 0);

                    // Fall back to preparing a fake version built from the known sectors
                    else if (ReadTrack(nSide, m_nHeadPos, m_pbBuffer = m_abBuffer, m_uBuffer = sizeof m_abBuffer))
                        ModifyStatus(DRQ, 0);
                    else
                    {
                        ModifyStatus(RECORD_NOT_FOUND, BUSY);
                        m_uBuffer = 0;
                    }
                    break;

                case WRITE_TRACK:
                    TRACE("FDC: WRITE_TRACK\n");
                    ModifyStatus(BUSY | DRQ, TRACK00 | DELETED_DATA);

                    // Set buffer pointer and count ready to write
                    m_pbBuffer = m_abBuffer;
                    m_uBuffer = sizeof m_abBuffer;

                    // Fail if read-only
                    if (m_pDisk && m_pDisk->IsReadOnly())
                        ModifyStatus(WRITE_PROTECT, BUSY | DRQ);

                    // Signal we're ready to start receiving the track data
                    else
                        ModifyStatus(DRQ, 0);
                    break;


                // Type IV Commands

                // Force interrupt
                case FORCE_INTERRUPT:
                    TRACE("FDC: FORCE_INTERRUPT\n");

                    // Leave motor on but reset everything else
                    if (m_pDisk)
                        m_pDisk->AbortAsyncOp();
                    m_sRegs.bStatus &= MOTOR_ON;

                    // Return to type 1 mode
                    m_sRegs.bCommand = 0;
                    break;
            }
        }
        break;

        case regTrack:
            TRACE("FDC: Set TRACK to %d\n", bVal_);
            m_sRegs.bTrack = bVal_;
            break;

        case regSector:
            TRACE("FDC: Set SECTOR to %d\n", bVal_);
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
                    ModifyStatus(0, BUSY | DRQ);

                    // Some commands require additional handling
                    switch (m_sRegs.bCommand)
                    {
                        case WRITE_1SECTOR:
                        case WRITE_MSECTOR:
                        {
                            TRACE("FDC: Writing full sector: side %d, track %d, sector %d\n", nSide, m_sRegs.bTrack, m_sRegs.bSector);

                            UINT uWritten;
                            // NB - WriteData may now be asynchronous, in which case BUSY will be returned
                            BYTE bStatus = m_pDisk->WriteData(m_abBuffer, &uWritten);
                            ModifyStatus(bStatus, BUSY|DRQ);
                        }
                        break;

                        case WRITE_TRACK:
                        {
                            // Examine and perform the format
                            BYTE bStatus = WriteTrack(nSide, m_nHeadPos, m_abBuffer, sizeof m_abBuffer);
                            ModifyStatus(bStatus, BUSY|DRQ);
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


// Find and return the data for the next ID field seen on the spinning disk
BYTE CDrive::ReadAddress (UINT uSide_, UINT uTrack_, IDFIELD* pIdField_)
{
    // Assume we won't find a record until we do
    BYTE bRetStatus = RECORD_NOT_FOUND;

    // Only check for sectors if there are some to check
    if (m_pDisk && m_pDisk->FindInit(uSide_, uTrack_))
    {
        // Fetch and advance the disk spin position
        int nSpinPos = m_pDisk->GetSpinPos(true);

        // Find the sector we're currently over
        for (int i = 1 ; i <= nSpinPos ; i++)
            m_pDisk->FindNext(pIdField_, &bRetStatus);
    }

    // Return the find status
    return bRetStatus;
}


// CRC-CCITT for id/data checksums, with bit and byte order swapped
WORD CDrive::CrcBlock (const void* pcv_, size_t uLen_, WORD wCRC_/*=0xffff*/)
{
    static WORD awCRC[256];

    // Build the table if not already built
    if (!awCRC[1])
    {
        for (int i = 0 ; i < 256 ; i++)
        {
            WORD w = i << 8;

            // 8 shifts, for each bit in the update byte
            for (int j = 0 ; j < 8 ; j++)
                w = (w << 1) ^ ((w & 0x8000) ? 0x1021 : 0);

            awCRC[i] = w;
        }
    }

    // Update the CRC with each byte in the block
    const BYTE* pb = reinterpret_cast<const BYTE*>(pcv_);
    while (uLen_--)
        wCRC_ = (wCRC_ << 8) ^ awCRC[((wCRC_ >> 8) ^ *pb++) & 0xff];

    // Return the updated CRC
    return wCRC_;
}


// Helper macro for function below
static void PutBlock (BYTE*& rpb_, BYTE bVal_, int nCount_=1)
{
    while (nCount_--)
        *rpb_++ = bVal_;
}

// Construct the raw track from the information of each sector on the track to make it look real
UINT CDrive::ReadTrack (UINT uSide_, UINT uTrack_, BYTE* pbTrack_, UINT uSize_)
{
    BYTE *pb = pbTrack_;

    // Start with a clean slate
    memset(pbTrack_, 0, uSize_);

    // Only check for sectors if there are some to check
    if (m_pDisk && m_pDisk->FindInit(uSide_, uTrack_))
    {
        IDFIELD id;
        BYTE    bStatus;

        // Gap 1 and track header (min 32)
        PutBlock(pb, 0x4e, 32);

        // Loop through all the sectors in the track
        while (m_pDisk->FindNext(&id, &bStatus))
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

            BYTE* pbData = pb;              // Data CRC begins here
            PutBlock(pb, 0xa1, 3);          // Gap 3: exactly 3 bytes of 0xa1

            PutBlock(pb, 0xfb);             // Data address mark: 1 byte of 0xfb

            // The data block only really exists if the ID field is valid
            if (!(bStatus & CRC_ERROR))
            {
                // Read the sector contents
                // NB - ReadData may now be asynchronous, in which case BUSY will be returned
                // If that's the case, WaitAsyncOp will wait until the operation has completed
                UINT uSize;
                bStatus = m_pDisk->ReadData(pb, &uSize);
                m_pDisk->WaitAsyncOp(&uSize, &bStatus);
                pb += uSize;

                // CRC the entire data area, ensuring it's invalid for data CRC errors
                WORD wCRC = CrcBlock(pbData, pb-pbData) ^ (bStatus & CRC_ERROR);
                PutBlock(pb, wCRC >> 8);    // CRC MSB
                PutBlock(pb, wCRC & 0xff);  // CRC LSB
            }

            PutBlock(pb, 0x4e, 16);         // Gap 4: min 16 bytes of 0x4e
        }
    }

    // Return the amount filled in
    return static_cast<UINT>(pb - pbTrack_);
}


// Verify the track position on the disk by looking for a sector with the correct track number and a valid CRC
BYTE CDrive::VerifyTrack (UINT uSide_, UINT uTrack_)
{
    // Assume we won't find a matching record until we do
    BYTE bRetStatus = RECORD_NOT_FOUND;

    // Only check for sectors if there are some to check
    if (m_pDisk && m_pDisk->FindInit(uSide_, uTrack_))
    {
        IDFIELD id;
        BYTE    bStatus;

        // Loop through all the sectors in the track
        while (m_pDisk->FindNext(&id, &bStatus))
        {
            // Does the track number match where we are?
            if (id.bTrack == uTrack_)
            {
                // Combine any ID field CRC errors with the returned status
                bRetStatus |= bStatus;

                // If the CRC is correct, we've got a match so return
                if (!bStatus)
                {
                    // Clear the 'record not found' bit as we've found a match
                    bRetStatus &= ~RECORD_NOT_FOUND;
                    break;
                }
            }
        }
    }

    // Return the verify status
    return bRetStatus;
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
BYTE CDrive::WriteTrack (UINT uSide_, UINT uTrack_, BYTE* pbTrack_, UINT uSize_)
{
    BYTE *pb = pbTrack_, *pbEnd = pb + uSize_;

    int nSectors = 0, nMaxSectors = MAX_TRACK_SECTORS;
    IDFIELD* paID = new IDFIELD[nMaxSectors];


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

            fValid &= ExpectBlock(pb, pbEnd, 0xfe, 1, 1);   // Write ID address mark: 1 byte of 0xfe


            // If there's enough data copy the IDFIELD info (CRC not included as the FDC generates it, below)
            if (pb + sizeof *paID <= pbEnd)
            {
                memcpy(&paID[nSectors], pb, sizeof *paID);
                paID[nSectors].bCRC1 = paID[nSectors].bCRC2 = 0;
                pb += (sizeof *paID - sizeof paID->bCRC1 - sizeof paID->bCRC2);
            }

            fValid &= ExpectBlock(pb, pbEnd, 0xf7, 1, 1);   // Write CRC: 1 byte of 0xf7 (writes 2 CRC bytes)

            fValid &= ExpectBlock(pb, pbEnd, 0x4e, 22);     // Gap 3: min 22 (spec says 24?) bytes of 0x4e
            fValid &= ExpectBlock(pb, pbEnd, 0x00, 8);      // Gap 3: min 8 bytes of 0x00
            fValid &= ExpectBlock(pb, pbEnd, 0xf5, 3, 3);   // Gap 3: exactly 3 bytes of 0xf5 (written as 0xa1)

            fValid &= ExpectBlock(pb, pbEnd, 0xfb, 1, 1);   // Write data address mark: 1 byte of 0xfb

            // Skip the data area (data in this area is very unlikely, and will only be implemented if needed!)
            pb += 128 << paID[nSectors].bSize;
            fValid &= (pb < pbEnd);

            fValid &= ExpectBlock(pb, pbEnd, 0xf7, 1, 1);   // Write CRC bytes: 1 byte of 0xf7

            fValid &= ExpectBlock(pb, pbEnd, 0x4e, 16);     // Gap 4: min 16 bytes of 0x4e

            // Only count the sector if it was valid
            if (fValid)
                nSectors++;
        }
    }

    // Present the format to the disk for laying out
    return m_pDisk ? m_pDisk->FormatTrack(uSide_, uTrack_, paID, nSectors) : WRITE_PROTECT;
}
