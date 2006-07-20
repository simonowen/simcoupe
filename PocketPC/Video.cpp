// Part of SimCoupe - A SAM Coupe emulator
//
// Video.cpp: WinCE display rendering
//
//  Copyright (c) 1999-2006  Simon Owen
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
#include "Options.h"
#include "UI.h"
#include "Util.h"
#include "Video.h"

GXDisplayProperties g_gxdp;
bool g_f3800;               // true if we're running on an iPAQ 3800, which requires special hacks

const int N_TOTAL_COLOURS = N_PALETTE_COLOURS+N_GUI_COLOURS;
DWORD aulPalette[N_TOTAL_COLOURS];
HPALETTE hpal, hpalOld;

bool Video::CreatePalettes (bool fDimmed_/*=false*/)
{
    int i;

    // Whether we're dimmed also depends on our active state
    fDimmed_ |= (g_fPaused && !g_fFrameStep) || (!g_fActive && GetOption(pauseinactive));

    // Video mode uses a palette?
    bool fPalette = (g_gxdp.ffFormat & kfPalette) != 0;

    LOGPALETTE* plp = reinterpret_cast<LOGPALETTE*>(new BYTE[sizeof(LOGPALETTE)+256*sizeof(PALETTEENTRY)]);

    // Get the current system palette
    if (fPalette)
    {
        HDC hdc = GetDC(NULL);
        GetSystemPaletteEntries(hdc, 0, 256, plp->palPalEntry);
        ReleaseDC(NULL, hdc);
    }

    // To handle any 16-bit pixel format we need to build some look-up tables
    WORD awRedTab[256], awGreenTab[256], awBlueTab[256];
    if (g_gxdp.cBPP == 15 || g_gxdp.cBPP == 16)
    {
        WORD wRMask, wGMask, wBMask;

        // The masks depend on the 16-bit mode
        if (g_gxdp.ffFormat & kfDirect565)
            wRMask = 0xf800, wGMask = 0x07e0, wBMask = 0x001f;
        else if (g_gxdp.ffFormat & kfDirect555)
            wRMask = 0x7c00, wGMask = 0x03e0, wBMask = 0x001f;
        else if (g_gxdp.ffFormat & kfDirect444)
            wRMask = 0x0f00, wGMask = 0x00f0, wBMask = 0x000f;

        // Create the lookup table
        for (DWORD dw = 0; dw < 256 ; dw++)
        {
            awRedTab[dw] = dw ? WORD(((DWORD(wRMask) * (dw+1)) >> 8) & wRMask) : 0;
            awGreenTab[dw] = dw ? WORD(((DWORD(wGMask * (dw+1))) >> 8) & wGMask) : 0;
            awBlueTab[dw] = dw ? WORD(((DWORD(wBMask) * (dw+1)) >> 8) & wBMask) : 0;
        }
    }

    const RGBA *pSAM = IO::GetPalette(fDimmed_), *pGUI = GUI::GetPalette();

    // Build the full palette from SAM and GUI colours
    for (i = 0; i < N_TOTAL_COLOURS ; i++)
    {
        // Look up the colour in the appropriate palette
        const RGBA* p = (i < N_PALETTE_COLOURS) ? &pSAM[i] : &pGUI[i-N_PALETTE_COLOURS];
        BYTE bRed = p->bRed, bGreen = p->bGreen, bBlue = p->bBlue;

        // Convert the colour to a grey intensity value, for grey/mono screens
        BYTE bGrey = static_cast<BYTE>(0.30 * bRed + 0.59 * bGreen + 0.11 * bBlue);

        // Write the palette components
        PALETTEENTRY pe = { bRed, bGreen, bBlue, 0 };
        plp->palPalEntry[i+10] = pe;

        // In 8 bit mode use offset palette positions to allow for system colours in the first 10
        if (fPalette)
            aulPalette[i] = i+10;

        // 1/2/4/8-bit grey
        else if (g_gxdp.cBPP == 1)
            aulPalette[i] = bGrey >> 7;
        else if (g_gxdp.cBPP == 2)
            aulPalette[i] = bGrey >> 6;
        else if (g_gxdp.cBPP == 4)
            aulPalette[i] = bGrey >> 4;
        else if (g_gxdp.cBPP == 8)
            aulPalette[i] = bGrey;
        else if (g_gxdp.cBPP == 16)
            aulPalette[i] = awRedTab[bRed] | awGreenTab[bGreen] | awBlueTab[bBlue];
        else
            aulPalette[i] = (static_cast<DWORD>(bRed) << 16) | (static_cast<DWORD>(bGreen) << 8) | bBlue;
    }

    // Is the palette required?
    if (fPalette)
    {
        plp->palNumEntries = 256;
        plp->palVersion = 0x300;
        hpal = CreatePalette(plp);

        // I've no idea how palettes work with GAPI, so this is a complete guess!
        HDC hdc = GetDC(g_hwnd);
        hpalOld = SelectPalette(hdc, hpal, FALSE);
        RealizePalette(hdc);
        ReleaseDC(g_hwnd, hdc);
    }

    // Because the pixel format may have changed, we need to refresh the SAM CLUT pixel values
    for (i = 0 ; i < 16 ; i++)
        clut[i] = aulPalette[clutval[i]];

    return true;
}


// function to initialize DirectDraw in windowed mode
bool Video::Init (bool fFirstInit_/*=false*/)
{
    TRACE("Entering Video::Init()\n");
    Exit(true);

    // Test for the iPAQ 38xx, which forces an intermediate buffer
    TCHAR szDevice[128];
    SystemParametersInfo(SPI_GETOEMINFO, sizeof szDevice, &szDevice, 0);
    g_gxdp = GXGetDisplayProperties();

    // Until the display code has settled, we'll only support 16-bit displays
    if (g_gxdp.cBPP != 16)
    {
        TCHAR sz[256];
        wsprintf(sz, _T("This version currently only works on 16-bit displays.\n\n")
                     _T("Please e-mail the device details below to:\nsupport@simcoupe.org\n\n")
                     _T("Device = %s\nScreen = %u x %u x %u\nPitch = %u / %u\nFlags = %08lx"),
                     szDevice, g_gxdp.cxWidth, g_gxdp.cyHeight, g_gxdp.cBPP, g_gxdp.cbxPitch, g_gxdp.cbyPitch, g_gxdp.ffFormat);
        MessageBox(NULL, sz, _T("Sorry!"), MB_OK|MB_ICONSTOP);
        return false;
    }

    // Daniel Jackson's GAPI fix for the colour Compaq Aero 21xx returning bad values
    if (g_gxdp.cbxPitch == 61440 && g_gxdp.cbyPitch == -2 && g_gxdp.ffFormat == 0x18)
    {
        g_gxdp.cbxPitch = 640;
        g_gxdp.ffFormat = kfDirect|kfDirect565;
    }

    // Test for the iPAQ 38xx, which forces an intermediate buffer
    g_f3800 = _tcsstr(szDevice, _T("H38")) != NULL;
    if (g_f3800)
    {
        g_gxdp.cxWidth = 240;
        g_gxdp.cyHeight = 320;
        g_gxdp.cbxPitch = -640;
        g_gxdp.cbyPitch = 2;
        g_gxdp.cBPP = 16;
        g_gxdp.ffFormat = kfDirect|kfDirect565|kfLandscape;
    }

    if (fFirstInit_)
    {
        // Ask for complete control over the display
        if (!GXOpenDisplay(g_hwnd, GX_FULLSCREEN))
        {
            TRACE("!!! GXOpenDisplay() failed!\n");
            return false;
        }

        // Some devices seem to need the viewport set (to the full height in our case)
        GXSetViewport(0, g_gxdp.cyHeight, 0, 0);
    }

    // Create the palette or colour look-up tables
    CreatePalettes();

    TRACE("Leaving Video::Init()\n");
    return true;
}

void Video::Exit (bool fReInit_/*=false*/)
{
    TRACE("Video::Exit()\n");

    if (!fReInit_)
    {
        // Give the display back to Windows
        GXCloseDisplay();

        // Clean up any palette
        if (hpalOld)
        {
            HDC hdc = GetDC(NULL);
            SelectPalette(hdc, hpalOld, FALSE);
            RealizePalette(hdc);
            ReleaseDC(NULL, hdc);
            DeleteObject(hpal);
            hpal = hpalOld = NULL;
        }
    }

    TRACE("Leaving Video::Exit()\n");
}

void Video::UpdatePalette ()
{
    if (hpal)
    {
        TRACE("Updating palette\n");
        HDC hdc = GetDC(NULL);
        SelectPalette(hdc, hpalOld, FALSE);
        RealizePalette(hdc);
        ReleaseDC(NULL, hdc);
    }
}
