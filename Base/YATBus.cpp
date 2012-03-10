// Part of SimCoupe - A SAM Coupe emulator
//
// Atom.cpp: YAMOD.ATBUS IDE interface
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

// For more information on Jarek Adamski's YAMOD.ATBUS interface, see:
//  http://8bit.yarek.pl/interface/yamod.atbus/

#include "SimCoupe.h"
#include "YATBus.h"

CYATBusDevice::CYATBusDevice ()
    : m_bLatch(0), m_fDataLatched(false)
{
}

BYTE CYATBusDevice::In (WORD wPort_)
{
    BYTE bRet = 0xff;

    // We're only interested in the bottom 4 bits
    switch (wPort_ & 0xf)
    {
        // Data port
        case 0:
        {
            // Data latched?
            if (m_fDataLatched)
            {
                // Return the latch contents, and clear it
                bRet = m_bLatch;
                m_fDataLatched = false;
            }
            else
            {
                // Read a WORD from the ATA interface
                // Bit 3 = CS0/CS1, bits 0-2 used for the low address bits
                WORD wData = m_pDisk ? m_pDisk->In(0x01f0 | ((wPort_ & 0x08) << 6) | (wPort_ & 0x07)) : 0xffff;

                // Store the high 8-bits in the latch
                m_bLatch = wData >> 8;
                m_fDataLatched = true;

                // Return the low 8-bits
                bRet = wData & 0xff;
            }
            break;
        }

        default:
            // Any non-data access clears the latch
            m_fDataLatched = false;

            // Read and return an 8-bit register
            bRet = m_pDisk ? m_pDisk->In(0x01f0 | ((wPort_ & 0x08) << 6) | (wPort_ & 0x7)) & 0xff : 0xff;
            break;
    }

    return bRet;
}

void CYATBusDevice::Out (WORD wPort_, BYTE bVal_)
{
    // We're only interested in the bottom 4 bits
    switch (wPort_ & 0xf)
    {
        case 0:
        {
            // Data already latched?
            if (!m_fDataLatched)
            {
                // No, so latch the supplied data
                m_bLatch = bVal_;
                m_fDataLatched = true;
            }
            else
            {
                // Write the 16-bit value formed from the supplied data and the latch
                if (m_pDisk)
                    m_pDisk->Out(0x01f0, (static_cast<WORD>(bVal_) << 8) | m_bLatch);

                // Clear the latch
                m_fDataLatched = false;
            }
            break;
        }

        default:
            // Write the supplied 8-bit register value
            if (m_pDisk)
                m_pDisk->Out(0x01f0 | ((wPort_ & 0x08) << 6) | (wPort_ & 0x7), bVal_);
            break;
    }
}
