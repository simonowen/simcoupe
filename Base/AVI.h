// Part of SimCoupe - A SAM Coupe emulator
//
// AVI.h: AVI movie recording
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

#ifndef AVI_H
#define AVI_H

#include "Screen.h"

class AVI
{
    public:
        static bool Start (bool fHalfSize_=false);
        static void Stop ();
        static void Toggle (bool fHalfSize_=false);
        static bool IsRecording ();

        static void AddFrame (CScreen *pScreen_);
        static void AddFrame (BYTE *pbAudio_, UINT uLen_);
};

#endif // AVI_H
