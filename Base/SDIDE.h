// Part of SimCoupe - A SAM Coupe emulator
//
// SDIDE.h: S D Software IDE interface
//
//  Copyright (c) 1999-2005  Simon Owen
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

#ifndef SDIDE_H
#define SDIDE_H

#include "IO.h"
#include "ATA.h"

class CSDIDEDevice : public CDiskDevice
{
    public:
        CSDIDEDevice (CATADevice* pDisk_);
        ~CSDIDEDevice ();

    public:
        void Reset ();
        BYTE In (WORD wPort_);
        void Out (WORD wPort_, BYTE bVal_);

        const char* GetPath() const { return m_pDisk->GetPath(); }

    protected:
        CATADevice* m_pDisk;

        BYTE m_bAddressLatch, m_bDataLatch;
        bool m_fDataLatched;
};

#endif
