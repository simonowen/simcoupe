// Part of SimCoupe - A SAM Coupe emulator
//
// HardDisk.h: Hard disk abstraction layer
//
//  Copyright (c) 2004-2012 Simon Owen
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

#include "IO.h"
#include "ATA.h"

const unsigned int HDD_ACTIVE_FRAMES = 2;    // Frames the HDD is considered active after a command


class CHardDisk : public CATADevice
{
    public:
        CHardDisk (const char* pcszDisk_);
        virtual ~CHardDisk ();

    public:
        static CHardDisk* OpenObject (const char* pcszDisk_);
        virtual bool Open () = 0;

    public:
        const char* GetPath () const { return m_pszDisk; }

    public:
        bool IsSDIDEDisk ();
        bool IsBDOSDisk (bool *pfByteSwapped=NULL);

    protected:
        static void CalculateGeometry (ATA_GEOMETRY* pg_);
        static void SetIdentityString (char* psz_, size_t uLen_, const char* pcszValue_);

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
        UINT m_uDataOffset, m_uSectorSize;
};


class CHardDiskDevice :  public CDiskDevice
{
    public:
        CHardDiskDevice () : m_pDisk(NULL) { }
        ~CHardDiskDevice () { delete m_pDisk; }

    public:
        void Reset () { if (m_pDisk) m_pDisk->Reset(); }

    public:
        bool Insert (const char *pcszDisk_, bool fReadOnly_=false) { return Insert(CHardDisk::OpenObject(pcszDisk_)); }
        void Eject () { delete m_pDisk, m_pDisk = NULL; }

    public:
        const char* DiskFile() const { return ""; }
        const char* DiskPath() const { return m_pDisk ? m_pDisk->GetPath() : ""; }
        bool IsLightOn () const { return IsActive(); }

    public:
        virtual bool Insert (CHardDisk *pDisk_) { delete m_pDisk; m_pDisk = pDisk_; return m_pDisk != NULL; }

    protected:
        CHardDisk* m_pDisk;
};

extern CHardDiskDevice *pAtom, *pSDIDE, *pYATBus;

#endif // HARDDISK_H
