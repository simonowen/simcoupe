// Part of SimCoupe - A SAM Coupe emulator
//
// Display.cpp: Win32 display rendering
//
//  Copyright (c) 1999-2011  Simon Owen
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
//  - change to blit only the changed portions of the screen, to speed things
//    up a lot on systems with no hardware accelerated blit

#include "SimCoupe.h"
#include <ddraw.h>

#include "Display.h"
#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "Video.h"
#include "UI.h"


bool* Display::pafDirty;

static RECT rSource, rTarget;


bool Display::Init (bool fFirstInit_/*=false*/)
{
    bool fRet;

    Exit(true);
    TRACE("-> Display::Init(%s)\n", fFirstInit_ ? "first" : "");

    pafDirty = new bool[Frame::GetHeight()];
    SetDirty();

    fRet = Video::Init(fFirstInit_);
    TRACE("<- Display::Init() returning %s\n", fRet ? "true" : "false");
    return fRet;
}

void Display::Exit (bool fReInit_/*=false*/)
{
    TRACE("-> Display::Exit(%s)\n", fReInit_ ? "reinit" : "");
    delete pafDirty; pafDirty = NULL;
    Video::Exit(fReInit_);
    TRACE("<- Display::Exit()\n");
}


void Display::SetDirty ()
{
    // Mark all display lines dirty
    for (int i = 0, nHeight = Frame::GetHeight() ; i < nHeight ; i++)
        pafDirty[i] = true;
}


// Draw the changed lines in the appropriate colour depth and hi/low resolution
bool Display::DrawChanges (CScreen* pScreen_, LPDIRECTDRAWSURFACE pSurface_)
{
    HRESULT hr;
    DDSURFACEDESC ddsd = { sizeof(ddsd) };

    // If we've changing from displaying the GUI back to scanline mode, clear the unused lines on the surface
    bool fInterlace = GetOption(scanlines) && !GUI::IsActive();

    // Lock the surface,  without taking the Win16Mutex if possible
    if (FAILED(hr = pSurface_->Lock(NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR|DDLOCK_WRITEONLY|DDLOCK_WAIT|DDLOCK_NOSYSLOCK, NULL))
     && FAILED(hr = pSurface_->Lock(NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR|DDLOCK_WRITEONLY|DDLOCK_WAIT, NULL)))
    {
        TRACE("!!! DrawChanges()  Failed to lock back surface (%#08lx)\n", hr);
        return false;
    }

    // In scanline mode, treat the back buffer as full height, drawing alternate lines
    if (fInterlace)
        ddsd.lPitch <<= 1;

    DWORD *pdwBack = reinterpret_cast<DWORD*>(ddsd.lpSurface), *pdw = pdwBack;
    LONG lPitchDW = ddsd.lPitch >> 2;
    bool *pfDirty = pafDirty, *pfHiRes = pScreen_->GetHiRes();

    BYTE *pbSAM = pScreen_->GetLine(0), *pb = pbSAM;
    LONG lPitch = pScreen_->GetPitch();

    int nDepth = ddsd.ddpfPixelFormat.dwRGBBitCount;
    int nBottom = pScreen_->GetHeight() >> (GUI::IsActive() ? 0 : 1);
    int nWidth = pScreen_->GetPitch(), nRightHi = nWidth >> 3, nRightLo = nRightHi >> 1;

    switch (nDepth)
    {
        case 8:
        {
            static const DWORD BASE_COLOUR = PALETTE_OFFSET * 0x01010101UL;

            for (int y = 0 ; y < nBottom ; pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, y++)
            {
                if (!pfDirty[y])
                    continue;

                if (pfHiRes[y])
                {
                    for (int x = 0 ; x < nRightHi ; x++)
                    {
                        pdw[0] = BASE_COLOUR + ((pb[3] << 24) | (pb[2] << 16) | (pb[1] << 8) | pb[0]);
                        pdw[1] = BASE_COLOUR + ((pb[7] << 24) | (pb[6] << 16) | (pb[5] << 8) | pb[4]);

                        pdw += 2;
                        pb += 8;
                    }

                    if (fInterlace)
                    {
                        pdw = pdwBack + lPitchDW/2;
                        memset(pdw, 0x00, nWidth);
                    }
                }
                else
                {
                    for (int x = 0 ; x < nRightLo ; x++)
                    {
                        pdw[0] = BASE_COLOUR + ((pb[1] << 16) | pb[0]) * 0x101UL;
                        pdw[1] = BASE_COLOUR + ((pb[3] << 16) | pb[2]) * 0x101UL;
                        pdw[2] = BASE_COLOUR + ((pb[5] << 16) | pb[4]) * 0x101UL;
                        pdw[3] = BASE_COLOUR + ((pb[7] << 16) | pb[6]) * 0x101UL;
                        pdw += 4;
                        pb += 8;
                    }

                    if (fInterlace)
                    {
                        pdw = pdwBack + lPitchDW/2;
                        memset(pdw, 0x00, nWidth);
                    }
                }

                pfDirty[y] = false;
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
                        // Draw 8 pixels at a time
                        pdw[0] = (aulPalette[pb[1]] << 16) | aulPalette[pb[0]];
                        pdw[1] = (aulPalette[pb[3]] << 16) | aulPalette[pb[2]];
                        pdw[2] = (aulPalette[pb[5]] << 16) | aulPalette[pb[4]];
                        pdw[3] = (aulPalette[pb[7]] << 16) | aulPalette[pb[6]];

                        pdw += 4;
                        pb += 8;
                    }

                    if (fInterlace)
                    {
                        pb = pbSAM;
                        pdw = pdwBack + lPitchDW/2;

                        for (int x = 0 ; x < nRightHi ; x++)
                        {
                            // Draw 8 pixels at a time
                            pdw[0] = (aulScanline[pb[1]] << 16) | aulScanline[pb[0]];
                            pdw[1] = (aulScanline[pb[3]] << 16) | aulScanline[pb[2]];
                            pdw[2] = (aulScanline[pb[5]] << 16) | aulScanline[pb[4]];
                            pdw[3] = (aulScanline[pb[7]] << 16) | aulScanline[pb[6]];

                            pdw += 4;
                            pb += 8;
                        }
                    }
                }
                else 
                {
                    for (int x = 0 ; x < nRightLo ; x++)
                    {
                        // Draw 8 pixels at a time
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

                        for (int x = 0 ; x < nRightLo ; x++)
                        {
                        // Draw 8 pixels at a time
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

                pfDirty[y] = false;
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

                if (pfHiRes[y])
                {
                    for (int x = 0 ; x < nRightHi ; x++)
                    {
                        BYTE *pb1 = (BYTE*)&aulPalette[pb[0]], *pb2 = (BYTE*)&aulPalette[pb[1]];
                        BYTE *pb3 = (BYTE*)&aulPalette[pb[2]], *pb4 = (BYTE*)&aulPalette[pb[3]];
                        pdw[0] = (((DWORD)pb2[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                        pdw[1] = (((DWORD)pb3[1]) << 24) | (((DWORD)pb3[2]) << 16) | (((DWORD)pb2[0]) << 8) | pb2[1];
                        pdw[2] = (((DWORD)pb4[0]) << 24) | (((DWORD)pb4[1]) << 16) | (((DWORD)pb4[2]) << 8) | pb3[0];

                        pb1 = (BYTE*)&aulPalette[pb[4]], pb2 = (BYTE*)&aulPalette[pb[5]];
                        pb3 = (BYTE*)&aulPalette[pb[6]], pb4 = (BYTE*)&aulPalette[pb[7]];
                        pdw[3] = (((DWORD)pb2[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                        pdw[4] = (((DWORD)pb3[1]) << 24) | (((DWORD)pb3[2]) << 16) | (((DWORD)pb2[0]) << 8) | pb2[1];
                        pdw[5] = (((DWORD)pb4[0]) << 24) | (((DWORD)pb4[1]) << 16) | (((DWORD)pb4[2]) << 8) | pb3[0];

                        pdw += 6;
                        pb += 8;
                    }

                    if (fInterlace)
                    {
                        pb = pbSAM;
                        pdw = pdwBack + lPitchDW/2;

                        for (int x = 0 ; x < nRightHi ; x++)
                        {
                            BYTE *pb1 = (BYTE*)&aulScanline[pb[0]], *pb2 = (BYTE*)&aulScanline[pb[1]];
                            BYTE *pb3 = (BYTE*)&aulScanline[pb[2]], *pb4 = (BYTE*)&aulScanline[pb[3]];
                            pdw[0] = (((DWORD)pb2[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                            pdw[1] = (((DWORD)pb3[1]) << 24) | (((DWORD)pb3[2]) << 16) | (((DWORD)pb2[0]) << 8) | pb2[1];
                            pdw[2] = (((DWORD)pb4[0]) << 24) | (((DWORD)pb4[1]) << 16) | (((DWORD)pb4[2]) << 8) | pb3[0];

                            pb1 = (BYTE*)&aulScanline[pb[4]], pb2 = (BYTE*)&aulScanline[pb[5]];
                            pb3 = (BYTE*)&aulScanline[pb[6]], pb4 = (BYTE*)&aulScanline[pb[7]];
                            pdw[3] = (((DWORD)pb2[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                            pdw[4] = (((DWORD)pb3[1]) << 24) | (((DWORD)pb3[2]) << 16) | (((DWORD)pb2[0]) << 8) | pb2[1];
                            pdw[5] = (((DWORD)pb4[0]) << 24) | (((DWORD)pb4[1]) << 16) | (((DWORD)pb4[2]) << 8) | pb3[0];

                            pdw += 6;
                            pb += 8;
                        }
                    }
                }
                else
                {
                    for (int x = 0 ; x < nRightLo ; x++)
                    {
                        BYTE *pb1 = (BYTE*)&aulPalette[pb[0]], *pb2 = (BYTE*)&aulPalette[pb[1]];
                        pdw[0]  = (((DWORD)pb1[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                        pdw[1]  = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[2]) << 16) | (((DWORD)pb1[0]) << 8) | pb1[1];
                        pdw[2]  = (((DWORD)pb2[0]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[0];

                        pb1 = (BYTE*)&aulPalette[pb[2]], pb2 = (BYTE*)&aulPalette[pb[3]];
                        pdw[3]  = (((DWORD)pb1[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                        pdw[4]  = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[2]) << 16) | (((DWORD)pb1[0]) << 8) | pb1[1];
                        pdw[5]  = (((DWORD)pb2[0]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[0];

                        pb1 = (BYTE*)&aulPalette[pb[4]], pb2 = (BYTE*)&aulPalette[pb[5]];
                        pdw[6]  = (((DWORD)pb1[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                        pdw[7]  = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[2]) << 16) | (((DWORD)pb1[0]) << 8) | pb1[1];
                        pdw[8]  = (((DWORD)pb2[0]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[0];

                        pb1 = (BYTE*)&aulPalette[pb[6]], pb2 = (BYTE*)&aulPalette[pb[7]];
                        pdw[9]  = (((DWORD)pb1[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                        pdw[10] = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[2]) << 16) | (((DWORD)pb1[0]) << 8) | pb1[1];
                        pdw[11] = (((DWORD)pb2[0]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[0];

                        pdw += 12;
                        pb += 8;
                    }

                    if (fInterlace)
                    {
                        pb = pbSAM;
                        pdw = pdwBack + lPitchDW/2;

                        for (int x = 0 ; x < nRightLo ; x++)
                        {
                            BYTE *pb1 = (BYTE*)&aulScanline[pb[0]], *pb2 = (BYTE*)&aulScanline[pb[1]];
                            pdw[0]  = (((DWORD)pb1[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                            pdw[1]  = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[2]) << 16) | (((DWORD)pb1[0]) << 8) | pb1[1];
                            pdw[2]  = (((DWORD)pb2[0]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[0];

                            pb1 = (BYTE*)&aulScanline[pb[2]], pb2 = (BYTE*)&aulScanline[pb[3]];
                            pdw[3]  = (((DWORD)pb1[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                            pdw[4]  = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[2]) << 16) | (((DWORD)pb1[0]) << 8) | pb1[1];
                            pdw[5]  = (((DWORD)pb2[0]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[0];

                            pb1 = (BYTE*)&aulScanline[pb[4]], pb2 = (BYTE*)&aulScanline[pb[5]];
                            pdw[6]  = (((DWORD)pb1[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                            pdw[7]  = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[2]) << 16) | (((DWORD)pb1[0]) << 8) | pb1[1];
                            pdw[8]  = (((DWORD)pb2[0]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[0];

                            pb1 = (BYTE*)&aulScanline[pb[6]], pb2 = (BYTE*)&aulScanline[pb[7]];
                            pdw[9]  = (((DWORD)pb1[2]) << 24) | (((DWORD)pb1[0]) << 16) | (((DWORD)pb1[1]) << 8) | pb1[2];
                            pdw[10] = (((DWORD)pb2[1]) << 24) | (((DWORD)pb2[2]) << 16) | (((DWORD)pb1[0]) << 8) | pb1[1];
                            pdw[11] = (((DWORD)pb2[0]) << 24) | (((DWORD)pb2[1]) << 16) | (((DWORD)pb2[2]) << 8) | pb2[0];

                            pdw += 12;
                            pb += 8;
                        }
                    }
                }

                pfDirty[y] = false;
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

                pfDirty[y] = false;
            }
        }
        break;
    }

    pSurface_->Unlock(ddsd.lpSurface);

    // Success
    return true;
}


// Update the display to show anything that's changed since last time
void Display::Update (CScreen* pScreen_)
{
    HRESULT hr = 0;

    // Check if we've lost the surface memory
    if (!pddsPrimary || FAILED(hr = pddsPrimary->Restore()) || 
        pddsFront && FAILED(hr = pddsFront->Restore()) ||
        pddsBack && FAILED (hr = pddsBack->Restore()))
    {
        // Reinitialise the video system if the mode has changed
        if (hr == DDERR_WRONGMODE)
            Video::Init();

        return;
    }

    // Now to get the image to the display...

    DDSURFACEDESC ddsdBack, ddsdPrimary;
    ddsdBack.dwSize = ddsdPrimary.dwSize = sizeof(ddsdBack);
    pddsBack->GetSurfaceDesc(&ddsdBack);

    bool fHalfHeight = !GUI::IsActive() && !GetOption(scanlines);


    // rBack is the total screen area
    RECT rBack = { 0, 0, ddsdBack.dwWidth, ddsdBack.dwHeight };

    // rFrom is the actual screen area that will be visible
    RECT rFrom = rBack;
    if (GetOption(ratio5_4))
        rFrom.right = MulDiv(rFrom.right, 5, 4);

    // rFront is the total target area we've got to play with
    RECT rFront;
    POINT ptOffset = { 0, 0 };

    // Full-screen mode uses the full screen area
    if (GetOption(fullscreen))
    {
        pddsPrimary->GetSurfaceDesc(&ddsdPrimary);
        SetRect(&rFront, 0, 0, ddsdPrimary.dwWidth, ddsdPrimary.dwHeight);
    }

    // For windowed mode we use the client area
    else
    {
        GetClientRect(g_hwnd, &rFront);
        ClientToScreen(g_hwnd, &ptOffset);
    }

    // Return if there's nothing to draw
    if (IsRectEmpty(&rFront))
        return;

    // rTo is the target area needed for our screen display
    RECT rTo = rFront;

    // Stretch to fit, preserving aspect ratio?
    if (GetOption(stretchtofit) || !GetOption(fullscreen))
    {
        // Fit to the width of the target region if it's too narrow
        if (MulDiv(rFrom.bottom, rFront.right, rFrom.right) > rFront.bottom)
            rTo.right = MulDiv(rFront.bottom, rFrom.right, rFrom.bottom);

        // Fit to the height of the target region if it's too short
        else if (MulDiv(rFrom.right, rFront.bottom, rFrom.bottom) > rFront.right)
            rTo.bottom = MulDiv(rFront.right, rFrom.bottom, rFrom.right);
    }
    else
    {
        // Start from a low-res 1:1 pixel back image size
        SetRect(&rTo, 0, 0, rFrom.right/2, rFrom.bottom/2);

        // Permit pixel-doubling to scale, even in non-stretch mode
        while (rTo.right*2 <= rFront.right && rTo.bottom*2 <= rFront.bottom)
        {
            rTo.right <<= 1;
            rTo.bottom <<= 1;
        }
    }

    // Centre the target view within the target area
    OffsetRect(&rTo, (rFront.right - rTo.right) >> 1, (rFront.bottom - rTo.bottom) >> 1);

    // Draw any changes
    if (DrawChanges(pScreen_, pddsBack))
    {
        if (fHalfHeight)
            rBack.bottom >>= 1;

        // Offset to the client area if necessary
        OffsetRect(&rTo, ptOffset.x, ptOffset.y);

        // Otherwise we need to blit to the primary buffer to get the image displayed
        if (FAILED(hr = pddsPrimary->Blt(&rTo, pddsBack, &rBack, DDBLT_WAIT, 0)))
            TRACE("!!! Blt (back to primary) failed with %#08lx\n", hr);
    }

    // Offset the front to cover the total viewing area, only some of which may be used
    OffsetRect(&rFront, ptOffset.x, ptOffset.y);

    // Calculate the border regions around the target image that require clearing
    RECT rLeftBorder = { rFront.left, rTo.top, rTo.left, rTo.bottom };
    RECT rTopBorder = { rFront.left, rFront.top, rFront.right, rTo.top };
    RECT rRightBorder = { rTo.right, rTo.top, rFront.right, rTo.bottom };
    RECT rBottomBorder = { rFront.left, rTo.bottom, rFront.right, rFront.bottom };

    DDBLTFX bltfx = { sizeof(bltfx) };
    bltfx.dwFillColor = 0;

    if (!IsRectEmpty(&rLeftBorder)) pddsPrimary->Blt(&rLeftBorder, NULL, NULL, DDBLT_COLORFILL|DDBLT_WAIT, &bltfx);
    if (!IsRectEmpty(&rTopBorder)) pddsPrimary->Blt(&rTopBorder, NULL, NULL, DDBLT_COLORFILL|DDBLT_WAIT, &bltfx);
    if (!IsRectEmpty(&rRightBorder)) pddsPrimary->Blt(&rRightBorder, NULL, NULL, DDBLT_COLORFILL|DDBLT_WAIT, &bltfx);
    if (!IsRectEmpty(&rBottomBorder)) pddsPrimary->Blt(&rBottomBorder, NULL, NULL, DDBLT_COLORFILL|DDBLT_WAIT, &bltfx);


    // Remember the source and target rects for cursor position mapping in the GUI
    rSource = rBack;
    rTarget = rTo;

    // Adjust for the client area when Windowed, and non-client area when full-screen
    if (GetOption(fullscreen))
        ClientToScreen(g_hwnd, &ptOffset);
    OffsetRect(&rTarget, -ptOffset.x, -ptOffset.y);
}

// Scale a Windows client size/movement to one relative to the SAM view port size
// Should round down and be consistent with positive and negative values
void Display::DisplayToSamSize (int* pnX_, int* pnY_)
{
    int nHalfWidth = !GUI::IsActive(), nHalfHeight = nHalfWidth && GetOption(scanlines);

    *pnX_ = *pnX_ * rSource.right / ((rTarget.right-rTarget.left) << nHalfWidth);
    *pnY_ = *pnY_ * rSource.bottom / ((rTarget.bottom-rTarget.top) << nHalfHeight);
}

// Map a Windows client point to one relative to the SAM view port
void Display::DisplayToSamPoint (int* pnX_, int* pnY_)
{
    *pnX_ -= rTarget.left;
    *pnY_ -= rTarget.top;
    DisplayToSamSize(pnX_, pnY_);
}
