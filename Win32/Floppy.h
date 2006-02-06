// Part of SimCoupe - A SAM Coupe emulator
//
// Floppy.h: W2K/XP/W2K3 direct floppy access using fdrawcmd.sys
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

class CFloppyStream : public CStream
{
    public:
        CFloppyStream (const char* pcszDevice_, bool fReadOnly_);
        virtual ~CFloppyStream ();

    public:
        static bool IsAvailable ();
        static bool IsRecognised (const char* pcszStream_);

    public:
        void Close ();
        unsigned long ThreadProc ();

    public:
        bool IsOpen () const { return m_hDevice != INVALID_HANDLE_VALUE; }

        bool Rewind () { return false; }
        size_t Read (void* pvBuffer_, size_t uLen_) { return 0; }
        size_t Write (void* pvBuffer_, size_t uLen_) { return 0; }

        BYTE ReadTrack (BYTE cyl_, BYTE head_, PBYTE pbData_);
        BYTE ReadWrite (bool fRead_, BYTE bSide_, BYTE bTrack_, BYTE* pbData_);
        bool ReadCustomTrack (BYTE cyl_, BYTE head_, PBYTE pbData_);
        bool ReadMGTTrack (BYTE cyl_, BYTE head_, PBYTE pbData_);

        bool IsBusy (BYTE* pbStatus_, bool fWait_);

    protected:
        HANDLE  m_hDevice, m_hThread;
        bool    m_fMGT;

        BYTE    m_bCommand, m_bSide, m_bTrack, *m_pbData, m_bStatus;
};

#endif  // FLOPPY_H
