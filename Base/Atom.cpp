// Part of SimCoupe - A SAM Coupe emulator
//
// Atom.cpp: ATOM hard disk inteface
//
//  Copyright (c) 1999-2003  Simon Owen
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

#include "SimCoupe.h"

#include "Atom.h"
#include "Options.h"

const unsigned int ATOM_LIGHT_DELAY = 2;    // Number of frames the hard disk LED remains on for after a command


CAtomDiskDevice::CAtomDiskDevice (CHardDisk* pDisk_)
    : CDiskDevice(dskAtom), m_uLightDelay(0)
{
    m_pDisk = new CATADevice(pDisk_);
}


CAtomDiskDevice::~CAtomDiskDevice ()
{
    delete m_pDisk;
}


BYTE CAtomDiskDevice::In (WORD wPort_)
{
    BYTE bRet = 0x00;

    switch (wPort_ & 7)
    {
        // Data high
        case 6:
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
        case 7:
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

    switch (wPort_ & 7)
    {
        // Address select
        case 5:
            // If the reset pin is low, reset the disk
            if (!(bVal_ & ATOM_NRESET))
                m_pDisk->Reset();

            m_bAddressLatch = bVal_;
            break;

        // Data high - store in the latch for later
        case 6:
            m_bDataLatch = bVal_;
            break;

        // Data low
        case 7:
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
