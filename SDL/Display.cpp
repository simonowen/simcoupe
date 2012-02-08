// Part of SimCoupe - A SAM Coupe emulator
//
// Display.cpp: SDL display rendering
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

// ToDo:
//  - change to handle multiple dirty regions

#include "SimCoupe.h"

#include "Display.h"
#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "Video.h"
#include "UI.h"


bool* Display::pafDirty;
SDL_Rect rSource, rTarget;

////////////////////////////////////////////////////////////////////////////////

// Writing to the display in DWORDs makes it endian sensitive, so we need to cover both cases
#if SDL_BYTEORDER == SDL_LIL_ENDIAN

inline DWORD PaletteDWORD (BYTE b1_, BYTE b2_, BYTE b3_, BYTE b4_, DWORD* pulPalette_)
    { return (pulPalette_[b4_] << 24) | (pulPalette_[b3_] << 16) | (pulPalette_[b2_] << 8) | pulPalette_[b1_]; }
inline DWORD PaletteDWORD (BYTE b1_, BYTE b2_, DWORD* pulPalette_)
    { return (pulPalette_[b2_] << 16) | pulPalette_[b1_]; }
inline DWORD MakeDWORD (BYTE b1_, BYTE b2_, BYTE b3_, BYTE b4_)
    { return (b4_ << 24) | (b3_ << 16) | (b2_ << 8) | b1_; }
inline DWORD MakeDWORD (BYTE b1_, BYTE b2_)
    { return ((b2_ << 16) | b1_) * 0x0101UL; }
inline DWORD JoinWORDs (DWORD w1_, DWORD w2_)
    { return ((w2_ << 16) | w1_); }
#else

inline DWORD PaletteDWORD (BYTE b1_, BYTE b2_, BYTE b3_, BYTE b4_, DWORD* pulPalette_)
    { return (pulPalette_[b1_] << 24) | (pulPalette_[b2_] << 16) | (pulPalette_[b3_] << 8) | pulPalette_[b4_]; }
inline DWORD PaletteDWORD (BYTE b1_, BYTE b2_, DWORD* pulPalette_)
    { return (pulPalette_[b1_] << 16) | pulPalette_[b2_]; }
inline DWORD MakeDWORD (BYTE b1_, BYTE b2_, BYTE b3_, BYTE b4_)
    { return (b1_ << 24) | (b2_ << 16) | (b3_ << 8) | b4_; }
inline DWORD MakeDWORD (BYTE b1_, BYTE b2_)
    { return ((b1_ << 16) | b2_) * 0x0101UL; }
inline DWORD JoinWORDs (DWORD w1_, DWORD w2_)
    { return ((w1_ << 16) | w2_); }
#endif

////////////////////////////////////////////////////////////////////////////////

bool Display::Init (bool fFirstInit_/*=false*/)
{
    Exit(true);

    pafDirty = new bool[Frame::GetHeight()];

    // These will be updated to the appropriate values on the first draw
    rSource.w = rTarget.w = Frame::GetWidth();
    rSource.h = rTarget.h = Frame::GetHeight() << 1;

    return Video::Init(fFirstInit_);
}

void Display::Exit (bool fReInit_/*=false*/)
{
    Video::Exit(fReInit_);

    if (pafDirty) { delete[] pafDirty; pafDirty = NULL; }
}

void Display::SetDirty ()
{
    // Mark all display lines dirty
    for (int i = 0, nHeight = Frame::GetHeight() ; i < nHeight ; i++)
        pafDirty[i] = true;
}


// Draw the changed lines in the appropriate colour depth and hi/low resolution
static bool DrawChanges (CScreen* pScreen_, SDL_Surface* pSurface_)
{
    // Lock the surface for direct access below
    if (SDL_MUSTLOCK(pSurface_) && SDL_LockSurface(pSurface_) < 0)
    {
        TRACE("!!! SDL_LockSurface failed: %s\n", SDL_GetError());
        return false;
    }

    bool fInterlace = !GUI::IsActive();

    DWORD *pdwBack = reinterpret_cast<DWORD*>(pSurface_->pixels), *pdw = pdwBack;
    long lPitchDW = pSurface_->pitch >> (fInterlace ? 1 : 2);
    bool *pfDirty = Display::pafDirty, *pfHiRes = pScreen_->GetHiRes();

    BYTE *pbSAM = pScreen_->GetLine(0), *pb = pbSAM;
    long lPitch = pScreen_->GetPitch();

    int nShift = fInterlace ? 1 : 0;
    int nDepth = pSurface_->format->BitsPerPixel;
    int nBottom = pScreen_->GetHeight() >> nShift;
    int nWidth = pScreen_->GetPitch(), nRightHi = nWidth >> 3, nRightLo = nRightHi >> 1;

    // What colour depth is the target surface?
    switch (nDepth)
    {
        case 8:
        {
            for (int y = 0 ; y < nBottom ; pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, y++)
            {
                if (!pfDirty[y])
                    continue;

                if (pfHiRes[y])
                {
                    for (int x = 0 ; x < nRightHi ; x++)
                    {
                        pdw[0] = MakeDWORD(pb[0], pb[1], pb[2], pb[3]);
                        pdw[1] = MakeDWORD(pb[4], pb[5], pb[6], pb[7]);

                        pdw += 2;
                        pb += 8;
                    }

                    if (fInterlace)
                    {
                        pb = pbSAM;
                        pdw = pdwBack + lPitchDW/2;

                        if (GetOption(scanlines))
                            memset(pdw, 0x00, nWidth);
                        else
                        {
                            for (int x = 0 ; x < nRightHi ; x++)
                            {
                                pdw[0] = MakeDWORD(pb[0], pb[1], pb[2], pb[3]);
                                pdw[1] = MakeDWORD(pb[4], pb[5], pb[6], pb[7]);

                                pdw += 2;
                                pb += 8;
                            }
                        }
                    }
                }
                else
                {
                    for (int x = 0 ; x < nRightLo ; x++)
                    {
                        pdw[0] = MakeDWORD(pb[0], pb[1]);
                        pdw[1] = MakeDWORD(pb[2], pb[3]);
                        pdw[2] = MakeDWORD(pb[4], pb[5]);
                        pdw[3] = MakeDWORD(pb[6], pb[7]);

                        pdw += 4;
                        pb += 8;
                    }

                    if (fInterlace)
                    {
                        pb = pbSAM;
                        pdw = pdwBack + lPitchDW/2;

                        if (GetOption(scanlines))
                            memset(pdw, 0x00, nWidth);
                        else
                        {
                            for (int x = 0 ; x < nRightLo ; x++)
                            {
                                pdw[0] = MakeDWORD(pb[0], pb[1]);
                                pdw[1] = MakeDWORD(pb[2], pb[3]);
                                pdw[2] = MakeDWORD(pb[4], pb[5]);
                                pdw[3] = MakeDWORD(pb[6], pb[7]);

                                pdw += 4;
                                pb += 8;
                            }
                        }
                    }
                }
            }
        }
        break;

        case 16:
        {
            nWidth <<= 1;

            for (int y = 0 ; y < nBottom ; pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, y++)
            {
                if (!pfDirty[y])
                    continue;

                if (pfHiRes[y])
                {
                    for (int x = 0 ; x < nRightHi ; x++)
                    {
                        pdw[0] = PaletteDWORD(pb[0], pb[1], aulPalette);
                        pdw[1] = PaletteDWORD(pb[2], pb[3], aulPalette);
                        pdw[2] = PaletteDWORD(pb[4], pb[5], aulPalette);
                        pdw[3] = PaletteDWORD(pb[6], pb[7], aulPalette);

                        pdw += 4;
                        pb += 8;
                    }

                    if (fInterlace)
                    {
                        pb = pbSAM;
                        pdw = pdwBack + lPitchDW/2;

                        if (!GetOption(scanlevel))
                            memset(pdw, 0x00, nWidth);
                        else
                        {
                            for (int x = 0 ; x < nRightHi ; x++)
                            {
                                pdw[0] = PaletteDWORD(pb[0], pb[1], aulScanline);
                                pdw[1] = PaletteDWORD(pb[2], pb[3], aulScanline);
                                pdw[2] = PaletteDWORD(pb[4], pb[5], aulScanline);
                                pdw[3] = PaletteDWORD(pb[6], pb[7], aulScanline);

                                pdw += 4;
                                pb += 8;
                            }
                        }
                    }
                }
                else
                {
                    for (int x = 0 ; x < nRightLo ; x++)
                    {
                        pdw[0] = aulPalette[pb[0]] * 0x10001UL;
                        pdw[1] = aulPalette[pb[1]] * 0x10001UL;
                        pdw[2] = aulPalette[pb[2]] * 0x10001UL;
                        pdw[3] = aulPalette[pb[3]] * 0x10001UL;
                        pdw[4] = aulPalette[pb[4]] * 0x10001UL;
                        pdw[5] = aulPalette[pb[5]] * 0x10001UL;
                        pdw[6] = aulPalette[pb[6]] * 0x10001UL;
                        pdw[7] = aulPalette[pb[7]] * 0x10001UL;

                        pdw += 8;
                        pb += 8;
                    }

                    if (fInterlace)
                    {
                        pb = pbSAM;
                        pdw = pdwBack + lPitchDW/2;

                        if (!GetOption(scanlevel))
                            memset(pdw, 0x00, nWidth);
                        else
                        {
                            for (int x = 0 ; x < nRightLo ; x++)
                            {
                                pdw[0] = aulScanline[pb[0]] * 0x10001UL;
                                pdw[1] = aulScanline[pb[1]] * 0x10001UL;
                                pdw[2] = aulScanline[pb[2]] * 0x10001UL;
                                pdw[3] = aulScanline[pb[3]] * 0x10001UL;
                                pdw[4] = aulScanline[pb[4]] * 0x10001UL;
                                pdw[5] = aulScanline[pb[5]] * 0x10001UL;
                                pdw[6] = aulScanline[pb[6]] * 0x10001UL;
                                pdw[7] = aulScanline[pb[7]] * 0x10001UL;

                                pdw += 8;
                                pb += 8;
                            }
                        }
                    }
                }
            }
        }
        break;

        case 24:
        {
            nWidth *= 3;

            for (int y = 0 ; y < nBottom ; pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, y++)
            {
                if (!pfDirty[y])
                    continue;

                if (pScreen_->IsHiRes(y))
                {
                    for (int x = 0 ; x < nRightHi ; x++)
                    {
                        BYTE *pb1 = (BYTE*)&aulPalette[pb[0]], *pb2 = (BYTE*)&aulPalette[pb[1]];
                        BYTE *pb3 = (BYTE*)&aulPalette[pb[2]], *pb4 = (BYTE*)&aulPalette[pb[3]];
                        pdw[0] = (((DWORD)pb2[0]) << 24) | (((DWORD)pb1[2]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[0];
                        pdw[1] = (((DWORD)pb3[1]) << 24) | (((DWORD)pb3[0]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[1];
                        pdw[2] = (((DWORD)pb4[2]) << 24) | (((DWORD)pb4[1]) << 16) | (((DWORD)pb4[0]) << 8) | pb3[2];

                        pb1 = (BYTE*)&aulPalette[pb[4]], pb2 = (BYTE*)&aulPalette[pb[5]];
                        pb3 = (BYTE*)&aulPalette[pb[6]], pb4 = (BYTE*)&aulPalette[pb[7]];
                        pdw[3] = (((DWORD)pb2[0]) << 24) | (((DWORD)pb1[2]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[0];
                        pdw[4] = (((DWORD)pb3[1]) << 24) | (((DWORD)pb3[0]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[1];
                        pdw[5] = (((DWORD)pb4[2]) << 24) | (((DWORD)pb4[1]) << 16) | (((DWORD)pb4[0]) << 8) | pb3[2];

                        pdw += 6;
                        pb += 8;
                    }

                    if (fInterlace)
                    {
                        pb = pbSAM;
                        pdw = pdwBack + lPitchDW/2;

                        if (!GetOption(scanlevel))
                            memset(pdw, 0x00, nWidth);
                        else
                        {
                            for (int x = 0 ; x < nRightHi ; x++)
                            {
                                BYTE *pb1 = (BYTE*)&aulScanline[pb[0]], *pb2 = (BYTE*)&aulScanline[pb[1]];
                                BYTE *pb3 = (BYTE*)&aulScanline[pb[2]], *pb4 = (BYTE*)&aulScanline[pb[3]];
                                pdw[0] = (((DWORD)pb2[0]) << 24) | (((DWORD)pb1[2]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[0];
                                pdw[1] = (((DWORD)pb3[1]) << 24) | (((DWORD)pb3[0]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[1];
                                pdw[2] = (((DWORD)pb4[2]) << 24) | (((DWORD)pb4[1]) << 16) | (((DWORD)pb4[0]) << 8) | pb3[2];

                                pb1 = (BYTE*)&aulScanline[pb[4]], pb2 = (BYTE*)&aulScanline[pb[5]];
                                pb3 = (BYTE*)&aulScanline[pb[6]], pb4 = (BYTE*)&aulScanline[pb[7]];
                                pdw[3] = (((DWORD)pb2[0]) << 24) | (((DWORD)pb1[2]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[0];
                                pdw[4] = (((DWORD)pb3[1]) << 24) | (((DWORD)pb3[0]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[1];
                                pdw[5] = (((DWORD)pb4[2]) << 24) | (((DWORD)pb4[1]) << 16) | (((DWORD)pb4[0]) << 8) | pb3[2];

                                pdw += 6;
                                pb += 8;
                            }
                        }
                    }
                }
                else
                {
                    for (int x = 0 ; x < nRightLo ; x++)
                    {
                        BYTE *pb1 = (BYTE*)&aulPalette[pb[0]], *pb2 = (BYTE*)&aulPalette[pb[1]];
                        pdw[0]  = (((DWORD)pb1[0]) << 24) | (((DWORD)pb1[2]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[0];
                        pdw[1]  = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[0]) << 16) | (((DWORD)pb1[2]) << 8) | pb1[1];
                        pdw[2]  = (((DWORD)pb2[2]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[0]) << 8) | pb2[2];

                        pb1 = (BYTE*)&aulPalette[pb[2]], pb2 = (BYTE*)&aulPalette[pb[3]];
                        pdw[3]  = (((DWORD)pb1[0]) << 24) | (((DWORD)pb1[2]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[0];
                        pdw[4]  = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[0]) << 16) | (((DWORD)pb1[2]) << 8) | pb1[1];
                        pdw[5]  = (((DWORD)pb2[2]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[0]) << 8) | pb2[2];

                        pb1 = (BYTE*)&aulPalette[pb[4]], pb2 = (BYTE*)&aulPalette[pb[5]];
                        pdw[6]  = (((DWORD)pb1[0]) << 24) | (((DWORD)pb1[2]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[0];
                        pdw[7]  = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[0]) << 16) | (((DWORD)pb1[2]) << 8) | pb1[1];
                        pdw[8]  = (((DWORD)pb2[2]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[0]) << 8) | pb2[2];

                        pb1 = (BYTE*)&aulPalette[pb[6]], pb2 = (BYTE*)&aulPalette[pb[7]];
                        pdw[9]  = (((DWORD)pb1[0]) << 24) | (((DWORD)pb1[2]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[0];
                        pdw[10] = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[0]) << 16) | (((DWORD)pb1[2]) << 8) | pb1[1];
                        pdw[11] = (((DWORD)pb2[2]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[0]) << 8) | pb2[2];

                        pdw += 12;
                        pb += 8;
                    }

                    if (fInterlace)
                    {
                        pb = pbSAM;
                        pdw = pdwBack + lPitchDW/2;

                        if (!GetOption(scanlevel))
                            memset(pdw, 0x00, nWidth);
                        else
                        {
                            for (int x = 0 ; x < nRightLo ; x++)
                            {
                                BYTE *pb1 = (BYTE*)&aulScanline[pb[0]], *pb2 = (BYTE*)&aulScanline[pb[1]];
                                pdw[0]  = (((DWORD)pb1[0]) << 24) | (((DWORD)pb1[2]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[0];
                                pdw[1]  = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[0]) << 16) | (((DWORD)pb1[2]) << 8) | pb1[1];
                                pdw[2]  = (((DWORD)pb2[2]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[0]) << 8) | pb2[2];

                                pb1 = (BYTE*)&aulScanline[pb[2]], pb2 = (BYTE*)&aulScanline[pb[3]];
                                pdw[3]  = (((DWORD)pb1[0]) << 24) | (((DWORD)pb1[2]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[0];
                                pdw[4]  = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[0]) << 16) | (((DWORD)pb1[2]) << 8) | pb1[1];
                                pdw[5]  = (((DWORD)pb2[2]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[0]) << 8) | pb2[2];

                                pb1 = (BYTE*)&aulScanline[pb[4]], pb2 = (BYTE*)&aulScanline[pb[5]];
                                pdw[6]  = (((DWORD)pb1[0]) << 24) | (((DWORD)pb1[2]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[0];
                                pdw[7]  = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[0]) << 16) | (((DWORD)pb1[2]) << 8) | pb1[1];
                                pdw[8]  = (((DWORD)pb2[2]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[0]) << 8) | pb2[2];

                                pb1 = (BYTE*)&aulScanline[pb[6]], pb2 = (BYTE*)&aulScanline[pb[7]];
                                pdw[9]  = (((DWORD)pb1[0]) << 24) | (((DWORD)pb1[2]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[0];
                                pdw[10] = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[0]) << 16) | (((DWORD)pb1[2]) << 8) | pb1[1];
                                pdw[11] = (((DWORD)pb2[2]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[0]) << 8) | pb2[2];

                                pdw += 12;
                                pb += 8;
                            }
                        }
                    }
                }
            }
        }
        break;

        case 32:
        {
            nWidth <<= 2;

            for (int y = 0 ; y < nBottom ; pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, y++)
            {
                if (!pfDirty[y])
                    continue;

                if (pfHiRes[y])
                {
                    for (int x = 0 ; x < nRightHi ; x++)
                    {
                        pdw[0] = aulPalette[pb[0]];
                        pdw[1] = aulPalette[pb[1]];
                        pdw[2] = aulPalette[pb[2]];
                        pdw[3] = aulPalette[pb[3]];
                        pdw[4] = aulPalette[pb[4]];
                        pdw[5] = aulPalette[pb[5]];
                        pdw[6] = aulPalette[pb[6]];
                        pdw[7] = aulPalette[pb[7]];

                        pdw += 8;
                        pb += 8;
                    }

                    if (fInterlace)
                    {
                        pb = pbSAM;
                        pdw = pdwBack + lPitchDW/2;

                        if (!GetOption(scanlevel))
                            memset(pdw, 0x00, nWidth);
                        else
                        {
                            for (int x = 0 ; x < nRightHi ; x++)
                            {
                                pdw[0] = aulScanline[pb[0]];
                                pdw[1] = aulScanline[pb[1]];
                                pdw[2] = aulScanline[pb[2]];
                                pdw[3] = aulScanline[pb[3]];
                                pdw[4] = aulScanline[pb[4]];
                                pdw[5] = aulScanline[pb[5]];
                                pdw[6] = aulScanline[pb[6]];
                                pdw[7] = aulScanline[pb[7]];

                                pdw += 8;
                                pb += 8;
                            }
                        }
                    }
                }
                else
                {
                    for (int x = 0 ; x < nRightLo ; x++)
                    {
                        pdw[0]  = pdw[1]  = aulPalette[pb[0]];
                        pdw[2]  = pdw[3]  = aulPalette[pb[1]];
                        pdw[4]  = pdw[5]  = aulPalette[pb[2]];
                        pdw[6]  = pdw[7]  = aulPalette[pb[3]];
                        pdw[8]  = pdw[9]  = aulPalette[pb[4]];
                        pdw[10] = pdw[11] = aulPalette[pb[5]];
                        pdw[12] = pdw[13] = aulPalette[pb[6]];
                        pdw[14] = pdw[15] = aulPalette[pb[7]];

                        pdw += 16;
                        pb += 8;
                    }

                    if (fInterlace)
                    {
                        pb = pbSAM;
                        pdw = pdwBack + lPitchDW/2;

                        if (!GetOption(scanlevel))
                            memset(pdw, 0x00, nWidth);
                        else
                        {
                            for (int x = 0 ; x < nRightLo ; x++)
                            {
                                pdw[0]  = pdw[1]  = aulScanline[pb[0]];
                                pdw[2]  = pdw[3]  = aulScanline[pb[1]];
                                pdw[4]  = pdw[5]  = aulScanline[pb[2]];
                                pdw[6]  = pdw[7]  = aulScanline[pb[3]];
                                pdw[8]  = pdw[9]  = aulScanline[pb[4]];
                                pdw[10] = pdw[11] = aulScanline[pb[5]];
                                pdw[12] = pdw[13] = aulScanline[pb[6]];
                                pdw[14] = pdw[15] = aulScanline[pb[7]];

                                pdw += 16;
                                pb += 8;
                            }
                        }
                    }
                }
            }
        }
        break;
    }

    // Unlock the surface now we're done drawing on it
    if (pSurface_ && SDL_MUSTLOCK(pSurface_))
        SDL_UnlockSurface(pSurface_);

    // Calculate the source rectangle for the full visible area
    rSource.x = 0;
    rSource.y = 0;
    rSource.w = pScreen_->GetPitch();
    rSource.h = nBottom;

    // Calculate the target rectangle on the target display
    rTarget.x = ((pFront->w - rSource.w) >> 1);
    rTarget.y = ((pFront->h - (rSource.h << nShift)) >> 1);
    rTarget.w = rSource.w;
    rTarget.h = rSource.h << nShift;


    // Find the first changed display line
    int nChangeFrom = 0;
    for ( ; nChangeFrom < nBottom && !pfDirty[nChangeFrom] ; nChangeFrom++);

    if (nChangeFrom < nBottom)
    {
        // Find the last change display line
        int nChangeTo = nBottom-1;
        for ( ; nChangeTo && !pfDirty[nChangeTo] ; nChangeTo--);

        // Clear the dirty flags for the changed block
        for (int i = nChangeFrom ; i <= nChangeTo ; pfDirty[i++] = false);

        // Calculate the dirty source and target areas - non-GUI displays require the height doubling
        SDL_Rect rect = { 0, nChangeFrom << nShift, pScreen_->GetPitch(), ((nChangeTo - nChangeFrom + 1) << nShift) };
        SDL_Rect rectFront = { (pFront->w - rect.w) >> 1, rect.y + ((pFront->h - (nBottom << nShift)) >> 1), rect.w, rect.h };

        // Blit the updated area and inform SDL it's changed
        SDL_BlitSurface(pBack, &rect, pFront, &rectFront);
        SDL_UpdateRects(pFront, 1, &rectFront);
    }

    // Success
    return true;
}


#ifdef USE_OPENGL

// OpenGL version of DisplayChanges
void DrawChangesGL (CScreen* pScreen_)
{
    bool fInterlace = GetOption(scanlines) && !GUI::IsActive();

    int nBottom = Frame::GetHeight();
    int nWidth = Frame::GetWidth(), nRightHi = nWidth >> 3, nRightLo = nRightHi >> 1;

    bool *pfDirty = Display::pafDirty, *pfHiRes = pScreen_->GetHiRes();

    BYTE *pbSAM = pScreen_->GetLine(0), *pb = pbSAM;
    long lPitch = pScreen_->GetPitch();

    long lPitchDW = static_cast<long>(reinterpret_cast<DWORD*>(&dwTextureData[0][1]) - reinterpret_cast<DWORD*>(&dwTextureData[0][0]));

    DWORD *pdwBack, *pdw;
    pdw = pdwBack = reinterpret_cast<DWORD*>(&dwTextureData[TEX_DISPLAY]);

    // 16-bit?
    if (g_glDataType != GL_UNSIGNED_BYTE)
    {
        // Halve the pitch since we're dealing in WORD-sized pixels
        lPitchDW >>= 1;

        for (int y = 0 ; y < nBottom ; pb = pbSAM += lPitch, y++)
        {
            int x;

            if (!pfDirty[y])
                ;
            else if (pfHiRes[y])
            {
                for (x = 0 ; x < nRightHi ; x++)
                {
                    pdw[0] = JoinWORDs(aulPalette[pb[0]], aulPalette[pb[1]]);
                    pdw[1] = JoinWORDs(aulPalette[pb[2]], aulPalette[pb[3]]);
                    pdw[2] = JoinWORDs(aulPalette[pb[4]], aulPalette[pb[5]]);
                    pdw[3] = JoinWORDs(aulPalette[pb[6]], aulPalette[pb[7]]);

                    pdw += 4;
                    pb += 8;
                }
            }
            else
            {
                for (x = 0 ; x < nRightLo ; x++)
                {
                    pdw[0] = aulPalette[pb[0]] * 0x00010001UL;
                    pdw[1] = aulPalette[pb[1]] * 0x00010001UL;
                    pdw[2] = aulPalette[pb[2]] * 0x00010001UL;
                    pdw[3] = aulPalette[pb[3]] * 0x00010001UL;
                    pdw[4] = aulPalette[pb[4]] * 0x00010001UL;
                    pdw[5] = aulPalette[pb[5]] * 0x00010001UL;
                    pdw[6] = aulPalette[pb[6]] * 0x00010001UL;
                    pdw[7] = aulPalette[pb[7]] * 0x00010001UL;

                    pdw += 8;
                    pb += 8;
                }
            }

            pdw = pdwBack += lPitchDW;
        }
    }
    else    // 32-bit
    {
        for (int y = 0 ; y < nBottom ; pb = pbSAM += lPitch, y++)
        {
            int x;

            if (!pfDirty[y])
                ;
            else if (pfHiRes[y])
            {
                for (x = 0 ; x < nRightHi ; x++)
                {
                    pdw[0] = aulPalette[pb[0]];
                    pdw[1] = aulPalette[pb[1]];
                    pdw[2] = aulPalette[pb[2]];
                    pdw[3] = aulPalette[pb[3]];
                    pdw[4] = aulPalette[pb[4]];
                    pdw[5] = aulPalette[pb[5]];
                    pdw[6] = aulPalette[pb[6]];
                    pdw[7] = aulPalette[pb[7]];

                    pdw += 8;
                    pb += 8;
                }
            }
            else
            {
                for (x = 0 ; x < nRightLo ; x++)
                {
                    pdw[0]  = pdw[1]  = aulPalette[pb[0]];
                    pdw[2]  = pdw[3]  = aulPalette[pb[1]];
                    pdw[4]  = pdw[5]  = aulPalette[pb[2]];
                    pdw[6]  = pdw[7]  = aulPalette[pb[3]];
                    pdw[8]  = pdw[9]  = aulPalette[pb[4]];
                    pdw[10] = pdw[11] = aulPalette[pb[5]];
                    pdw[12] = pdw[13] = aulPalette[pb[6]];
                    pdw[14] = pdw[15] = aulPalette[pb[7]];

                    pdw += 16;
                    pb += 8;
                }
            }

            pdw = pdwBack += lPitchDW;
        }
    }

    // Calculate the source rectangle for the full visible area
    rSource.x = 0;
    rSource.y = 0;
    rSource.w = pScreen_->GetPitch();
    rSource.h = nBottom;

    // Find the first changed display line
    int nChangeFrom = 0;
    for ( ; nChangeFrom < nBottom && !pfDirty[nChangeFrom] ; nChangeFrom++);

    if (nChangeFrom < nBottom)
    {
        // Find the last change display line
        int nChangeTo = nBottom-1;
        for ( ; nChangeTo && !pfDirty[nChangeTo] ; nChangeTo--);

        // Clear the dirty flags for the changed block
        for (int i = nChangeFrom ; i <= nChangeTo ; pfDirty[i++] = false);

        // Offset and length of the change block
        int y = nChangeFrom, w = Frame::GetWidth(), h = nChangeTo-nChangeFrom+1;

        // Bind to the display texture
        glBindTexture(GL_TEXTURE_2D, auTextures[TEX_DISPLAY]);

        // Set up the data adjustments for the sub-image
        glPixelStorei(GL_UNPACK_ROW_LENGTH, TEX_WIDTH);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, y);

        // Update the changed block
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, w, h, g_glPixelFormat, g_glDataType, dwTextureData[TEX_DISPLAY][0]);

        // Restore defaults, just in case
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    }

    glPushMatrix();
    float flHeight = static_cast<float>(nBottom);

    if (GUI::IsActive())
    {
        glScalef(1.0f, -1.0f, 1.0f);            // Flip vertically
        glTranslatef(0.0f, -flHeight, 0.0f);    // Centre image
    }
    else
    {
        glScalef(1.0f, -2.0f, 1.0f);            // Flip and double vertically
        glTranslatef(0.0f, -flHeight/2, 0.0f);  // Centre image
    }

    glCallList(dlist);
    glPopMatrix();

    if (fInterlace)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_DST_COLOR, GL_ZERO);

        glBindTexture(GL_TEXTURE_2D, auTextures[TEX_SCANLINE]);
        glBegin(GL_QUADS);

        // Stretch the texture over the full display width
        glTexCoord2f(0.0f,1.0f); glVertex2f(0.0f, TEX_HEIGHT);
        glTexCoord2f(0.0f,0.0f); glVertex2f(0.0f, 0.0f);
        glTexCoord2f(1.0f,0.0f); glVertex2f(TEX_WIDTH, 0.0f);
        glTexCoord2f(1.0f,1.0f); glVertex2f(TEX_WIDTH, TEX_HEIGHT);

        glEnd();
        glDisable(GL_BLEND);
    }

    glFlush();
    SDL_GL_SwapBuffers();
}

#endif


// Update the display to show anything that's changed since last time
void Display::Update (CScreen* pScreen_)
{
    // Draw any changed lines
#ifdef USE_OPENGL
    DrawChangesGL(pScreen_);
#else
    DrawChanges(pScreen_, pBack);
#endif
}


// Scale a client size/movement to one relative to the SAM view port size
// Should round down and be consistent with positive and negative values
void Display::DisplayToSamSize (int* pnX_, int* pnY_)
{
#ifdef USE_OPENGL
    int nHalfWidth = !GUI::IsActive(), nHalfHeight = nHalfWidth;
#else
    int nHalfWidth = !GUI::IsActive(), nHalfHeight = 0;
#endif

    *pnX_ = *pnX_ * (rSource.w >> nHalfWidth)  / rTarget.w;
    *pnY_ = *pnY_ * (rSource.h >> nHalfHeight) / rTarget.h;
}

// Map a client point to one relative to the SAM view port
void Display::DisplayToSamPoint (int* pnX_, int* pnY_)
{
    *pnX_ -= rTarget.x;
    *pnY_ -= rTarget.y;
    DisplayToSamSize(pnX_, pnY_);
}
