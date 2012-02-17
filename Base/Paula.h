// Part of SimCoupe - A SAM Coupe emulator
//
// Paula.h: Paula 4-bit dual-DAC interface
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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef PAULA_H
#define PAULA_H

#include "IO.h"

class CPaulaDevice : public CIoDevice
{
    public:
        CPaulaDevice () { }

    public:
        void Out (WORD wPort_, BYTE bVal_);
};

extern CPaulaDevice *pPaula;

#endif  // PAULA_H
