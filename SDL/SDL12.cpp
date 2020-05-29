// Part of SimCoupe - A SAM Coupe emulator
//
// SDL12.cpp: Software surfaces for SDL 1.2
//
//  Copyright (c) 1999-2015 Simon Owen
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "SimCoupe.h"
#include "SDL12.h"

#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "UI.h"

#ifndef HAVE_LIBSDL2

#define FULLSCREEN_DEPTH    16

static uint32_t aulPalette[N_PALETTE_COLOURS];
static uint32_t aulScanline[N_PALETTE_COLOURS];


SDLSurface::SDLSurface()
{
    m_rTarget.x = m_rTarget.y = 0;
    m_rTarget.w = Frame::GetWidth();
    m_rTarget.h = Frame::GetHeight();
}

SDLSurface::~SDLSurface()
{
    if (pBack) SDL_FreeSurface(pBack), pBack = nullptr;
    if (pIcon) SDL_FreeSurface(pIcon), pIcon = nullptr;
}


int SDLSurface::GetCaps() const
{
    return 0;
}

bool SDLSurface::Init(bool fFirstInit_)
{
    TRACE("-> Video::Init(%s)\n", fFirstInit_ ? "first" : "");

    pIcon = SDL_LoadBMP(OSD::MakeFilePath(MFP_RESOURCE, "SimCoupe.bmp"));
    if (pIcon)
        SDL_WM_SetIcon(pIcon, nullptr);

    if (fFirstInit_)
    {
        const SDL_VideoInfo* pvi = SDL_GetVideoInfo();
        nDesktopWidth = pvi->current_w;
        nDesktopHeight = pvi->current_h;
        TRACE("Desktop resolution: %dx%d\n", nDesktopWidth, nDesktopHeight);
    }

    UpdateSize();
    return true;
}


void SDLSurface::Update(CScreen* pScreen_, bool* pafDirty_)
{
    // Draw any changed lines to the back buffer
    if (!DrawChanges(pScreen_, pafDirty_))
        return;
}

// Create whatever's needed for actually displaying the SAM image
void SDLSurface::UpdatePalette()
{
    // Determine the scanline brightness level adjustment, in the range -100 to +100
    int nScanAdjust = GetOption(scanlines) ? (GetOption(scanlevel) - 100) : 0;
    if (nScanAdjust < -100) nScanAdjust = -100;

    const COLOUR* pSAM = IO::GetPalette();

    // Build the full palette from SAM and GUI colours
    for (int i = 0; i < N_PALETTE_COLOURS; i++)
    {
        // Look up the colour in the SAM palette
        const COLOUR* p = &pSAM[i];
        uint8_t r = p->bRed, g = p->bGreen, b = p->bBlue;

        aulPalette[i] = SDL_MapRGB(pBack->format, r, g, b);
        AdjustBrightness(r, g, b, nScanAdjust);
        aulScanline[i] = SDL_MapRGB(pBack->format, r, g, b);
    }

    // Ensure the display is redrawn to reflect the changes
    Video::SetDirty();
}

bool SDLSurface::DrawChanges(CScreen* pScreen_, bool* pafDirty_)
{
    if (!pBack)
        return false;

    // Lock the surface for direct access below
    if (SDL_MUSTLOCK(pBack) && SDL_LockSurface(pBack) < 0)
    {
        TRACE("!!! SDL_LockSurface failed: %s\n", SDL_GetError());
        return false;
    }

    int nWidth = Frame::GetWidth();
    int nHeight = Frame::GetHeight();

    int nRightHi = nWidth >> 3;

    bool fInterlace = !GUI::IsActive();
    if (fInterlace) nHeight >>= 1;

    uint32_t* pdwBack = reinterpret_cast<uint32_t*>(pBack->pixels), * pdw = pdwBack;
    long lPitchDW = pBack->pitch >> (fInterlace ? 1 : 2);

    uint8_t* pbSAM = pScreen_->GetLine(0), * pb = pbSAM;
    long lPitch = pScreen_->GetPitch();

    int nShift = fInterlace ? 1 : 0;
    int nDepth = pBack->format->BitsPerPixel;


    // What colour depth is the target surface?
    switch (nDepth)
    {
    case 16:
    {
        nWidth <<= 1;

        for (int y = 0; y < nHeight; pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, y++)
        {
            if (!pafDirty_[y])
                continue;

            for (int x = 0; x < nRightHi; x++)
            {
                pdw[0] = SDL_SwapLE32((aulPalette[pb[1]] << 16) | aulPalette[pb[0]]);
                pdw[1] = SDL_SwapLE32((aulPalette[pb[3]] << 16) | aulPalette[pb[2]]);
                pdw[2] = SDL_SwapLE32((aulPalette[pb[5]] << 16) | aulPalette[pb[4]]);
                pdw[3] = SDL_SwapLE32((aulPalette[pb[7]] << 16) | aulPalette[pb[6]]);

                pdw += 4;
                pb += 8;
            }

            if (fInterlace)
            {
                pb = pbSAM;
                pdw = pdwBack + lPitchDW / 2;

                if (!GetOption(scanlevel))
                    memset(pdw, 0x00, nWidth);
                else
                {
                    for (int x = 0; x < nRightHi; x++)
                    {
                        pdw[0] = SDL_SwapLE32((aulScanline[pb[1]] << 16) | aulScanline[pb[0]]);
                        pdw[1] = SDL_SwapLE32((aulScanline[pb[3]] << 16) | aulScanline[pb[2]]);
                        pdw[2] = SDL_SwapLE32((aulScanline[pb[5]] << 16) | aulScanline[pb[4]]);
                        pdw[3] = SDL_SwapLE32((aulScanline[pb[7]] << 16) | aulScanline[pb[6]]);

                        pdw += 4;
                        pb += 8;
                    }
                }
            }
        }
    }
    break;

    case 32:
    {
        nWidth <<= 2;

        for (int y = 0; y < nHeight; pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, y++)
        {
            if (!pafDirty_[y])
                continue;

            for (int x = 0; x < nRightHi; x++)
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
                pdw = pdwBack + lPitchDW / 2;

                if (!GetOption(scanlevel))
                    memset(pdw, 0x00, nWidth);
                else
                {
                    for (int x = 0; x < nRightHi; x++)
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
    }
    break;
    }

    // Unlock the surface now we're done drawing on it
    if (pBack && SDL_MUSTLOCK(pBack))
        SDL_UnlockSurface(pBack);

    // Find the first changed display line
    int nChangeFrom = 0;
    for (; nChangeFrom < nHeight && !pafDirty_[nChangeFrom]; nChangeFrom++);

    if (nChangeFrom < nHeight)
    {
        // Find the last change display line
        int nChangeTo = nHeight - 1;
        for (; nChangeTo && !pafDirty_[nChangeTo]; nChangeTo--);

        // Clear the dirty flags for the changed block
        for (int i = nChangeFrom; i <= nChangeTo; pafDirty_[i++] = false);

        // Calculate the dirty source and target areas - non-GUI displays require the height doubling
        SDL_Rect rect;
        rect.x = 0;
        rect.y = nChangeFrom << nShift;
        rect.w = pScreen_->GetPitch();
        rect.h = ((nChangeTo - nChangeFrom + 1) << nShift);

        SDL_Rect rectFront;
        rectFront.x = (pFront->w - rect.w) >> 1;
        rectFront.y = rect.y + ((pFront->h - (nHeight << nShift)) >> 1);
        rectFront.w = rect.w;
        rectFront.h = rect.h;

        // Blit the updated area and inform SDL it's changed
        SDL_BlitSurface(pBack, &rect, pFront, &rectFront);
        SDL_UpdateRects(pFront, 1, &rectFront);
    }

    // Success
    return true;
}

void SDLSurface::UpdateSize()
{
    int nWidth = Frame::GetWidth();
    int nHeight = Frame::GetHeight();

    // Use 16-bit for fullscreen or the current desktop depth for windowed
    int nDepth = GetOption(fullscreen) ? FULLSCREEN_DEPTH : 0;

    // Full screen mode requires a display mode change
    if (!GetOption(fullscreen))
        pFront = SDL_SetVideoMode(nWidth, nHeight, 0, SDL_HWSURFACE);
    else
    {
        // Work out the best-fit mode for the visible frame area
        if (nWidth <= 640 && nHeight <= 480)
            nWidth = 640, nHeight = 480;
        else if (nWidth <= 800 && nHeight <= 600)
            nWidth = 800, nHeight = 600;
        else
            nWidth = 1024, nHeight = 768;

        // Set the video mode
        pFront = SDL_SetVideoMode(nWidth, nHeight, nDepth, SDL_FULLSCREEN | SDL_HWSURFACE);
    }

    // Did we fail to create the front buffer?
    if (!pFront)
        TRACE("Failed to create front buffer!\n");

    // Create a back buffer in the same format as the front
    else if (!(pBack = SDL_CreateRGBSurface(SDL_HWSURFACE, nWidth, nHeight, pFront->format->BitsPerPixel,
        pFront->format->Rmask, pFront->format->Gmask, pFront->format->Bmask, pFront->format->Amask)))
        TRACE("Can't create back buffer: %s\n", SDL_GetError());
    else
    {
        // Clear out any garbage from the back surface
        SDL_FillRect(pBack, nullptr, 0);
    }

    UpdatePalette();
}


// Map a native size/offset to SAM view port
void SDLSurface::DisplayToSamSize(int* pnX_, int* pnY_)
{
    int nHalfWidth = !GUI::IsActive();
    int nHalfHeight = nHalfWidth;

    *pnX_ = *pnX_ * Frame::GetWidth() / (m_rTarget.w << nHalfWidth);
    *pnY_ = *pnY_ * Frame::GetHeight() / (m_rTarget.h << nHalfHeight);
}

// Map a native client point to SAM view port
void SDLSurface::DisplayToSamPoint(int* pnX_, int* pnY_)
{
    *pnX_ -= m_rTarget.x;
    *pnY_ -= m_rTarget.y;
    DisplayToSamSize(pnX_, pnY_);
}

#endif // !HAVE_LIBSDL2
