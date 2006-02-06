// Part of SimCoupe - A SAM Coupe emulator
//
// Profile.h: Emulator profiling for on-screen stats
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

#ifndef PROFILE_H
#define PROFILE_H

#ifndef PROFILE_T
#define PROFILE_T       DWORD
#endif

#ifndef AddTime
#define AddTime(x)      sprintf(sz + strlen(sz), " %s:%ums", #x, s_sProfile.prof##x)
#endif

// The member names in this structure must be prof<something>
typedef struct
{
    PROFILE_T profCPU;
    PROFILE_T profGfx;
    PROFILE_T profSnd;
    PROFILE_T profBlt;
    PROFILE_T profIdle;
}
PROFILE;


// Macros to reference values in the structure above
#define ProfileStart(type)  do { PROFILE_T profStart = OSD::GetProfileTime(), *pprofUpdate = &Profile::s_sProfile.prof##type
#define ProfileEnd()        *pprofUpdate += OSD::GetProfileTime()-profStart; } while (0)

class Profile
{
    public:
        static void Reset () { memset(&s_sProfile, 0, sizeof(s_sProfile)); }
        static const char* GetStats ();

        static PROFILE s_sProfile;
};

#endif  // PROFILE_H
