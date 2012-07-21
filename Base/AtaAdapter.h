// Part of SimCoupe - A SAM Coupe emulator
//
// AtaAdapter.h: ATA bus adapter
//
//  Copyright (c) 2012 Simon Owen
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

#ifndef ATAADAPTER_H
#define ATAADAPTER_H

#include "HardDisk.h"

class CAtaAdapter : public CIoDevice
{
    public:
        CAtaAdapter ();
        ~CAtaAdapter ();

    public:
        BYTE In (WORD wPort_);
        void Out (WORD wPort_, BYTE bVal_);

        void Reset ();
        void FrameEnd () { if (m_uActive) m_uActive--; }

    public:
        const char* DiskPath (int nDevice_) const;
        bool IsActive () const { return m_uActive != 0; }

    public:
        bool Insert (const char *pcszDisk_, int nDevice_);
        virtual bool Insert (CHardDisk *pDisk_, int nDevice_);

    protected:
        WORD InWord (WORD wPort_);

    protected:
        UINT m_uActive; // active when non-zero, decremented by FrameEnd()

    private:
        BYTE m_bActiveDevice;
        CHardDisk *m_pDisk0;
        CHardDisk *m_pDisk1;
};

extern CAtaAdapter *pAtom, *pSDIDE, *pYATBus;

#endif // ATAADAPTER_H
