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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "SimCoupe.h"

#include "AtomLite.h"
#include "Options.h"


uint8_t CAtomLiteDevice::In(uint16_t wPort_)
{
    uint8_t bRet = 0xff;

    switch (wPort_ & ATOM_LITE_REG_MASK)
    {
        // Both data ports behave the same
    case 6:
    case 7:
        switch (m_bAddressLatch & ATOM_LITE_ADDR_MASK)
        {
            // Dallas clock
        case 0x1d:
            bRet = m_Dallas.In(wPort_ << 8);
            break;

            // ATA device
        default:
            bRet = CAtaAdapter::InWord(m_bAddressLatch & ATOM_LITE_ADDR_MASK) & 0xff;
            break;
        }
        break;

    default:
        TRACE("AtomLite: Unrecognised read from %#04x\n", wPort_);
        break;
    }

    return bRet;
}

void CAtomLiteDevice::Out(uint16_t wPort_, uint8_t bVal_)
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
        switch (m_bAddressLatch & ATOM_LITE_ADDR_MASK)
        {
            // Dallas clock
        case 0x1d:
            m_Dallas.Out(wPort_ << 8, bVal_);
            break;

            // ATA device
        default:
            m_uActive = HDD_ACTIVE_FRAMES;
            CAtaAdapter::Out(m_bAddressLatch & ATOM_LITE_ADDR_MASK, bVal_);
            break;
        }
        break;

    default:
        TRACE("AtomLite: Unhandled write to %#04x with %#02x\n", wPort_, bVal_);
        break;
    }
}


bool CAtomLiteDevice::Attach(CHardDisk* pDisk_, int nDevice_)
{
    if (pDisk_)
    {
        bool fByteSwapped = false;

        // Require an Atom Lite format disk, rejecting Atom disks
        if (pDisk_->IsBDOSDisk(&fByteSwapped) && fByteSwapped)
            return false;

        // Disable legacy ATA requests that CF cards don't support
        pDisk_->SetLegacy(false);
    }

    CAtaAdapter::Attach(pDisk_, nDevice_);

    return pDisk_ != nullptr;
}
