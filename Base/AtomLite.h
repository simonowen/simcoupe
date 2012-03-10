// Part of SimCoupe - A SAM Coupe emulator
//
// AtomLite.h: Atom-Lite hard disk interface
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

#ifndef ATOMLITE_H
#define ATOMLITE_H

#include "HardDisk.h"
#include "Clock.h"

const BYTE ATOM_LITE_ADDR_MASK = 0x1f;  // Chip select mask
const BYTE ATOM_LITE_REG_MASK  = 0x07;  // Device address mask

class CAtomLiteDevice : public CHardDiskDevice
{
    public:
        CAtomLiteDevice ();

    public:
        BYTE In (WORD wPort_);
        void Out (WORD wPort_, BYTE bVal_);

    protected:
        CDallasClock m_Dallas;
        BYTE m_bAddressLatch, m_bDataLatch;
};

#endif // ATOMLITE_H
