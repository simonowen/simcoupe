// Part of SimCoupe - A SAM Coupé emulator
//
// MIDI.h: SDL MIDI interface
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

#ifndef MIDI_H
#define MIDI_H

#include "IO.h"

class CMidiDevice : public CIoDevice
{
    public:
        CMidiDevice();
        ~CMidiDevice();

    public:
        BYTE In (WORD wPort_);
        void Out (WORD wPort_, BYTE bVal_);

    protected:
        BYTE    m_abIn[256], m_abOut[256];  // Buffers for MIDI IN and MIDI OUT data
        int     m_nIn, m_nOut;              // Number of bytes in the buffers above

        int     m_nDevice;                  // Device handle, or -1 if not open
};

#endif
