// Part of SimCoupe - A SAM Coupe emulator
//
// BlueAlpha.cpp: Blue Alpha Sampler
//
//  Copyright (c) 1999-2010  Simon Owen
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

#ifndef BLUEALPHA_H
#define BLUEALPHA_H

#include "IO.h"

#define BLUE_ALPHA_CLOCK_TIME   (REAL_TSTATES_PER_SECOND/BlueAlphaSampler::GetClockFreq()/2)    // half period

class BlueAlphaSampler
{
    public:
        static void Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

    public:
        static void Reset ();
        static bool Clock ();
        static int GetClockFreq ();

        static BYTE In (WORD wPort_);
        static void Out (WORD wPort_, BYTE bVal_);
};

#endif  // BLUEALPHA_H
