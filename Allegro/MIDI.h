// Part of SimCoupe - A SAM Coupe emulator
//
// MIDI.h: Allegro MIDI interface
//
//  Copyright (c) 1999-2002  Simon Owen
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
        bool        m_fAvailable;   // true if MIDI is ready for use
        BYTE        m_abOut[256];   // Buffer to build up MIDI OUT messages
        int         m_nOut;         // Number of bytes currently in abOut
};

#endif
