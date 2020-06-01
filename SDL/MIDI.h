// Part of SimCoupe - A SAM Coupe emulator
//
// MIDI.h: SDL MIDI interface
//
//  Copyright (c) 1999-2012 Simon Owen
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

#pragma once

#include "SAMIO.h"

class MidiDevice : public IoDevice
{
public:
    MidiDevice();
    ~MidiDevice();

public:
    uint8_t In(uint16_t wPort_) override;
    void Out(uint16_t wPort_, uint8_t bVal_) override;

public:
    bool SetDevice(const char* pcszDevice_);

protected:
    uint8_t m_abIn[256]{};       // Buffers for MIDI IN and MIDI OUT data
    uint8_t m_abOut[256]{};
    int m_nIn = 0, m_nOut = 0; // Number of bytes in the buffers above

    int m_nDevice = -1;        // Device handle, or -1 if not open
};

extern std::unique_ptr<MidiDevice> pMidi;
