// Part of SimCoupe - A SAM Coupe emulator
//
// Mouse.h: Mouse interface
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

#ifndef MOUSE_H
#define MOUSE_H

#include "IO.h"

#define MOUSE_RESET_TIME       USECONDS_TO_TSTATES(30)      // Mouse is reset 30us after the last read
#define MOUSE_ACTIVE_TIME      1000                         // Device in active use if last read within 1000ms

// Mouse buffer format, as read
typedef struct
{
    BYTE bStrobe, bDummy, bButtons;
    BYTE bY256, bY16, bY1;
    BYTE bX256, bX16, bX1;
}
MOUSEBUFFER;


class CMouseDevice : public CIoDevice
{
    public:
        CMouseDevice ();

    public:
        void Reset ();
        BYTE In (WORD wPort_);

    public:
        void Move (int nDeltaX_, int nDeltaY_);
        void SetButton (int nButton_, bool fPressed_=true);
        bool IsActive () const;

    protected:
        int m_nDeltaX, m_nDeltaY;   // System change in X and Y since last read
        int m_nReadX, m_nReadY;     // Read change in X and Y
        BYTE m_bButtons;            // Current button states
        DWORD m_dwLastRead;         // When the mouse was last read

        MOUSEBUFFER m_sMouse;
        UINT m_uBuffer;             // Read position in mouse data
};

extern CMouseDevice *pMouse;

#endif // MOUSE_H
