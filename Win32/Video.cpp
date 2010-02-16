// Part of SimCoupe - A SAM Coupe emulator
//
// Video.cpp: Win32 core video functionality using DirectDraw
//
//  Copyright (c) 1999-2010  Simon Owen
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

#include "Action.h"
#include "Frame.h"
#include "Display.h"
#include "GUI.h"
#include "IO.h"
#include "Options.h"
#include "UI.h"
#include "Util.h"
#include "Video.h"


const int N_TOTAL_COLOURS = N_PALETTE_COLOURS+N_GUI_COLOURS;

// SAM RGB values in native surface format
DWORD aulPalette[N_TOTAL_COLOURS], aulScanline[N_TOTAL_COLOURS];    // normal and scanline palettes

// DirectDraw back and front surfaces
LPDIRECTDRAWSURFACE pddsPrimary, pddsFront, pddsBack;

HINSTANCE hinstDDraw;
LPDIRECTDRAW pdd;
LPDIRECTDRAWPALETTE pddPal;
LPDIRECTDRAWCLIPPER pddClipper;
DDCAPS ddcaps;

HRESULT hr;

HRESULT ClearSurface (LPDIRECTDRAWSURFACE pdds_);
LPDIRECTDRAWSURFACE CreateSurface (DWORD dwCaps_, DWORD dwWidth_=0, DWORD dwHeight_=0, LPDDPIXELFORMAT pddpf_=NULL, DWORD dwRequiredCaps_=0);


// function to initialize DirectDraw in windowed mode
bool Video::Init (bool fFirstInit_)
{
    bool fRet = false;

    Exit(true);
    TRACE("-> Video::Init(%s)\n", fFirstInit_ ? "first" : "");

    // Create the main DirectDraw object, reversing the acceleration option if the first attempt failed
    HRESULT hr = pfnDirectDrawCreate(GetOption(hwaccel) ? NULL : (LPGUID)DDCREATE_EMULATIONONLY, &pdd, NULL);
    if (FAILED(hr))
        hr = pfnDirectDrawCreate(!GetOption(hwaccel) ? NULL : (LPGUID)DDCREATE_EMULATIONONLY, &pdd, NULL);

    if (FAILED(hr))
        Message(msgError, "DirectDrawCreate() failed with %#08lx", hr);
    else
    {
        // Get the driver capabilites so we know what we need to set up
        ddcaps.dwSize = sizeof(ddcaps);
        pdd->GetCaps(&ddcaps, NULL);

        // Use exclusive mode for full-screen, or normal mode for windowed
        hr = pdd->SetCooperativeLevel(g_hwnd, GetOption(fullscreen) ? DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN|DDSCL_ALLOWREBOOT : DDSCL_NORMAL);
        if (FAILED(hr))
            Message(msgError, "SetCooperativeLevel() failed with %#08lx", hr);
        else
        {
            DDSURFACEDESC ddsd = { sizeof(ddsd) };

            // Full screen mode requires a display mode change
            if (GetOption(fullscreen))
            {
                // Set up safe fullscreen defaults that will fit the largest SAM view (768x624)
                int nWidth = 1024, nHeight = 768, nDepth = 32;

                // Fetch the windowed mode details to use for fullscreen
                if (SUCCEEDED(pdd->GetDisplayMode(&ddsd)) && !(~ddsd.dwFlags & (DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT)))
                {
                    nWidth = ddsd.dwWidth;
                    nHeight = ddsd.dwHeight;
                    nDepth = ddsd.ddpfPixelFormat.dwRGBBitCount;
                }

                // Loop while we can't select the mode we want
                if (FAILED(hr = pdd->SetDisplayMode(nWidth, nHeight, nDepth)))
                {
                    TRACE("!!! Failed to set %dx%dx%d mode! (%#08lx)\n", nWidth, nHeight, nDepth, hr);
                    return false;
                }
            }

            // Set up what we need for the primary surface
            memset(&ddsd, 0, sizeof(ddsd));
            ddsd.dwFlags = DDSD_CAPS;
            ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

            // Create the primary surface
            if (!(pddsPrimary = CreateSurface(DDSCAPS_PRIMARYSURFACE)))
            {
                // Only report this failure on startup
                if (fFirstInit_)
                    Message(msgError, "Failed to create primary surface!", hr);
            }

            // Use a clipper to keep the emulator image within the window area
            else if (FAILED(hr = pdd->CreateClipper(0, &pddClipper, NULL)))
                Message(msgError, "CreateClipper() failed with %#08lx", hr);
            else if (FAILED(hr = pddClipper->SetHWnd(0, g_hwnd)))
                Message(msgError, "Clipper SetHWnd() failed with %#08lx", hr);
            else if (FAILED(hr = pddsPrimary->SetClipper(pddClipper)))
                Message(msgError, "SetClipper() failed with %#08lx", hr);
            else
            {
                // Get the dimensions needed by the back buffer
                DWORD dwWidth = Frame::GetWidth();
                DWORD dwHeight = Frame::GetHeight();

                // Set up the required capabilities for the back buffer
                DWORD dwRequiredFX = (DDFXCAPS_BLTSTRETCHX | DDFXCAPS_BLTSTRETCHY);

                // Create the back buffer
                if (!pddsBack && !(pddsBack = CreateSurface(0, dwWidth, dwHeight, NULL, dwRequiredFX)))
                    Message(msgError, "Failed to create back buffer (%#08lx)", hr);
                else
                {
                    // If we tried for a video memory backbuffer but didn't manage it, update the video option to show that
                    pddsBack->GetSurfaceDesc(&ddsd);
                    TRACE("Back buffer is in %s memory\n", (ddsd.ddsCaps.dwCaps & DDSCAPS_VIDEOMEMORY) ? "video" : "system");

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


HRESULT ClearSurface (LPDIRECTDRAWSURFACE pdds_)
{
    // Black fill colour
    DDBLTFX bltfx = { sizeof(bltfx) };
    bltfx.dwFillColor = 0;

    // Fill the surface to clear it
    return pdds_->Blt(NULL, NULL, NULL, DDBLT_COLORFILL|DDBLT_WAIT, &bltfx);
}

LPDIRECTDRAWSURFACE CreateSurface (DWORD dwCaps_, DWORD dwWidth_/*=0*/, DWORD dwHeight_/*=0*/,
    LPDDPIXELFORMAT pddpf_/*=NULL*/, DWORD dwRequiredCaps_/*=0*/)
{
    LPDIRECTDRAWSURFACE pdds = NULL;

    DDSURFACEDESC ddsd = { sizeof(ddsd) };
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

    // Only back buffers need to be checked for lockability
    if (!(dwCaps_ & DDSCAPS_PRIMARYSURFACE))
    {
        // Make sure the surface is lockable, as some VRAM surfaces may not be
        if (SUCCEEDED(hr = pdds->Lock(NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR|DDLOCK_WRITEONLY|DDLOCK_WAIT, NULL)))
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
    DDSURFACEDESC ddsd = { sizeof(ddsd) };
    (pddsFront ? pddsFront : pddsBack)->GetSurfaceDesc(&ddsd);
    bool fPalette = (ddsd.ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8) != 0;

    // Get the current Windows palette, so we can preserve the first and last 10 entries, which are used for
    // standard Windows UI components like menus and dialogue boxes, otherwise it looks crap!
    PALETTEENTRY pal[256];
    if (fPalette)
    {
        HDC hdc = GetDC(NULL);
        GetSystemPaletteEntries(hdc, 0, 256, pal);
        ReleaseDC(NULL, hdc);
    }

    // Determine the scanline brightness level adjustment, in the range -100 to +100
    int nScanAdjust = GetOption(scanlevel) - 100;
    if (nScanAdjust < -100) nScanAdjust = -100;

    const RGBA *pSAM = IO::GetPalette(fDimmed_), *pGUI = GUI::GetPalette();

    // Build the full palette from SAM and GUI colours
    for (int i = 0; i < N_TOTAL_COLOURS ; i++)
    {
        // Look up the colour in the appropriate palette
        const RGBA* p = (i < N_PALETTE_COLOURS) ? &pSAM[i] : &pGUI[i-N_PALETTE_COLOURS];
        BYTE r = p->bRed, g = p->bGreen, b = p->bBlue;

        // In 8 bit mode use offset palette positions to allow for system colours in the first 10
        if (fPalette)
        {
            PALETTEENTRY pe = { r,g,b, PC_NOCOLLAPSE };

            // Leave the first PALETTE_OFFSET entries for Windows GUI colours
            // There aren't enough palette entries for scanlines, so use the same colour
            pal[PALETTE_OFFSET+i] = pe;
            aulPalette[i] = aulScanline[i] = PALETTE_OFFSET+i;
        }

        // Other modes build up the require pixel format from the surface information
        else
        {
            DDPIXELFORMAT *ddpf = &ddsd.ddpfPixelFormat;

            // Set regular pixel
            aulPalette[i] = RGB2Native(r,g,b, ddpf->dwRBitMask, ddpf->dwGBitMask, ddpf->dwBBitMask);

            // Set scanline pixel
            AdjustBrightness(r,g,b, nScanAdjust);
            aulScanline[i] = RGB2Native(r,g,b, ddpf->dwRBitMask, ddpf->dwGBitMask, ddpf->dwBBitMask);
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
        if (FAILED(hr = pdd->CreatePalette(DDPCAPS_8BIT, pal, &pddPal, NULL)))
            Message(msgError, "CreatePalette() failed with %#08lx", hr);
        else
            pddsPrimary->SetPalette(pddPal);    // ignore any error as there's nothing we can do
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
