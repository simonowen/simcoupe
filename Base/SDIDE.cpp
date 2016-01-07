// Part of SimCoupe - A SAM Coupe emulator
//
// SDIDE.cpp: S D Software IDE interface
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

// S D Software IDE interface by Nev Young (nevilley@nfy53.demon.co.uk)

// Notes:
//  H-DOS uses writes to ATA port 0xee to reset the disk, but details of
//  this have not been found in any ATA documentation I've seen.  Also
//  unhandled is a read from ATA port 0xff, which H-DOS appears to use to
//  ensure the latches are in a known state.

#include "SimCoupe.h"
#include "SDIDE.h"

#include "Options.h"


BYTE CSDIDEDevice::In (WORD wPort_)
{
    BYTE bRet = 0xff;

    switch (wPort_ & 0xff)
    {
        // Data (high latched)
        case SDIDE_DATA:
            if (m_fDataLatched)
                bRet = m_bDataLatch;
            else
            {
                WORD wData = CAtaAdapter::InWord(0x0100 | m_bAddressLatch);
                m_bDataLatch = wData >> 8;
                bRet = wData & 0xff;
            }

            m_fDataLatched = !m_fDataLatched;
            break;

        default:
            TRACE("SDIDE: Unrecognised read from %#04x\n", wPort_);
            break;
    }

    return bRet;
}

void CSDIDEDevice::Out (WORD wPort_, BYTE bVal_)
{
    switch (wPort_ & 0xff)
    {
        // Register (latched)
        case SDIDE_REG:
            m_bAddressLatch = bVal_;
            m_fDataLatched = false;
            break;

        // Data (low latched)
        case SDIDE_DATA:
            if (!m_fDataLatched)
                m_bDataLatch = bVal_;
            else
                 CAtaAdapter::Out(0x0100 | m_bAddressLatch, (static_cast<WORD>(bVal_) << 8) | m_bDataLatch);

            m_fDataLatched = !m_fDataLatched;
            break;
    }
}
