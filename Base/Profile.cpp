// Part of SimCoupe - A SAM Coupe emulator
//
// Profile.cpp: Emulator profiling for on-screen stats
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

// Notes:
//  Uses OSD::GetProfileTime() for an accurate time stamp.
//  DWORD types are used for millisecond values by default, ut PROFILE_T
//  can be re-defined on platforms that support more accurate values
//  through the use of different types.

#include "SimCoupe.h"
#include "Profile.h"

#include "Options.h"
#include "OSD.h"
#include "UI.h"

////////////////////////////////////////////////////////////////////////////////

PROFILE Profile::s_sProfile;


#ifdef USE_LOWRES
#define AddPercent(x)   sprintf(sz + strlen(sz), "  %c:%lu%%", #x[0], ((s_sProfile.prof##x + profTotal/200UL) * 100UL) / profTotal)
#else
#define AddPercent(x)   sprintf(sz + strlen(sz), "  %s:%lu%%", #x, ((s_sProfile.prof##x + profTotal/200UL) * 100UL) / profTotal)
#endif

const char* Profile::GetStats ()
{
    static char sz[64];
    sz[0] = '\0';

    PROFILE_T profTotal = s_sProfile.profCPU + s_sProfile.profGfx + s_sProfile.profSnd + s_sProfile.profBlt + s_sProfile.profIdle;

    if (profTotal)
    {
        switch (GetOption(profile))
        {
            case 2:
            {
                strcat(sz, " ");
                AddPercent(CPU);
                AddPercent(Gfx);
                AddPercent(Snd);
                AddPercent(Blt);
                AddPercent(Idle);
                break;
            }

            case 3:
            {
                strcat(sz, " ");
                AddTime(CPU);
                AddTime(Gfx);
                AddTime(Snd);
                AddTime(Blt);
                break;
            }
        }
    }

    return sz;
}
