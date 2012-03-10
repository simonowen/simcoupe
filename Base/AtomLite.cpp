// Part of SimCoupe - A SAM Coupe emulator
//
// AtomLite.cpp: Atom-Lite hard disk interface
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

#include "SimCoupe.h"
#include "AtomLite.h"

const unsigned int ATOM_LIGHT_DELAY = 2;    // Number of frames the hard disk LED remains on for after a command


CAtomLiteDevice::CAtomLiteDevice ()
    : m_bAddressLatch(0), m_bDataLatch(0)
{
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
