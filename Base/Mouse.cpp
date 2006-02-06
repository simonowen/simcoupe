// Part of SimCoupe - A SAM Coupe emulator
//
// Mouse.cpp: Mouse interface
//
//  Copyright (c) 1999-2006  Simon Owen
//  Copyright (c) 1996-2001  Allan Skillman
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


#define MOUSE_RESET_TIME       USECONDS_TO_TSTATES(50)      // Mouse is reset 50us after the last read

// Buffer structure for the mouse data
typedef struct
{
    BYTE    bStrobe, bDummy, bButtons;

    BYTE    bY256, bY16, bY1;
    BYTE    bX256, bX16, bX1;
}
MOUSEBUFFER;


static MOUSEBUFFER sMouse;
static UINT uBuffer;            // Read position in mouse data

static DWORD dwReadTime;        // Global cycle time of last mouse read

static int nDeltaX, nDeltaY;    // System change in X and Y since last read
static int nReadX, nReadY;      // Read change in X and Y
static BYTE bButtons;           // Current button states

////////////////////////////////////////////////////////////////////////////////

void Mouse::Init (bool fFirstInit_/*=false*/)
{
    // Clear cached mouse data
    nDeltaX = nDeltaY = 0;
    bButtons = 0;

    sMouse.bStrobe = sMouse.bDummy = 0xff;
    uBuffer = 0;
}

void Mouse::Exit (bool fReInit_/*=false*/)
{
}


BYTE Mouse::Read (DWORD dwTime_)
{
    // If the read timeout has expired, reset the mouse back to non-strobed
    if (uBuffer && (dwTime_ - dwReadTime) >= MOUSE_RESET_TIME)
        uBuffer = 0;

    // If the first real data byte is about to be read, update the mouse buffer
    if (uBuffer == 2)
    {
        // Button states
        sMouse.bButtons = ~bButtons;

        // Horizontal movement
        sMouse.bX256 = (nDeltaX & 0xf00) >> 8;
        sMouse.bX16  = (nDeltaX & 0x0f0) >> 4;
        sMouse.bX1   = (nDeltaX & 0x00f);

        // Vertical movement
        sMouse.bY256 = (nDeltaY & 0xf00) >> 8;
        sMouse.bY16  = (nDeltaY & 0x0f0) >> 4;
        sMouse.bY1   = (nDeltaY & 0x00f);

        nReadX = nDeltaX;
        nReadY = nDeltaY;
    }

    // Read the next byte
    BYTE bRet = reinterpret_cast<BYTE*>(&sMouse)[uBuffer++];

    // Has the full buffer been read?
    if (uBuffer == sizeof(sMouse))
    {
        // Subtract the read values from the overall tracked changes
        nDeltaX -= nReadX;
        nDeltaY -= nReadY;
        nReadX = nReadY = 0;

        // Move back to the start of the data, but stay strobed
        uBuffer = 1;
    }

    // Remember the last read time, so we timeout after the appropriate amount of time
    dwReadTime = dwTime_;

    return bRet;
}

// Move the mouse
void Mouse::Move (int nDeltaX_, int nDeltaY_)
{
    nDeltaX += nDeltaX_;
    nDeltaY += nDeltaY_;
}

// Press or release a mouse button
void Mouse::SetButton (int nButton_, bool fPressed_/*=true*/)
{
    // If enabled, swap mouse buttons 2 and 3
    if (GetOption(swap23) && (nButton_ == 2 || nButton_ == 3))
        nButton_ ^= 1;

    // Work out the bit position for the button
    BYTE bBit = 1 << (nButton_-1);

    // Reset or set the bit depending on whether the button is being pressed or released
    if (fPressed_)
        bButtons |= bBit;
    else
        bButtons &= ~bBit;
}
