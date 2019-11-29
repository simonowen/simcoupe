// Part of SimCoupe - A SAM Coupe emulator
//
// MIDI.h: Win32 MIDI interface
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef MIDI_H
#define MIDI_H

#include "SAMIO.h"

class CMidiDevice : public CIoDevice
{
public:
    CMidiDevice();
    ~CMidiDevice();

public:
    BYTE In(WORD wPort_) override;
    void Out(WORD wPort_, BYTE bVal_) override;

public:
    bool SetDevice(const char* pcszDevice_);

protected:
    HMIDIOUT m_hMidiOut = nullptr; // Handle for Windows MIDI OUT device

    BYTE m_abOut[256];       // Buffer to build up MIDI OUT messages
    int m_nOut = 0;          // Number of bytes currently in abOut
};

extern CMidiDevice* pMidi;

#endif // MIDI_H
