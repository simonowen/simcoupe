// Part of SimCoupe - A SAM Coupé emulator
//
// Mouse.cpp: Mouse interface
//
//  Copyright (c) 1996-2001  Allan Skillman
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

// Changed 1999-2001 by Simon Owen
//  - moved interrupt reset out of the main CPU loop, for speed reasons
//  - removed the X mouse warp assumption

// ToDo:
//  - have mouse interrupt/reset as a CPU event, now it's no longer expensive

#include "SimCoupe.h"
#include "Mouse.h"

#include "CPU.h"
#include "Options.h"
#include "Util.h"

namespace Mouse
{

#define MOUSE_ACTIVE_TIME       USECONDS_TO_TSTATES(100)        // Mouse remains active for 100us

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

static bool fStrobed;           // true if the mouse has been strobed
static DWORD dwStrobeTime;      // Global cycle time of last strobe

static int nDeltaX, nDeltaY;    // Change in X and Y since last read
static BYTE bButtons;           // Current button states


static void Reset ();

////////////////////////////////////////////////////////////////////////////////

void Init (bool fFirstInit_/*=false*/)
{
    bButtons = 0xff;
    Reset();
}

void Exit (bool fReInit_/*=false*/)
{
    Reset();
}


BYTE Read (DWORD dwTime_)
{
    // If the mouse has been strobed, check to see if it's due a reset
    if ((fStrobed && ((dwTime_ - dwStrobeTime) >= MOUSE_ACTIVE_TIME)) || uBuffer >= sizeof(sMouse))
        Reset();

    // Is the first byte about to be read?
    if (!uBuffer)
    {
        // Flag the mouse as strobed and remember the strobe time
        fStrobed = true;
        dwStrobeTime = dwTime_;

        // Update the structure if there's something to show
        if (nDeltaX|nDeltaY)
        {
            // Horizontal movement
            sMouse.bX256 = (nDeltaX & 0xf00) >> 8;
            sMouse.bX16  = (nDeltaX & 0x0f0) >> 4;
            sMouse.bX1   = (nDeltaX & 0x00f);

            // Vertical movement
            sMouse.bY256 = (nDeltaY & 0xf00) >> 8;
            sMouse.bY16  = (nDeltaY & 0x0f0) >> 4;
            sMouse.bY1   = (nDeltaY & 0x00f);
        }

        // Button states
        sMouse.bButtons = bButtons;
    }

    // If we've still not returned all the mouse data, return the next byte
    BYTE bRet = reinterpret_cast<BYTE*>(&sMouse)[uBuffer++];

    // If we've reached the end of the buffer, clear the mouse offsets (as they've been read)
    if (uBuffer == sizeof(sMouse)-1)
        nDeltaX = nDeltaY = 0;

    return bRet;
}

// Move the mouse
void Move (int nDeltaX_, int nDeltaY_)
{
    nDeltaX += nDeltaX_;
    nDeltaY += nDeltaY_;
}

// Press or release a mouse button
void SetButton (int nButton_, bool fPressed_/*=true*/)
{
    // Work out the bit position for the button
    BYTE bBit = 1 << (nButton_-1);

    // Reset or set the bit depending on whether the button is being pressed or released
    if (fPressed_)
        bButtons &= ~bBit;
    else
        bButtons |= bBit;
}

static void Reset ()
{
    // Mouse not strobed, and next byte is the first byte
    fStrobed = false;
    uBuffer = 0;

    // Initialise the mouse data
    memset(&sMouse, 0, sizeof sMouse);
    sMouse.bStrobe = sMouse.bDummy = sMouse.bButtons = 0xff;
}

};
