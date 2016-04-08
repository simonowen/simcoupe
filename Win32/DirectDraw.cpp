// Part of SimCoupe - A SAM Coupe emulator
//
// DirectDraw.cpp: DirectDraw display
//
//  Copyright (c) 2012-2015 Simon Owen
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
#include "DirectDraw.h"

#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "UI.h"
#include "Video.h"

#pragma comment(lib, "ddraw.lib")

// Normal and scanline palettes in native surface values (faster kept at file scope)
static DWORD aulPalette[N_PALETTE_COLOURS];
static DWORD aulScanline[N_PALETTE_COLOURS];


DirectDrawVideo::DirectDrawVideo ()
{
    m_nWidth = Frame::GetWidth();
    m_nHeight = Frame::GetHeight();
}

DirectDrawVideo::~DirectDrawVideo ()
{
    if (m_pddClipper) m_pddClipper->Release();
    if (m_pddsBack) m_pddsBack->Release();
    if (m_pddsPrimary) m_pddsPrimary->Release();

    if (m_pdd)
    {
        // Should be done automatically, but let's do it just in case
        m_pdd->RestoreDisplayMode();
        m_pdd->SetCooperativeLevel(g_hwnd, DDSCL_NORMAL);
        m_pdd->Release();
    }
}

int DirectDrawVideo::GetCaps () const
{
	return VCAP_STRETCH;
}


bool DirectDrawVideo::Init (bool fFirstInit_)
{
    bool fRet = false;
    HRESULT hr;

    // Determine the current display depth
    HDC hdc = GetDC(nullptr);
    int nBPP = GetDeviceCaps(hdc, BITSPIXEL);
    ReleaseDC(nullptr, hdc);

    // Check the display depth is supported
    if (nBPP != 16 && nBPP != 32)
    {
        Message(msgError, "SimCoupe requires a 16-bit or 32-bit display mode.");
        return false;
    }

    __try
    {
        // Create the main DirectDraw object, reversing the acceleration option if the first attempt failed
        hr = DirectDrawCreate(GetOption(hwaccel) ? nullptr : (LPGUID)DDCREATE_EMULATIONONLY, &m_pdd, nullptr);
        if (FAILED(hr))
            hr = DirectDrawCreate(GetOption(hwaccel) ? (LPGUID)DDCREATE_EMULATIONONLY : nullptr, &m_pdd, nullptr);

        if (FAILED(hr))
        {
            Message(msgError, "DirectDrawCreate() failed (%#08lx).", hr);
            return false;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Message(msgError, "DDRAW::DirectDrawCreate not found.\n");
        return false;
    }

    // Get the driver capabilites so we know what we need to set up
    DDCAPS ddcaps = { sizeof(ddcaps) };
    m_pdd->GetCaps(&ddcaps, nullptr);

    // Use exclusive mode for full-screen, or normal mode for windowed
    hr = m_pdd->SetCooperativeLevel(g_hwnd, GetOption(fullscreen) ? DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN|DDSCL_ALLOWREBOOT : DDSCL_NORMAL);
    if (FAILED(hr))
        Message(msgError, "SetCooperativeLevel() failed (%#08lx).", hr);
    else
    {
        // Set up what we need for the primary surface
        DDSURFACEDESC ddsd = { sizeof(ddsd) };
        ddsd.dwFlags = DDSD_CAPS;
        ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

        // Create the primary surface
        if (!(m_pddsPrimary = CreateSurface(DDSCAPS_PRIMARYSURFACE)))
            Message(msgError, "Failed to create primary DirectDraw surface (%#08lx).", hr);

        // Use a clipper to keep the emulator image within the window area
        else if (FAILED(hr = m_pdd->CreateClipper(0, &m_pddClipper, nullptr)))
            Message(msgError, "CreateClipper() failed (%#08lx).", hr);
        else if (FAILED(hr = m_pddClipper->SetHWnd(0, hwndCanvas)))
            Message(msgError, "Clipper SetHWnd() failed (%#08lx).", hr);
        else if (FAILED(hr = m_pddsPrimary->SetClipper(m_pddClipper)))
            Message(msgError, "SetClipper() failed (%#08lx).", hr);
        else
        {
            // Set up the required capabilities for the back buffer
            DWORD dwRequiredFX = (DDFXCAPS_BLTSTRETCHX | DDFXCAPS_BLTSTRETCHY);

            // Create the back buffer
            m_pddsBack = CreateSurface(0, m_nWidth, m_nHeight, dwRequiredFX);

            if (!m_pddsBack)
                Message(msgError, "Failed to create DirectDraw back surface (%#08lx).", hr);
            else
            {
                // Clear the back buffer
                DDBLTFX bltfx = { sizeof(bltfx) };
                bltfx.dwFillColor = 0;
                m_pddsBack->Blt(nullptr, nullptr, nullptr, DDBLT_COLORFILL|DDBLT_WAIT, &bltfx);

                UpdatePalette();
                fRet = true;
            }
        }
    }

    return fRet;
}

void DirectDrawVideo::UpdatePalette ()
{
    // Don't attempt anything without a surface pointer
    if (!m_pddsBack)
        return;

    // Check the format of the back buffer
    DDSURFACEDESC ddsd = { sizeof(ddsd) };
    m_pddsBack->GetSurfaceDesc(&ddsd);

    const DDPIXELFORMAT *ddpf = &ddsd.ddpfPixelFormat;
    const COLOUR *pSAM = IO::GetPalette();

    // Build the palette from SAM colours
    for (int i = 0; i < N_PALETTE_COLOURS ; i++)
    {
        // Look up the colour in the SAM palette
        const COLOUR *p = &pSAM[i];
        BYTE r = p->bRed, g = p->bGreen, b = p->bBlue;

        // Set regular pixel
        aulPalette[i] = RGB2Native(r,g,b, ddpf->dwRBitMask, ddpf->dwGBitMask, ddpf->dwBBitMask);

        // Determine scanline pixel
        AdjustBrightness(r,g,b, GetOption(scanlevel)-100);
        aulScanline[i] = RGB2Native(r,g,b, ddpf->dwRBitMask, ddpf->dwGBitMask, ddpf->dwBBitMask);
    }

    // Redraw to reflect any changes
    Video::SetDirty();
}

// Update the display to show anything that's changed since last time
void DirectDrawVideo::Update (CScreen* pScreen_, bool *pafDirty_)
{
    HRESULT hr = 0;
    if (!m_pdd)
        return;

    // Reinitialise if the frame size has changed
    if (m_nWidth != Frame::GetWidth() || m_nHeight != Frame::GetHeight())
    {
        Video::Init();
        return;
    }

    // Check if we've lost the surface memory
    if (!m_pddsPrimary || FAILED(hr = m_pddsPrimary->Restore()) || (m_pddsBack && FAILED(hr = m_pddsBack->Restore())))
    {
        // Reinitialise the video system if the mode has changed, but only if we're active
        if (!m_pddsPrimary || hr == DDERR_WRONGMODE || GetActiveWindow() == g_hwnd)
            Video::Init();

        return;
    }

    // Draw any changed lines to the back buffer
    if (!DrawChanges(pScreen_, pafDirty_))
        return;

    // rFront is the display area in which to display it
    RECT rFront;
    GetWindowRect(hwndCanvas, &rFront);

    // Return if there's nothing to draw
    if (IsRectEmpty(&rFront))
        return;

    // Remember the target rect for cursor position mapping in the GUI
    GetClientRect(hwndCanvas, &m_rTarget);


    DDSURFACEDESC ddsd = { sizeof(ddsd) };
    m_pddsBack->GetSurfaceDesc(&ddsd);

    RECT rBack;
    rBack.left = rBack.top = 0;
    rBack.right = static_cast<LONG>(ddsd.dwWidth);
    rBack.bottom = static_cast<LONG>(ddsd.dwHeight);

    // In non-scanline mode we need to double the image height
    if (!GetOption(scanlines) && !GUI::IsActive())
        rBack.bottom >>= 1;

    // Display the main SAM image
    if (FAILED(hr = m_pddsPrimary->Blt(&rFront, m_pddsBack, &rBack, DDBLT_WAIT, 0)))
        TRACE("!!! Blt (back to primary) failed with %#08lx\n", hr);
}


LPDIRECTDRAWSURFACE DirectDrawVideo::CreateSurface (DWORD dwCaps_, DWORD dwWidth_, DWORD dwHeight_, DWORD dwRequiredCaps_)
{
    LPDIRECTDRAWSURFACE pdds = nullptr;

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

        DDCAPS ddcaps = { sizeof(ddcaps) };
        m_pdd->GetCaps(&ddcaps, nullptr);

        // Force a system surface if the hardware doesn't support stretching, as the emulated Blt VRAM reads are VERY slow
        if ((~ddcaps.dwFXCaps & dwRequiredCaps_) != 0)
            ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
    }

    HRESULT hr;
    if (FAILED(hr = m_pdd->CreateSurface(&ddsd, &pdds, nullptr)))
        TRACE("!!! Failed to create surface (%#08lx)\n", hr);

    // Only back buffers need to be checked for lockability
    if (!(dwCaps_ & DDSCAPS_PRIMARYSURFACE))
    {
        // Make sure the surface is lockable, as some VRAM surfaces may not be
        if (SUCCEEDED(hr = pdds->Lock(nullptr, &ddsd, DDLOCK_SURFACEMEMORYPTR|DDLOCK_WRITEONLY|DDLOCK_WAIT, nullptr)))
            pdds->Unlock(ddsd.lpSurface);

        // If we've not just tried a system surface, try one now
        else if (!(ddsd.ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY))
        {
            // Release the unlockable surface as it's of no use to us
            pdds->Release();

            // Force into system memory and try again
            ddsd.ddsCaps.dwCaps &= ~DDSCAPS_VIDEOMEMORY;
            ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;

            if (FAILED(hr = m_pdd->CreateSurface(&ddsd, &pdds, nullptr)))
                TRACE("!!! Failed to create forced system surface (%#08lx)\n", hr);
        }
    }

    return pdds;
}

// Draw the changed lines in the appropriate colour depth and hi/low resolution
bool DirectDrawVideo::DrawChanges (CScreen* pScreen_, bool *pafDirty_)
{
    HRESULT hr;
    DDSURFACEDESC ddsd = { sizeof(ddsd) };

    // Lock the surface,  without taking the Win16Mutex if possible
    if (FAILED(hr = m_pddsBack->Lock(nullptr, &ddsd, DDLOCK_SURFACEMEMORYPTR|DDLOCK_WRITEONLY|DDLOCK_WAIT|DDLOCK_NOSYSLOCK, nullptr))
     && FAILED(hr = m_pddsBack->Lock(nullptr, &ddsd, DDLOCK_SURFACEMEMORYPTR|DDLOCK_WRITEONLY|DDLOCK_WAIT, nullptr)))
    {
        TRACE("!!! DrawChanges()  Failed to lock back surface (%#08lx)\n", hr);
        return false;
    }

    // If we've changing from displaying the GUI back to scanline mode, clear the unused lines on the surface
    bool fInterlace = GetOption(scanlines) && !GUI::IsActive();

    // In scanline mode, treat the back buffer as full height, drawing alternate lines
    if (fInterlace)
        ddsd.lPitch <<= 1;

    DWORD *pdwBack = reinterpret_cast<DWORD*>(ddsd.lpSurface), *pdw = pdwBack;
    LONG lPitchDW = ddsd.lPitch >> 2;

    BYTE *pbSAM = pScreen_->GetLine(0), *pb = pbSAM;
    LONG lPitch = pScreen_->GetPitch();

    int nDepth = ddsd.ddpfPixelFormat.dwRGBBitCount;
    int nBottom = pScreen_->GetHeight() >> (GUI::IsActive() ? 0 : 1);
    int nWidth = pScreen_->GetPitch(), nRightHi = nWidth >> 3;

    switch (nDepth)
    {
        case 16:
        {
            nWidth <<= 1;

            for (int y = 0 ; y < nBottom ; pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, y++)
            {
                if (!pafDirty_[y])
                    continue;

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

                pafDirty_[y] = false;
            }
        }
        break;

        case 32:
        {
            nWidth <<= 2;

            for (int y = 0 ; y < nBottom ; pdw = pdwBack += lPitchDW, pb = pbSAM += lPitch, y++)
            {
                if (!pafDirty_[y])
                    continue;

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

                pafDirty_[y] = false;
            }
        }
        break;
    }

    m_pddsBack->Unlock(ddsd.lpSurface);

    // Success
    return true;
}


// Map a native size/offset to SAM view port
void DirectDrawVideo::DisplayToSamSize (int* pnX_, int* pnY_)
{
    int nHalfWidth = !GUI::IsActive();
    int nHalfHeight = nHalfWidth && GetOption(scanlines);

    *pnX_ = *pnX_ * m_nWidth  / (m_rTarget.right << nHalfWidth);
    *pnY_ = *pnY_ * m_nHeight / (m_rTarget.bottom << nHalfHeight);
}

// Map a native client point to SAM view port
void DirectDrawVideo::DisplayToSamPoint (int* pnX_, int* pnY_)
{
    DisplayToSamSize(pnX_, pnY_);
}
