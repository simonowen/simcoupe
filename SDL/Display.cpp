// Part of SimCoupe - A SAM Coupe emulator
//
// Display.cpp: SDL display rendering
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

// ToDo:
//  - change to handle multiple dirty regions

#include "SimCoupe.h"

#include "Display.h"
#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "Profile.h"
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
bool DrawChanges (CScreen* pScreen_, SDL_Surface* pSurface_)
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

    ProfileStart(Gfx);

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

                        if (!GetOption(scanlines))
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

                        if (!GetOption(scanlines))
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

    ProfileEnd();

    ProfileStart(Blt);


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

    ProfileEnd();

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

    long lPitchDW = reinterpret_cast<DWORD*>(&dwTextureData[0][1]) - reinterpret_cast<DWORD*>(&dwTextureData[0][0]);

    DWORD *pdwBack1, *pdwBack2, *pdwBack3, *pdw1, *pdw2, *pdw3;
    pdw1 = pdwBack1 = reinterpret_cast<DWORD*>(&dwTextureData[0]);
    pdw2 = pdwBack2 = reinterpret_cast<DWORD*>(&dwTextureData[1]);
    pdw3 = pdwBack3 = reinterpret_cast<DWORD*>(&dwTextureData[2]);

    ProfileStart(Gfx);

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
                for (x = 0 ; x < 32 ; x++)
                {
                    pdw1[0] = JoinWORDs(aulPalette[pb[0]], aulPalette[pb[1]]);
                    pdw1[1] = JoinWORDs(aulPalette[pb[2]], aulPalette[pb[3]]);
                    pdw1[2] = JoinWORDs(aulPalette[pb[4]], aulPalette[pb[5]]);
                    pdw1[3] = JoinWORDs(aulPalette[pb[6]], aulPalette[pb[7]]);

                    pdw1 += 4;
                    pb += 8;
                }

                for (x = 32 ; x < 64 ; x++)
                {
                    pdw2[0] = JoinWORDs(aulPalette[pb[0]], aulPalette[pb[1]]);
                    pdw2[1] = JoinWORDs(aulPalette[pb[2]], aulPalette[pb[3]]);
                    pdw2[2] = JoinWORDs(aulPalette[pb[4]], aulPalette[pb[5]]);
                    pdw2[3] = JoinWORDs(aulPalette[pb[6]], aulPalette[pb[7]]);

                    pdw2 += 4;
                    pb += 8;
                }

                for (x = 64 ; x < nRightHi ; x++)
                {
                    pdw3[0] = JoinWORDs(aulPalette[pb[0]], aulPalette[pb[1]]);
                    pdw3[1] = JoinWORDs(aulPalette[pb[2]], aulPalette[pb[3]]);
                    pdw3[2] = JoinWORDs(aulPalette[pb[4]], aulPalette[pb[5]]);
                    pdw3[3] = JoinWORDs(aulPalette[pb[6]], aulPalette[pb[7]]);

                    pdw3 += 4;
                    pb += 8;
                }
            }
            else
            {
                for (x = 0 ; x < 16 ; x++)
                {
                    pdw1[0] = aulPalette[pb[0]] * 0x00010001UL;
                    pdw1[1] = aulPalette[pb[1]] * 0x00010001UL;
                    pdw1[2] = aulPalette[pb[2]] * 0x00010001UL;
                    pdw1[3] = aulPalette[pb[3]] * 0x00010001UL;
                    pdw1[4] = aulPalette[pb[4]] * 0x00010001UL;
                    pdw1[5] = aulPalette[pb[5]] * 0x00010001UL;
                    pdw1[6] = aulPalette[pb[6]] * 0x00010001UL;
                    pdw1[7] = aulPalette[pb[7]] * 0x00010001UL;

                    pdw1 += 8;
                    pb += 8;
                }

                for (x = 16 ; x < 32 ; x++)
                {
                    pdw2[0] = aulPalette[pb[0]] * 0x00010001UL;
                    pdw2[1] = aulPalette[pb[1]] * 0x00010001UL;
                    pdw2[2] = aulPalette[pb[2]] * 0x00010001UL;
                    pdw2[3] = aulPalette[pb[3]] * 0x00010001UL;
                    pdw2[4] = aulPalette[pb[4]] * 0x00010001UL;
                    pdw2[5] = aulPalette[pb[5]] * 0x00010001UL;
                    pdw2[6] = aulPalette[pb[6]] * 0x00010001UL;
                    pdw2[7] = aulPalette[pb[7]] * 0x00010001UL;

                    pdw2 += 8;
                    pb += 8;
                }

                for (x = 32 ; x < nRightLo ; x++)
                {
                    pdw3[0] = aulPalette[pb[0]] * 0x00010001UL;
                    pdw3[1] = aulPalette[pb[1]] * 0x00010001UL;
                    pdw3[2] = aulPalette[pb[2]] * 0x00010001UL;
                    pdw3[3] = aulPalette[pb[3]] * 0x00010001UL;
                    pdw3[4] = aulPalette[pb[4]] * 0x00010001UL;
                    pdw3[5] = aulPalette[pb[5]] * 0x00010001UL;
                    pdw3[6] = aulPalette[pb[6]] * 0x00010001UL;
                    pdw3[7] = aulPalette[pb[7]] * 0x00010001UL;

                    pdw3 += 8;
                    pb += 8;
                }
            }

            if (y == 255)
            {
                pdw1 = pdwBack1 = reinterpret_cast<DWORD*>(&dwTextureData[3]);
                pdw2 = pdwBack2 = reinterpret_cast<DWORD*>(&dwTextureData[4]);
                pdw3 = pdwBack3 = reinterpret_cast<DWORD*>(&dwTextureData[5]);
            }
            else if (y == 511)
            {
                pdw1 = pdwBack1 = reinterpret_cast<DWORD*>(&dwTextureData[6]);
                pdw2 = pdwBack2 = reinterpret_cast<DWORD*>(&dwTextureData[7]);
                pdw3 = pdwBack3 = reinterpret_cast<DWORD*>(&dwTextureData[8]);
            }
            else
            {
                pdw1 = pdwBack1 += lPitchDW;
                pdw2 = pdwBack2 += lPitchDW;
                pdw3 = pdwBack3 += lPitchDW;
            }
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
                for (x = 0 ; x < 32 ; x++)
                {
                    pdw1[0] = aulPalette[pb[0]];
                    pdw1[1] = aulPalette[pb[1]];
                    pdw1[2] = aulPalette[pb[2]];
                    pdw1[3] = aulPalette[pb[3]];
                    pdw1[4] = aulPalette[pb[4]];
                    pdw1[5] = aulPalette[pb[5]];
                    pdw1[6] = aulPalette[pb[6]];
                    pdw1[7] = aulPalette[pb[7]];

                    pdw1 += 8;
                    pb += 8;
                }

                for (x = 32 ; x < 64 ; x++)
                {
                    pdw2[0] = aulPalette[pb[0]];
                    pdw2[1] = aulPalette[pb[1]];
                    pdw2[2] = aulPalette[pb[2]];
                    pdw2[3] = aulPalette[pb[3]];
                    pdw2[4] = aulPalette[pb[4]];
                    pdw2[5] = aulPalette[pb[5]];
                    pdw2[6] = aulPalette[pb[6]];
                    pdw2[7] = aulPalette[pb[7]];

                    pdw2 += 8;
                    pb += 8;
                }

                for (x = 64 ; x < nRightHi ; x++)
                {
                    pdw3[0] = aulPalette[pb[0]];
                    pdw3[1] = aulPalette[pb[1]];
                    pdw3[2] = aulPalette[pb[2]];
                    pdw3[3] = aulPalette[pb[3]];
                    pdw3[4] = aulPalette[pb[4]];
                    pdw3[5] = aulPalette[pb[5]];
                    pdw3[6] = aulPalette[pb[6]];
                    pdw3[7] = aulPalette[pb[7]];

                    pdw3 += 8;
                    pb += 8;
                }
            }
            else
            {
                for (x = 0 ; x < 16 ; x++)
                {
                    pdw1[0]  = pdw1[1]  = aulPalette[pb[0]];
                    pdw1[2]  = pdw1[3]  = aulPalette[pb[1]];
                    pdw1[4]  = pdw1[5]  = aulPalette[pb[2]];
                    pdw1[6]  = pdw1[7]  = aulPalette[pb[3]];
                    pdw1[8]  = pdw1[9]  = aulPalette[pb[4]];
                    pdw1[10] = pdw1[11] = aulPalette[pb[5]];
                    pdw1[12] = pdw1[13] = aulPalette[pb[6]];
                    pdw1[14] = pdw1[15] = aulPalette[pb[7]];

                    pdw1 += 16;
                    pb += 8;
                }

                for (x = 16 ; x < 32 ; x++)
                {
                    pdw2[0]  = pdw2[1]  = aulPalette[pb[0]];
                    pdw2[2]  = pdw2[3]  = aulPalette[pb[1]];
                    pdw2[4]  = pdw2[5]  = aulPalette[pb[2]];
                    pdw2[6]  = pdw2[7]  = aulPalette[pb[3]];
                    pdw2[8]  = pdw2[9]  = aulPalette[pb[4]];
                    pdw2[10] = pdw2[11] = aulPalette[pb[5]];
                    pdw2[12] = pdw2[13] = aulPalette[pb[6]];
                    pdw2[14] = pdw2[15] = aulPalette[pb[7]];

                    pdw2 += 16;
                    pb += 8;
                }

                for (x = 32 ; x < nRightLo ; x++)
                {
                    pdw3[0]  = pdw3[1]  = aulPalette[pb[0]];
                    pdw3[2]  = pdw3[3]  = aulPalette[pb[1]];
                    pdw3[4]  = pdw3[5]  = aulPalette[pb[2]];
                    pdw3[6]  = pdw3[7]  = aulPalette[pb[3]];
                    pdw3[8]  = pdw3[9]  = aulPalette[pb[4]];
                    pdw3[10] = pdw3[11] = aulPalette[pb[5]];
                    pdw3[12] = pdw3[13] = aulPalette[pb[6]];
                    pdw3[14] = pdw3[15] = aulPalette[pb[7]];

                    pdw3 += 16;
                    pb += 8;
                }
            }

            if (y == 255)
            {
                pdw1 = pdwBack1 = reinterpret_cast<DWORD*>(&dwTextureData[3]);
                pdw2 = pdwBack2 = reinterpret_cast<DWORD*>(&dwTextureData[4]);
                pdw3 = pdwBack3 = reinterpret_cast<DWORD*>(&dwTextureData[5]);
            }
            else if (y == 511)
            {
                pdw1 = pdwBack1 = reinterpret_cast<DWORD*>(&dwTextureData[6]);
                pdw2 = pdwBack2 = reinterpret_cast<DWORD*>(&dwTextureData[7]);
                pdw3 = pdwBack3 = reinterpret_cast<DWORD*>(&dwTextureData[8]);
            }
            else
            {
                pdw1 = pdwBack1 += lPitchDW;
                pdw2 = pdwBack2 += lPitchDW;
                pdw3 = pdwBack3 += lPitchDW;
            }
        }
    }
    ProfileEnd();


    ProfileStart(Blt);

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

        // Work out the width of each texture used for the display image
        int nWidth = Frame::GetWidth(), w1, w2, w3;
        nWidth -= (w1 = min(nWidth, 256));
        w3 = (nWidth -= (w2 = min(nWidth, 256)));

        // Some OpenGL drivers can't update partial horizontal regions, so use the full texture width
        // nVidia drivers are fine, but Banshee and BeOS (software) drivers and known to suffer.
        w1 = w2 = w3 = 256;

        if (nChangeFrom < 256)
        {
            int y = max(nChangeFrom-0,0), h = min(nChangeTo-0,255)-y+1, d = y*lPitchDW;

            glBindTexture(GL_TEXTURE_2D,auTextures[0]);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,y,w1,h,g_glPixelFormat,g_glDataType,dwTextureData[0][0]+d);

            glBindTexture(GL_TEXTURE_2D,auTextures[1]);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,y,w2,h,g_glPixelFormat,g_glDataType,dwTextureData[1][0]+d);

            glBindTexture(GL_TEXTURE_2D,auTextures[2]);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,y,w3,h,g_glPixelFormat,g_glDataType,dwTextureData[2][0]+d);
        }

        if (nChangeFrom <= 511 && nChangeTo >= 256)
        {
            int y = max(nChangeFrom-256,0), h = min(nChangeTo-256,255)-y+1, d = y*lPitchDW;

            glBindTexture(GL_TEXTURE_2D,auTextures[3]);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,y,w1,h,g_glPixelFormat,g_glDataType,dwTextureData[3][0]+d);

            glBindTexture(GL_TEXTURE_2D,auTextures[4]);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,y,w2,h,g_glPixelFormat,g_glDataType,dwTextureData[4][0]+d);

            glBindTexture(GL_TEXTURE_2D,auTextures[5]);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,y,w3,h,g_glPixelFormat,g_glDataType,dwTextureData[5][0]+d);
        }

        if (nChangeTo >= 512)
        {
            int y = max(nChangeFrom-512,0), h = min(nChangeTo-512,255)-y+1, d = y*lPitchDW;

            glBindTexture(GL_TEXTURE_2D,auTextures[6]);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,y,w1,h,g_glPixelFormat,g_glDataType,dwTextureData[6][0]+d);

            glBindTexture(GL_TEXTURE_2D,auTextures[7]);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,y,w2,h,g_glPixelFormat,g_glDataType,dwTextureData[7][0]+d);

            glBindTexture(GL_TEXTURE_2D,auTextures[8]);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,y,w3,h,g_glPixelFormat,g_glDataType,dwTextureData[8][0]+d);
        }
    }

    glPushMatrix();
    float flHeight = static_cast<float>(nBottom);
    float flWidth = static_cast<float>(nWidth);

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

        glBindTexture(GL_TEXTURE_2D, auTextures[N_TEXTURES]);
        glBegin(GL_QUADS);

        // Add scanlines to each vertical tile separately
        for (int y = 0 ; y < (N_TEXTURES/3) ; y++)
        {
            float flSize = 256.0f, flX = 0.0, flY = flSize*y;
            float flMin = 0.0f, flMax = 1.0f;

            // Stretch the texture over the full display width
            glTexCoord2f(flMin,flMax); glVertex2f(flX,         flY+flSize);
            glTexCoord2f(flMin,flMin); glVertex2f(flX,         flY);
            glTexCoord2f(flMax,flMin); glVertex2f(flX+flWidth, flY);
            glTexCoord2f(flMax,flMax); glVertex2f(flX+flWidth, flY+flSize);
        }

        glEnd();
        glDisable(GL_BLEND);
    }


    glFlush();
    SDL_GL_SwapBuffers();

    ProfileEnd();
}

#endif


// Update the display to show anything that's changed since last time
void Display::Update (CScreen* pScreen_)
{
    // Don't draw if fullscreen but not active
    if (GetOption(fullscreen) && !g_fActive)
        return;

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

// Scale a size/movement in the SAM view port to one relative to the client
// Should round down and be consistent with positive and negative values
void Display::SamToDisplaySize (int* pnX_, int* pnY_)
{
#ifdef USE_OPENGL
    int nHalfWidth = !GUI::IsActive(), nHalfHeight = nHalfWidth;
#else
    int nHalfWidth = !GUI::IsActive(), nHalfHeight = 0;
#endif

    *pnX_ = *pnX_ * rTarget.w / (rSource.w >> nHalfWidth);
    *pnY_ = *pnY_ * rTarget.h / (rSource.h >> nHalfHeight);
}

// Map a client point to one relative to the SAM view port
void Display::DisplayToSamPoint (int* pnX_, int* pnY_)
{
    *pnX_ -= rTarget.x;
    *pnY_ -= rTarget.y;
    DisplayToSamSize(pnX_, pnY_);
}

// Map a point in the SAM view port to a point relative to the client position
void Display::SamToDisplayPoint (int* pnX_, int* pnY_)
{
    SamToDisplaySize(pnX_, pnY_);
    *pnX_ += rTarget.x;
    *pnY_ += rTarget.y;
}
