// Part of SimCoupe - A SAM Coupe emulator
//
// PNG.cpp: Screenshot saving in PNG format
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

// Notes:
//  This module uses definitions and information taken from the libpng
//  header files. See:  http://www.libpng.org/pub/png/libpng.html
//
//  This modules relies on Zlib for compression, and if USE_ZLIB is not
//  defined at compile time the whole implementation will be missing.
//  SaveImage() becomes a no-op, and the screenshot function will not work.

#include "SimCoupe.h"
#include "PNG.h"

#include "zlib.h"

#include "Frame.h"
#include "Options.h"

namespace PNG
{

#ifdef USE_ZLIB

// 32-bit values in PNG data are always network byte order (big endian), so define a helper macro if a conversion is needed
#ifndef __BIG_ENDIAN__
#define htonul(ul)  (((ul << 24) & 0xff000000) | ((ul << 8) & 0x00ff0000) | ((ul >> 8) & 0x0000ff00) | ((ul >> 24) & 0x000000ff))
#else
#define htonul(ul)  (ul)
#endif


// Write a PNG chunk block with header and CRC
static bool WriteChunk (FILE* hFile_, DWORD dwType_, BYTE* pbData_, size_t uLength_)
{
    // Write chunk length
    DWORD dw = static_cast<DWORD>(htonul(uLength_));
    size_t uWritten = fwrite(&dw, 1, sizeof(dw), hFile_);

    // Write type (big endian) and start CRC with it
    dw = htonul(dwType_);
    uWritten += fwrite(&dw, 1, sizeof(dw), hFile_);
    auto crc = crc32(0, reinterpret_cast<UINT8*>(&dw), sizeof(dw));

    if (uLength_)
    {
        // Write and chunk data and include in CRC
        uWritten += fwrite(pbData_, 1, uLength_, hFile_);
        crc = crc32(crc, pbData_, static_cast<uInt>(uLength_));
    }

    // Write CRC (big endian)
    dw = static_cast<DWORD>(htonul(crc));
    uWritten += fwrite(&dw, 1, sizeof(dw), hFile_);

    // Return true if we wrote everything
    return uWritten == ((3 * sizeof(dw)) + uLength_);
}


// Write a pre-prepared image out to disk
static bool WriteFile (FILE* hFile_, PNG_INFO* pPNG_)
{
    char szProgram[] = "SimCoupe";

    // Prepare the image header describing what we've got
    PNG_IHDR ihdr {};
    ihdr.abWidth[0] = (pPNG_->dwWidth >> 24) & 0xff;
    ihdr.abWidth[1] = (pPNG_->dwWidth >> 16) & 0xff;
    ihdr.abWidth[2] = (pPNG_->dwWidth >> 8) & 0xff;
    ihdr.abWidth[3] = pPNG_->dwWidth & 0xff;
    ihdr.abHeight[0] = (pPNG_->dwHeight >> 24) & 0xff;
    ihdr.abHeight[1] = (pPNG_->dwHeight >> 16) & 0xff;
    ihdr.abHeight[2] = (pPNG_->dwHeight >> 8) & 0xff;
    ihdr.abHeight[3] = pPNG_->dwHeight & 0xff;
    ihdr.bBitDepth = 8;
    ihdr.bColourType = PNG_COLOR_MASK_COLOR;
    ihdr.bCompressionType = PNG_COMPRESSION_TYPE_BASE;
    ihdr.bFilterType = PNG_FILTER_TYPE_DEFAULT;
    ihdr.bInterlaceType = PNG_INTERLACE_NONE;

    // Write everything out, returning true only if everything succeeds
    return ((fwrite(PNG_SIGNATURE, 1, sizeof(PNG_SIGNATURE)-1, hFile_) == sizeof(PNG_SIGNATURE)-1) &&
            WriteChunk(hFile_, PNG_CN_IHDR, reinterpret_cast<BYTE*>(&ihdr), sizeof(ihdr)) &&
            WriteChunk(hFile_, PNG_CN_IDAT, pPNG_->pbImage, pPNG_->uCompressedSize) &&
            WriteChunk(hFile_, PNG_CN_tEXt, reinterpret_cast<BYTE*>(szProgram), strlen(szProgram)) &&
            WriteChunk(hFile_, PNG_CN_IEND, nullptr, 0));
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
        pPNG_->uCompressedSize = static_cast<DWORD>(ulSize);
        pPNG_->pbImage = pbCompressed;
        pbCompressed = nullptr;

        // Success :-)
        fRet = true;
    }

    delete[] pbCompressed;
    return fRet;
}


static bool SaveFile (FILE *f_, CScreen *pScreen_)
{
    // Are we to stretch the saved image?
    int nDen = 5, nNum = 4;
    bool fStretch = GetOption(ratio5_4);

    // Calculate the intensity reduction for scanlines, in the range -100 to +100
    int nScanAdjust = (GetOption(scanlines) && !GetOption(scanhires)) ? (GetOption(scanlevel) - 100) : 0;
    if (nScanAdjust < -100) nScanAdjust = -100;

    PNG_INFO png {};
    png.dwWidth = pScreen_->GetPitch();
    png.dwHeight = pScreen_->GetHeight();
    if (fStretch) png.dwWidth = png.dwWidth *nDen/nNum;

    png.uSize = png.dwHeight * (1 + (png.dwWidth * 3));
    if (!(png.pbImage = new BYTE[png.uSize]))
        return false;

    memset(png.pbImage, 0, png.uSize);
    const COLOUR *pPal = IO::GetPalette();


    BYTE *pb = png.pbImage;

    for (UINT y = 0; y < png.dwHeight ; y++)
    {
        BYTE *pbS = pScreen_->GetLine(y >> 1);

        // Each image line begins with the filter type
        *pb++ = PNG_FILTER_TYPE_DEFAULT;

        for (UINT x = 0 ; x < png.dwWidth ; x++)
        {
            // Map the image pixel back to the display pixel
            int n = fStretch ? (x * nNum/nDen) : x;
            BYTE b = pbS[n], b2 = pbS[n+1];

            // Look up the pixel components in the palette
            BYTE red = pPal[b].bRed, green = pPal[b].bGreen, blue = pPal[b].bBlue;

            // In stretch mode we may need to blend the neighbouring pixels
            if (fStretch && (x % nDen))
            {
                // Determine how much of the current pixel to use
                int nPercent = (x%nDen)*100/nNum;
                AdjustBrightness(red, green, blue, nPercent-100);

                // Determine how much of the next pixel
                int nPercent2 = 100-nPercent;
                BYTE red2 = pPal[b2].bRed, green2 = pPal[b2].bGreen, blue2 = pPal[b2].bBlue;
                AdjustBrightness(red2, green2, blue2, nPercent2-100);

                // Combine the part pixels for the overall colour
                red += red2;
                green += green2;
                blue += blue2;
            }

            // Odd lines are dimmed if scanlines are enabled
            if (nScanAdjust && (y & 1))
                AdjustBrightness(red, green, blue, nScanAdjust);

            // Add the pixel to the image data
            *pb++ = red;
            *pb++ = green;
            *pb++ = blue;
        }
    }

    // Compress and write the image
    bool fRet = CompressImageData(&png) && WriteFile(f_, &png);
    delete[] png.pbImage; png.pbImage = nullptr;

    return fRet;
}

#endif // USE_ZLIB


// Process and save the supplied SAM image data to a file in PNG format
bool Save (CScreen* pScreen_)
{
    bool fRet = false;

#ifdef USE_ZLIB
    char szPath[MAX_PATH], *pszFile;

    // Create a unique filename in the format snapNNNN.png
    pszFile = Util::GetUniqueFile("png", szPath, sizeof(szPath));

    // Create the new file
    FILE* f = fopen(szPath, "wb");
    if (!f)
    {
        Frame::SetStatus("Failed to open %s for writing!", szPath);
        return false;
    }

    // Perform the actual save
    fRet = SaveFile(f, pScreen_);
    fclose(f);

    // Report what happened
    if (fRet)
        Frame::SetStatus("Saved %s", pszFile);
    else
        Frame::SetStatus("PNG save failed!?");
#else
    Frame::SetStatus("Screen saving requires zLib");
#endif

    return fRet;
}

} // namespace PNG
