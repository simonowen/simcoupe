// Part of SimCoupe - A SAM Coupé emulator
//
// Atom.h: ATOM hard disk inteface
//
//  Copyright (c) 1999-2001  Simon Owen
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

#ifndef ATOM_H
#define ATOM_H

#include "IO.h"
#include "CDisk.h"
#include "ATA.h"

const BYTE ATOM_ADDR_MASK   = 0x07;
const BYTE ATOM_NCS1        = 0x08;     // Chip select 1 (negative logic)
const BYTE ATOM_NCS3        = 0x10;     // Chip select 3 (negative logic)
const BYTE ATOM_NRESET      = 0x20;     // Reset pin (negative logic)


typedef list< pair<UINT,CDisk*> > CACHELIST;
typedef vector<string> STRINGARRAY;

// BDOS-specific disk implementation for convenient record access, and caching
class CBDOSDevice : public CATADevice
{
    public:
        CBDOSDevice ();
        ~CBDOSDevice ();

    public:
        bool DiskReadWrite (bool fWrite_);  // Override for disk reads and writes

    protected:
        bool Init ();

    protected:
        FILE* m_hfDisk;             // File containing HDD boot sector and (dynamically generated) record name list

        STRINGARRAY m_asRecords;    // Array of paths for each record considered part of the disk
        CACHELIST m_lCached;        // MRU sorted cache of CDisk record objects

        UINT m_uReservedSectors;     // Number of sectors on start of disk reserved for boot sector and record name list
};


class CAtomDiskDevice : public CDiskDevice
{
    public:
        CAtomDiskDevice ();
        ~CAtomDiskDevice ();

    public:
        int GetType () const { return dskAtom; }

        BYTE In (WORD wPort_);
        void Out (WORD wPort_, BYTE bVal_);
        void FrameEnd ();

        bool IsLightOn () const { return m_uLightDelay != 0; }

    protected:
        CBDOSDevice* m_pDisk;

        BYTE m_bAddressLatch, m_bDataLatch;

        UINT m_uLightDelay;      // Delay before switching disk light off
};

#endif
