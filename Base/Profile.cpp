// Part of SimCoupe - A SAM Coupé emulator
//
// Profile.cpp: Emulator profiling for on-screen stats
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

namespace Profile
{
PROFILE g_sProfile;
PROFILE_T profTotal, profLast, *approfStack[10];
UINT uStackPos = 0;


void Reset ()
{
    profTotal = 0;
    memset(&g_sProfile, 0, sizeof g_sProfile);
    memset(&approfStack, 0, sizeof approfStack);
    uStackPos = 0;

    profLast = OSD::GetProfileTime();
}

void ProfileUpdate ()
{
    // Work out how much time has passed since the last check
    PROFILE_T profNow = OSD::GetProfileTime(), profElapsed = profNow - profLast;

    // If there was an item set, update it with the elapsed time
    if (approfStack[uStackPos])
    {
        *approfStack[uStackPos] += profElapsed;
        profTotal += profElapsed;
    }

    // Update the 'last' time for next time
    profLast = profNow;
}

void ProfileStart_(PROFILE_T* pprofNew_)
{
    if (GetOption(profile))
    {
        ProfileUpdate();

        // Remember the new item to time and the start time
        if (uStackPos < (sizeof approfStack / sizeof approfStack[0]))
            approfStack[++uStackPos] = pprofNew_;
#ifdef _DEBUG
        else
            UI::ShowMessage(msgFatal, "Profile stack overflow!\n");
#endif
    }
}

void ProfileEnd ()
{
    if (GetOption(profile))
    {
        ProfileUpdate();

        if (uStackPos)
            uStackPos--;
#ifdef _DEBUG
        else
            UI::ShowMessage(msgFatal, "Profile stack underflow!\n");
#endif
    }
}


#define AddPercent(x)   sprintf(sz + strlen(sz), "  %s:%lu%%", #x, ((g_sProfile.prof##x + profTotal/200UL) * 100UL) / profTotal)

const char* GetStats ()
{
    static char sz[64];
    sz[0] = '\0';

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

};  // namespace Profile
