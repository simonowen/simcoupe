// Part of SimCoupe - A SAM Coupe emulator
//
// Floppy.h: WinCE direct floppy access (dummy module)
//
//  Copyright (c) 1999-2003  Simon Owen
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

class Floppy
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);
};


class CFloppyStream : public CStream
{
    public:
        CFloppyStream (const char* pcszStream_, bool fReadOnly_) : CStream(pcszStream_, fReadOnly_) { }
        virtual ~CFloppyStream () { Close(); }

    public:
        static bool IsRecognised (const char* pcszStream_) { return false; }

    public:
        bool IsOpen () const { return false; }
        bool Rewind () { return false; }
        size_t Read (void* pvBuffer_, size_t uLen_) { return 0; }
        size_t Write (void* pvBuffer_, size_t uLen_) { return 0; }

        BYTE Read (int nSide_, int nTrack_, int nSector_, BYTE* pbData_, UINT* puSize_) { return RECORD_NOT_FOUND; }
        BYTE Write (int nSide_, int nTrack_, int nSector_, BYTE* pbData_, UINT* puSize_) { return WRITE_PROTECT; }

        bool GetAsyncStatus (UINT* puSize_, BYTE* pbStatus_) { return false; }
        bool WaitAsyncOp (UINT* puSize_, BYTE* pbStatus_) { return false; }
        void AbortAsyncOp () { }

    protected:
        void Close () { }
        BYTE TranslateError () const;
};

#endif  // FLOPPY_H
