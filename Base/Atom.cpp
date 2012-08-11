// Part of SimCoupe - A SAM Coupe emulator
//
// Atom.cpp: ATOM hard disk interface
//
//  Copyright (c) 1999-2012 Simon Owen
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

// For more information on Edwin Blink's Atom interface, see:
//  http://www.designing.myweb.nl/samcoupe/hardware/atomhdinterface/atom.htm

#include "SimCoupe.h"

#include "Atom.h"
#include "Options.h"


CAtomDevice::CAtomDevice ()
    : m_bAddressLatch(0), m_bReadLatch(0), m_bWriteLatch(0)
{
}


BYTE CAtomDevice::In (WORD wPort_)
{
    BYTE bRet = 0xff;

    switch (wPort_ & ATOM_REG_MASK)
    {
        // Data high
        case 6:
        {
            // Read a 16-bit data value
            WORD wData = CAtaAdapter::InWord(m_bAddressLatch & ATOM_ADDR_MASK);

            // Store the low-byte in the read latch and return the high-byte
            m_bReadLatch = wData & 0xff;
            bRet = wData >> 8;

            break;
        }

        // Data low
        case 7:
            // Return the low-byte from the read latch
            bRet = m_bReadLatch;
            break;

        default:
            TRACE("Atom: Unrecognised read from %#04x\n", wPort_);
            break;
    }

    return bRet;
}

void CAtomDevice::Out (WORD wPort_, BYTE bVal_)
{
    switch (wPort_ & ATOM_REG_MASK)
    {
        // Address select
        case 5:
            m_bAddressLatch = bVal_;

            // If the reset pin is low, reset the disk
            if (~bVal_ & ATOM_NRESET)
                CAtaAdapter::Reset();

            break;

        // Data high - store in the write latch for later
        case 6:
            m_bWriteLatch = bVal_;
            break;

        // Data low
        case 7:
            // If reset is asserted, ignore the write
            if (~m_bAddressLatch & ATOM_NRESET)
                break;

            m_uActive = HDD_ACTIVE_FRAMES;
            CAtaAdapter::Out(m_bAddressLatch & ATOM_ADDR_MASK, (m_bWriteLatch << 8) | bVal_);
            break;

        default:
            TRACE("Atom: Unhandled write to %#04x with %#02x\n", wPort_, bVal_);
            break;
    }
}


bool CAtomDevice::Attach (CHardDisk *pDisk_, int nDevice_)
{
    bool fByteSwapped = false;

    if (pDisk_)
    {
        // Optionally byte-swap Atom Lite media to work with the original Atom
        if (GetOption(autobyteswap) && pDisk_->IsBDOSDisk(&fByteSwapped))
            pDisk_->SetByteSwap(!fByteSwapped);

        // Have the disk support older requests
        pDisk_->SetLegacy(true);
    }

    CAtaAdapter::Attach(pDisk_, nDevice_);

    return pDisk_ != NULL;
}
