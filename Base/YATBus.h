// Part of SimCoupe - A SAM Coupe emulator
//
// Atom.h: YAMOD.ATBUS IDE interface
//
//  Copyright (c) 1999-2004  Simon Owen
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

#ifndef YATBUS_H
#define YATBUS_H

#include "IO.h"
#include "ATA.h"

class CYATBusDevice : public CDiskDevice
{
    public:
        CYATBusDevice (CHardDisk* pDisk_);
        ~CYATBusDevice ();

    public:
        void Reset ();
        BYTE In (WORD wPort_);
        void Out (WORD wPort_, BYTE bVal_);

    protected:
        CATADevice* m_pDisk;

        BYTE m_bLatch;          // data latch for 16-bit reads/writes
        bool m_fDataLatched;    // true if the latch is full
};

#endif
