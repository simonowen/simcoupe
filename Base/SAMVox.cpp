// Part of SimCoupe - A SAM Coupe emulator
//
// SAMVox.cpp: SAM Vox 4-channel 8-bit DAC
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

// Hardware use reverse-engineered using Modules Tracker by ZKSOFT

#include "SimCoupe.h"
#include "SAMVox.h"

#include "Sound.h"

void CSAMVoxDevice::Out (WORD wPort_, BYTE bVal_)
{
    switch (wPort_ & 3)
    {
        case 0:
            pDAC->OutputRight(bVal_);
            break;

        case 1:
            pDAC->OutputLeft(bVal_);
            break;

        case 2:
            pDAC->OutputRight2(bVal_);
            break;

        case 3:
            pDAC->OutputLeft2(bVal_);
            break;
    }
}
