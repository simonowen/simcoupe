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

//  ToDo: possibly separate out the OpenGL and SDL code more?

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
WORD awY[N_TOTAL_COLOURS], awU[N_TOTAL_COLOURS], awV[N_TOTAL_COLOURS];

SDL_Surface *pBack, *pFront, *pIcon;


// Mask for the current SimCoupe.bmp image used as the SDL program icon
static BYTE abIconMask[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x1f, 0xe0, 0x00,
    0x00, 0x3f, 0xf8, 0x00, 0x00, 0x7f, 0xfc, 0x00,
    0x00, 0xff, 0xfe, 0x00, 0x01, 0xff, 0xff, 0x00,
    0x01, 0xff, 0xff, 0x00, 0x03, 0xff, 0xff, 0x80,
    0x03, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0x80,
    0x03, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0x80,
    0x08, 0x1f, 0xf0, 0x20, 0x1c, 0x7f, 0xfc, 0x70,
    0x3c, 0xff, 0xfe, 0x78, 0x7f, 0xff, 0xff, 0xfc,
    0x7f, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0xf8,
    0x03, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0x80,
    0x03, 0xff, 0xff, 0x80, 0x03, 0xff, 0xff, 0x80,
    0x03, 0xff, 0xff, 0x80, 0x00, 0xfc, 0x7e, 0x00,
    0x01, 0xfe, 0xff, 0x00, 0x01, 0xfe, 0xff, 0x00,
    0x01, 0xfe, 0xff, 0x00, 0x01, 0xfe, 0xff, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


#ifdef USE_OPENGL

GLuint dlist;
GLuint auTextures[6];
DWORD dwTextureData[6][256][256];


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

void TestFormat (GLenum format, GLenum type)
{
    glTexImage2D(GL_PROXY_TEXTURE_2D, 0, GL_RGB, 64, 64, 0, format, type, NULL);

    GLenum error = glGetError();
    if (error)
        TRACE("Format:%d type:%d  NOT supported\n");
    else
    {
        GLint r, g, b, a;
        r = g = b = a = 0;
        glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE, &r);
        glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_GREEN_SIZE, &g);
        glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_BLUE_SIZE, &b);
        glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0, GL_TEXTURE_ALPHA_SIZE, &a);

        TRACE("Format:%d type:%d  supported as:  [%d,%d,%d,%d]\n", r, g, b, a);
    }
}

void InitGL ()
{
    int nWidth = Frame::GetWidth(), nHeight = Frame::GetHeight();

    glViewport(0, 0, GetOption(ratio5_4) ? (nWidth * 5)/4 : nWidth, nHeight << 1);

    glEnable(GL_TEXTURE_2D);
    glGenTextures(6,auTextures);

    for (int i = 0 ; i < 6 ; i++)
    {
        glBindTexture(GL_TEXTURE_2D,auTextures[i]);

        bool fFiltered = true;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, fFiltered ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, fFiltered ? GL_LINEAR : GL_NEAREST);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, dwTextureData[i]);
        glBork("glTexImage2D");

        TestFormat(GL_RGB, GL_UNSIGNED_BYTE);
        TestFormat(GL_RGBA, GL_UNSIGNED_BYTE);
    }

    dlist=glGenLists(1);
    glNewList(dlist,GL_COMPILE);

    glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
    glClear(GL_COLOR_BUFFER_BIT);

    for (int yy = 0 ; yy < 2 ; yy++)
    {
        for (int xx = 0 ; xx < 3 ; xx++)
        {
            float flWidth = 2.0 * 256 / nWidth, flHeight = 256.0 / nHeight;
            float flX = -1.0 + (2.0 * 256 * xx / nWidth), flY = -1.0 + (256.0 * yy / nHeight);

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
            (dwWidth *= 5) >>= 2;

        const SDL_VideoInfo* pInfo = SDL_GetVideoInfo();
        int nDepth = GetOption(fullscreen) ? GetOption(depth) : pInfo->vfmt->BitsPerPixel;

        // Should the surfaces be in video memory?  (they need to be for any hardware acceleration)
        DWORD dwOptions = (GetOption(surface) >= 2) ? SDL_HWSURFACE : 0;

#ifdef USE_OPENGL
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

            if (GetOption(fullscreen))
                dwOptions |= SDL_FULLSCREEN;

            if (!(pFront = SDL_SetVideoMode(dwWidth, dwHeight, nDepth, (dwOptions | SDL_HWPALETTE | SDL_HWSURFACE | SDL_OPENGL))))
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

    fDimmed_ |= (g_fPaused && !g_fFrameStep) || GUI::IsActive() || (!g_fActive && GetOption(pauseinactive));
    const RGBA *pSAM = IO::GetPalette(fDimmed_), *pGUI = GUI::GetPalette();

    // Build the full palette from SAM and GUI colours
    for (int i = 0; i < N_TOTAL_COLOURS ; i++)
    {
        // Look up the colour in the appropriate palette
        const RGBA* p = (i < N_PALETTE_COLOURS) ? &pSAM[i] : &pGUI[i-N_PALETTE_COLOURS];
        BYTE bRed = p->bRed, bGreen = p->bGreen, bBlue = p->bBlue, bAlpha = p->bAlpha;

        if (!pBack)
            aulPalette[i] = (bAlpha << 24) | (bBlue << 16) | (bGreen << 8) | bRed;
        else if (!fPalette)
            aulPalette[i] = SDL_MapRGB(pBack->format, bRed, bGreen, bBlue);
        else
        {
            aulPalette[i] = PALETTE_OFFSET+i;

            acPalette[i].r = bRed;
            acPalette[i].g = bGreen;
            acPalette[i].b = bBlue;
        }
    }

    // If a palette is required, set it now
    if (fPalette)
        SDL_SetPalette(pBack, SDL_LOGPAL|SDL_PHYSPAL, acPalette, PALETTE_OFFSET, N_TOTAL_COLOURS);

    // Because the pixel format may have changed, we need to refresh the SAM CLUT pixel values
    for (int c = 0 ; c < 16 ; c++)
        clut[c] = aulPalette[clutval[c]];

    // Ensure the display is redrawn to reflect the changes
    Display::SetDirty();

    return true;
}
