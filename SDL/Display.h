// Part of SimCoupe - A SAM Coupé emulator
//
// Display.h: SDL display rendering
//
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

#ifndef DISPLAY_H
#define DISPLAY_H

#include "CScreen.h"


namespace Display
{
    bool Init (bool fFirstInit_=false);
    void Exit (bool fReInit_=false);

    bool IsDirty (int nLine_);
    void SetDirty (int nLine_=-1);

    void Update (CScreen* pScreen_);
};

#endif  // DISPLAY_H
