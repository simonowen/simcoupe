// Part of SimCoupe - A SAM Coupe emulator
//
// Atom.h: ATOM hard disk interface
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

#ifndef ATOM_H
#define ATOM_H

#include "AtaAdapter.h"

const BYTE ATOM_ADDR_MASK = 0x1f;   // Chip select mask
const BYTE ATOM_REG_MASK = 0x07;    // Device address mask
const BYTE ATOM_NRESET = 0x20;      // Reset pin (negative logic)

class CAtomDevice : public CAtaAdapter
{
public:
    BYTE In(WORD wPort_) override;
    void Out(WORD wPort_, BYTE bVal_) override;

public:
    bool Attach(CHardDisk* pDisk_, int nDevice_) override;

protected:
    BYTE m_bAddressLatch = 0;
    BYTE m_bReadLatch = 0;
    BYTE m_bWriteLatch = 0;
};

#endif // ATOM_H
