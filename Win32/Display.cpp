// Part of SimCoupe - A SAM Coupé emulator
//
// Display.cpp: Win32 display rendering
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
//  - change to blit only the changed portions of the screen, to speed things
//    up a lot on systems with no hardware accelerated blit

#include "SimCoupe.h"
#include <ddraw.h>

#include "Display.h"
#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "Profile.h"
#include "Video.h"
#include "UI.h"


bool* Display::pafDirty;

RECT rLastOverlay, rSource, rTarget;
DWORD dwColourKey;


void SetOverlayColourKey ()
{
    LPDIRECTDRAWSURFACE pddsOverlay = pddsFront ? pddsFront : pddsBack;
    DDSURFACEDESC ddsd = { sizeof ddsd };

    // Are we using an overlay surface?
    if (SUCCEEDED(pddsOverlay->GetSurfaceDesc(&ddsd)) && (ddsd.ddsCaps.dwCaps & DDSCAPS_OVERLAY))
    {
        DDSURFACEDESC ddsd = { sizeof ddsd };
        pddsPrimary->GetSurfaceDesc(&ddsd);

        HDC hdc;
        pddsPrimary->GetDC(&hdc);

        // Save the pixel from 0,0 on the display
        COLORREF rgbPrev = GetPixel(hdc, 0, 0);

        // Use the classic shocking pink if the display is palettised, or a nicer near-black colour otherwise
        SetPixel(hdc, 0, 0, (ddsd.ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8) ? RGB(0xff,0x00,0xff) : RGB(0x08,0x08,0x08));

        pddsPrimary->ReleaseDC(hdc);

        // Lock the surface and see what the value is for the current mode
        HRESULT hr;
        if (FAILED(hr = pddsPrimary->Lock(NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR|DDLOCK_WAIT, NULL)))
            TRACE("Failed to lock primary surface in SetOverlayColour() (%#08lx)\n", hr);
        else
        {
            // Extract the real colour value for the colour key
            dwColourKey = *reinterpret_cast<DWORD*>(ddsd.lpSurface);
            pddsPrimary->Unlock(NULL);

            // If less than 32-bit, limit the colour value to the number of bits used for the screen depth
            if (ddsd.ddpfPixelFormat.dwRGBBitCount < 32)
                dwColourKey &= (1 << ddsd.ddpfPixelFormat.dwRGBBitCount) - 1;

            TRACE("Colour key used: %#08lx\n", dwColourKey);
        }

        // Restore the previous pixel
        pddsPrimary->GetDC(&hdc);
        SetPixel(hdc, 0, 0, rgbPrev);
        pddsPrimary->ReleaseDC(hdc);
    }
}


bool Display::Init (bool fFirstInit_/*=false*/)
{
    Exit(true);
    TRACE("-> Display::Init(%s)\n", fFirstInit_ ? "first" : "");

    pafDirty = new bool[Frame::GetHeight()];

    bool fRet = Video::Init(fFirstInit_);
    if (fRet)
        SetOverlayColourKey();

    TRACE("<- Display::Init() returning %s\n", fRet ? "true" : "false");
    return fRet;
}

void Display::Exit (bool fReInit_/*=false*/)
{
    Video::Exit(fReInit_);
    TRACE("-> Display::Exit(%s)\n", fReInit_ ? "reinit" : "");

    if (pafDirty) { delete pafDirty; pafDirty = NULL; }

    TRACE("<- Display::Exit()\n");
}


void Display::SetDirty ()
{
    // Mark all display lines dirty
    for (int i = 0, nHeight = Frame::GetHeight() ; i < nHeight ; i++)
        pafDirty[i] = true;

    // Ensure the overlay is updated to reflect the changes
    SetRectEmpty(&rLastOverlay);
}


// Draw the changed lines in the appropriate colour depth and hi/low resolution
bool Display::DrawChanges (CScreen* pScreen_, LPDIRECTDRAWSURFACE pSurface_)
{
    ProfileStart(Gfx);

    HRESULT hr;
    DDSURFACEDESC ddsd = { sizeof ddsd };
    if (!pSurface_ || FAILED(hr = pSurface_->Restore()))
    {
        TRACE("!!! DrawChanges(): Failed to restore surface [%#08lx] (%#08lx)\n", pSurface_, hr);
        ProfileEnd();
        return false;
    }

    // If we've changing from displaying the GUI back to scanline mode, clear the unused lines on the surface
    static bool fOldInterlace = false;
    bool fInterlace = GetOption(scanlines) && !GUI::IsActive();
    if (!fOldInterlace && fInterlace)
        Video::ClearSurface(pSurface_);
    fOldInterlace = fInterlace;


    // Lock the surface,  without taking the Win16Mutex if possible
    if (FAILED(hr = pSurface_->Lock(NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR|DDLOCK_WAIT|DDLOCK_NOSYSLOCK, NULL))
          && FAILED(hr = pSurface_->Lock(NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR|DDLOCK_WAIT, NULL)))
    {
        TRACE("!!! DrawChanges()  Failed to lock back surface (%#08lx)\n", hr);
        ProfileEnd();
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

    bool fYUV = (ddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC) != 0;
    int nDepth = ddsd.ddpfPixelFormat.dwRGBBitCount;
    int nBottom = pScreen_->GetHeight() >> (GUI::IsActive() ? 0 : 1);
    int nRightHi = pScreen_->GetPitch() >> 3, nRightLo = nRightHi >> 1;

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
                }

                pfDirty[y] = false;
            }
        }
        break;

        case 16:
        {
            for (int y = 0 ; y < nBottom ; pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, y++)
            {
                if (!pfDirty[y])
                    continue;

                // RBG?
                if (!fYUV)
                {
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
                    }
                }

                // YUV
                else
                {
                    if (pfHiRes[y])
                    {
                        for (int x = 0 ; x < nRightHi ; x++)
                        {
                            BYTE b0, b1;
                            b0 = pb[0]; b1 = pb[1]; pdw[0] = ((DWORD)(awV[b1] | awY[b1]) << 16) | awU[b0] | awY[b0];
                            b0 = pb[2]; b1 = pb[3]; pdw[1] = ((DWORD)(awV[b1] | awY[b1]) << 16) | awU[b0] | awY[b0];
                            b0 = pb[4]; b1 = pb[5]; pdw[2] = ((DWORD)(awV[b1] | awY[b1]) << 16) | awU[b0] | awY[b0];
                            b0 = pb[6]; b1 = pb[7]; pdw[3] = ((DWORD)(awV[b1] | awY[b1]) << 16) | awU[b0] | awY[b0];

                            pdw += 4;
                            pb += 8;
                        }
                    }
                    else
                    {
                        for (int x = 0 ; x < nRightLo ; x++)
                        {
                            BYTE b;
                            b = pb[0]; pdw[0] = ((DWORD)(awV[b] | awY[b]) << 16) | awU[b] | awY[b];
                            b = pb[1]; pdw[1] = ((DWORD)(awV[b] | awY[b]) << 16) | awU[b] | awY[b];
                            b = pb[2]; pdw[2] = ((DWORD)(awV[b] | awY[b]) << 16) | awU[b] | awY[b];
                            b = pb[3]; pdw[3] = ((DWORD)(awV[b] | awY[b]) << 16) | awU[b] | awY[b];
                            b = pb[4]; pdw[4] = ((DWORD)(awV[b] | awY[b]) << 16) | awU[b] | awY[b];
                            b = pb[5]; pdw[5] = ((DWORD)(awV[b] | awY[b]) << 16) | awU[b] | awY[b];
                            b = pb[6]; pdw[6] = ((DWORD)(awV[b] | awY[b]) << 16) | awU[b] | awY[b];
                            b = pb[7]; pdw[7] = ((DWORD)(awV[b] | awY[b]) << 16) | awU[b] | awY[b];

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
                }

                pfDirty[y] = false;
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

                pfDirty[y] = false;
            }
        }
        break;
    }

    pSurface_->Unlock(ddsd.lpSurface);

    ProfileEnd();

    // Success
    return true;
}


// Update the display to show anything that's changed since last time
void Display::Update (CScreen* pScreen_)
{
    HRESULT hr;

    bool fHalfHeight = !GUI::IsActive() && !GetOption(scanlines);

    // Don't draw if fullscreen but not active
    if (GetOption(fullscreen) && !g_fActive)
        return;

    // If the screen mode has changed we'll need to reinitialise the video
    if (pddsPrimary && (FAILED(hr = pddsPrimary->Restore()) && hr == DDERR_WRONGMODE) && !GetOption(fullscreen))
    {
        TRACE("Display::Update(): DDERR_WRONGMODE, reinit video...\n");
        Frame::Init();
        return;
    }

    // Draw any changed lines
    if (!DrawChanges(pScreen_, pddsBack))
    {
        TRACE("Display::Update(): DrawChanges() failed\n");
        return;
    }


    // Now to get the image to the display...

    LPDIRECTDRAWSURFACE pddsOverlay = pddsFront ? pddsFront : pddsBack;

    DDSURFACEDESC ddsdBack, ddsdOverlay, ddsdPrimary;
    ddsdBack.dwSize = ddsdOverlay.dwSize = ddsdPrimary.dwSize = sizeof ddsdBack;
    pddsBack->GetSurfaceDesc(&ddsdBack);

    pddsOverlay->GetSurfaceDesc(&ddsdOverlay);
    pddsPrimary->GetSurfaceDesc(&ddsdPrimary);

    bool fOverlay = (ddsdBack.ddsCaps.dwCaps & DDSCAPS_OVERLAY) != 0;

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
        SetRect(&rFront, 0, 0, ddsdPrimary.dwWidth, ddsdPrimary.dwHeight);

    // For windowed mode we use the client area
    else
    {
        GetClientRect(g_hwnd, &rFront);
        ClientToScreen(g_hwnd, &ptOffset);
    }

    // rTo is the target area needed for our screen display
    RECT rTo = rFront;

    // Restrict the target area to maintain the aspect ratio?
    if (GetOption(stretchtofit) || !GetOption(fullscreen))
    {
        // Fit to the width of the target region if it's too narrow
        if (MulDiv(rFrom.bottom, rFront.right, rFrom.right) > rFront.bottom)
            rTo.right = MulDiv(rFront.bottom, rFrom.right, rFrom.bottom);

        // Fit to the height of the target region if it's too short
        else if (MulDiv(rFrom.right, rFront.bottom, rFrom.bottom) > rFront.right)
            rTo.bottom = MulDiv(rFront.right, rFrom.bottom, rFrom.right);
    }

    // No-stretch mode, useful for video cards that don't have hardware support
    else
        SetRect(&rTo, 0, 0, rFrom.right, rFrom.bottom);

    // Centre the target view within the target area
    OffsetRect(&rTo, (rFront.right - rTo.right) >> 1, (rFront.bottom - rTo.bottom) >> 1);

    ProfileStart(Blt);

    // If we're using an overlay we need to fill the visible area with the colour key
    if (fOverlay)
    {
        if (!GetOption(fullscreen))
        {
            // Restrict the target area to what can actually be displayed, as the overlay can't overlay the screen edges
            RECT rClip;
            HDC hdc = GetDC(g_hwnd);
            GetClipBox(hdc, &rClip);
            ReleaseDC(g_hwnd, hdc);
            IntersectRect(&rClip, &rTo, &rClip);

            // Modify the source screen portion to include only the portion visible in the clipped area
            SetRect(&rBack, MulDiv(rBack.right, rClip.left - rTo.left, rTo.right - rTo.left),
                            MulDiv(rBack.bottom, rClip.top - rTo.top, rTo.bottom - rTo.top),
                            MulDiv(rBack.right, rClip.right - rTo.left, rTo.right - rTo.left),
                            MulDiv(rBack.bottom, rClip.bottom - rTo.top, rTo.bottom - rTo.top));

            // Offset the target area to its final screen position in the window's client area
            OffsetRect(&(rTo = rClip), ptOffset.x, ptOffset.y);
        }


        // If we have a middle buffer, we need to blit the image onto it to get the overlay to display it
        if (pddsFront)
            pddsFront->Blt(NULL, pddsBack, NULL, DDBLT_WAIT, 0);


        if (fHalfHeight)
            rBack.bottom >>= 1;

        // Set up the destination colour key so the overlay doesn't appear over the top of everything
        DDOVERLAYFX ddofx = { sizeof ddofx };
        ddofx.dckDestColorkey.dwColorSpaceLowValue = ddofx.dckDestColorkey.dwColorSpaceHighValue = dwColourKey;

        // If the overlay position is the same, force an update [removed as this was very slow on some cards!]
        if (EqualRect(&rTo, &rLastOverlay))
            ;//pddsOverlay->UpdateOverlay(&rBack, pddsPrimary, &rTo, DDOVER_REFRESHALL|DDOVER_KEYDESTOVERRIDE, NULL);

        // Otherwise if it's not empty, show it and update it
        else if (!IsRectEmpty(&rTo) && SUCCEEDED(hr = pddsOverlay->UpdateOverlay(&rBack, pddsPrimary, &rTo, DDOVER_SHOW | DDOVER_KEYDESTOVERRIDE, &ddofx)))
            rLastOverlay = rTo;

        // Otherwise hide it for now
        else
        {
            TRACE("UpdateOverlay() failed with %#08lx\n", hr);
            pddsOverlay->UpdateOverlay(NULL, pddsPrimary, NULL, DDOVER_HIDE, NULL);
        }


        // Fill the appropriate area with the colour key
        DDBLTFX bltfx = { sizeof bltfx };
        bltfx.dwFillColor = dwColourKey;
        pddsPrimary->Blt(&rTo, NULL, NULL, DDBLT_COLORFILL|DDBLT_WAIT, &bltfx);
    }
    else
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

    DDBLTFX bltfx = { sizeof bltfx };
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

    ProfileEnd();
}

// Map a Windows client point to one relative to the SAM view port
void Display::DisplayToSam (int* pnX_, int* pnY_)
{
    int nHalfWidth = !GUI::IsActive(), nHalfHeight = nHalfWidth && GetOption(scanlines);

    *pnX_ = MulDiv(*pnX_ - rTarget.left, rSource.right, rTarget.right-rTarget.left) >> nHalfWidth;
    *pnY_ = MulDiv(*pnY_ - rTarget.top, rSource.bottom, rTarget.bottom-rTarget.top) >> nHalfHeight;
}

// Map a point in the SAM view port to a point relative to the Windows client position
void Display::SamToDisplay (int* pnX_, int* pnY_)
{
    int nHalfWidth = !GUI::IsActive(), nHalfHeight = nHalfWidth && GetOption(scanlines);

    *pnX_ = MulDiv(*pnX_ << nHalfWidth, rTarget.right-rTarget.left, rSource.right) + rTarget.left;
    *pnY_ = MulDiv(*pnY_ << nHalfHeight, rTarget.bottom-rTarget.top, rSource.bottom) + rTarget.top;
}
