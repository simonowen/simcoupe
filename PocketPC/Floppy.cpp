// Part of SimCoupe - A SAM Coupe emulator
//
// Floppy.cpp: WinCE direct floppy access (dummy module)
//
//  Copyright (c) 1999-2006  Simon Owen
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

#include "SimCoupe.h"
#include "Floppy.h"

/*static*/ bool CFloppyStream::IsRecognised (const char* pcszStream_)
{
    return false;
}

void CFloppyStream::Close ()
{
}

BYTE CFloppyStream::StartCommand (BYTE bCommand_, PTRACK pTrack_, UINT uSector_, BYTE *pbData_)
{
    return BUSY;
}

bool CFloppyStream::IsBusy (BYTE* pbStatus_, bool fWait_)
{
    *pbStatus_ = LOST_DATA;
    return false;
}
