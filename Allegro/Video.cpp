// Part of SimCoupe - A SAM Coupe emulator
//
// Video.cpp: Allegro video handling for surfaces, screen modes, palettes etc.
//
//  Copyright (c) 1999-2005  Simon Owen
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
#include "Video.h"

#include "Action.h"
#include "Display.h"
#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "UI.h"

#ifdef ALLEGRO_DOS

BEGIN_GFX_DRIVER_LIST
    GFX_DRIVER_VBEAF
    GFX_DRIVER_VESA3
    GFX_DRIVER_VESA2L
    GFX_DRIVER_VESA2B
    GFX_DRIVER_VESA1
    GFX_DRIVER_VGA
END_GFX_DRIVER_LIST

BEGIN_COLOR_DEPTH_LIST
  COLOR_DEPTH_8
END_COLOR_DEPTH_LIST

#endif // ALLEGRO_DOS


const int N_TOTAL_COLOURS = N_PALETTE_COLOURS + N_GUI_COLOURS;

// SAM RGB values in the appropriate format
DWORD aulPalette[N_TOTAL_COLOURS];

BITMAP *pBack, *pFront;


// function to initialize DirectDraw in windowed mode
bool Video::Init (bool fFirstInit_/*=false*/)
{
    bool fRet = false;

    Exit(true);
    TRACE("-> Video::Init(%s)\n", fFirstInit_ ? "first" : "");

#ifdef ALLEGRO_DOS
    // Limit DOS to 8-bit full-screen, disable the stretching features, and force scanlines (for now)
    SetOption(depth, 8);
    SetOption(fullscreen, true);
    SetOption(ratio5_4, false);
    SetOption(scanlines, true);
#endif

    DWORD dwWidth = Frame::GetWidth(), dwHeight = Frame::GetHeight();

    // In 5:4 mode we'll stretch the viewing surfaces by 25%
    if (GetOption(ratio5_4))
        (dwWidth *= 5) >>= 2;

    int nDepth = GetOption(fullscreen) ? GetOption(depth) : desktop_color_depth();
    set_color_depth(nDepth);

    if (!GetOption(fullscreen))
        set_gfx_mode(GFX_AUTODETECT_WINDOWED, dwWidth, dwHeight, 0, 0);
    else
    {
        int nWidth, nHeight;

        // Work out the best-fit mode for the visible frame area
        if (dwWidth <= 640 && dwHeight <= 480)
            nWidth = 640, nHeight = 480;
        else if (dwWidth <= 800 && dwHeight <= 600)
            nWidth = 800, nHeight = 600;
        else
            nWidth = 1024, nHeight = 768;

        // Set the video mode, or keep reducing the requirements until we find one
        while (set_gfx_mode(GFX_AUTODETECT_FULLSCREEN, nWidth, nHeight, 0, 0) < 0)
        {
            TRACE("!!! Failed to set %dx%dx%d mode: %s\n", nWidth, nHeight, nDepth, allegro_error);

            // If we're already on the lowest depth, try lower resolutions
            if (nDepth == 8)
            {
                if (nHeight == 768)
                    nWidth = 800, nHeight = 600;
                else if (nHeight == 600)
                    nWidth = 640, nHeight = 480;
                else if (nHeight == 480)
                {
                    TRACE("set_gfx_mode() failed with ALL modes!\n");
                    return false;
                }
            }

            // Fall back to a lower depth
            else if (nDepth == 24)
                nDepth = 16;
            else
                nDepth >>= 1;

            // Update the depth
            set_color_depth(nDepth);
            SetOption(depth, nDepth);
        }
    }

    TRACE("GFX capabilities = %#x\n", gfx_capabilities);

    pBack = create_system_bitmap(dwWidth, dwHeight);
    pFront = screen;

    // Did we fail to create the front buffer?
    if (!pBack)
        TRACE("Can't create back buffer: %s\n", allegro_error);
    else
    {
        // Clear out any garbage from the back surface
        clear_to_color(pBack, 0);

        // Create the appropriate palette needed for the surface (including a hardware palette for 8-bit mode)
        CreatePalettes();

        fRet = UI::Init(fFirstInit_);
    }

    if (!fRet)
        Exit();

    TRACE("<- Video::Init() returning %s\n", fRet ? "true" : "FALSE");
    return fRet;
}

// Cleanup DirectX by releasing all the interfaces we have
void Video::Exit (bool fReInit_/*=false*/)
{
    TRACE("-> Video::Exit(%s)\n", fReInit_ ? "reinit" : "");

    if (pBack) { release_bitmap(pBack); pBack = NULL; }

    if (pFront)
    {
        if (!is_screen_bitmap(pFront))
            release_bitmap(pFront);
        pFront = NULL;
    }

    if (!fReInit_)
        allegro_exit();

    TRACE("<- Video::Exit()\n");
}


// Create whatever's needed for actually displaying the SAM image
bool Video::CreatePalettes (bool fDimmed_)
{
    PALETTE pal;

    int nDepth = bitmap_color_depth(pBack);
    bool fPalette = (nDepth == 8);

    fDimmed_ |= (g_fPaused && !g_fFrameStep) || GUI::IsActive() || (!g_fActive && GetOption(pauseinactive));
    const RGBA *pSAM = IO::GetPalette(fDimmed_), *pGUI = GUI::GetPalette();

    // Build the full palette from SAM and GUI colours
    for (int i = 0; i < N_TOTAL_COLOURS ; i++)
    {
        // Look up the colour in the appropriate palette
        const RGBA* p = (i < N_PALETTE_COLOURS) ? &pSAM[i] : &pGUI[i-N_PALETTE_COLOURS];
        BYTE bRed = p->bRed, bGreen = p->bGreen, bBlue = p->bBlue;  //, bAlpha = p->bAlpha;

        if (!fPalette)
            aulPalette[i] = makecol_depth(nDepth, bRed, bGreen, bBlue);
        else
        {
            aulPalette[i] = i;

            // The palette components are 6-bit
            pal[i].r = bRed >> 2;
            pal[i].g = bGreen >> 2;
            pal[i].b = bBlue >> 2;
        }
    }

    // If a palette is required, set it now
    if (fPalette)
        set_palette_range(pal, 0, N_TOTAL_COLOURS-1, 1);

    // Because the pixel format may have changed, we need to refresh the SAM CLUT pixel values
    for (int c = 0 ; c < 16 ; c++)
        clut[c] = aulPalette[clutval[c]];

    // Ensure the display is redrawn to reflect the changes
    Display::SetDirty();

    return true;
}
