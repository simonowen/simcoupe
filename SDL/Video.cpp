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

//  ToDo:
//   - possibly separate out the OpenGL and SDL code further?

#include "SimCoupe.h"
#include "Video.h"

#include "Display.h"
#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "UI.h"

const int N_TOTAL_COLOURS = N_PALETTE_COLOURS + N_GUI_COLOURS;

// SAM RGB values in appropriate format, and YUV values pre-shifted for overlay surface
DWORD aulPalette[N_TOTAL_COLOURS];

SDL_Surface *pBack, *pFront, *pIcon;


// Mask for the current SimCoupe.bmp image used as the SDL program icon
static BYTE abIconMask[128] =
{
    0x00, 0x01, 0x00, 0x00, 0x00, 0x07, 0xc0, 0x00,
    0x00, 0x1f, 0xf0, 0x00, 0x00, 0x7f, 0xfc, 0x00,
    0x00, 0xff, 0xfe, 0x00, 0x01, 0xff, 0xff, 0x00,
    0x03, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0x80,
    0x07, 0xff, 0xff, 0xc0, 0x07, 0xff, 0xff, 0xc0,
    0x07, 0xff, 0xff, 0xc0, 0x07, 0xff, 0xff, 0xc0,
    0x07, 0xff, 0xff, 0xc0, 0x07, 0xff, 0xff, 0xc0,
    0x28, 0x1f, 0xf0, 0x28, 0x7c, 0x7d, 0xdc, 0x7c,
    0xfc, 0xff, 0xfe, 0x7e, 0x7f, 0xff, 0xff, 0xfc,
    0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xfc,
    0x03, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0x80,
    0x07, 0xff, 0xff, 0xc0, 0x07, 0xff, 0xff, 0xc0,
    0x07, 0xff, 0xff, 0xc0, 0x07, 0xff, 0xff, 0xc0,
    0x00, 0xf8, 0x3e, 0x00, 0x01, 0xfc, 0x7f, 0x00,
    0x03, 0xfe, 0xff, 0x80, 0x03, 0xfe, 0xff, 0x80,
    0x03, 0xfe, 0xff, 0x80, 0x03, 0xfe, 0xff, 0x80
};


#ifdef USE_OPENGL

GLuint dlist;
GLuint auTextures[N_TEXTURES];
DWORD dwTextureData[N_TEXTURES][256][256];


void glBork (const char* pcsz_)
{
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        char szError[64];
        sprintf(szError, "%s: %#04x", pcsz_, (UINT)error);
#ifdef _WINDOWS
        MessageBox(NULL, szError, "glBork", MB_ICONSTOP);
#else
        fprintf(stderr, "%s\n", szError);
#endif
        exit(0);
    }
}

void InitGL ()
{
    int nWidth = Frame::GetWidth(), nHeight = Frame::GetHeight();
    int nW = GetOption(ratio5_4) ? nWidth * 5/4 : nWidth;

    if (!GetOption(stretchtofit))
    {
        // Centralise what we have without any scaling
        int nX = (pFront->w-nW)>>1, nY = (pFront->h-nHeight)>>2;
        rTarget.x = nX, rTarget.y = nY, rTarget.w = nW, rTarget.h = nHeight;
    }
    else
    {
        // Calculate the scaled widths/heights and positions we might need
        int nW2 = nW * pFront->h / nHeight, nH2 = nHeight * pFront->w / nW;
        int nX = (pFront->w-nW2)>>1, nY = (pFront->h-nH2)>>1;

        // Scale to fill the width or the height, depending on which fits best
        if (nH2 > pFront->h)
            rTarget.x = nX, rTarget.y = 0, rTarget.w = nW2, rTarget.h = pFront->h;
        else
            rTarget.x = 0, rTarget.y = nY, rTarget.w = pFront->w, rTarget.h = nH2;
    }

    // Hack: offset by 1 to stop the GUI half bleeding into the bottom line of the emulation view!
    glViewport(rTarget.x, rTarget.y-1, rTarget.w, rTarget.h);
    glBork("glViewport");

    glEnable(GL_TEXTURE_2D);
    glGenTextures(N_TEXTURES,auTextures);

    for (int i = 0 ; i < N_TEXTURES ; i++)
    {
        glBindTexture(GL_TEXTURE_2D,auTextures[i]);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GetOption(filter) ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GetOption(filter) ? GL_LINEAR : GL_NEAREST);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, dwTextureData[i]);
        glBork("glTexImage2D");
    }

    dlist=glGenLists(1);
    glNewList(dlist,GL_COMPILE);

    glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
    glClear(GL_COLOR_BUFFER_BIT);

    for (int yy = 0 ; yy < (N_TEXTURES/3) ; yy++)
    {
        for (int xx = 0 ; xx < 3 ; xx++)
        {
            float flWidth = 2.0f * 256 / nWidth, flHeight = 2.0f * 256 / nHeight;
            float flX = -1.0f + (2.0f * 256 * xx / nWidth), flY = -1.0f + (2.0f * 256 * yy / nHeight);

            if (flHeight)
            {
                glBindTexture(GL_TEXTURE_2D, auTextures[3*yy + xx]);

                glBegin(GL_QUADS);
                glTexCoord2f(0.0,1.0); glVertex2f(flX,           flY + flHeight);
                glTexCoord2f(0.0,0.0); glVertex2f(flX,           flY);
                glTexCoord2f(1.0,0.0); glVertex2f(flX + flWidth, flY);
                glTexCoord2f(1.0,1.0); glVertex2f(flX + flWidth, flY + flHeight);
                glEnd();
            }
        }
    }

    glEndList();
}

#endif

// function to initialize DirectDraw in windowed mode
bool Video::Init (bool fFirstInit_/*=false*/)
{
    bool fRet = false;

    // The lack of stretching support means the SDL version currently lacks certain features, so force the options for now
    SetOption(scanlines, true);
#ifndef USE_OPENGL
    SetOption(stretchtofit, false);
    SetOption(ratio5_4, false);
#endif


    Exit(true);

    TRACE("-> Video::Init(%s)\n", fFirstInit_ ? "first" : "");

    if (fFirstInit_ && SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
        TRACE("SDL_InitSubSystem(SDL_INIT_VIDEO) failed: %s: %s\n", SDL_GetError());
    else
    {
        SDL_WM_SetIcon(pIcon = SDL_LoadBMP(OSD::GetFilePath("SimCoupe.bmp")), abIconMask);

        DWORD dwWidth = Frame::GetWidth(), dwHeight = Frame::GetHeight();

        // In 5:4 mode we'll stretch the viewing surfaces by 25%
        if (GetOption(ratio5_4))
            dwWidth = (dwWidth * 5) >> 2;

        int nDepth = GetOption(fullscreen) ? GetOption(depth) : 0;

        // Should the surfaces be in video memory?  (they need to be for any hardware acceleration)
        DWORD dwOptions =  ((GetOption(surface) >= 2) ? SDL_HWSURFACE : 0);
        dwOptions |= (nDepth == 8) ? SDL_HWPALETTE : 0;

#ifdef USE_OPENGL
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

            if (GetOption(fullscreen))
                dwOptions |= SDL_FULLSCREEN;

            if (!(pFront = SDL_SetVideoMode(dwWidth, dwHeight, nDepth, (dwOptions | SDL_HWSURFACE | SDL_OPENGL))))
            {
                char sz[128];
                sprintf(sz, "SDL_SetVideoMode() failed: %s", SDL_GetError());
                UI::ShowMessage(msgFatal, sz);
            }

            InitGL();
            CreatePalettes();
            fRet = UI::Init(fFirstInit_);
#else
        // Full screen mode requires a display mode change
        if (!GetOption(fullscreen))
            pFront = SDL_SetVideoMode(dwWidth, dwHeight, nDepth, dwOptions);
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
            while (!(pFront = SDL_SetVideoMode(nWidth, nHeight, nDepth, SDL_FULLSCREEN | dwOptions)))
            {
                TRACE("!!! Failed to set %dx%dx%d mode: %s\n", nWidth, nHeight, nDepth, SDL_GetError());

                // If we're already on the lowest depth, try lower resolutions
                if (nDepth == 8)
                {
                    if (nHeight == 768)
                        nWidth = 800, nHeight = 600;
                    else if (nHeight == 600)
                        nWidth = 640, nHeight = 480;
                    else if (nHeight == 480)
                    {
                        TRACE("SDL_SetVideoMode() failed with ALL modes!\n");
                        return false;
                    }
                }

                // Fall back to a lower depth
                else if (nDepth == 24)
                    nDepth = 16;
                else
                    nDepth >>= 1;
            }

            // Update the depth option, just in case it was changed above
            SetOption(depth, nDepth);
        }

        // Did we fail to create the front buffer?
        if (!pFront)
            TRACE("Failed to create front buffer!\n");

        // Create a back buffer in the same format as the front
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
#endif
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

    if (pBack) { SDL_FreeSurface(pBack); pBack = NULL; }
    if (pFront) { SDL_FreeSurface(pFront); pFront = NULL; }
    if (pIcon) { SDL_FreeSurface(pIcon); pIcon = NULL; }

    if (!fReInit_)
        SDL_QuitSubSystem(SDL_INIT_VIDEO);

    TRACE("<- Video::Exit()\n");
}


// Create whatever's needed for actually displaying the SAM image
bool Video::CreatePalettes (bool fDimmed_)
{
    SDL_Color acPalette[N_TOTAL_COLOURS];
    bool fPalette = pBack && (pBack->format->BitsPerPixel == 8);
    TRACE("CreatePalette: fPalette = %s\n", fPalette ? "true" : "false");

    fDimmed_ |= (g_fPaused && !g_fFrameStep) || GUI::IsActive() || (!g_fActive && GetOption(pauseinactive));
    const RGBA *pSAM = IO::GetPalette(fDimmed_), *pGUI = GUI::GetPalette();

    // Build the full palette from SAM and GUI colours
    for (int i = 0; i < N_TOTAL_COLOURS ; i++)
    {
        // Look up the colour in the appropriate palette
        const RGBA* p = (i < N_PALETTE_COLOURS) ? &pSAM[i] : &pGUI[i-N_PALETTE_COLOURS];
        BYTE bRed = p->bRed, bGreen = p->bGreen, bBlue = p->bBlue, bAlpha = p->bAlpha;

        // OpenGL?
        if (!pBack)
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
            aulPalette[i] = (bAlpha << 24) | (bBlue << 16) | (bGreen << 8) | bRed;
#else
            aulPalette[i] = (bRed << 24) | (bGreen << 16) | (bBlue << 8) | bAlpha;
#endif
        else if (!fPalette)
            aulPalette[i] = SDL_MapRGB(pBack->format, bRed, bGreen, bBlue);
        else
        {
            aulPalette[i] = i;

            acPalette[i].r = bRed;
            acPalette[i].g = bGreen;
            acPalette[i].b = bBlue;
        }
    }

    // If a palette is required, set it on both surfaces now
    if (fPalette)
    {
        SDL_SetPalette(pBack,  SDL_LOGPAL, acPalette, 0, N_TOTAL_COLOURS);
        SDL_SetPalette(pFront, SDL_LOGPAL|SDL_PHYSPAL, acPalette, 0, N_TOTAL_COLOURS);
    }

    // Because the pixel format may have changed, we need to refresh the SAM CLUT pixel values
    for (int c = 0 ; c < 16 ; c++)
        clut[c] = aulPalette[clutval[c]];

    // Ensure the display is redrawn to reflect the changes
    Display::SetDirty();

    return true;
}
