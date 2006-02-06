// Part of SimCoupe - A SAM Coupe emulator
//
// Display.cpp: Allegro display rendering
//
//  Copyright (c) 1999-2002  Simon Owen
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
//   At present, the DOS version draws lines to a buffer, which are then
//   copied directly to the display surface.  All other versions draw to a
//   back buffer, which is then blitted/stretched to the screen as required.
//   The DOS version sacrifices the 5:4 ratio and non-scanlines modes for
//   a little extra speed.

// ToDo:
//  - change to handle multiple dirty regions
//  - use back-buffer for DOS too?

#include "SimCoupe.h"

#include "Display.h"
#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "Profile.h"
#include "Video.h"
#include "UI.h"


bool* Display::pafDirty;

Display::RECT rSource, rTarget;

#ifdef ALLEGRO_DOS
static BYTE abLine[WIDTH_PIXELS << 3], abScanLine[WIDTH_PIXELS << 3];
#endif

////////////////////////////////////////////////////////////////////////////////

// Writing to the display in DWORDs makes it endian sensitive, so we need to cover both cases
#ifndef __BIG_ENDIAN__

inline DWORD PaletteDWORD (BYTE b1_, BYTE b2_, BYTE b3_, BYTE b4_, DWORD* pulPalette_)
    { return (pulPalette_[b4_] << 24) | (pulPalette_[b3_] << 16) | (pulPalette_[b2_] << 8) | pulPalette_[b1_]; }
inline DWORD PaletteDWORD (BYTE b1_, BYTE b2_, DWORD* pulPalette_)
    { return (pulPalette_[b2_] << 16) | pulPalette_[b1_]; }
inline DWORD MakeDWORD (BYTE b1_, BYTE b2_, BYTE b3_, BYTE b4_)
    { return (b4_ << 24) | (b3_ << 16) | (b2_ << 8) | b1_; }
inline DWORD MakeDWORD (BYTE b1_, BYTE b2_)
    { return ((b2_ << 16) | b1_) * 0x0101UL; }

#else

inline DWORD PaletteDWORD (BYTE b1_, BYTE b2_, BYTE b3_, BYTE b4_, DWORD* pulPalette_)
    { return (pulPalette_[b1_] << 24) | (pulPalette_[b2_] << 16) | (pulPalette_[b3_] << 8) | pulPalette_[b4_]; }
inline DWORD PaletteDWORD (BYTE b1_, BYTE b2_, DWORD* pulPalette_)
    { return (pulPalette_[b1_] << 16) | pulPalette_[b2_]; }
inline DWORD MakeDWORD (BYTE b1_, BYTE b2_, BYTE b3_, BYTE b4_)
    { return (b1_ << 24) | (b2_ << 16) | (b3_ << 8) | b4_; }
inline DWORD MakeDWORD (BYTE b1_, BYTE b2_)
    { return ((b1_ << 16) | b2_) * 0x0101UL; }

#endif

////////////////////////////////////////////////////////////////////////////////

bool Display::Init (bool fFirstInit_/*=false*/)
{
    Exit(true);

    pafDirty = new bool[Frame::GetHeight()];

#ifdef ALLEGRO_DOS
	memset(abScanLine, 0x00, sizeof(abScanLine));
#endif

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
bool DrawChanges (CScreen* pScreen_, BITMAP* pSurface_)
{
    ProfileStart(Gfx);

#ifdef ALLEGRO_DOS
    // DOS treats the screen as the front surface
    pSurface_ = screen;
#endif

    // Lock the surface for direct access below
    acquire_bitmap(pSurface_);
    acquire_bitmap(pFront);

    bool fInterlace = !GUI::IsActive();

    DWORD *pdwBack = reinterpret_cast<DWORD*>(pSurface_->line[0]), *pdw = pdwBack;
    long lPitchDW = (pSurface_->line[1] - pSurface_->line[0]) >> (fInterlace ? 1 : 2);
    bool *pfDirty = Display::pafDirty, *pfHiRes = pScreen_->GetHiRes();

    BYTE *pbSAM = pScreen_->GetLine(0), *pb = pbSAM;
    long lPitch = pScreen_->GetPitch();

    int nShift = fInterlace ? 1 : 0;
    int nDepth = bitmap_color_depth(pSurface_);
    int nBottom = pScreen_->GetHeight() >> nShift;
    int nWidth = pScreen_->GetPitch(), nRightHi = nWidth >> 3, nRightLo = nRightHi >> 1;

    // Calculate the offset to centralise the image on the screen (in native pixels)
	int nDisplayedWidth = GetOption(ratio5_4) ? nWidth * 5/4 : nWidth;
    int nOffset = (SCREEN_W - nDisplayedWidth) >> 1;
    pdw = reinterpret_cast<DWORD*>(reinterpret_cast<BYTE*>(pdw) + nOffset);

    // What colour depth is the target surface?
    switch (nDepth)
    {
        case 8:
        {
            for (int y = 0 ; y < nBottom ; pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, y++)
            {
                if (!pfDirty[y])
                    continue;

#ifdef ALLEGRO_DOS
                // Draw to a memory buffer
                pdw = reinterpret_cast<DWORD*>(abLine);
#endif

                if (pfHiRes[y])
                {
                    for (int x = 0 ; x < nRightHi ; x++)
                    {
                        pdw[0] = MakeDWORD(pb[0], pb[1], pb[2], pb[3]);
                        pdw[1] = MakeDWORD(pb[4], pb[5], pb[6], pb[7]);

                        pdw += 2;
                        pb += 8;
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
                }

#ifdef ALLEGRO_DOS
                // The DOS version updates the display directly with the changed line
                DWORD dwOffset = bmp_write_line(pFront, y << nShift) + nOffset;
                WORD wSegment = screen->seg, wLength = nRightHi << 1;

                asm __volatile__ (" 	\n"
                    "push %%es			\n"
                    "cld				\n"
                    "movw %%bx, %%es	\n"
                    "rep				\n"
                    "movsl				\n"
                    "pop %%es"
                    : /* no outputs */
                    : "S" (abLine), "b" (wSegment), "D" (dwOffset), "c" (wLength)
                    : "cc"
                );
/*
				if (fInterlace)
				{
					dwOffset = bmp_write_line(pFront, (y << nShift)+1) + nOffset;

					if (!GetOption(scanlines))
					{
						asm __volatile__ (" 	\n"
							"push %%es			\n"
							"cld				\n"
							"movw %%bx, %%es	\n"
							"rep				\n"
							"movsl				\n"
							"pop %%es"
							: // no outputs
							: "S" (abScanLine), "b" (wSegment), "D" (dwOffset), "c" (wLength)
							: "cc"
						);
					}
					else
					{
						asm __volatile__ (" 	\n"
							"push %%es			\n"
							"cld				\n"
							"movw %%bx, %%es	\n"
							"rep				\n"
							"movsl				\n"
							"pop %%es"
							: // no outputs
							: "S" (abLine), "b" (wSegment), "D" (dwOffset), "c" (wLength)
							: "cc"
						);
					}
				}
*/
#endif
            }
        }
        break;

#ifndef ALLEGRO_DOS

        case 16:
        {
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
                }
            }
        }
        break;

        case 24:
        {
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
                }
            }
        }
        break;

        case 32:
        {
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
                }
            }
        }
        break;
#endif  // ALLEGRO_DOS
    }

    ProfileEnd();

    ProfileStart(Blt);

    // Calculate the source rectangle for the full visible area
    rSource.x = 0;
    rSource.y = 0;
    rSource.w = pScreen_->GetPitch();
    rSource.h = nBottom;

    // Calculate the target rectangle on the target display
    rTarget.x = ((SCREEN_W - nDisplayedWidth) >> 1);
    rTarget.y = ((SCREEN_H - (rSource.h << nShift)) >> 1);
    rTarget.w = nDisplayedWidth;
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

        // Dirty region updating only needs to be done for non-DOS versions
#ifndef ALLEGRO_DOS
        if (fInterlace)
            nChangeFrom <<= 1, nChangeTo <<= 1;

		// Re-evaluate whether we need to stretch the image vertically
        nShift = !GetOption(scanlines) && fInterlace;

        // Calculate the dirty source and target areas - non-GUI displays require the height doubling
        Display::RECT rBack  = { rSource.x, nChangeFrom, rSource.w, (nChangeTo - nChangeFrom) + 1 };
        Display::RECT rFront = { rTarget.x, rTarget.y + (nChangeFrom << nShift), rTarget.w,
                                rTarget.y + ((nChangeTo - nChangeFrom + 1) << nShift) };

        // Blit if the source and target are the same size, otherwise stretch
        if (rBack.w == rFront.w && rBack.h == rFront.h)
            blit(pSurface_, pFront, rBack.x, rBack.y, rFront.x, rFront.y, rBack.w, rBack.h);
        else
            stretch_blit(pSurface_, pFront, rBack.x, rBack.y, rBack.w, rBack.h, rFront.x, rFront.y, rFront.w, rFront.h);
#endif
    }

    // Unlock the bitmaps now we're done with em
    release_bitmap(pFront);
    release_bitmap(pSurface_);

    ProfileEnd();

    // Success
    return true;
}


// Update the display to show anything that's changed since last time
void Display::Update (CScreen* pScreen_)
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


// Scale a client size/movement to one relative to the SAM view port size
// Should round down and be consistent with positive and negative values
void Display::DisplayToSamSize (int* pnX_, int* pnY_)
{
    int nHalfWidth = !GUI::IsActive();

    *pnX_ = *pnX_ * (rSource.w >> nHalfWidth) / rTarget.w;
    *pnY_ = *pnY_ * rSource.h / rTarget.h;
}

// Scale a size/movement in the SAM view port to one relative to the client
// Should round down and be consistent with positive and negative values
void Display::SamToDisplaySize (int* pnX_, int* pnY_)
{
    int nHalfWidth = !GUI::IsActive();

    *pnX_ = *pnX_ * rTarget.w / (rSource.w >> nHalfWidth);
    *pnY_ = *pnY_ * rTarget.h / rSource.h;
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
