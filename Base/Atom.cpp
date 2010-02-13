// Part of SimCoupe - A SAM Coupe emulator
//
// Atom.cpp: Atom and Atom Lite hard disk intefaces
//
//  Copyright (c) 1999-2010  Simon Owen
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


CAtomDiskDevice::CAtomDiskDevice (CATADevice* pDisk_)
    : CDiskDevice(dskAtom), m_bAddressLatch(0), m_bDataLatch(0), m_uLightDelay(0)
{
    m_pDisk = pDisk_;
}


CAtomDiskDevice::~CAtomDiskDevice ()
{
    delete m_pDisk;
}


void CAtomDiskDevice::Reset ()
{
    if (m_pDisk)
        m_pDisk->Reset();
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
            if (m_pDisk)
                wData = m_pDisk->In(m_bAddressLatch & ATOM_ADDR_MASK);

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

void CAtomDiskDevice::FrameEnd ()
{
    // If the drive light is currently on, reduce the counter
    if (m_uLightDelay)
        m_uLightDelay--;
}

///////////////////////////////////////////////////////////////////////////////

CAtomLiteDevice::CAtomLiteDevice (CATADevice* pDisk_)
    : CDiskDevice(dskAtomLite), m_bAddressLatch(0), m_bDataLatch(0), m_uLightDelay(0)
{
    m_pDisk = pDisk_;
}


CAtomLiteDevice::~CAtomLiteDevice ()
{
    delete m_pDisk;
}


void CAtomLiteDevice::Reset ()
{
    if (m_pDisk)
        m_pDisk->Reset();
}

BYTE CAtomLiteDevice::In (WORD wPort_)
{
    BYTE bRet = 0xff;

    switch (wPort_ & ATOM_LITE_REG_MASK)
    {
        // Both data ports behave the same
        case 6:
        case 7:
            // Dallas clock or disk addressed?
            if (m_bAddressLatch == 0x1d)
                bRet = m_Dallas.In(wPort_ << 8);
            else if (m_pDisk)
                bRet = m_pDisk->In(m_bAddressLatch & ATOM_LITE_ADDR_MASK) & 0xff;
            break;

        default:
            TRACE("AtomLite: Unrecognised read from %#04x\n", wPort_);
            break;
    }

    return bRet;
}

void CAtomLiteDevice::Out (WORD wPort_, BYTE bVal_)
{
    switch (wPort_ & ATOM_LITE_REG_MASK)
    {
        // Address select
        case 5:
            // Bits 5-7 are unused, so strip them
            m_bAddressLatch = (bVal_ & ATOM_LITE_ADDR_MASK);
            break;

        // Both data ports behave the same
        case 6:
        case 7:
            // Dallas clock or disk addressed?
            if (m_bAddressLatch == 0x1d)
                m_Dallas.Out(wPort_ << 8, bVal_);
            else if (m_pDisk)
            {
                m_uLightDelay = ATOM_LIGHT_DELAY;
                m_pDisk->Out(m_bAddressLatch & ATOM_LITE_ADDR_MASK, bVal_);
            }
            break;

        default:
            TRACE("AtomLite: Unhandled write to %#04x with %#02x\n", wPort_, bVal_);
            break;
    }
}

void CAtomLiteDevice::FrameEnd ()
{
    // If the drive light is currently on, reduce the counter
    if (m_uLightDelay)
        m_uLightDelay--;
}
