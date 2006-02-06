// Part of SimCoupe - A SAM Coupe emulator
//
// PNG.cpp: Screenshot saving in PNG format
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

// Notes:
//  This module uses definitions and information taken from the libpng
//  header files. See:  http://www.libpng.org/pub/png/libpng.html
//
//  This modules relies on Zlib for compression, and if USE_ZLIB is not
//  defined at compile time the whole implementation will be missing.
//  SaveImage() becomes a no-op, and the screenshot function will not work.

#include "SimCoupe.h"
#include "PNG.h"

#ifdef USE_ZLIB
#include "zlib.h"

#include "GUI.h"
#include "Options.h"
#include "Util.h"


// 32-bit values in PNG data are always network byte order (big endian), so define a helper macro if a conversion is needed
#ifndef __BIG_ENDIAN__
#define ntohul(ul)  (((ul << 24) & 0xff000000) | ((ul << 8) & 0x00ff0000) | ((ul >> 8) & 0x0000ff00) | ((ul >> 24) & 0x000000ff))
#else
#define ntohul(ul)  (ul)
#endif


// Write a PNG chunk block with header and CRC
static bool WriteChunk (FILE* hFile_, DWORD dwType_, BYTE* pbData_, size_t uLength_)
{
    // Write chunk length
    DWORD dw = static_cast<DWORD>(ntohul(uLength_));
    size_t uWritten = fwrite(&dw, 1, sizeof dw, hFile_);

    // Write type (big endian) and start CRC with it
    dw = ntohul(dwType_);
    uWritten += fwrite(&dw, 1, sizeof dw, hFile_);
    DWORD crc = crc32(0, reinterpret_cast<UINT8*>(&dw), sizeof dw);

    if (uLength_)
    {
        // Write and chunk data and include in CRC
        uWritten += fwrite(pbData_, 1, uLength_, hFile_);
        crc = crc32(crc, pbData_, static_cast<uInt>(uLength_));
    }

    // Write CRC (big endian)
    dw = ntohul(crc);
    uWritten += fwrite(&dw, 1, sizeof dw, hFile_);

    // Return true if we wrote everything
    return uWritten == ((3 * sizeof dw) + uLength_);
}


// Write a pre-prepared image out to disk
static bool WriteFile (FILE* hFile_, PNG_INFO* pPNG_)
{
    char szProgram[] = "SimCoupe";

    // Prepare the image header describing what we've got
    PNG_IHDR ihdr = { 0 };
    ihdr.dwWidth = ntohul(pPNG_->dwWidth);
    ihdr.dwHeight = ntohul(pPNG_->dwHeight);
    ihdr.bBitDepth = 8;
    ihdr.bColourType = PNG_COLOR_MASK_COLOR;
    ihdr.bCompressionType = PNG_COMPRESSION_TYPE_BASE;
    ihdr.bFilterType = PNG_FILTER_TYPE_DEFAULT;
    ihdr.bInterlaceType = PNG_INTERLACE_NONE;

    // Write everything out, returning true only if everything succeeds
    return ((fwrite(PNG_SIGNATURE, 1, sizeof PNG_SIGNATURE - 1, hFile_) == sizeof PNG_SIGNATURE - 1) &&
            WriteChunk(hFile_, PNG_CN_IHDR, reinterpret_cast<BYTE*>(&ihdr), sizeof ihdr) &&
            WriteChunk(hFile_, PNG_CN_IDAT, pPNG_->pbImage, pPNG_->uCompressedSize) &&
            WriteChunk(hFile_, PNG_CN_tEXt, reinterpret_cast<BYTE*>(szProgram), strlen(szProgram)) &&
            WriteChunk(hFile_, PNG_CN_IEND, NULL, 0));
}


// ZLib compress the image data (the default, and currently only method for PNG)
static bool CompressImageData (PNG_INFO* pPNG_)
{
    bool fRet = false;

    // ZLib says the compressed size could be at least 0.1% more than the source, plus 12 bytes
    uLongf ulSize = ((pPNG_->uSize * 1001) / 1000) + 12;
    BYTE* pbCompressed = new BYTE[ulSize];

    // Compress the image data
    if (pbCompressed && compress(pbCompressed, &ulSize, pPNG_->pbImage, pPNG_->uSize) == Z_OK)
    {
        // Delete the uncompressed version
        delete[] pPNG_->pbImage;

        // Save the compressed image and size
        pPNG_->uCompressedSize = ulSize;
        pPNG_->pbImage = pbCompressed;
        pbCompressed = NULL;

        // Success :-)
        fRet = true;
    }

    delete[] pbCompressed;
    return fRet;
}


// Process and save the supplied SAM image data to a file in PNG format
bool SaveImage (FILE* hFile_, CScreen* pScreen_)
{
    // In 5:4 mode we need to stretch the output image
    bool fStretch = GetOption(ratio5_4);

    // Calculate the intensity reduction for scanlines, in the range -100 to +100
    int nScanAdjust = GetOption(scanlines) ? (GetOption(scanlevel) - 100) : 0;
    if (nScanAdjust < -100) nScanAdjust = -100;

    PNG_INFO png = {0};
    png.dwWidth = pScreen_->GetPitch();
    png.dwHeight = pScreen_->GetHeight();
    if (fStretch) png.dwWidth = png.dwWidth *5/4;

    png.uSize = png.dwHeight * (1 + (png.dwWidth * 3));
    if (!(png.pbImage = new BYTE[png.uSize]))
        return false;

    memset(png.pbImage, 0, png.uSize);
    const RGBA* pPal = IO::GetPalette();


    BYTE *pb = png.pbImage;

    for (UINT y = 0; y < png.dwHeight ; y++)
    {
        BYTE *pbS = pScreen_->GetHiResLine(y >> 1);

        // Each image line begins with the filter type
        *pb++ = PNG_FILTER_TYPE_DEFAULT;

        for (UINT x = 0 ; x < png.dwWidth ; x++)
        {
            // Map the image pixel back to the display pixel, allowing for 5:4 mode
            int n = fStretch ? (x * 4/5) : x;
            BYTE b = pbS[n];

            // Look up the pixel components in the palette
            BYTE red = pPal[b].bRed, green = pPal[b].bGreen, blue = pPal[b].bBlue;

            // In 5:4 mode, 3/4 of pixels require blending with neighbouring pixels for output
            if (fStretch && (n&3))
            {
                // Determine how much of the original pixel is on the left
                int nPercent = 25*(n&3);
                AdjustBrightness(red, green, blue, nPercent-100);

                // Determine how much of the neighbouring pixel is on the right
                BYTE b2 = pbS[n+1];
                BYTE red2 = pPal[b2].bRed, green2 = pPal[b2].bGreen, blue2 = pPal[b2].bBlue;
                AdjustBrightness(red2, green2, blue2, -nPercent);

                // Combine the part pixels for the overall colour
                red += red2;
                green += green2;
                blue += blue2;
            }

            // Odd lines are dimmed if scanlines are enabled
            if (nScanAdjust && (y & 1))
                AdjustBrightness(red, green, blue, nScanAdjust);

            // Add the pixel to the image data
            *pb++ = red, *pb++ = green, *pb++ = blue;
        }
    }

    // Compress and write the image
    bool fRet = CompressImageData(&png) && WriteFile(hFile_, &png);
    delete[] png.pbImage;
    return fRet;
}

#endif  // USE_ZLIB
