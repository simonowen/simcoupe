// Part of SimCoupe - A SAM Coupe emulator
//
// Video.h: SDL video handling for surfaces, screen modes, palettes etc.
//
//  Copyright (c) 1999-2012 Simon Owen
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

#ifndef VIDEO_H
#define VIDEO_H

class Video
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

        static void Update ();
        static bool CreatePalettes ();
};


extern DWORD aulPalette[], aulScanline[];
extern SDL_Surface *pBack, *pFront;


#ifdef USE_OPENGL

#define glExtension(x)  !!strstr(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)), (x))

// Use a 1024x1024 texture for the display
#define TEX_WIDTH	1024
#define TEX_HEIGHT	1024

enum { TEX_DISPLAY, TEX_SCANLINE, TEX_COUNT };

extern GLuint dlist;
extern GLuint auTextures[];
extern DWORD dwTextureData[TEX_COUNT][TEX_WIDTH][TEX_HEIGHT];
extern GLenum g_glPixelFormat, g_glDataType;

#endif

#endif  // VIDEO_H
