// Part of SimCoupe - A SAM Coupe emulator
//
// Mouse.cpp: Mouse interface
//
//  Copyright (c) 1999-2012 Simon Owen
//  Copyright (c) 1996-2001 Allan Skillman
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

// Changed 1999-2001 by Simon Owen
//  - moved interrupt reset out of the main CPU loop, for speed reasons
//  - removed the X mouse warp assumption

#include "SimCoupe.h"
#include "Mouse.h"

#include "CPU.h"
#include "Options.h"
#include "Util.h"

////////////////////////////////////////////////////////////////////////////////

CMouseDevice::CMouseDevice ()
    : m_nDeltaX(0), m_nDeltaY(0), m_bButtons(0), m_uBuffer(0)
{
    m_sMouse.bStrobe = m_sMouse.bDummy = 0xff;
}


void CMouseDevice::Reset ()
{
    // No longer strobed
    m_uBuffer = 0;
}

BYTE CMouseDevice::In (WORD wPort_)
{
    // If the first real data byte is about to be read, update the mouse buffer
    if (m_uBuffer == 2)
    {
        // Button states
        m_sMouse.bButtons = ~m_bButtons;

        // Horizontal movement
        m_sMouse.bX256 = (m_nDeltaX & 0xf00) >> 8;
        m_sMouse.bX16  = (m_nDeltaX & 0x0f0) >> 4;
        m_sMouse.bX1   = (m_nDeltaX & 0x00f);

        // Vertical movement
        m_sMouse.bY256 = (m_nDeltaY & 0xf00) >> 8;
        m_sMouse.bY16  = (m_nDeltaY & 0x0f0) >> 4;
        m_sMouse.bY1   = (m_nDeltaY & 0x00f);

        m_nReadX = m_nDeltaX;
        m_nReadY = m_nDeltaY;
    }

    // Read the next byte
    BYTE bRet = reinterpret_cast<BYTE*>(&m_sMouse)[m_uBuffer++];

    // Has the full buffer been read?
    if (m_uBuffer == sizeof(m_sMouse))
    {
        // Subtract the read values from the overall tracked changes
        m_nDeltaX -= m_nReadX;
        m_nDeltaY -= m_nReadY;
        m_nReadX = m_nReadY = 0;

        // Move back to the start of the data, but stay strobed
        m_uBuffer = 1;
    }

    // Cancel any pending reset event, and schedule a fresh one
    if (m_uBuffer) CancelCpuEvent(evtMouseReset);
    AddCpuEvent(evtMouseReset, g_dwCycleCounter + MOUSE_RESET_TIME);

    return bRet;
}


// Move the mouse
void CMouseDevice::Move (int nDeltaX_, int nDeltaY_)
{
    m_nDeltaX += nDeltaX_;
    m_nDeltaY += nDeltaY_;
}

// Press or release a mouse button
void CMouseDevice::SetButton (int nButton_, bool fPressed_/*=true*/)
{
    // If enabled, swap mouse buttons 2 and 3
    if (GetOption(swap23) && (nButton_ == 2 || nButton_ == 3))
        nButton_ ^= 1;

    // Work out the bit position for the button
    BYTE bBit = 1 << (nButton_-1);

    // Reset or set the bit depending on whether the button is being pressed or released
    if (fPressed_)
        m_bButtons |= bBit;
    else
        m_bButtons &= ~bBit;
}
