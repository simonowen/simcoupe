// Part of SimCoupe - A SAM Coupe emulator
//
// Display.cpp: WinCE display rendering
//
//  Copyright (c) 1999-2003  Simon Owen
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

#include "Display.h"
#include "Frame.h"
#include "Input.h"
#include "Options.h"
#include "Profile.h"
#include "Video.h"
#include "UI.h"

bool* Display::pafDirty;


bool Display::Init (bool fFirstInit_/*=false*/)
{
    Exit(true);

    pafDirty = new bool[Frame::GetHeight()];
    SetDirty();

    return Video::Init(fFirstInit_);
}

void Display::Exit (bool fReInit_/*=false*/)
{
    Video::Exit(fReInit_);

    delete[] pafDirty; pafDirty = NULL;
}


void Display::SetDirty ()
{
    // Mark all display lines dirty
    for (int i = 0, nHeight = Frame::GetHeight() ; pafDirty && i < nHeight ; i++)
        pafDirty[i] = true;
}


// Update the display to show anything that's changed since last time
void Display::Update (CScreen* pScreen_)
{
    if (!g_fActive)
        return;

    // Bypass the iPAQ 3800 internal buffer, going straight for the display
    BYTE* pbLine = g_f3800 ? reinterpret_cast<BYTE*>(0xac0755a0)
                           : reinterpret_cast<BYTE*>(GXBeginDraw());
    if (!pbLine)
        return;

    // Since there's no blit stage, profile native screen drawing instead
    ProfileStart(Blt);

    bool* pfHiRes = pScreen_->GetHiRes();

    // Decide whether odd/even/interlaced pixels should be displayed
    static bool fEven;
    fEven = !GetOption(mode3) ? 0 : (GetOption(mode3) == 1) ? 1 : !fEven;

    // Landscape mode?
    if (GetOption(fullscreen))
    {
        int nWidth = min(g_gxdp.cyHeight, static_cast<DWORD>(pScreen_->GetPitch() >> 1));
        int nHeight = min(g_gxdp.cxWidth, static_cast<DWORD>(pScreen_->GetHeight()));
        int nXpitch = g_gxdp.cbxPitch, nYpitch = -g_gxdp.cbyPitch;

        // Force landscape rotated left for now
        if (1)
            pbLine += g_gxdp.cbyPitch * (g_gxdp.cyHeight - 1);
        else
        {
            // Landscape, rotated right
            pbLine += g_gxdp.cbxPitch * (g_gxdp.cxWidth - 1);
            nXpitch = -nXpitch;
            nYpitch = -nYpitch;
        }

        // Centre the view on the display
        int nXoffset = (g_gxdp.cxWidth - nHeight) >> 1;
        int nYoffset = (g_gxdp.cyHeight - nWidth) >> 1;
        pbLine += (nXoffset * nXpitch) + (nYoffset * nYpitch);

        switch (g_gxdp.cBPP)
        {
            // Only 16-bit supported, for now
            case 16:
            {
                // Convert the pitch from BYTEs to WORDs
                nYpitch >>= 1;

                for (int y = 0 ; y < nHeight ; y++, pbLine += nXpitch)
                {
                    if (!pafDirty[y])
                        continue;

                    bool fScreenLine = IsScreenLine(s_nViewTop+y);
                    BYTE *pbSAM = pScreen_->GetLine(y);
                    WORD* pwDest = reinterpret_cast<WORD*>(pbLine);

                    // Hi-res / mode 3 ?
                    if (pfHiRes[y])
                    {
                        if (fEven && fScreenLine)
                        {
                            // Draw the even pixels
                            for (int x = 0 ; x < nWidth ; x += 8)
                            {
                                *pwDest = aulPalette[pbSAM[0]];  pwDest += nYpitch;
                                *pwDest = aulPalette[pbSAM[2]];  pwDest += nYpitch;
                                *pwDest = aulPalette[pbSAM[4]];  pwDest += nYpitch;
                                *pwDest = aulPalette[pbSAM[6]];  pwDest += nYpitch;
                                *pwDest = aulPalette[pbSAM[8]];  pwDest += nYpitch;
                                *pwDest = aulPalette[pbSAM[10]]; pwDest += nYpitch;
                                *pwDest = aulPalette[pbSAM[12]]; pwDest += nYpitch;
                                *pwDest = aulPalette[pbSAM[14]]; pwDest += nYpitch;

                                pbSAM += 16;
                            }
                        }
                        else
                        {
                            // Draw the odd pixels
                            for (int x = 0 ; x < nWidth ; x += 8)
                            {
                                *pwDest = aulPalette[pbSAM[1]];  pwDest += nYpitch;
                                *pwDest = aulPalette[pbSAM[3]];  pwDest += nYpitch;
                                *pwDest = aulPalette[pbSAM[5]];  pwDest += nYpitch;
                                *pwDest = aulPalette[pbSAM[7]];  pwDest += nYpitch;
                                *pwDest = aulPalette[pbSAM[9]];  pwDest += nYpitch;
                                *pwDest = aulPalette[pbSAM[11]]; pwDest += nYpitch;
                                *pwDest = aulPalette[pbSAM[13]]; pwDest += nYpitch;
                                *pwDest = aulPalette[pbSAM[15]]; pwDest += nYpitch;

                                pbSAM += 16;
                            }
                        }
                    }
                    else
                    {
                        // Normal resolution
                        for (int x = 0 ; x < nWidth ; x += 8)
                        {
                            *pwDest = aulPalette[pbSAM[0]]; pwDest += nYpitch;
                            *pwDest = aulPalette[pbSAM[1]]; pwDest += nYpitch;
                            *pwDest = aulPalette[pbSAM[2]]; pwDest += nYpitch;
                            *pwDest = aulPalette[pbSAM[3]]; pwDest += nYpitch;
                            *pwDest = aulPalette[pbSAM[4]]; pwDest += nYpitch;
                            *pwDest = aulPalette[pbSAM[5]]; pwDest += nYpitch;
                            *pwDest = aulPalette[pbSAM[6]]; pwDest += nYpitch;
                            *pwDest = aulPalette[pbSAM[7]]; pwDest += nYpitch;

                            pbSAM += 8;
                        }
                    }

                    // Keep hi-res lines dirty in interlaced mode, so they're redrawn
                    pafDirty[y] = pfHiRes[y] && fScreenLine && GetOption(mode3) == 2;
                }
                break;
            }
        }
    }
    else    // Portrait mode
    {
        int nWidth = min(g_gxdp.cxWidth, static_cast<DWORD>(pScreen_->GetPitch() >> 1));
        int nHeight = min(g_gxdp.cyHeight-SIP_HEIGHT, static_cast<DWORD>(pScreen_->GetHeight()));
        int nXpitch = g_gxdp.cbxPitch, nYpitch = g_gxdp.cbyPitch;

        switch (g_gxdp.cBPP)
        {
            // Only 16-bit supported, for now
            case 16:
            {
                nWidth = SCREEN_PIXELS;     // we skip 1 in 16 pixels to make this fit
                nXpitch >>= 1;

                for (int y = 0 ; y < nHeight ; y++, pbLine += nYpitch)
                {
                    if (!pafDirty[y])
                        continue;

                    bool fScreenLine = IsScreenLine(s_nViewTop+y);
                    BYTE *pbSAM = pScreen_->GetLine(y);
                    WORD* pwDest = reinterpret_cast<WORD*>(pbLine);

                    // Hi-res / mode 3 ?
                    if (pfHiRes[y])
                    {
                        if (fEven && fScreenLine)
                        {
                            // Draw even pixels
                            for (int x = 0; x < nWidth; x += 16)
                            {
                                *pwDest = aulPalette[pbSAM[0]];  pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[2]];  pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[4]];  pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[6]];  pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[8]];  pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[10]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[12]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[14]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[16]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[18]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[20]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[22]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[24]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[26]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[28]]; pwDest += nXpitch;

                                pbSAM += 32;
                            }
                        }
                        else
                        {
                            // Draw odd pixels
                            for (int x = 0; x < nWidth; x += 16)
                            {
                                *pwDest = aulPalette[pbSAM[1]];  pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[3]];  pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[5]];  pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[7]];  pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[9]];  pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[11]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[13]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[15]];  pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[17]];  pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[19]];  pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[21]];  pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[23]];  pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[25]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[27]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[29]]; pwDest += nXpitch;

                                pbSAM += 32;
                            }
                        }
                    }
                    else
                    {
                        // Check for special case pitch, which should be a little quicker
                        if (nXpitch == 2)
                        {
                            DWORD* pdwDest = reinterpret_cast<DWORD*>(pwDest);

                            // Draw in DWORDs - perhaps more efficient?
                            for (int x = 0; x < nWidth; x += 32)
                            {
                                pdwDest[0] = (aulPalette[pbSAM[1]] << 16) | aulPalette[pbSAM[0]];
                                pdwDest[1] = (aulPalette[pbSAM[3]] << 16) | aulPalette[pbSAM[2]];
                                pdwDest[2] = (aulPalette[pbSAM[5]] << 16) | aulPalette[pbSAM[4]];
                                pdwDest[3] = (aulPalette[pbSAM[7]] << 16) | aulPalette[pbSAM[6]];
                                pdwDest[4] = (aulPalette[pbSAM[9]] << 16) | aulPalette[pbSAM[8]];
                                pdwDest[5] = (aulPalette[pbSAM[11]] << 16) | aulPalette[pbSAM[10]];
                                pdwDest[6] = (aulPalette[pbSAM[13]] << 16) | aulPalette[pbSAM[12]];
                                pdwDest[7] = (aulPalette[pbSAM[16]] << 16) | aulPalette[pbSAM[14]];
                                pdwDest[8] = (aulPalette[pbSAM[18]] << 16) | aulPalette[pbSAM[17]];
                                pdwDest[9] = (aulPalette[pbSAM[20]] << 16) | aulPalette[pbSAM[19]];
                                pdwDest[10] = (aulPalette[pbSAM[22]] << 16) | aulPalette[pbSAM[21]];
                                pdwDest[11] = (aulPalette[pbSAM[24]] << 16) | aulPalette[pbSAM[23]];
                                pdwDest[12] = (aulPalette[pbSAM[26]] << 16) | aulPalette[pbSAM[25]];
                                pdwDest[13] = (aulPalette[pbSAM[28]] << 16) | aulPalette[pbSAM[27]];
                                pdwDest[14] = (aulPalette[pbSAM[30]] << 16) | aulPalette[pbSAM[29]];

                                pdwDest += 15;
                                pbSAM += 32;
                            }
                        }
                        else
                        {
                            // Normal resolution
                            for (int x = 0; x < nWidth; x += 16)
                            {
                                *pwDest = aulPalette[pbSAM[0]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[1]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[2]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[3]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[4]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[5]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[6]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[7]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[8]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[9]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[10]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[11]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[12]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[13]]; pwDest += nXpitch;
                                *pwDest = aulPalette[pbSAM[14]]; pwDest += nXpitch;

                                pbSAM += 16;
                            }
                        }
                    }

                    // Keep hi-res lines dirty in interlaced mode, so they're redrawn
                    pafDirty[y] = pfHiRes[y] && fScreenLine && GetOption(mode3) == 2;
                }
                break;
            }
        }
    }

    // Skip the iPAQ 3800 back-buffer copying, which we don't use/need
    if (!g_f3800)
        GXEndDraw();

    ProfileEnd();
}
