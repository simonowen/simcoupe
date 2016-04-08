// Part of SimCoupe - A SAM Coupe emulator
//
// Keyin.cpp: Automatic keyboard input
//
//  Copyright (c) 2012 Simon Owen
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

#include "SimCoupe.h"
#include "Keyin.h"
#include "Memory.h"

namespace Keyin
{

static BYTE *pbInput;
static int nPos = -1;
static bool fMapChars = true;

BYTE MapChar (BYTE b_);


void String (const char *pcsz_, bool fMapChars_)
{
    // Clean up any existing input
    Stop();

    // Determine the source length and allocate a buffer for it
    size_t nLen = strlen(pcsz_);
    pbInput = new BYTE[nLen+1];

    // Copy the string, appending a null terminator just in case
    memcpy(pbInput, pcsz_, nLen);
    pbInput[nLen] = '\0';

    // Copy character mapping flag and set starting offset
    fMapChars = fMapChars_;
    nPos = 0;
}

void Stop ()
{
    // Clean up
    delete[] pbInput, pbInput = nullptr;

    // Normal speed
    g_nTurbo &= ~TURBO_KEYIN;
}

bool CanType ()
{
    // For safety, ensure ROM0 and the system variable page are present
    return GetSectionPage(SECTION_A) == ROM0 && GetSectionPage(SECTION_B) == 0;
}

bool IsTyping ()
{
    // Do we have a string?
    return pbInput != nullptr;
}

bool Next ()
{
    char bKey = 0;

    // Return if we're not typing or the previous key hasn't been consumed
    if (!IsTyping() || (PageReadPtr(0)[0x5c3b-0x4000] & 0x20))
        return false;

    // Read the next key
    bKey = pbInput[nPos++];

    // Stop at the first null character
    if (!bKey)
    {
        Stop();
        return false;
    }

    // Are we to perform character mapping? (disable to allow keyword codes)
    if (fMapChars)
    {
        // Map the character to a SAM key code, if required
        bKey = MapChar(bKey);

        // Ignore characters without a mapping
        if (!bKey)
            return false;
    }

    // Simulate the key press
    PageWritePtr(0)[0x5c08-0x4000] = bKey;  // set key in LASTK
    PageWritePtr(0)[0x5c3b-0x4000] |= 0x20; // signal key available in FLAGS

    // Run at turbo speed during input
    g_nTurbo |= TURBO_KEYIN;

    // Key simulated
    return true;
}


// Map special case input characters to the SAM key code equivalent
BYTE MapChar (BYTE b_)
{
    static BYTE abMap[256];

    // Does the map need initialising?
    if (!abMap['A'])
    {
        int i;

        // Normal 7-bit ASCII range, excluding control characters
        // Note: SAM uses 0x60 for GBP and 0x7f for (c)
        for (i = ' ' ; i <= 0x7f ; i++)
            abMap[i] = i;

        // Preserve certain control characters
        abMap['\t'] = '\t';     // horizontal tab
        abMap['\n'] = '\r';     // convert LF to CR
    }

    // Return the mapped character, or 0 if none
    return abMap[b_];
}

} // namespace Keyin
