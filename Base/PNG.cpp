// Part of SimCoupe - A SAM Coupé emulator
//
// PNG.cpp: Screenshot saving in PNG format
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

// ToDo:
//  - add support for saving the GUI too, which requires a special palette

// Notes:
//  This module uses definitions and information taken from the libpng
//  header files. See:  http://www.libpng.org/pub/png/libpng.html
//
//  This modules rules on Zlib for compression, and if USE_ZLIB is not
//  defined at compile time the whole implementation will be missing.
//  SaveImage() becomes a no-op, and the screenshot function will not work.

#include "SimCoupe.h"
#include "PNG.h"

#ifdef USE_ZLIB
#include "zlib.h"

#include "Frame.h"
#include "Options.h"


// 32-bit values in PNG data are always network byte order (big endian), so define a helper macro if a conversion is needed
#ifndef __BIG_ENDIAN__
#define ntohul(ul)  (((ul << 24) & 0xff000000) | ((ul << 8) & 0x00ff0000) | ((ul >> 8) & 0x0000ff00) | ((ul >> 24) & 0x000000ff))
#else
#define ntohul(ul)  (ul)
#endif


// Remove palette entries not used by the image
void OptimisePalette (PNG_INFO* pPNG_)
{
    BYTE abLookup[N_PALETTE_COLOURS], abOldPalette[3*N_PALETTE_COLOURS];
    memset(abLookup, 0, sizeof abLookup);
    memcpy(abOldPalette, pPNG_->pbPalette, 3*pPNG_->uPaletteSize);

    // Scan the entire imagine to check which palette colours are actually being used
    UINT u;
    for (u = 0 ; u < pPNG_->uSize ; u++)
        abLookup[pPNG_->pbImage[u]] = 1;

    // Reduce the palette to remove entries not used in the image
    UINT uPen = 0;
    for (u = 0, uPen = 0 ; u < pPNG_->uPaletteSize ; u++)
    {
        // Skip the entry if it's not being used
        if (!abLookup[u])
            continue;

        // Move the entry to the next free slot
        pPNG_->pbPalette[3*uPen+0] = abOldPalette[3*u+0];
        pPNG_->pbPalette[3*uPen+1] = abOldPalette[3*u+1];
        pPNG_->pbPalette[3*uPen+2] = abOldPalette[3*u+2];

        // Remember the new location of the old entry
        abLookup[u] = uPen++;
    }

    // Remap the palette colours in the image to the optimised version
    for (u = 0 ; u < pPNG_->uSize ; u++)
        pPNG_->pbImage[u] = abLookup[pPNG_->pbImage[u]];

    // Update the number of palette entries with the actual number in use
    pPNG_->uPaletteSize = uPen;
}


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
    PNG_IHDR ihdr;
    memset(&ihdr, 0, sizeof ihdr);
    ihdr.dwWidth = ntohul(pPNG_->dwWidth);
    ihdr.dwHeight = ntohul(pPNG_->dwHeight);
    ihdr.bBitDepth = 8;
    ihdr.bColourType = PNG_COLOR_TYPE_PALETTE;

    // Write everything out, returning true only if everything succeeds
    return ((fwrite(PNG_SIGNATURE, 1, sizeof PNG_SIGNATURE - 1, hFile_) == sizeof PNG_SIGNATURE - 1) &&
            WriteChunk(hFile_, PNG_CN_IHDR, reinterpret_cast<BYTE*>(&ihdr), sizeof ihdr) &&
            WriteChunk(hFile_, PNG_CN_PLTE, pPNG_->pbPalette, pPNG_->uPaletteSize*3) &&
            WriteChunk(hFile_, PNG_CN_IDAT, pPNG_->pbImage, pPNG_->uCompressedSize) &&
            WriteChunk(hFile_, PNG_CN_tEXt, reinterpret_cast<BYTE*>(szProgram), strlen(szProgram)) &&
            WriteChunk(hFile_, PNG_CN_IEND, NULL, 0));
}


// ZLib compress the image data (the default, and currently only method for PNG)
static bool CompressImageData (PNG_INFO* pPNG_)
{
    bool fRet = false;

    // ZLib says the compressed size could be at least 0.1% more than the source, plus 12 bytes
    DWORD dwSize = ((pPNG_->uSize * 1001) / 1000) + 12;
    BYTE* pbCompressed = new BYTE[dwSize];

    if (pbCompressed)
    {
        // Compress the image, but clean-up if we don't manage it
        if (compress(pbCompressed, &dwSize, pPNG_->pbImage, pPNG_->uSize) == Z_OK)
        {
            // Delete the uncompressed version
            delete pPNG_->pbImage;

            // Save the compressed image and size
            pPNG_->uCompressedSize = dwSize;
            pPNG_->pbImage = pbCompressed;
            pbCompressed = NULL;

            // Success :-)
            fRet = true;
        }

        // Failed, so just clean up
        else
            delete pbCompressed;

    }

    return fRet;
}


// Process and save the supplied SAM image data to a file in PNG format
bool SaveImage (FILE* hFile_, CScreen* pScreen_)
{
    bool fRet = false;
    PNG_INFO png = { 0 };

    // The generated image includes the full screen size, but requires the height to be doubled
    png.dwWidth = pScreen_->GetPitch();
    png.dwHeight = pScreen_->GetHeight();

    // Allocate enough space for the palette
    png.uPaletteSize = N_PALETTE_COLOURS;
    png.pbPalette = new BYTE[3 * png.uPaletteSize];

    // Work out the image size, allocate a block for it and zero it
    png.uSize = png.dwHeight * (png.dwWidth+1);
    png.pbImage = new BYTE[png.uSize];
    memset(png.pbImage, 0, png.dwHeight * (png.dwWidth+1));

    if (png.pbPalette && png.pbImage)
    {
        static const BYTE ab[] = { 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff };

        for (int c = 0; c < N_PALETTE_COLOURS; c++)
        {
            png.pbPalette[3*c]   = ab[(c&0x02)     | ((c&0x20) >> 3) | ((c&0x08) >> 3)];
            png.pbPalette[3*c+1] = ab[(c&0x04) >> 1| ((c&0x40) >> 4) | ((c&0x08) >> 3)];
            png.pbPalette[3*c+2] = ab[(c&0x01) << 1| ((c&0x10) >> 2) | ((c&0x08) >> 3)];
        }

        // If scanlines are enabled, we'll skip every other line
        UINT uStep = GetOption(scanlines) ? 2 : 1;

        // Copy the image data, leaving a filter byte zero before each line
        for (UINT u = 0; u < png.dwHeight ; u += uStep)
            memcpy(&png.pbImage[1 + u*(png.dwWidth+1)], pScreen_->GetHiResLine(u >> 1), png.dwWidth);

        // Remove unused palette entries
        OptimisePalette(&png);

        // Compress the data and write it out to the file
        fRet = CompressImageData(&png) && WriteFile(hFile_, &png);
    }

    // Clean up now we're done
    delete png.pbPalette;
    delete png.pbImage;

    return fRet;
}

#endif  // USE_ZLIB
