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
//  Uses OSD::GetTime(), which must return millisecond accurate time value
//  for the module to work properly.

#include "SimCoupe.h"
#include "Profile.h"

#include "Options.h"
#include "OSD.h"
#include "UI.h"

////////////////////////////////////////////////////////////////////////////////

namespace Profile
{
PROFILE g_sProfile;
DWORD dwTotal, dwLast, *apdwStack[10];
UINT uStackPos = 0;


void Reset ()
{
    dwTotal = 0;
    memset(&g_sProfile, 0, sizeof g_sProfile);
    memset(&apdwStack, 0, sizeof apdwStack);
    uStackPos = 0;

    dwLast = OSD::GetTime();
}

void ProfileUpdate ()
{
    // Work out how much time has passed since the last check
    DWORD dwNow = OSD::GetTime(), dwElapsed = dwNow - dwLast;

    // If there was an item set, update it with the elapsed time
    if (apdwStack[uStackPos])
    {
        *apdwStack[uStackPos] += dwElapsed;
        dwTotal += dwElapsed;
    }

    // Update the 'last' time for next time
    dwLast = dwNow;
}

void ProfileStart_(DWORD* pdwNew_)
{
    if (GetOption(profile))
    {
        ProfileUpdate();

        // Remember the new item to time and the start time
        if (uStackPos < (sizeof apdwStack / sizeof apdwStack[0]))
            apdwStack[++uStackPos] = pdwNew_;
        else
            UI::ShowMessage(msgFatal, "Profile stack overflow!\n");
    }
}

void ProfileEnd ()
{
    if (GetOption(profile))
    {
        ProfileUpdate();

        if (uStackPos)
            uStackPos--;
        else
            UI::ShowMessage(msgFatal, "Profile stack underflow!\n");
    }
}


#define AddPercent(x)   sprintf(sz + strlen(sz), " %s:%lu%%", #x, ((g_sProfile.dw##x + dwTotal/200UL) * 100UL) / dwTotal)
#define AddTime(x)      sprintf(sz + strlen(sz), " %s:%lums", #x, g_sProfile.dw##x)

const char* GetStats ()
{
    static char sz[64];
    sz[0] = '\0';

    if (dwTotal)
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
                AddTime(Idle);
                break;
            }
        }
    }

    return sz;
}

};  // namespace Profile
