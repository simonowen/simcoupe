// Part of SimCoupe - A SAM Coupé emulator
//
// Video.cpp: SDL video handling for surfaces, screen modes, palettes etc.
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

#include "SimCoupe.h"
#include "Video.h"

#include "Frame.h"
#include "Display.h"
#include "Options.h"
#include "UI.h"
#include "Util.h"


DWORD aulPalette[N_PALETTE_COLOURS];                                            // SAM palette values in appropriate format for video surface
WORD awY[N_PALETTE_COLOURS], awU[N_PALETTE_COLOURS], awV[N_PALETTE_COLOURS];    // YUV values pre-shifted for overlay surface

SDL_Surface *pBack, *pFront, *pIcon;


namespace Video
{
// Mask for the current SimCoupe.bmp image used as the SDL program icon
static BYTE abIconMask[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1f, 0xe0, 0x00,
    0x00, 0x3f, 0xf8, 0x00, 0x00, 0x7f, 0xfc, 0x00, 0x00, 0xff, 0xfe, 0x00, 0x01, 0xff, 0xff, 0x00,
    0x01, 0xff, 0xff, 0x00, 0x03, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0x80,
    0x03, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0x80, 0x08, 0x1f, 0xf0, 0x20, 0x1c, 0x7f, 0xfc, 0x70,
    0x3c, 0xff, 0xfe, 0x78, 0x7f, 0xff, 0xff, 0xfc, 0x7f, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0xf8,
    0x03, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0x80,
    0x03, 0xff, 0xff, 0x80, 0x00, 0xfc, 0x7e, 0x00, 0x01, 0xfe, 0xff, 0x00, 0x01, 0xfe, 0xff, 0x00,
    0x01, 0xfe, 0xff, 0x00, 0x01, 0xfe, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


// function to initialize DirectDraw in windowed mode
bool Init (bool fFirstInit_/*=false*/)
{
    bool fRet = false;

    // The lack of stretching support means the SDL version currently lacks certain features, so force the options for now
    SetOption(scanlines, true);


    Exit(true);

    TRACE("-> Video::Init(%s)\n", fFirstInit_ ? "first" : "");

    if (fFirstInit_ && SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
        TRACE("SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s: %s\n", SDL_GetError());
    else
    {
        SDL_WM_SetIcon(pIcon = SDL_LoadBMP(OSD::GetFilePath("SimCoupe.bmp")), abIconMask);

        CScreen* pScreen = Frame::GetScreen();
        DWORD dwWidth = pScreen->GetPitch(), dwHeight = pScreen->GetHeight() << 1;

        // In 5:4 mode we'll stretch the viewing surfaces by 25%
        if (GetOption(ratio5_4))
            (dwWidth *= 5) >>= 2;

        int nDepth = GetOption(fullscreen) ? GetOption(depth) : SDL_GetVideoInfo()->vfmt->BitsPerPixel;

        // Should the surfaces be in video memory?  (they need to be for any hardware acceleration)
        DWORD dwOptions = (GetOption(surface) >= 2) ? SDL_HWSURFACE : 0;

        // Full screen mode requires a display mode change
        if (!GetOption(fullscreen))
            pFront = SDL_SetVideoMode(dwWidth, dwHeight, nDepth, dwOptions | ((nDepth == 8) ? SDL_HWPALETTE : 0));
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
            while (!(pFront = SDL_SetVideoMode(nWidth, nHeight, nDepth, SDL_FULLSCREEN | dwOptions | ((nDepth == 8) ? SDL_HWPALETTE : 0))))
            {
                TRACE("!!! Failed to set %dx%dx%d mode: %s\n", nWidth, nHeight, nDepth, SDL_GetError());
                if (nHeight == 768)
                    nWidth = 800, nHeight = 600;
                else if (nHeight == 600)
                    nWidth = 640, nHeight = 480;
                else if (nHeight == 480 && nDepth != 8)
                    nWidth = 1024, nHeight = 768, nDepth >>= 1;
                else
                    break;
            }

            // Update the depth option, just in case it was changed above
            SetOption(depth, nDepth);
        }

        // Did we fail to create the front buffer?
        if (!pFront)
            TRACE("Failed to create front buffer!\n");

        // Create a 
        else if (!(pBack = SDL_CreateRGBSurface(dwOptions, dwWidth, dwHeight, pFront->format->BitsPerPixel,
                pFront->format->Rmask, pFront->format->Gmask, pFront->format->Bmask, pFront->format->Amask)))
            TRACE("Can't create back buffer: %s\n", SDL_GetError());
        else
        {
            // Clear out any garbage from the back surface
            SDL_FillRect(pBack, NULL, 0);

            // Create the appropriate palette needed for the surface (including a hardware palette for 8-bit mode)
            CreatePalettes();

            fRet = UI::Init(fFirstInit_);
        }
    }

    if (!fRet)
        Exit();

    TRACE("<- Video::Init() returning %s\n", fRet ? "true" : "FALSE");
    return fRet;
}

// Cleanup DirectX by releasing all the interfaces we have
void Exit (bool fReInit_/*=false*/)
{
    TRACE("-> Video::Exit(%s)\n", fReInit_ ? "reinit" : "");

    if (pBack) { SDL_FreeSurface(pBack); pBack = NULL; }
    if (pFront) { SDL_FreeSurface(pFront); pFront = NULL; }
    if (pIcon) { SDL_FreeSurface(pIcon); pIcon = NULL; }

    if (!fReInit_)
        SDL_QuitSubSystem(SDL_INIT_VIDEO);

    TRACE("<- Video::Exit()\n");
}


// Create whatever's needed for actually displaying the SAM image
bool CreatePalettes (bool fDimmed_/*=false*/)
{
    bool fPalette = pBack->format->BitsPerPixel == 8;
    UINT uBPP = pBack->format->BitsPerPixel;

    // To handle any 16-bit pixel format we need to build some look-up tables
    WORD awRedTab[256], awGreenTab[256], awBlueTab[256];
    if (uBPP == 15 || uBPP == 16)
    {
        WORD wRMask = pBack->format->Rmask;
        WORD wGMask = pBack->format->Gmask;
        WORD wBMask = pBack->format->Bmask;

        // Create the lookup table
        for (DWORD dw = 0; dw < 256 ; dw++)
        {
            awRedTab[dw] = dw ? WORD(((DWORD(wRMask) * (dw+1)) >> 8) & wRMask) : 0;
            awGreenTab[dw] = dw ? WORD(((DWORD(wGMask * (dw+1))) >> 8) & wGMask) : 0;
            awBlueTab[dw] = dw ? WORD(((DWORD(wBMask) * (dw+1)) >> 8) & wBMask) : 0;
        }
    }

    static const BYTE ab[] = { 0x00, 0x3f, 0x5f, 0x7f, 0x9f, 0xbf, 0xdf, 0xff };

    SDL_Color acPalette[N_PALETTE_COLOURS];

    // Loop through SAM's full palette of 128 colours
    for (int i = 0; i < N_PALETTE_COLOURS ; i++)
    {
        // Convert from SAM palette position to 3-bit RGB
        BYTE bBlue  = ab[(i&0x01) << 1| ((i&0x10) >> 2) | ((i&0x08) >> 3)];
        BYTE bRed   = ab[(i&0x02)     | ((i&0x20) >> 3) | ((i&0x08) >> 3)];
        BYTE bGreen = ab[(i&0x04) >> 1| ((i&0x40) >> 4) | ((i&0x08) >> 3)];

        if (fDimmed_)
        {
            bBlue = (BYTE)(int(bBlue) * 2 / 3);
            bRed = (BYTE)(int(bRed) * 2 / 3);
            bGreen = (BYTE)(int(bGreen) * 2 / 3);
        }

        if (!fPalette)
            aulPalette[i] = SDL_MapRGB(pBack->format, bRed, bGreen, bBlue);
        else
        {
            aulPalette[i] = i+10;

            acPalette[i].r = bRed;
            acPalette[i].g = bGreen;
            acPalette[i].b = bBlue;
        }
    }

    if (fPalette)
        SDL_SetPalette(pBack, SDL_LOGPAL|SDL_PHYSPAL, acPalette, 10, N_PALETTE_COLOURS);

    // Because the pixel format may have changed, we need to refresh the SAM CLUT pixel values
    for (int c = 0 ; c < 16 ; c++)
        clut[c] = aulPalette[clutval[c]];

    return true;
}

}   // namespace Video
