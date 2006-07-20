// Part of SimCoupe - A SAM Coupe emulator
//
// PNG.h: Screenshot saving in PNG format
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

#ifndef PNG_H
#define PNG_H

#ifdef USE_ZLIB

#include "CScreen.h"

// Define just the stuff we need - taken from libPNG's png.h

typedef unsigned short      UINT16;
typedef unsigned char       UINT8;

#define PNG_SIGNATURE       "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"

#define PNG_CN_IHDR         0x49484452L
#define PNG_CN_IDAT         0x49444154L
#define PNG_CN_IEND         0x49454E44L
#define PNG_CN_tEXt         0x74455874L

#define PNG_COLOR_MASK_COLOR        2   // RGB
#define PNG_COMPRESSION_TYPE_BASE   0   // Deflate method 8, 32K window
#define PNG_FILTER_TYPE_DEFAULT     0   // Single row per-byte filtering
#define PNG_INTERLACE_NONE          0   // Non-interlaced image


// PNG header structure must be byte-packed
#pragma pack(1)

typedef struct tagPNG_IHDR
{
    DWORD   dwWidth;
    DWORD   dwHeight;
    BYTE    bBitDepth;
    BYTE    bColourType;
    BYTE    bCompressionType;
    BYTE    bFilterType;
    BYTE    bInterlaceType;
}
PNG_IHDR, *PPNG_IHDR;

#pragma pack()


// PNG support
typedef struct tagPNG_INFO
{
    DWORD dwWidth, dwHeight;
    BYTE* pbImage;
    UINT uSize, uCompressedSize;
}
PNG_INFO, *PPNG_INFO;


bool SaveImage (FILE* hFile_, CScreen* pScreen_);

#endif  // USE_ZLIB

#endif  // PNG_H
