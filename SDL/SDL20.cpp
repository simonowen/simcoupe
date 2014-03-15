// Part of SimCoupe - A SAM Coupe emulator
//
// SDL20.cpp: Hardware accelerated textures for SDL 2.0
//
//  Copyright (c) 1999-2014 Simon Owen
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
#include "SDL20.h"

#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "UI.h"

#ifdef USE_SDL2

static DWORD aulPalette[N_PALETTE_COLOURS];
static DWORD aulScanline[N_PALETTE_COLOURS];


SDLTexture::SDLTexture ()
    : m_pWindow(NULL), m_pRenderer(NULL), m_pTexture(NULL), m_pScanlineTexture(NULL), m_fFilter(GetOption(filter))
{
    m_rTarget.x = m_rTarget.y = 0;
    m_rTarget.w = Frame::GetWidth();
    m_rTarget.h = Frame::GetHeight();

    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");

#ifdef SDL_VIDEO_FULLSCREEN_SPACES
    SDL_SetHint(SDL_VIDEO_FULLSCREEN_SPACES, "1");
#endif
}

SDLTexture::~SDLTexture ()
{
    if (m_pScanlineTexture) SDL_DestroyTexture(m_pScanlineTexture), m_pScanlineTexture = NULL;
    if (m_pTexture) SDL_DestroyTexture(m_pTexture), m_pTexture = NULL;
    if (m_pRenderer) SDL_DestroyRenderer(m_pRenderer), m_pRenderer = NULL;
    if (m_pWindow) SDL_DestroyWindow(m_pWindow), m_pWindow = NULL;

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}


int SDLTexture::GetCaps () const
{
    return VCAP_STRETCH | VCAP_FILTER | VCAP_SCANHIRES;
}

bool SDLTexture::Init (bool fFirstInit_)
{
    TRACE("-> Video::Init(%s)\n", fFirstInit_ ? "first" : "");

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    {
        TRACE("SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s\n", SDL_GetError());
        return false;
    }

    // Original frame
    int nWidth = Frame::GetWidth();
    int nHeight = Frame::GetHeight();

    // Apply window scaling and aspect ratio
    if (!GetOption(scale)) SetOption(scale, 2);
    int nWindowWidth = nWidth * GetOption(scale) / 2;
    int nWindowHeight = nHeight * GetOption(scale) / 2;
    if (GetOption(ratio5_4)) nWindowWidth = nWindowWidth * 5/4;

    // Create window hidden initially
    Uint32 flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE;
    m_pWindow = SDL_CreateWindow(WINDOW_CAPTION, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, nWindowWidth, nWindowHeight, flags);
    if (!m_pWindow)
    {
        TRACE("Failed to create SDL2 window!\n");
        return false;
    }

    // Limit window to 50% size (typically 384x240)
    SDL_SetWindowMinimumSize(m_pWindow, nWidth/2, nHeight/2);

    m_pRenderer = SDL_CreateRenderer(m_pWindow, -1, SDL_RENDERER_ACCELERATED);
    if (!m_pRenderer)
    {
        TRACE("Failed to create SDL2 renderer!\n");
        SDL_DestroyWindow(m_pWindow), m_pWindow = NULL;
        return false;
    }

    SDL_RendererInfo ri;
    SDL_GetRendererInfo(m_pRenderer, &ri);

    // Ensure the renderer is accelerated
    if (!(ri.flags & SDL_RENDERER_ACCELERATED))
    {
        TRACE("SDLTexture: skipping non-accelerated renderer\n");
        SDL_DestroyRenderer(m_pRenderer), m_pRenderer = NULL;
        SDL_DestroyWindow(m_pWindow), m_pWindow = NULL;
        return false;
    }

    UpdateSize();
    UpdatePalette();
    SDL_ShowWindow(m_pWindow);

    return true;
}


void SDLTexture::Update (CScreen* pScreen_, bool *pafDirty_)
{
    // Draw any changed lines to the back buffer
    if (!DrawChanges(pScreen_, pafDirty_))
        return;
}

// Create whatever's needed for actually displaying the SAM image
void SDLTexture::UpdatePalette ()
{
    // Determine the scanline brightness level adjustment, in the range -100 to +100
    int nScanAdjust = GetOption(scanlines) ? (GetOption(scanlevel) - 100) : 0;
    if (nScanAdjust < -100) nScanAdjust = -100;

    const COLOUR *pSAM = IO::GetPalette();

    int w, h;
    Uint32 uFormat, uRmask, uGmask, uBmask, uAmask;
    SDL_QueryTexture(m_pTexture, &uFormat, NULL, &w, &h);
    SDL_PixelFormatEnumToMasks(uFormat, &m_nDepth, &uRmask, &uGmask, &uBmask, &uAmask);

    // Build the full palette from SAM and GUI colours
    for (int i = 0; i < N_PALETTE_COLOURS ; i++)
    {
        // Look up the colour in the SAM palette
        const COLOUR *p = &pSAM[i];
        BYTE r = p->bRed, g = p->bGreen, b = p->bBlue, a = 0xff;

        aulPalette[i] = RGB2Native(r,g,b,a, uRmask, uGmask, uBmask, uAmask);
        AdjustBrightness(r,g,b, nScanAdjust);
        aulScanline[i] = RGB2Native(r,g,b,a, uRmask, uGmask, uBmask, uAmask);
    }

    // Ensure the display is redrawn to reflect the changes
    Video::SetDirty();
}


// OpenGL version of DisplayChanges
bool SDLTexture::DrawChanges (CScreen* pScreen_, bool *pafDirty_)
{
    // Force GUI filtering with odd scaling factors, otherwise respect the options
    bool fFilter = GUI::IsActive() ? GetOption(filtergui) || (GetOption(scale) & 1) : GetOption(filter);

    // If the required filter state has changed, apply it
    if (m_fFilter != fFilter)
    {
        m_fFilter = fFilter;
        UpdateSize();
    }

    if (!m_pTexture)
        return false;

    int nWidth = Frame::GetWidth();
    int nHeight = Frame::GetHeight();

    bool fHalfHeight = !GUI::IsActive();
    if (fHalfHeight) nHeight /= 2;

    int nChangeFrom = 0, nChangeTo = nHeight-1;
    for ( ; nChangeFrom < nHeight && !pafDirty_[nChangeFrom] ; nChangeFrom++);
    if (nChangeFrom == nHeight)
        return true;

    for ( ; nChangeTo && !pafDirty_[nChangeTo] ; nChangeTo--);

    // With bilinear filtering enabled, the GUI display in the lower half bleeds
    // into the bottom line of the display, so clear it when changing modes.
    static bool fLastHalfHeight = true;
    if (fHalfHeight && !fLastHalfHeight)
        pScreen_->FillRect(0, nChangeTo = nHeight, pScreen_->GetPitch(), 1, BLACK);
    fLastHalfHeight = fHalfHeight;

    // Lock only the portion we're changing
    SDL_Rect rLock = { 0, nChangeFrom, nWidth, nChangeTo-nChangeFrom+1 };
    void *pvPixels = NULL;
    int nPitch = 0;

    // Lock the surface for direct access below
    if (SDL_LockTexture(m_pTexture, &rLock, &pvPixels, &nPitch) != 0)
    {
        TRACE("!!! SDL_LockSurface failed: %s\n", SDL_GetError());
        return false;
    }

    int nRightHi = nWidth >> 3;
    int nRightLo = nRightHi >> 1;

    DWORD *pdwBack = reinterpret_cast<DWORD*>(pvPixels), *pdw = pdwBack;
    long lPitchDW = nPitch >> 2;
    bool *pfHiRes = pScreen_->GetHiRes();

    BYTE *pbSAM = pScreen_->GetLine(nChangeFrom), *pb = pbSAM;
    long lPitch = pScreen_->GetPitch();


    // What colour depth is the target surface?
    switch (m_nDepth)
    {
        case 16:
        {
            for (int y = nChangeFrom ; y <= nChangeTo ; pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, y++)
            {
                if (!pafDirty_[y])
                    continue;

                if (pfHiRes[y])
                {
                    for (int x = 0 ; x < nRightHi ; x++)
                    {
                        pdw[0] = SDL_SwapLE32((aulPalette[pb[1]] << 16) | aulPalette[pb[0]]);
                        pdw[1] = SDL_SwapLE32((aulPalette[pb[3]] << 16) | aulPalette[pb[2]]);
                        pdw[2] = SDL_SwapLE32((aulPalette[pb[5]] << 16) | aulPalette[pb[4]]);
                        pdw[3] = SDL_SwapLE32((aulPalette[pb[7]] << 16) | aulPalette[pb[6]]);

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

        case 32:
        {
            for (int y = nChangeFrom ; y <= nChangeTo ; pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, y++)
            {
                if (!pafDirty_[y])
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
    }

    // Unlock the texture now we're done drawing on it
    SDL_UnlockTexture(m_pTexture);

    SDL_Rect rTexture = { 0,0, nWidth, nHeight };
    SDL_Rect rWindow = { 0,0, 0,0 };
    SDL_GetWindowSize(m_pWindow, &rWindow.w, &rWindow.h);

    nWidth = Frame::GetWidth();
    nHeight = Frame::GetHeight();
    if (GetOption(ratio5_4)) nWidth = nWidth * 5/4;

    int nWidthFit = nWidth * rWindow.h / nHeight;
    int nHeightFit = nHeight * rWindow.w / nWidth;

    if (nWidthFit <= rWindow.w)
    {
        nWidth = nWidthFit;
        nHeight = rWindow.h;
    }
    else if (nHeightFit <= rWindow.h)
    {
        nWidth = rWindow.w;
        nHeight = nHeightFit;
    }

    rWindow.x = (rWindow.w - nWidth) / 2;
    rWindow.y = (rWindow.h - nHeight) / 2;
    rWindow.w = nWidth;
    rWindow.h = nHeight;
    m_rTarget = rWindow;

    SDL_RenderClear(m_pRenderer);
    SDL_RenderCopy(m_pRenderer, m_pTexture, &rTexture, &rWindow);

    if (m_pScanlineTexture && GetOption(scanlines) && !GUI::IsActive())
    {
        SDL_Rect rScanlines = { 0, 0, 1, GetOption(scanhires) ? rWindow.h : Frame::GetHeight() };

        SDL_SetTextureBlendMode(m_pScanlineTexture, SDL_BLENDMODE_BLEND);
        SDL_RenderCopy(m_pRenderer, m_pScanlineTexture, &rScanlines, &rWindow);
    }

    SDL_RenderPresent(m_pRenderer);

    return true;
}

void SDLTexture::UpdateSize ()
{
    // Toggle fullscreen state if necessary
    bool fFullscreen = (SDL_GetWindowFlags(m_pWindow) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    if (GetOption(fullscreen) != fFullscreen)
        SDL_SetWindowFullscreen(m_pWindow, GetOption(fullscreen) ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);

    if (m_pScanlineTexture) SDL_DestroyTexture(m_pScanlineTexture), m_pScanlineTexture = NULL;
    if (m_pTexture) SDL_DestroyTexture(m_pTexture), m_pTexture = NULL;

    int nWidth = Frame::GetWidth();
    int nHeight = Frame::GetHeight();

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, m_fFilter ? "linear" : "nearest");
    m_pTexture = SDL_CreateTexture(m_pRenderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_STREAMING, nWidth, nHeight);

    SDL_DisplayMode displaymode;
    SDL_GetDesktopDisplayMode(0, &displaymode);

    m_pScanlineTexture = SDL_CreateTexture(m_pRenderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_STATIC, 1, displaymode.h);

    if (m_pScanlineTexture)
    {
        int w, h, nDepth;
        Uint32 uFormat, uRmask, uGmask, uBmask, uAmask;
        SDL_QueryTexture(m_pScanlineTexture, &uFormat, NULL, &w, &h);
        SDL_PixelFormatEnumToMasks(uFormat, &nDepth, &uRmask, &uGmask, &uBmask, &uAmask);

        Uint32 ulScanline0 = RGB2Native(0,0,0, (100-GetOption(scanlevel))*0xff/100, uRmask, uGmask, uBmask, uAmask);
        Uint32 ulScanline1 = RGB2Native(0,0,0, 0, uRmask, uGmask, uBmask, uAmask);
        Uint32 *pbScanlines = new Uint32[h];

        for (int j = 0 ; j < h ; j++)
            pbScanlines[j] = (j&1) ? ulScanline1 : ulScanline0;

        SDL_UpdateTexture(m_pScanlineTexture, NULL, pbScanlines, sizeof(Uint32));
        delete[] pbScanlines;
    }
}


// Map a native size/offset to SAM view port
void SDLTexture::DisplayToSamSize (int* pnX_, int* pnY_)
{
    int nHalfWidth = !GUI::IsActive();
    int nHalfHeight = nHalfWidth;

    *pnX_ = *pnX_ * Frame::GetWidth()  / (m_rTarget.w << nHalfWidth);
    *pnY_ = *pnY_ * Frame::GetHeight() / (m_rTarget.h << nHalfHeight);
}

// Map a native client point to SAM view port
void SDLTexture::DisplayToSamPoint (int* pnX_, int* pnY_)
{
    *pnX_ -= m_rTarget.x;
    *pnY_ -= m_rTarget.y;
    DisplayToSamSize(pnX_, pnY_);
}

#endif // USE_SDL2
