// Part of SimCoupe - A SAM Coupe emulator
//
// Video.cpp: Win32 core video functionality using DirectDraw
//
//  Copyright (c) 1999-2004  Simon Owen
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

#include <ddraw.h>
#include <mmsystem.h>

#include "Frame.h"
#include "Display.h"
#include "GUI.h"
#include "IO.h"
#include "Options.h"
#include "UI.h"
#include "Util.h"
#include "Video.h"


const int N_TOTAL_COLOURS = N_PALETTE_COLOURS+N_GUI_COLOURS;

// SAM RGB values in appropriate format, and YUV values pre-shifted for overlay surface
DWORD aulPalette[N_TOTAL_COLOURS];
WORD awY[N_TOTAL_COLOURS], awU[N_TOTAL_COLOURS], awV[N_TOTAL_COLOURS];

// DirectDraw back and front surfaces
LPDIRECTDRAWSURFACE pddsPrimary, pddsFront, pddsBack;


LPDIRECTDRAW pdd;
LPDIRECTDRAWPALETTE pddPal;
LPDIRECTDRAWCLIPPER pddClipper;
DDCAPS ddcaps;

HRESULT hr;

LPDIRECTDRAWSURFACE CreateSurface (DWORD dwCaps_, DWORD dwWidth_=0, DWORD dwHeight_=0, LPDDPIXELFORMAT pddpf_=NULL, DWORD dwRequiredCaps_=0);
LPDIRECTDRAWSURFACE CreateOverlay (DWORD dwWidth_, DWORD dwHeight_, LPDDPIXELFORMAT pddpf_=NULL);


// function to initialize DirectDraw in windowed mode
bool Video::Init (bool fFirstInit_)
{
    bool fRet = false;

    Exit();
    TRACE("-> Video::Init(%s)\n", fFirstInit_ ? "first" : "");

    // Turn off scanlines if we're using stretch to fit, as it looks ugly otherwise!
    if (GetOption(stretchtofit))
        SetOption(scanlines, false);

    // Create the main DirectDraw object
    HRESULT hr = DirectDrawCreate(GetOption(surface) ? NULL : (LPGUID)DDCREATE_EMULATIONONLY, &pdd, NULL);
    if (FAILED(hr))
        Message(msgError, "DirectDrawCreate() failed with %#08lx", hr);
    else
    {
        // Get the driver capabilites so we know what we need to set up
        ddcaps.dwSize = sizeof ddcaps;
        pdd->GetCaps(&ddcaps, NULL);

        // Use exclusive mode for full-screen, or normal mode for windowed
        hr = pdd->SetCooperativeLevel(g_hwnd, GetOption(fullscreen) ? DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN|DDSCL_ALLOWREBOOT : DDSCL_NORMAL);
        if (FAILED(hr))
            Message(msgError, "SetCooperativeLevel() failed with %#08lx", hr);
        else
        {
            // Get the dimensions of viewable area as displayed on the screen
            DWORD dwWidth = Frame::GetWidth(), dwHeight = Frame::GetHeight();
            TRACE("Frame:: dwWidth = %lu, dwHeight = %lu\n", dwWidth, dwHeight);
            if (GetOption(ratio5_4))
                dwWidth = MulDiv(dwWidth, 5, 4);

            // Work out the screen dimensions needed for full screen mode
            int nWidth, nHeight, nDepth = GetOption(depth);

            // Full screen mode requires a display mode change
            if (GetOption(fullscreen))
            {
                // Work out the best-fit mode
                if (dwWidth <= 640 && dwHeight <= 480)
                    nWidth = 640, nHeight = 480;
                else if (dwWidth <= 800 && dwHeight <= 600)
                    nWidth = 800, nHeight = 600;
                else if (dwWidth <= 1024 && dwHeight <= 768)
                    nWidth = 1024, nHeight = 768;

                // Loop while we can't select the mode we want
                while (FAILED(hr = pdd->SetDisplayMode(nWidth, nHeight, nDepth)))
                {
                    TRACE("!!! Failed to set %dx%dx%d mode!\n", nWidth, nHeight, nDepth);

                    // If we're already on the lowest depth, try lower resolutions
                    if (nDepth == 8)
                    {
                        if (nHeight == 768)
                            nWidth = 800, nHeight = 600;
                        else if (nHeight == 600)
                            nWidth = 640, nHeight = 480;
                        else if (nHeight == 480)
                        {
                            Message(msgError, "SetDisplayMode() failed with ALL modes! (%#08lx)\n", hr);
                            return false;
                        }
                    }

                    // Fall back to a lower depth
                    else if (nDepth == 24)
                        nDepth = 16;
                    else
                        nDepth >>= 1;
                }
            }

            // Remember the depth we're using, just in case it changed
            SetOption(depth, nDepth);

            // Set up what we need for the primary surface
            DDSURFACEDESC ddsd = { sizeof ddsd };
            ddsd.dwFlags = DDSD_CAPS;
            ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

            // Create the primary surface
            if (!(pddsPrimary = CreateSurface(DDSCAPS_PRIMARYSURFACE)))
            {
                // Only report this failure on startup
                if (fFirstInit_)
                    Message(msgError, "Failed to create primary surface!", hr);
            }

            // If we're running in a window we need a clipper to keep the image within the window
            else if (!GetOption(fullscreen) && FAILED(hr = pdd->CreateClipper(0, &pddClipper, NULL)))
                Message(msgError, "CreateClipper() failed with %#08lx", hr);
            else
            {
                // If we're running windowed we need to use a clipper to keep our drawing within the client area of the window
                if (!GetOption(fullscreen))
                {
                    // create a clipper for the primary surface
                    if (FAILED(hr = pddClipper->SetHWnd(0, g_hwnd)))
                        Message(msgError, "Clipper SetHWnd() failed with %#08lx", hr);
                    else if (FAILED(hr = pddsPrimary->SetClipper(pddClipper)))
                        Message(msgError, "SetClipper() failed with %#08lx", hr);
                }

                // Get the dimensions needed by the back buffer
                dwWidth = Frame::GetWidth();
                dwHeight = Frame::GetHeight();

                // Are we to use a video overlay?
                DDPIXELFORMAT ddpf;
                if (GetOption(surface) >= 3)
                {
                    // Create the overlay, but falling back to a Video surface if we can't
                    if (pddsFront = CreateOverlay(dwWidth, dwHeight, &ddpf))
                    {
                        // Is the overlay surface lockable?
                        if (SUCCEEDED(hr = pddsFront->Lock(NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR|DDLOCK_WAIT, NULL)))
                        {
                            // If so, we'll use it directly for the back buffer
                            pddsFront->Unlock(ddsd.lpSurface);
                            swap(pddsBack, pddsFront);
                            TRACE("Using lockable overlay surface directly\n");
                        }

                        // Create a new back buffer with the same pixel format as the front buffer
                        else if (!(pddsBack = CreateSurface(0, dwWidth, dwHeight, &ddpf)))
                        {
                            // Free the overlay as we can't seem to do anything with it
                            pddsFront->Release();
                            pddsFront = NULL;
                        }
                    }

                    // If we failed to use the overlay, fall back to trying a video surface
                    if (!pddsBack)
                        SetOption(surface, 2);
                }

                // Set up the required capabilities for the back buffer
                DWORD dwCaps = (GetOption(surface) < 2) ? DDSCAPS_SYSTEMMEMORY : 0;
                DWORD dwRequiredFX = (DDFXCAPS_BLTSTRETCHX | DDFXCAPS_BLTSTRETCHY);

                // Create the back buffer.  If we're using an overlay, try for the same pixel format
                if (!pddsBack && !(pddsBack = CreateSurface(dwCaps, dwWidth, dwHeight, NULL, dwRequiredFX)))
                    Message(msgError, "Failed to create back buffer (%#08lx)", hr);
                else
                {
                    // If we tried for a video memory backbuffer but didn't manage it, update the video option to show that
                    pddsBack->GetSurfaceDesc(&ddsd);
                    TRACE("Back buffer is in %s memory\n", (ddsd.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) ? "video" : "system");
                    if (GetOption(surface) == 2 && !(ddsd.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY))
                        SetOption(surface, 1);

                    // Create the SAM and DirectX palettes
                    if (CreatePalettes())
                    {
                        ClearSurface(pddsBack);
                        UpdatePalette();
                        UI::ResizeWindow();
                        fRet = true;
                    }
                }
            }
        }
    }

    TRACE("<- Video::Init() returning %s\n", fRet ? "true" : "false");
    return fRet;
}

// Cleanup DirectX by releasing all the interfaces we have
void Video::Exit (bool fReInit_/*=false*/)
{
    TRACE("-> Video::Exit(%s)\n", fReInit_ ? "reinit" : "");

    if (pddPal) { TRACE("Releasing palette\n"); pddPal->Release(); pddPal = NULL; }
    if (pddClipper) { TRACE("Releasing clipper\n"); pddClipper->Release(); pddClipper = NULL; }

    if (pddsFront) { TRACE("Releasing front buffer\n"); pddsFront->Release(); pddsFront = NULL; }
    if (pddsBack) { TRACE("Releasing back buffer\n"); pddsBack->Release(); pddsBack = NULL; }
    if (pddsPrimary) { TRACE("Releasing primary buffer\n"); pddsPrimary->Release(); pddsPrimary = NULL; }

    if (pdd)
    {
        // Should be done automatically, but let's do it just in case
        pdd->RestoreDisplayMode();
        pdd->SetCooperativeLevel(g_hwnd, DDSCL_NORMAL);

        TRACE("Releasing DD\n"); pdd->Release();
        pdd = NULL;
    }

    TRACE("<- Video::Exit()\n");
}


HRESULT Video::ClearSurface (LPDIRECTDRAWSURFACE pdds_)
{
    HRESULT hr;

    // Get details on the surface to clear
    DDSURFACEDESC ddsd = { sizeof ddsd };
    pdds_->GetSurfaceDesc(&ddsd);

    // Try and clear it with a simple blit, using the appropriate black colour
    DDBLTFX bltfx = { sizeof bltfx };
    bltfx.dwFillColor = (ddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC) ? (((awV[0] | awY[0]) << 16) | awU[0] | awY[0]) : 0;

    // Hopefully this will work ok!
    if (FAILED(hr = pdds_->Blt(NULL, NULL, NULL, DDBLT_COLORFILL|DDBLT_WAIT, &bltfx)))
    {
        // Bah, that failed so we'll have to do it the hard way!
        if (SUCCEEDED(hr = pdds_->Lock(NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR|DDLOCK_WAIT, NULL)))
        {
            // Get the surface pointer, and convert width to pairs of (WORD-sized) pixels and pitch to DWORDs
            DWORD* pdw = reinterpret_cast<DWORD*>(ddsd.lpSurface);
            ddsd.dwWidth >>= 1;
            ddsd.lPitch >>= 2;

            // Loop through each surface line
            for (int i = 0 ; i < (int)ddsd.dwHeight; i++, pdw += ddsd.lPitch)
            {
                // Fill the line with the required colour
                for (int j = 0 ; j < (int)ddsd.dwWidth ; j++)
                    pdw[j] = bltfx.dwFillColor;
            }

            pdds_->Unlock(ddsd.lpSurface);
        }
    }

    return hr;
}

// Get an appropriate colour key value to display the overlay surface
DWORD Video::GetOverlayColourKey ()
{
    DWORD dwColourKey = 0;
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

    return dwColourKey;
}


LPDIRECTDRAWSURFACE CreateOverlay (DWORD dwWidth_, DWORD dwHeight_, LPDDPIXELFORMAT pddpf_/*=NULL*/)
{
    static const DDPIXELFORMAT addpf[] =
    {
        { sizeof DDPIXELFORMAT, DDPF_RGB, 0, 16, 0xf800, 0x07e0, 0x001f, 0 },           // 5-6-5 RGB
        { sizeof DDPIXELFORMAT, DDPF_RGB, 0, 16, 0x7c00, 0x03e0, 0x001f, 0 },           // 5-5-5 RGB
        { sizeof DDPIXELFORMAT, DDPF_FOURCC, MAKEFOURCC('U','Y','V','Y'), 0,0,0,0,0 },
        { sizeof DDPIXELFORMAT, DDPF_FOURCC, MAKEFOURCC('Y','U','Y','2'), 0,0,0,0,0 },
    };

    LPDIRECTDRAWSURFACE pdds = NULL;


    // If an overlay is requested, make sure the hardware supports them first
    if (!(~ddcaps.dwCaps & (DDCAPS_OVERLAY | DDCAPS_OVERLAYSTRETCH)))
    {
        DDSURFACEDESC ddsd = { sizeof ddsd };
        ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
        ddsd.ddsCaps.dwCaps = DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
        ddsd.dwWidth = dwWidth_;
        ddsd.dwHeight = dwHeight_;

        // There's no reliable way to get the supported formats so we just have to try them until one works!
        for (int i = (GetOption(surface) == 3) << 1 ; i < (sizeof addpf / sizeof addpf[0]) ; i++)
        {
            // Set the next pixel format to try
            ddsd.ddpfPixelFormat = addpf[i];

            // Make sure we can create and lock the surface before accepting it (some cards don't allow it)
            if (FAILED(hr = pdd->CreateSurface(&ddsd, &pdds, NULL)))
                TRACE("Overlay CreateSurface() failed with %#08lx\n", hr);
            else
            {
                // If the caller wants the pixel format, make a copy
                if (pddpf_)
                    *pddpf_ = addpf[i];

                // Update the options to specify whether we're using an RGB or YUV overlay
                SetOption(surface, (addpf[i].dwFlags & DDPF_RGB) ? 4 : 3);
                break;
            }
        }
    }

    return pdds;
}

LPDIRECTDRAWSURFACE CreateSurface (DWORD dwCaps_, DWORD dwWidth_/*=0*/, DWORD dwHeight_/*=0*/,
    LPDDPIXELFORMAT pddpf_/*=NULL*/, DWORD dwRequiredCaps_/*=0*/)
{
    LPDIRECTDRAWSURFACE pdds = NULL;

    DDSURFACEDESC ddsd = { sizeof ddsd };
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = dwCaps_;
    ddsd.dwWidth = dwWidth_;
    ddsd.dwHeight = dwHeight_;

    // Primary surfaces are a special case and don't use the extra attributes
    if (!(dwCaps_ & DDSCAPS_PRIMARYSURFACE))
    {
        // Use the supplied width and height
        ddsd.dwFlags |= DDSD_WIDTH | DDSD_HEIGHT;

        // Force a system surface if the hardware doesn't support stretching, as the emulated Blt VRAM reads are VERY slow
        if ((~ddcaps.dwFXCaps & dwRequiredCaps_) != 0)
            ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;

        // Use any supplied pixel format
        if (pddpf_)
        {
            ddsd.dwFlags |= DDSD_PIXELFORMAT;
            ddsd.ddpfPixelFormat = *pddpf_;
        }
    }

    HRESULT hr;
    if (FAILED(hr = pdd->CreateSurface(&ddsd, &pdds, NULL)))
        TRACE("!!! Failed to create surface (%#08lx)\n", hr);

    // Make sure the surface is lockable, as some VRAM surfaces may not be
    else if (SUCCEEDED(hr = pdds->Lock(NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR|DDLOCK_WAIT, NULL)))
        pdds->Unlock(ddsd.lpSurface);

    // If we've not just tried a system surface, try one now
    else if (!(ddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY))
    {
        // Release the unlockable surface as it's of no use to us
        pdds->Release();

        // Force into system memory and try again
        ddsd.ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY;
        ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;

        if (FAILED(hr = pdd->CreateSurface(&ddsd, &pdds, NULL)))
            TRACE("!!! Failed to create forced system surface (%#08lx)\n", hr);
    }

    return pdds;
}


bool Video::CreatePalettes (bool fDimmed_/*=false*/)
{
    // Don't attempt anything without a surface pointer
    if (!pddsFront && !pddsBack)
        return false;

    // Whether the display is dimmed depends on a number of things
    fDimmed_ |= (g_fPaused && !g_fFrameStep) || GUI::IsActive() || (!g_fActive && GetOption(pauseinactive));

    // Ok, let's look at what the target requirements are, as it determines the format we draw in
    DDSURFACEDESC ddsd = { sizeof ddsd };
    (pddsFront ? pddsFront : pddsBack)->GetSurfaceDesc(&ddsd);

    bool fYUV = (ddsd.ddpfPixelFormat.dwFlags & DDPF_FOURCC) != 0;
    bool fPalette = (ddsd.ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8) != 0;
    UINT uBPP = ddsd.ddpfPixelFormat.dwRGBBitCount;

    // Get the current Windows palette, so we can preserve the first and last 10 entries, which are used for
    // standard Windows UI components like menus and dialogue boxes, otherwise it looks crap!
    PALETTEENTRY pal[256];
    if (fPalette)
    {
        HDC hdc = GetDC(NULL);
        GetSystemPaletteEntries(hdc, 0, 256, pal);
        ReleaseDC(NULL, hdc);
    }

    const RGBA *pSAM = IO::GetPalette(fDimmed_), *pGUI = GUI::GetPalette();

    // Build the full palette from SAM and GUI colours
    for (int i = 0; i < N_TOTAL_COLOURS ; i++)
    {
        // Look up the colour in the appropriate palette
        const RGBA* p = (i < N_PALETTE_COLOURS) ? &pSAM[i] : &pGUI[i-N_PALETTE_COLOURS];
        BYTE bRed = p->bRed, bGreen = p->bGreen, bBlue = p->bBlue;

        // If colour is disabled, convert to the appropriate shade of grey
        if (0)
        {
            BYTE bGrey = static_cast<BYTE>(0.30 * bRed + 0.59 * bGreen + 0.11 * bBlue);
            bRed = bGreen = bBlue = bGrey;
        }

        // Using YUV on an overlay?
        if (fYUV)
        {
            // RGB to YUV
            BYTE bY = static_cast<BYTE>(bRed *  0.299 + bGreen *  0.587 + bBlue *  0.114);
            BYTE bU = static_cast<BYTE>(bRed * -0.169 + bGreen * -0.332 + bBlue *  0.500  + 128.0);
            BYTE bV = static_cast<BYTE>(bRed *  0.500 + bGreen * -0.419 + bBlue * -0.0813 + 128.0);
/*
            // YUV to RGB  (we don't actually need em here, but they're handy!)
            BYTE bR = BY + (1.4075 * (BV - 128));
            BYTE bG = BY - (0.3455 * (BU - 128) - (0.7169 * (BV - 128)));
            BYTE bB = BY + (1.7790 * (BU - 128));
*/
            // Pre-shift the YUV data for the two formats we currently support
            if (ddsd.ddpfPixelFormat.dwFourCC == MAKEFOURCC('Y','U','Y','2'))
            {
                awY[i] = bY;
                awU[i] = WORD(bU) << 8;
                awV[i] = WORD(bV) << 8;
            }
            else if (ddsd.ddpfPixelFormat.dwFourCC == MAKEFOURCC('U','Y','V','Y'))
            {
                awY[i] = WORD(bY) << 8;
                awU[i] = bU;
                awV[i] = bV;
            }
            else
            {
                TRACE("Unknown YUV FOURCC: %#08lx\n", ddsd.ddpfPixelFormat.dwFourCC);
                DebugBreak();
            }

            aulPalette[i] = (static_cast<DWORD>(bBlue) << 16) | (static_cast<DWORD>(bGreen) << 8) | bRed;
        }

        // In 8 bit mode use offset palette positions to allow for system colours in the first 10
        else if (fPalette)
        {
            PALETTEENTRY pe = { bRed, bGreen, bBlue, PC_NOCOLLAPSE };

            // Leave the first PALETTE_OFFSET entries for Windows GUI colours
            pal[PALETTE_OFFSET+i] = pe;
            aulPalette[i] = PALETTE_OFFSET+i;
        }

        // Other modes build up the require pixel format from the surface information
        else
        {
            DWORD dwRMask = ddsd.ddpfPixelFormat.dwRBitMask;
            DWORD dwGMask = ddsd.ddpfPixelFormat.dwGBitMask;
            DWORD dwBMask = ddsd.ddpfPixelFormat.dwBBitMask;

            DWORD dwRed   = static_cast<DWORD>(((static_cast<__int64>(dwRMask) * (bRed+1))   >> 8) & dwRMask);
            DWORD dwGreen = static_cast<DWORD>(((static_cast<__int64>(dwGMask) * (bGreen+1)) >> 8) & dwGMask);
            DWORD dwBlue  = static_cast<DWORD>(((static_cast<__int64>(dwBMask) * (bBlue+1))  >> 8) & dwBMask);

            aulPalette[i] = dwRed | dwGreen | dwBlue;
        }
    }

    // Free any existing DirectX palette
    if (pddPal) { pddPal->Release(); pddPal = NULL; }

    // In non-palettised modes the screen needs to be redrawn to reflect the changes
    if (!fPalette)
        Display::SetDirty();
    else
    {
        // Create and activate the palette
        if (FAILED(pdd->CreatePalette(DDPCAPS_8BIT, pal, &pddPal, NULL)))
            Message(msgError, "CreatePalette() failed with %#08lx", hr);
        else if (FAILED(pddsPrimary->SetPalette(pddPal)))
            Message(msgError, "SetPalette() failed with %#08lx", hr);
    }

    // Because the pixel format may have changed, we need to refresh the SAM CLUT pixel values
    for (int c = 0 ; c < 16 ; c++)
        clut[c] = aulPalette[clutval[c]];

    return true;
}

void Video::UpdatePalette ()
{
    if (pddPal)
    {
        TRACE("Updating palette\n");
        pddsPrimary->SetPalette(pddPal);
    }
}
