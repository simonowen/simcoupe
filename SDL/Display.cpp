// Part of SimCoupe - A SAM Coupé emulator
//
// Display.cpp: SDL display rendering
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

// ToDo:
//  - change to handle multiple dirty regions

#include "SimCoupe.h"

#include "Display.h"
#include "Frame.h"
#include "Options.h"
#include "Profile.h"
#include "Video.h"
#include "UI.h"


// Writing to the display in DWORDs makes it endian sensitive, so we need to cover both cases
#if SDL_BYTEORDER == SDL_LIL_ENDIAN

inline DWORD PaletteDWORD (BYTE b1_, BYTE b2_, BYTE b3_, BYTE b4_)
    { return (aulPalette[b4_] << 24) | (aulPalette[b3_] << 16) | (aulPalette[b2_] << 8)  |  aulPalette[b1_]; }
inline DWORD PaletteDWORD (BYTE b1_, BYTE b2_)
    { return (aulPalette[b2_] << 16) | aulPalette[b1_]; }

#else

inline DWORD PaletteDWORD (BYTE b1_, BYTE b2_, BYTE b3_, BYTE b4_)
    { return (aulPalette[b1_] << 24) | (aulPalette[b2_] << 16) | (aulPalette[b3_] << 8)  |  aulPalette[b4_]; }
inline DWORD PaletteDWORD (BYTE b1_, BYTE b2_)
    { return (aulPalette[b1_] << 16) | aulPalette[b2_]; }

#endif


namespace Display
{
DWORD* pdwDirty;
bool* pafDirty;     // 808, 1108, 698, 596   (boot, MM title, MM start, MM walk left)

bool Init (bool fFirstInit_/*=false*/)
{
    Exit(true);

    pafDirty = new bool[Frame::GetScreen()->GetHeight()];

    return Video::Init(fFirstInit_);
}

void Exit (bool fReInit_/*=false*/)
{
    Video::Exit(fReInit_);

    if (pafDirty) { delete pafDirty; pafDirty = NULL; }
}

void SetDirty (int nLine_/*=-1*/)
{
    if (nLine_ != -1)
        pafDirty[nLine_] = true;
    else
    {
        // Mark all display lines dirty
        for (int i = 0, nHeight = Frame::GetScreen()->GetHeight() ; i < nHeight ; i++)
            pafDirty[i] = true;
    }
}


// Draw the changed lines in the appropriate colour depth and hi/low resolution
bool DrawChanges (CScreen* pScreen_, SDL_Surface* pSurface_)
{
    DWORD *pdwBack, *pdw;
    long lPitchDW;
    int nDepth;

    if (SDL_MUSTLOCK(pSurface_) && SDL_LockSurface(pSurface_) < 0)
    {
        TRACE("!!! SDL_LockSurface failed: %s\n", SDL_GetError());
        return false;
    }

    pdw = pdwBack = reinterpret_cast<DWORD*>(pSurface_->pixels);
    lPitchDW = pSurface_->pitch >> 1;
    nDepth = pSurface_->format->BitsPerPixel;

    ProfileStart(Gfx);

    int nBottom = pScreen_->GetHeight(), nRightHi = pScreen_->GetPitch() >> 3, nRightLo = nRightHi >> 1;


    bool *pfDirty = pafDirty;

    BYTE *pbSAM = pScreen_->GetLine(0), *pb = pbSAM;
    long lPitch = pScreen_->GetPitch();

    int nChangeFrom = -1, nChangeTo = -1;

    switch (nDepth)
    {
        case 8:
        {
            for (int y = 0 ; y < nBottom ; y++, pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, *pfDirty++ = false)
            {
                if (!*pfDirty)
                    continue;
                else if (nChangeFrom < 0)
                    nChangeFrom = y;
                nChangeTo = y;

                if (pScreen_->IsHiRes(y))
                {
                    for (int x = 0 ; x < nRightHi ; x++)
                    {
#if 0
                        *pdw++ = (aulPalette[pb[3]] << 24) | (aulPalette[pb[2]] << 16) |
                                 (aulPalette[pb[1]] << 8)  |  aulPalette[pb[0]];
                        *pdw++ = (aulPalette[pb[7]] << 24) | (aulPalette[pb[6]] << 16) |
                                 (aulPalette[pb[5]] << 8)  |  aulPalette[pb[4]];
#else
                        *pdw++ = PaletteDWORD(pb[0], pb[1], pb[2], pb[3]);
                        *pdw++ = PaletteDWORD(pb[4], pb[5], pb[6], pb[7]);
#endif
                        pb += 8;
                    }
                }
                else
                {
                    for (int x = 0 ; x < nRightLo ; x++)
                    {
#if 0
                        *pdw++ = ((aulPalette[pb[1]] << 16) | aulPalette[pb[0]]) * 0x101UL;
                        *pdw++ = ((aulPalette[pb[3]] << 16) | aulPalette[pb[2]]) * 0x101UL;
                        *pdw++ = ((aulPalette[pb[5]] << 16) | aulPalette[pb[4]]) * 0x101UL;
                        *pdw++ = ((aulPalette[pb[7]] << 16) | aulPalette[pb[6]]) * 0x101UL;
#else
                        *pdw++ = PaletteDWORD(pb[0], pb[0], pb[1], pb[1]);
                        *pdw++ = PaletteDWORD(pb[2], pb[2], pb[3], pb[3]);
                        *pdw++ = PaletteDWORD(pb[4], pb[4], pb[5], pb[5]);
                        *pdw++ = PaletteDWORD(pb[6], pb[6], pb[7], pb[7]);
#endif
                        pb += 8;
                    }
                }
            }
        }
        break;

        case 16:
        {
            for (int y = 0 ; y < nBottom ; y++, pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, *pfDirty++ = false)
            {
                if (!*pfDirty)
                    continue;
                else if (nChangeFrom < 0)
                    nChangeFrom = y;
                nChangeTo = y;

                if (pScreen_->IsHiRes(y))
                {
                    for (int x = 0 ; x < nRightHi ; x++)
                    {
#if 1
                        // Draw 8 pixels at a time
                        *pdw++ = (aulPalette[pb[1]] << 16) | aulPalette[pb[0]]; pb += 2;
                        *pdw++ = (aulPalette[pb[1]] << 16) | aulPalette[pb[0]]; pb += 2;
                        *pdw++ = (aulPalette[pb[1]] << 16) | aulPalette[pb[0]]; pb += 2;
                        *pdw++ = (aulPalette[pb[1]] << 16) | aulPalette[pb[0]]; pb += 2;
#else
                        *pdw++ = PaletteDWORD(pb[0], pb[1]);
                        *pdw++ = PaletteDWORD(pb[2], pb[3]);
                        *pdw++ = PaletteDWORD(pb[4], pb[5]);
                        *pdw++ = PaletteDWORD(pb[6], pb[7]);
                        pb += 8;
#endif
                    }
                }
                else
                {
                    for (int x = 0 ; x < nRightLo ; x++)
                    {
                        // Draw 8 pixels at a time
                        *pdw++ = aulPalette[*pb++] * 0x10001UL;
                        *pdw++ = aulPalette[*pb++] * 0x10001UL;
                        *pdw++ = aulPalette[*pb++] * 0x10001UL;
                        *pdw++ = aulPalette[*pb++] * 0x10001UL;
                        *pdw++ = aulPalette[*pb++] * 0x10001UL;
                        *pdw++ = aulPalette[*pb++] * 0x10001UL;
                        *pdw++ = aulPalette[*pb++] * 0x10001UL;
                        *pdw++ = aulPalette[*pb++] * 0x10001UL;
                    }
                }
            }
        }
        break;

        case 24:
        {
            for (int y = 0 ; y < nBottom ; y++, pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, *pfDirty++ = false)
            {
                if (!*pfDirty)
                    continue;
                else if (nChangeFrom < 0)
                    nChangeFrom = y;
                nChangeTo = y;

                if (pScreen_->IsHiRes(y))
                {
                    for (int x = 0 ; x < nRightHi ; x++)
                    {
                        BYTE *pb1 = (BYTE*)&aulPalette[*pb++], *pb2 = (BYTE*)&aulPalette[*pb++];
                        BYTE *pb3 = (BYTE*)&aulPalette[*pb++], *pb4 = (BYTE*)&aulPalette[*pb++];
                        *pdw++ = (((DWORD)pb2[0]) << 24) | (((DWORD)pb1[2]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[0];
                        *pdw++ = (((DWORD)pb3[1]) << 24) | (((DWORD)pb3[0]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[1];
                        *pdw++ = (((DWORD)pb4[2]) << 24) | (((DWORD)pb4[1]) << 16) | (((DWORD)pb4[0]) << 8) | pb3[2];

                        pb1 = (BYTE*)&aulPalette[*pb++], pb2 = (BYTE*)&aulPalette[*pb++];
                        pb3 = (BYTE*)&aulPalette[*pb++], pb4 = (BYTE*)&aulPalette[*pb++];
                        *pdw++ = (((DWORD)pb2[0]) << 24) | (((DWORD)pb1[2]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[0];
                        *pdw++ = (((DWORD)pb3[1]) << 24) | (((DWORD)pb3[0]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[1];
                        *pdw++ = (((DWORD)pb4[2]) << 24) | (((DWORD)pb4[1]) << 16) | (((DWORD)pb4[0]) << 8) | pb3[2];
                    }
                }
                else
                {
                    for (int x = 0 ; x < nRightLo ; x++)
                    {
                        BYTE *pb1 = (BYTE*)&aulPalette[*pb++], *pb2 = (BYTE*)&aulPalette[*pb++];
                        *pdw++ = (((DWORD)pb1[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                        *pdw++ = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[2]) << 16) | (((DWORD)pb1[0]) << 8) | pb1[1];
                        *pdw++ = (((DWORD)pb2[0]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[0];

                        pb1 = (BYTE*)&aulPalette[*pb++], pb2 = (BYTE*)&aulPalette[*pb++];
                        *pdw++ = (((DWORD)pb1[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                        *pdw++ = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[2]) << 16) | (((DWORD)pb1[0]) << 8) | pb1[1];
                        *pdw++ = (((DWORD)pb2[0]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[0];

                        pb1 = (BYTE*)&aulPalette[*pb++], pb2 = (BYTE*)&aulPalette[*pb++];
                        *pdw++ = (((DWORD)pb1[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                        *pdw++ = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[2]) << 16) | (((DWORD)pb1[0]) << 8) | pb1[1];
                        *pdw++ = (((DWORD)pb2[0]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[0];

                        pb1 = (BYTE*)&aulPalette[*pb++], pb2 = (BYTE*)&aulPalette[*pb++];
                        *pdw++ = (((DWORD)pb1[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                        *pdw++ = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[2]) << 16) | (((DWORD)pb1[0]) << 8) | pb1[1];
                        *pdw++ = (((DWORD)pb2[0]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[0];
                    }
                }
            }
        }
        break;

        case 32:
        {
            for (int y = 0 ; y < nBottom ; y++, pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, *pfDirty++ = false)
            {
                if (!*pfDirty)
                    continue;
                else if (nChangeFrom < 0)
                    nChangeFrom = y;
                nChangeTo = y;

                if (pScreen_->IsHiRes(y))
                {
                    for (int x = 0 ; x < nRightHi ; x++)
                    {
                        // Draw 8 pixels at a time
                        *pdw++ = aulPalette[*pb++];
                        *pdw++ = aulPalette[*pb++];
                        *pdw++ = aulPalette[*pb++];
                        *pdw++ = aulPalette[*pb++];
                        *pdw++ = aulPalette[*pb++];
                        *pdw++ = aulPalette[*pb++];
                        *pdw++ = aulPalette[*pb++];
                        *pdw++ = aulPalette[*pb++];
                    }
                }
                else
                {
                    for (int x = 0 ; x < nRightLo ; x++)
                    {
                        // Draw 8 pixels at a time
                        pdw[0] = pdw[1] = aulPalette[*pb++];
                        pdw[2] = pdw[3] = aulPalette[*pb++];
                        pdw[4] = pdw[5] = aulPalette[*pb++];
                        pdw[6] = pdw[7] = aulPalette[*pb++];
                        pdw[8] = pdw[9] = aulPalette[*pb++];
                        pdw[10] = pdw[11] = aulPalette[*pb++];
                        pdw[12] = pdw[13] = aulPalette[*pb++];
                        pdw[14] = pdw[15] = aulPalette[*pb++];
                        pdw += 16;
                    }
                }
            }
        }
        break;
    }

    if (pSurface_ && SDL_MUSTLOCK(pSurface_))
        SDL_UnlockSurface(pSurface_);

    ProfileEnd();


    ProfileStart(Blt);

    if (nChangeFrom >= 0)
    {
        SDL_Rect rect = { 0, nChangeFrom << 1, nRightHi << 3, ((nChangeTo - nChangeFrom) << 1) + 1 };
        SDL_Rect rectFront = { (pFront->w - rect.w) >> 1, rect.y + ((pFront->h - (nBottom << 1)) >> 1), rect.w, rect.h };

        SDL_BlitSurface(pBack, &rect, pFront, &rectFront);
        SDL_UpdateRects(pFront, 1, &rectFront);
    }

    ProfileEnd();

    // Success
    return true;
}


// Update the display to show anything that's changed since last time
void Update (CScreen* pScreen_)
{
    // Don't draw if fullscreen but not active
    if (GetOption(fullscreen) && !g_fActive)
        return;

    // Draw any changed lines
    if (!DrawChanges(pScreen_, pBack))
    {
        TRACE("Display::Update(): DrawChanges() failed\n");
        return;
    }
}

};
