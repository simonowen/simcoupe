// Part of SimCoupe - A SAM Coupé emulator
//
// Floppy.h: Win32 direct floppy access
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

#ifndef FLOPPY_H
#define FLOPPY_H

#include "CStream.h"

namespace Floppy
{
    bool Init ();
    void Exit (bool fReInit_=true);
}

class CFloppyStream : public CStream
{
    public:
        CFloppyStream (const char* pcszStream_, bool fReadOnly_);
        virtual ~CFloppyStream () { Close(); }

    public:
        static bool IsRecognised (const char* pcszStream_);
        static bool LoadDriver ();
        static bool UnloadDriver ();

    public:
        bool IsOpen () const { return m_hDevice != INVALID_HANDLE_VALUE; }
        bool Rewind () { m_dwPos = 0; return true; }
        long Read (void* pvBuffer_, long lLen_) { return 0; }
        long Write (void* pvBuffer_, long lLen_) { return 0; }

        BYTE Read (int nSide_, int nTrack_, int nSector_, BYTE* pbData_, UINT* puSize_);
        BYTE Write (int nSide_, int nTrack_, int nSector_, BYTE* pbData_, UINT* puSize_);

    protected:
        HANDLE  m_hDevice;
        DWORD   m_dwPos;

    protected:
        void Close ();
};

#endif  // FLOPPY_H
