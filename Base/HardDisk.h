// Part of SimCoupe - A SAM Coupe emulator
//
// HardDisk.h: Hard disk abstraction layer
//
//  Copyright (c) 2004-2005 Simon Owen
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

#ifndef HARDDISK_H
#define HARDDISK_H

#include "ATA.h"


class CHardDisk : public CATADevice
{
    public:
        CHardDisk (const char* pcszDisk_);
        virtual ~CHardDisk ();

    public:
        static CHardDisk* OpenObject (const char* pcszDisk_);
        virtual bool Open () = 0;

        const char* GetPath () const { return m_pszDisk; }

        bool IsSDIDEDisk ();
        bool IsBDOSDisk ();

    protected:
        static bool CalculateGeometry (ATA_GEOMETRY* pg_);

    protected:
        char* m_pszDisk;
};


class CHDFHardDisk : public CHardDisk
{
    public:
        CHDFHardDisk (const char* pcszDisk_) : CHardDisk(pcszDisk_), m_hfDisk(NULL) { }
        ~CHDFHardDisk () { Close(); }

    public:
        static bool Create (const char* pcszDisk_, UINT uCylinders_, UINT uHeads_, UINT uSectors_);

    public:
        bool IsOpen () const { return m_hfDisk != NULL; }
        bool Open ();
        void Close ();

        bool ReadSector (UINT uSector_, BYTE* pb_);
        bool WriteSector (UINT uSector_, BYTE* pb_);

    protected:
        FILE* m_hfDisk;
};

#endif
