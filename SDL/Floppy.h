// Part of SimCoupe - A SAM Coupe emulator
//
// Floppy.h: SDL direct floppy access
//
//  Copyright (c) 1999-2006  Simon Owen
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
#include "VL1772.h"

class CFloppyStream : public CStream
{
    public:
        CFloppyStream (const char* pcszStream_, bool fReadOnly_=false)
            : CStream(pcszStream_, fReadOnly_), m_nFloppy(-1), m_uTrack(0) { }
        ~CFloppyStream () { Close(); }

    public:
        static bool IsRecognised (const char* pcszStream_);

    public:
        void Close ();

    public:
        bool IsOpen () const { return m_nFloppy != -1; }

        bool Rewind () { return false; }
        size_t Read (void* pvBuffer_, size_t uLen_) { return 0; }
        size_t Write (void* pvBuffer_, size_t uLen_) { return 0; }

        BYTE ReadTrack (BYTE cyl_, BYTE head_, PBYTE pbData_);
        BYTE ReadWrite (bool fRead_, BYTE bSide_, BYTE bTrack_, BYTE* pbData_);
        bool ReadCustomTrack (BYTE cyl_, BYTE head_, PBYTE pbData_);
        bool ReadMGTTrack (BYTE cyl_, BYTE head_, PBYTE pbData_);

        bool IsBusy (BYTE* pbStatus_, bool fWait_);

    protected:
        bool Open ();
        bool ReadWrite (bool fRead_, UINT uSide_, UINT uTrack_, UINT uSector_, BYTE* pbData_, UINT* puSize_);

    protected:
        int m_nFloppy;
        UINT m_uTrack;
};

#endif  // FLOPPY_H
