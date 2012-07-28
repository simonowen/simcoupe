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
#include "Options.h"


CAtomLiteDevice::CAtomLiteDevice ()
    : m_bAddressLatch(0)
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
            else
                bRet = CAtaAdapter::InWord(m_bAddressLatch & ATOM_LITE_ADDR_MASK) & 0xff;
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

            // If the reset pin is low, reset the disk
            if (~bVal_ & ATOM_LITE_NRESET)
                CAtaAdapter::Reset();

            break;

        // Both data ports behave the same
        case 6:
        case 7:
            // Dallas clock or disk addressed?
            if ((m_bAddressLatch & ATOM_LITE_ADDR_MASK) == 0x1d)
                m_Dallas.Out(wPort_ << 8, bVal_);
            else
            {
                // If reset is held, ignore the disk write
                if (~bVal_ & ATOM_LITE_NRESET)
                    break;

                m_uActive = HDD_ACTIVE_FRAMES;
                CAtaAdapter::Out(m_bAddressLatch & ATOM_LITE_ADDR_MASK, bVal_);
            }
            break;

        default:
            TRACE("AtomLite: Unhandled write to %#04x with %#02x\n", wPort_, bVal_);
            break;
    }
}


bool CAtomLiteDevice::Insert (CHardDisk *pDisk_, int nDevice_)
{
    bool fByteSwapped = false;

    if (pDisk_)
    {
        // Optionally byte-swap original Atom media to work with the Atom Lite
        if (GetOption(autobyteswap) && pDisk_->IsBDOSDisk(&fByteSwapped))
            pDisk_->SetByteSwap(fByteSwapped);

        // CF media doesn't support old requests
        pDisk_->SetLegacy(false);
    }

    CAtaAdapter::Insert(pDisk_, nDevice_);

    return pDisk_ != NULL;
}
