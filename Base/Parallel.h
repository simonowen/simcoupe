// Part of SimCoupe - A SAM Coupe emulator
//
// Parallel.cpp: Parallel interface
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

#ifndef PARALLEL_H
#define PARALLEL_H

#include "Sound.h"
#include "IO.h"


class CPrinterDevice : public CIoDevice
{
    public:
        CPrinterDevice();
        ~CPrinterDevice ();

    public:
        BYTE In (WORD wPort_);
        void Out (WORD wPort_, BYTE bVal_);

    protected:
        BYTE    m_bPrint, m_bStatus;

        bool    m_fFailed;              // true if we've failed to start a print job and complained
        int     m_nPrinter;
        BYTE    m_abPrinter[2048];

    protected:
        bool IsOpen () const { return false; }

        virtual bool Open ();
        virtual void Close ();
        virtual void Write (BYTE bPrint_);
        virtual void Flush ();
};


class CMonoDACDevice : public CIoDevice
{
    public:
        void Out (WORD wPort_, BYTE bVal_);
};


class CStereoDACDevice : public CIoDevice
{
    public:
        CStereoDACDevice () : m_bVal(0x80) { }

    public:
        void Out (WORD wPort_, BYTE bVal_);

    protected:
        BYTE m_bVal;
};

#endif  // PARALLEL_H
