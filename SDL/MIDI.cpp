// Part of SimCoupe - A SAM Coupé emulator
//
// MIDI.cpp: SDL MIDI interface
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

// ToDo:
//  - probably use SDL_mixer for MIDI, even tho it's software only at present

#include "SimCoupe.h"

#include "MIDI.h"
#include "Options.h"
#include "Util.h"



CMidiDevice::CMidiDevice ()
{
    // No previous data, status byte or handle
    m_nOut = 0;
    m_abOut[0] = '0';

    // Open the MIDI device here
}


CMidiDevice::~CMidiDevice ()
{
    // Close the MIDI device here
}


BYTE CMidiDevice::In (WORD wPort_)
{
    // No MIDI-IN support yet
    return 0x00;
}


void CMidiDevice::Out (WORD wPort_, BYTE bVal_)
{
    // Protect against very long System Exclusive blocks
    if ((m_nOut == (sizeof m_abOut - 1)) && bVal_ != 0xf7)
    {
        TRACE("!!! MIDI: System Exclusive buffer overflow, discarding %#02x\n", bVal_);
        return;
    }

    // Do we have the start of a message while an incomplete message remains?
    if (m_nOut && (bVal_ & 0x80))
    {
        TRACE("!!! MIDI: Discarding incomplete %d byte message\n", m_nOut);
        m_nOut = 0;
    }

    // Is the start of the message a non-status byte?
    else if (!m_nOut && !(bVal_ & 0x80))
    {
        // Use the previous status byte if there is one
        if (m_abOut[0] & 0x80)
            m_nOut = 1;

        // Discard the byte as there isn't much we can do with it
        else
        {
            TRACE("!!! MIDI: Discarding leading non-status byte: %#02x\n", bVal_);
            return;
        }
    }

    // Add the new byte to the message we're building up
    m_abOut[m_nOut++] = bVal_;

    // Spot the end of a System Exclusive variable length block (we don't do anything with it yet)
    if (m_abOut[0] == 0xf0 && bVal_ == 0xf7)
        TRACE("MIDI: Variable block of %d bytes\n", m_nOut - 2);

    // Break out if the command we're building up hasn't got the required number of parameters yet
    else if (((m_abOut[0] & 0xfd) == 0xf1) || ((m_abOut[0] & 0xe0) == 0xc0))    // 1 byte
    {
        if (m_nOut != 2)
            return;
    }
    else if ((m_abOut[0] & 0xf0) == 0xf0)   // 0 bytes
    {
        if (m_nOut != 1)
            return;
    }
    else
    {
        if (m_nOut != 3)
            return;
    }

#ifdef _DEBUG
    switch (m_nOut)
    {
        case 1:     TRACE("MIDI: Sending 1 byte message from: %02x\n", m_abOut[0]);                                                         break;
        case 2:     TRACE("MIDI: Sending 2 byte message from: %02x %02x\n", m_abOut[0], m_abOut[1]);                                            break;
        case 3:     TRACE("MIDI: Sending 3 byte message from: %02x %02x %02x\n", m_abOut[0], m_abOut[1], m_abOut[2]);                               break;
        case 4:     TRACE("MIDI: Sending 4 byte message from: %02x %02x %02x %02x\n", m_abOut[0], m_abOut[1], m_abOut[2], m_abOut[3]);              break;
        default:    TRACE("MIDI: Sending %d byte message from: %02x %02x %02x %02x ...\n", m_nOut, m_abOut[0], m_abOut[1], m_abOut[2], m_abOut[3]); break;
    }
#endif

    // Output the MIDI message here

    // Prepare for the next message, clearing out 
    m_nOut = m_abOut[1] = m_abOut[2] = m_abOut[3] = 0;
}
