// Part of SimCoupe - A SAM Coupe emulator
//
// Paula.cpp: Paula 4-bit dual-DAC interface
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
#include "Paula.h"

#include "Sound.h"

void PaulaDevice::Out(uint16_t wPort_, uint8_t bVal_)
{
    switch (wPort_ & 1)
    {
    case 0:
        // A nibble to each channel on the first DAC
        pDAC->OutputLeft(bVal_ << 4);
        pDAC->OutputRight(bVal_ & 0xf0);
        break;

    case 1:
        // A nibble to each channel on the second DAC
        pDAC->OutputLeft2(bVal_ << 4);
        pDAC->OutputRight2(bVal_ & 0xf0);
        break;
    }
}
