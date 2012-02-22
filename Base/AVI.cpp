// Part of SimCoupe - A SAM Coupe emulator
//
// AVI.cpp: AVI movie recording
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

#include "SimCoupe.h"
#include "AVI.h"

#include "Frame.h"
#include "Options.h"
#include "Sound.h"

static BYTE *pbCurr;

static char szPath[MAX_PATH];
static FILE *f;

static WORD width, height;
static bool fHalfSize = false;

static DWORD dwRiffPos, dwMoviPos;
static DWORD dwVideoMax, dwAudioMax;
static DWORD dwVideoFrames, dwAudioFrames, dwAudioSamples;
static bool fWantVideo;
static int nNext;


static void WriteLittleEndianWORD (WORD w_)
{
    fputc(w_ & 0xff, f);
    fputc(w_ >> 8, f);
}

static void WriteLittleEndianDWORD (DWORD dw_)
{
#ifdef __LITTLE_ENDIAN__
    fwrite(&dw_, sizeof(dw_), 1, f);
#else
    fputc(dw_ & 0xff, f);
    fputc((dw_ >> 8) & 0xff, f);
    fputc((dw_ >> 16) & 0xff, f);
    fputc((dw_ >> 24) & 0xff, f);
#endif
}

static DWORD ReadLittleEndianDWORD ()
{
#ifdef __LITTLE_ENDIAN__
    DWORD dw;
    fread(&dw, sizeof(dw), 1, f);
    return dw;
#else
    BYTE ab[4];
    fread(ab, sizeof(ab), 1, f);

    return (ab[3] << 24) | (ab[2] << 16) | (ab[1] << 8) | ab[0];
#endif
}

static DWORD WriteChunkStart (FILE *f_, const char *pszChunk_, const char *pszType_=NULL)
{
    // Write the chunk type
    if (pszChunk_ && !fwrite(pszChunk_, 4, 1, f_))
        return 0;

    // Remember the length offset, and skip the length field (to be completed by WriteChunkEnd)
    DWORD dwPos = ftell(f_);
    fseek(f_, sizeof(DWORD), SEEK_CUR);

    // If we have a type, write that too
    if (pszType_)
        fwrite(pszType_, 4, 1, f_);

    // Return the offset to the length, so it can be completed later using WriteChunkEnd
    return dwPos;
}

static DWORD WriteChunkEnd (FILE *f_, DWORD dwPos_)
{
    // Remember the current position, and calculate the chunk size (not including the length field)
    DWORD dwPos = ftell(f_), dwSize = dwPos-dwPos_-sizeof(DWORD);
    
    // Seek back to the length field
    fseek(f_, dwPos_, SEEK_SET);

    // Write the chunk size
    WriteLittleEndianDWORD(dwSize);

    // Restore original position (should always be end of file, but we'll use the value from earlier)
    fseek(f_, dwPos, SEEK_SET);

    // Return the data size
    return dwSize;
}

static void WriteAVIHeader (FILE *f_)
{
    DWORD dwPos = WriteChunkStart(f_, "avih");

    WriteLittleEndianDWORD(19968);			// microseconds per frame: 1000000*TSTATES_PER_FRAME/REAL_TSTATES_PER_SECOND
    WriteLittleEndianDWORD((dwVideoMax*EMULATED_FRAMES_PER_SECOND)+(dwAudioMax*EMULATED_FRAMES_PER_SECOND));	// approximate max data rate
    WriteLittleEndianDWORD(0);				// reserved
    WriteLittleEndianDWORD((1<<8)|(1<<4));	// flags: bit 4 = has index(idx1), bit 5 = use index for AVI structure, bit 8 = interleaved file, bit 16 = optimized for live video capture, bit 17 = copyrighted data
    WriteLittleEndianDWORD(dwVideoFrames);	// total number of video frames
    WriteLittleEndianDWORD(0);				// initial frame number for interleaved files
    WriteLittleEndianDWORD(2);				// number of streams in the file (video+audio)
    WriteLittleEndianDWORD(0/*dwVideoMax*/);// suggested buffer size for reading the file
    WriteLittleEndianDWORD(width);			// pixel width
    WriteLittleEndianDWORD(height);			// pixel height
    WriteLittleEndianDWORD(0);				// 4 reserved DWORDs (must be zero)
    WriteLittleEndianDWORD(0);
    WriteLittleEndianDWORD(0);
    WriteLittleEndianDWORD(0);

    WriteChunkEnd(f_, dwPos);
}

static void WriteVideoHeader (FILE *f_)
{
    DWORD dwPos = WriteChunkStart(f_, "strh", "vids");

    fwrite("mrle", 4, 1, f);				// 'mrle' = Microsoft Run Length Encoding Video Codec
    WriteLittleEndianDWORD(0);				// flags, unused
    WriteLittleEndianDWORD(0);				// priority and language, unused
    WriteLittleEndianDWORD(0);				// initial frames
    WriteLittleEndianDWORD(TSTATES_PER_FRAME); // scale
    WriteLittleEndianDWORD(REAL_TSTATES_PER_SECOND); // rate
    WriteLittleEndianDWORD(0);				// start time
    WriteLittleEndianDWORD(dwVideoFrames);	// total frames in stream
    WriteLittleEndianDWORD(dwVideoMax);		// suggested buffer size
    WriteLittleEndianDWORD(10000);			// quality
    WriteLittleEndianDWORD(0);				// sample size
    WriteLittleEndianWORD(0);				// left
    WriteLittleEndianWORD(0);				// top
    WriteLittleEndianWORD(width);			// right
    WriteLittleEndianWORD(height);			// bottom

    WriteChunkEnd(f_, dwPos);

    dwPos = WriteChunkStart(f_, "strf");

    WriteLittleEndianDWORD(40);				// sizeof(BITMAPINFOHEADER)
    WriteLittleEndianDWORD(width);			// biWidth;
    WriteLittleEndianDWORD(height);			// biHeight;
    WriteLittleEndianWORD(1);				// biPlanes;
    WriteLittleEndianWORD(8);				// biBitCount (8 = 256 colours)
    WriteLittleEndianDWORD(1);				// biCompression (1 = BI_RLE8)
    WriteLittleEndianDWORD(width*height);	// biSizeImage;
    WriteLittleEndianDWORD(0);				// biXPelsPerMeter;
    WriteLittleEndianDWORD(0);				// biYPelsPerMeter;
    WriteLittleEndianDWORD(256);			// biClrUsed;
    WriteLittleEndianDWORD(0);				// biClrImportant;

    const COLOUR *pcPal = IO::GetPalette();
    int i;

    // The first half of the palette contains SAM colours
    for (i = 0 ; i < N_PALETTE_COLOURS ; i++)
    {
        // Note: colour order is BGR
        fputc(pcPal[i].bBlue, f);
        fputc(pcPal[i].bGreen, f);
        fputc(pcPal[i].bRed, f);
        fputc(0, f);	// RGBQUAD has this as reserved (zero) rather than alpha
    }

    // If scanlines are enabled, determine the appropriate brightness adjustment
    int nScanAdjust = GetOption(scanlines) ? (GetOption(scanlevel) - 100) : 0;
    if (nScanAdjust < -100) nScanAdjust = -100;

    // The second half of the palette contains colours in scanline intensity
    for (i = 0 ; i < N_PALETTE_COLOURS ; i++)
    {
        BYTE r = pcPal[i].bRed, g = pcPal[i].bGreen, b = pcPal[i].bBlue;
        AdjustBrightness(r,g,b, nScanAdjust);

        // Note: colour order is BGR
        fputc(b, f);
        fputc(g, f);
        fputc(r, f);
        fputc(0, f);	// RGBQUAD has this as reserved (zero) rather than alpha
    }

    WriteChunkEnd(f_, dwPos);
}

static void WriteAudioHeader (FILE *f_)
{
    DWORD dwPos = WriteChunkStart(f_, "strh", "auds");

    fwrite("\0\0\0\0", 4, 1, f);			// FOURCC not specified (PCM below)
    WriteLittleEndianDWORD(0);				// flags, unused
    WriteLittleEndianDWORD(0);				// priority and language, unused
    WriteLittleEndianDWORD(1);				// initial frames
    WriteLittleEndianDWORD(SAMPLE_BLOCK);	// scale
    WriteLittleEndianDWORD(SAMPLE_FREQ*SAMPLE_BLOCK); // rate
    WriteLittleEndianDWORD(0);				// start time
    WriteLittleEndianDWORD(dwAudioSamples);	// total samples in stream
    WriteLittleEndianDWORD(dwAudioMax);		// suggested buffer size
    WriteLittleEndianDWORD(0xffffffff);		// quality
    WriteLittleEndianDWORD(SAMPLE_BLOCK);	// sample size
    WriteLittleEndianDWORD(0);				// two unused rect coords
    WriteLittleEndianDWORD(0);				// two more unused rect coords

    WriteChunkEnd(f_, dwPos);

    dwPos = WriteChunkStart(f_, "strf");

    WriteLittleEndianWORD(1);				// format tag (1 = WAVE_FORMAT_PCM)
    WriteLittleEndianWORD(SAMPLE_CHANNELS);	// channels
    WriteLittleEndianDWORD(SAMPLE_FREQ);	// samples per second
    WriteLittleEndianDWORD(SAMPLE_FREQ*SAMPLE_BLOCK); // average bytes per second
    WriteLittleEndianWORD(SAMPLE_BLOCK);	// block align
    WriteLittleEndianWORD(SAMPLE_BITS);		// bits per sample
    WriteLittleEndianWORD(0);				// extra structure size

    WriteChunkEnd(f_, dwPos);
}

static void WriteIndex (FILE *f_)
{
    // The chunk index is the final addition
    DWORD dwIdx1Pos = WriteChunkStart(f_, "idx1");
    fseek(f, -4, SEEK_CUR);
    WriteLittleEndianDWORD(0);

    // Locate the start of the movi data (after the chunk header)
    dwMoviPos += 2*sizeof(DWORD);

    // Calculate the number of entries in the index
    DWORD dwIndexEntries = dwVideoFrames + dwAudioFrames;
    DWORD dwVideoFrame = 0;

    // Loop through all frames in the file
    for (UINT u = 0 ; u < dwIndexEntries ; u++)
    {
        BYTE abType[4];

        // Read the type and size from the movi chunk
        fseek(f, dwMoviPos, SEEK_SET);
        fread(abType, sizeof(abType), 1, f);
        DWORD dwSize = ReadLittleEndianDWORD();
        fseek(f, 0, SEEK_END);

        // Every 50th frame is a key frame
        bool fKeyFrame = (abType[1] == '0') && !(dwVideoFrame++ % EMULATED_FRAMES_PER_SECOND);

        // Write the type, flags, offset and size to the index
        fwrite(abType, sizeof(abType), 1, f);
        WriteLittleEndianDWORD(fKeyFrame ? 0x10 : 0x00);
        WriteLittleEndianDWORD(dwMoviPos);
        WriteLittleEndianDWORD(dwSize);

        dwMoviPos += 2*sizeof(DWORD) + dwSize;
    }

    // Complete the index and riff chunks
    WriteChunkEnd(f_, dwIdx1Pos);
}

static void WriteFileHeaders (FILE *f_)
{
    fseek(f_, 0, SEEK_SET);

    dwRiffPos = WriteChunkStart(f_, "RIFF", "AVI ");
    DWORD dwHdrlPos = WriteChunkStart(f_, "LIST", "hdrl");

    WriteAVIHeader(f_);

    DWORD dwPos = WriteChunkStart(f_, "LIST", "strl");
    WriteVideoHeader(f_);
    WriteChunkEnd(f_, dwPos);

    dwPos = WriteChunkStart(f_, "LIST", "strl");
    WriteAudioHeader(f_);
    WriteChunkEnd(f_, dwPos);
/*
    dwPos = WriteChunkStart("vedt");
    fseek(f, 8, SEEK_CUR);
    WriteChunkEnd(dwPos);
*/
    // Align movi data to 2048-byte boundary
    dwPos = WriteChunkStart(f_, "JUNK");
    fseek(f_, (-ftell(f)-3*sizeof(DWORD))&0x3ff, SEEK_CUR);
    WriteChunkEnd(f_, dwPos);

    WriteChunkEnd(f_, dwHdrlPos);

    // Start of movie data
    dwMoviPos = WriteChunkStart(f_, "LIST", "movi");
}



static int FindRunFragment (BYTE *pb_, BYTE *pbP_, int nWidth_, int *pnJump_)
{
    int x, nRun = 0;

    for (x = 0 ; x < nWidth_ ; x++, pb_++, pbP_++)
    {
        // Include matching pixels in the run
        if (*pb_ == *pbP_)
            nRun++;
        // Ignore runs below the jump overhead
        else if (nRun < 4)
            nRun = 0;
        // Accept the run fragment
        else
            break;
    }

    // Is the run length below the jump overhead?
    if (nRun < 4)
    {
        // Encode the full block
        *pnJump_ = 0;
        return nWidth_;
    }

    // Return the jump length over matching pixels and fragment length before it
    *pnJump_ = nRun;
    return x - nRun;
}

static void EncodeAbsolute (BYTE *pb_, int nLength_)
{
    // Short lengths conflict with RLE codes, and must be encoded as colour runs instead
    if (nLength_ < 3)
    {
        while (nLength_--)
        {
            fputc(0x01, f);		// length=1
            fputc(*pb_++, f);	// colour
        }
    }
    else
    {
        fputc(0x00, f);					// escape
        fputc(nLength_, f);				// length
        fwrite(pb_, nLength_, 1, f);	// data

        // Absolute blocks must maintain 16-bit alignment, so output a dummy byte if necessary
        if (nLength_ & 1)
            fputc(0x00, f);
    }
}

static void EncodeBlock (BYTE *pb_, int nLength_)
{
    while (nLength_)
    {
        // Maximum run length is 255 or the width of the fragment
        int nMaxRun = min(255,nLength_);

        // Start with a run of 1 pixel
        int nRun = 1;
        BYTE bColour = *pb_++;

        // Find the extent of the solid run
        for ( ; *pb_ == bColour && nRun < nMaxRun ; nRun++, pb_++);

        // If the run is more than one byte, outputting as a colour run is the best we can do
        // Also do this if the fragment is too short to use an absolute run
        if (nRun > 1 || nLength_ < 3)
        {
            fputc(nRun, f);
            fputc(bColour, f);
            nLength_ -= nRun;

            // Try for another colour run
            continue;
        }

        // Single pixel runs become the start of an absolute block
        pb_--;

        for (nRun = 0 ; nRun < nMaxRun ; nRun++)
        {
            // If there's not enough left for a colour run, include it in the absolute block
            if ((nMaxRun - nRun) < 4)
                nRun = nMaxRun-1;
            // Break if we find 4 identical pixels ahead of us, which can be encoded as a run
            else if (pb_[nRun+0] == pb_[nRun+1] && pb_[nRun+1] == pb_[nRun+2] && pb_[nRun+2] == pb_[nRun+3])
                break;
        }

        EncodeAbsolute(pb_, nRun);
        pb_ += nRun;
        nLength_ -= nRun;

#if defined(_DEBUG) && defined(WIN32)
        if (nLength_ < 0)
            DebugBreak();
#endif
    }
}


bool AVI::Start (bool fHalfSize_)
{
    if (f)
        return false;

    // Find a unique filename to use, in the format movNNNN.avi
    char szTemplate[MAX_PATH];
    snprintf(szTemplate, MAX_PATH, "%smov%%04d.avi", OSD::GetDirPath(GetOption(datapath)));
    nNext = Util::GetUniqueFile(szTemplate, nNext, szPath, sizeof(szPath));

    // Create the file
    f = fopen(szPath, "wb+");
    if (!f)
        return false;

    // Reset the frame counters
    dwVideoFrames = dwAudioFrames = dwAudioSamples = 0;
    dwVideoMax = dwAudioMax = 0;

    // Set the size and flag we want a video frame first
    fHalfSize = fHalfSize_;
    fWantVideo = true;

    Frame::SetStatus("Recording AVI");
    return true;
}

void AVI::Stop ()
{
    // Ignore if we're not recording
    if (!f)
        return;

    // Silence the sound in case index generation is slow
    Sound::Silence();

    // Complete the movi chunk, add the index, and complete the RIFF
    WriteChunkEnd(f, dwMoviPos);
    WriteIndex(f);
    WriteChunkEnd(f, dwRiffPos);

    // Write the completed file headers
    WriteFileHeaders(f);
    fseek(f, 0, SEEK_END);

    // Close the recording
    fclose(f);
    f = NULL;

    // Free current frame data
    delete[] pbCurr;
    pbCurr = NULL;

    if (dwVideoFrames > 50)
        Frame::SetStatus("Saved mov%04d.avi", nNext-1);
    else
    {
        Frame::SetStatus("Cancelled AVI");
        unlink(szPath);
    }
}

void AVI::Toggle (bool fHalfSize_)
{
    if (!f)
        Start(fHalfSize_);
    else
        Stop();
}

bool AVI::IsRecording ()
{
    return f != NULL;
}


// Add a video frame to the file
void AVI::AddFrame (CScreen *pScreen_)
{
    // Ignore if we're not recording or we're expecting audio
    if (!f || !fWantVideo)
        return;

    if (ftell(f) == 0)
    {
        // Store the dimensions, and allocate+invalidate the frame copy
        width = pScreen_->GetPitch() >> (fHalfSize?1:0);
        height = pScreen_->GetHeight() >> (fHalfSize?1:0);
        pbCurr = new BYTE[width*height];
        memset(pbCurr, 0xff, width*height);

        // Write the placeholder file headers
        WriteFileHeaders(f);
    }

    // Old-style AVI has a 2GB size limit, so stop if we're within 1MB of that
    if (ftell(f) >= 0x7ff00000)
    {
        Stop();
        return;
    }

    // Set a key frame once per second, which encodes the full frame
    bool fKeyFrame = !(dwVideoFrames % EMULATED_FRAMES_PER_SECOND);

    // Start of frame chunk
    DWORD dwPos = WriteChunkStart(f, "00dc");

    int x, nFrag, nJump = 0, nJumpX = 0, nJumpY = 0;

    for (int y = height-1 ; y > 0 ; y--)
    {
        bool fHiRes;
        BYTE *pbLine = pScreen_->GetLine(y>>(fHalfSize?0:1), fHiRes);
        static BYTE abLine[WIDTH_PIXELS*2];

        // Is the recording hi-res but the line low-res?
        if (!fHalfSize && !fHiRes)
        {
            // Double each pixel for a hi-res line
            for (int i = 0 ; i < width ; i += 2)
                abLine[i] = abLine[i+1] = pbLine[i/2];

            pbLine = abLine;
        }
        // Is the recording low-res but the line hi-res?
        else if (fHalfSize && fHiRes)
        {
            // Decide if we should sample the odd pixel for mode 3 lines
            if (GetOption(mode3))
                pbLine++;

            // Use only half the pixels for a low-res line
            for (int i = 0 ; i < width ; i++)
                abLine[i] = pbLine[i*2];

            pbLine = abLine;
        }

        // If this is a scanline, adjust the pixel values to use the 2nd palette section
        if (!fHalfSize && (y&1))
        {
            // It's no problem if pbLine is already pointing to abLine
            DWORD *pdwS = (DWORD*)pbLine, *pdwD = (DWORD*)abLine;

            // Set bit 7 of each colour, in 4-pixels blocks
            for (int i = 0 ; i < width ; i += sizeof(DWORD))
                *pdwD++ = *pdwS++ | 0x80808080;

            pbLine = abLine;
        }

        BYTE *pb = pbLine, *pbP = pbCurr+(width*y);

        for (x = 0 ; x < width ; )
        {
            // Use the full width as a different fragment if this is a key frame
            // otherwise find the next section different from the previous frame
            nFrag = fKeyFrame ? width : FindRunFragment(pb, pbP, width-x, &nJump);

            // If we've nothing to encode, advance by the jump block
            if (!nFrag)
            {
                nJumpX += nJump;

                x += nJump;
                pb += nJump;
                pbP += nJump;

                continue;
            }

            // Convert negative jumps to positive jumps on the following line
            if (nJumpX < 0)
            {
                fputc(0x00, f);	// escape
                fputc(0x00, f);	// eol

                nJumpX = x;
                nJumpY--;
            }

            // Completely process the jump, positioning us ready for the data
            while (nJumpX | nJumpY)
            {
                int ndX = min(nJumpX,255), ndY = min(nJumpY,255);

                fputc(0x00, f);	// escape
                fputc(0x02, f);	// jump
                fputc(ndX, f);	// dx
                fputc(ndY, f);	// dy

                nJumpX -= ndX;
                nJumpY -= ndY;
            }

            // Encode the fragment
            EncodeBlock(pb, nFrag);

            // Advance by the size of the fragment
            x += nFrag;
            pb += nFrag;
            pbP += nFrag;
        }

        // Update our copy of the frame line
        memcpy(pbCurr+(width*y), pbLine, width);

        // Jump to the next line
        nJumpY++;
        nJumpX -= x;
    }

    fputc(0x00, f);	// escape
    fputc(0x01, f);	// eoi

    // Complete frame chunk
    DWORD dwSize = WriteChunkEnd(f, dwPos);
    dwVideoFrames++;

    // Track the maximum video data size
    if (dwSize > dwVideoMax)
        dwVideoMax = dwSize;

    // Want audio next
    fWantVideo = false;
}

// Add an audio frame to the file
void AVI::AddFrame (BYTE *pb_, UINT uLen_)
{
    // Ignore if we're not recording or we're expecting video
    if (!f || fWantVideo)
        return;

    // Write the audio chunk
    DWORD dwPos = WriteChunkStart(f, "01wb");
    fwrite(pb_, uLen_, 1, f);
    DWORD dwSize = WriteChunkEnd(f, dwPos);

    // Update counters
    dwAudioSamples += uLen_ / SAMPLE_BLOCK;
    dwAudioFrames++;

    // Track the maximum audio data size
    if (dwSize > dwAudioMax)
        dwAudioMax = dwSize;

    // Want video next
    fWantVideo = true;
}
