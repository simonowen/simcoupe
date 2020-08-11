// Part of SimCoupe - A SAM Coupe emulator
//
// Mouse.cpp: Mouse interface
//
//  Copyright (c) 1999-2014 Simon Owen
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

// Changed 1999-2001 by Simon Owen
//  - moved interrupt reset out of the main CPU loop, for speed reasons
//  - removed the X mouse warp assumption

#include "SimCoupe.h"
#include "Mouse.h"

#include "CPU.h"
#include "Options.h"


MouseDevice::MouseDevice()
{
    memset(&m_sMouse, 0, sizeof(m_sMouse));
    m_sMouse.bStrobe = m_sMouse.bDummy = 0xff;
}


void MouseDevice::Reset()
{
    // No longer strobed
    m_uBuffer = 0;
}

uint8_t MouseDevice::In(uint16_t /*wPort_*/)
{
    // If the first real data byte is about to be read, update the mouse buffer
    if (m_uBuffer == 2)
    {
        // Button states
        m_sMouse.bButtons = ~m_bButtons;

        // Horizontal movement
        m_sMouse.bX256 = (m_nDeltaX & 0xf00) >> 8;
        m_sMouse.bX16 = (m_nDeltaX & 0x0f0) >> 4;
        m_sMouse.bX1 = (m_nDeltaX & 0x00f);

        // Vertical movement
        m_sMouse.bY256 = (m_nDeltaY & 0xf00) >> 8;
        m_sMouse.bY16 = (m_nDeltaY & 0x0f0) >> 4;
        m_sMouse.bY1 = (m_nDeltaY & 0x00f);

        // Keep track of the movement we're reporting
        m_nReadX = m_nDeltaX;
        m_nReadY = m_nDeltaY;
    }

    // Read the next byte
    uint8_t bRet = reinterpret_cast<uint8_t*>(&m_sMouse)[m_uBuffer++];

    // Has the full buffer been read?
    if (m_uBuffer == sizeof(m_sMouse))
    {
        // Subtract the read values from the overall tracked changes
        m_nDeltaX -= m_nReadX;
        m_nDeltaY -= m_nReadY;
        m_nReadX = m_nReadY = 0;

        // Move back to the start of the data, but stay strobed
        m_uBuffer = 1;

        // If it's not the ROM reading the mouse, remember the last read time
        if (REG_PC != 0xd4d6)
            read_time = std::chrono::steady_clock::now();
    }

    // Cancel any pending reset event, and schedule a fresh one
    if (m_uBuffer) CancelCpuEvent(EventType::MouseReset);
    AddCpuEvent(EventType::MouseReset, g_dwCycleCounter + MOUSE_RESET_TIME);

    return bRet;
}


// Move the mouse
void MouseDevice::Move(int nDeltaX_, int nDeltaY_)
{
    m_nDeltaX += nDeltaX_;
    m_nDeltaY += nDeltaY_;
}

// Press or release a mouse button
void MouseDevice::SetButton(int nButton_, bool fPressed_/*=true*/)
{
    // Work out the bit position for the button
    uint8_t bBit = 1 << (nButton_ - 1);

    // Reset or set the bit depending on whether the button is being pressed or released
    if (fPressed_)
        m_bButtons |= bBit;
    else
        m_bButtons &= ~bBit;
}

// Report whether the mouse is actively in use
bool MouseDevice::IsActive() const
{
    auto now = std::chrono::steady_clock::now();
    return read_time && (now - *read_time) <= MOUSE_ACTIVE_TIME;
}
