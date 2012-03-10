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

const unsigned int ATOM_LIGHT_DELAY = 2;    // Number of frames the hard disk LED remains on for after a command


CAtomDiskDevice::CAtomDiskDevice ()
    : m_bAddressLatch(0), m_bDataLatch(0)
{
}


BYTE CAtomDiskDevice::In (WORD wPort_)
{
    BYTE bRet = 0xff;

    switch (wPort_ & ATOM_REG_MASK)
    {
        // Data high
        case 6:
        {
            WORD wData = 0xffff;

            // Read a 16-bit data value, if a disk is present
            wData = m_pDisk ? m_pDisk->In(m_bAddressLatch & ATOM_ADDR_MASK) : 0xffff;

            // Store the low-byte in the latch and return the high-byte
            m_bDataLatch = wData & 0xff;
            bRet = wData >> 8;

            break;
        }

        // Data low
        case 7:
            // Return the previously stored low-byte
            bRet = m_bDataLatch;
            break;

        default:
            TRACE("Atom: Unrecognised read from %#04x\n", wPort_);
            break;
    }

    return bRet;
}

void CAtomDiskDevice::Out (WORD wPort_, BYTE bVal_)
{
    switch (wPort_ & ATOM_REG_MASK)
    {
        // Address select
        case 5:
            // Bits 6+7 are unused, so strip them
            m_bAddressLatch = (bVal_ & ATOM_ADDR_MASK);

            // If the reset pin is low, reset the disk
            if (~bVal_ & ATOM_NRESET && m_pDisk)
                m_pDisk->Reset();

            break;

        // Data high - store in the latch for later
        case 6:
            m_bDataLatch = bVal_;
            break;

        // Data low
        case 7:
            if (m_pDisk)
            {
                m_uLightDelay = ATOM_LIGHT_DELAY;
                m_pDisk->Out(m_bAddressLatch & ATOM_ADDR_MASK, (m_bDataLatch << 8) | bVal_);
            }
            break;

        default:
            TRACE("Atom: Unhandled write to %#04x with %#02x\n", wPort_, bVal_);
            break;
    }
}
