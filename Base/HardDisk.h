// Part of SimCoupe - A SAM Coupe emulator
//
// HardDisk.h: Hard disk abstraction layer
//
//  Copyright (c) 2004 Simon Owen
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

typedef struct
{
    UINT uTotalSectors;
    UINT uCylinders, uHeads, uSectors;
}
HARDDISK_GEOMETRY;


class CHardDisk
{
    public:
        CHardDisk ();
        virtual ~CHardDisk ();

    public:
        static CHardDisk* OpenObject (const char* pcszDisk_);

        const DEVICEIDENTITY* GetIdentity () const { return &m_sIdentity; }
        void GetGeometry (HARDDISK_GEOMETRY* pGeom_) const { memcpy(pGeom_, &m_sGeometry, sizeof m_sGeometry); }

        virtual bool IsOpen () const  = 0;
        virtual bool Open (const char* pcszDisk_) = 0;
        virtual void Close () = 0;

        virtual bool ReadSector (UINT uSector_, BYTE* pb_) = 0;
        virtual bool WriteSector (UINT uSector_, BYTE* pb_) = 0;

        bool IsSDIDEDisk ();
        bool IsBDOSDisk ();

    protected:
        bool CalculateGeometry (HARDDISK_GEOMETRY* pg_);

    protected:
        HARDDISK_GEOMETRY m_sGeometry;
        DEVICEIDENTITY m_sIdentity;
};


class CHDFHardDisk : public CHardDisk
{
    public:
        CHDFHardDisk () : m_hfDisk(NULL) { }
        ~CHDFHardDisk () { Close(); }

    public:
        static bool Create (const char* pcszDisk_, UINT uCylinders_, UINT uHeads_, UINT uSectors_);

    public:
        bool IsOpen () const { return m_hfDisk != NULL; }
        bool Open (const char* pcszDisk_);
        void Close ();

        bool ReadSector (UINT uSector_, BYTE* pb_);
        bool WriteSector (UINT uSector_, BYTE* pb_);

    protected:
        FILE* m_hfDisk;
};

#endif
