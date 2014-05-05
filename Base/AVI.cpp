// Part of SimCoupe - A SAM Coupe emulator
//
// AVI.cpp: AVI movie recording
//
//  Copyright (c) 1999-2014 Simon Owen
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

static BYTE *pbCurr, *pbResample;

static char szPath[MAX_PATH], *pszFile;
static FILE *f;

static WORD width, height;
static bool fHalfSize = false;

static long lRiffPos, lMoviPos;
static long lVideoMax, lAudioMax;
static DWORD dwVideoFrames, dwAudioFrames, dwAudioSamples;
static bool fWantVideo;

// These hold the option settings during recording, so they can't change
static int nAudioReduce = 0;
static bool fScanlines = false;

static bool WriteLittleEndianWORD (WORD w_)
{
    fputc(w_ & 0xff, f);
    return fputc(w_ >> 8, f) != EOF;
}

static bool WriteLittleEndianDWORD (DWORD dw_)
{
    fputc(dw_ & 0xff, f);
    fputc((dw_ >> 8) & 0xff, f);
    fputc((dw_ >> 16) & 0xff, f);
    return fputc((dw_ >> 24) & 0xff, f) != EOF;
}

static DWORD ReadLittleEndianDWORD ()
{
    BYTE ab[4] = {};
    if (fread(ab, 1, sizeof(ab), f) != sizeof(ab))
        return 0;

    return (ab[3] << 24) | (ab[2] << 16) | (ab[1] << 8) | ab[0];
}

static long WriteChunkStart (FILE *f_, const char *pszChunk_, const char *pszType_=NULL)
{
    // Write the chunk type
    if (pszChunk_ && fwrite(pszChunk_, 1, 4, f_) != 4)
        return 0;

    // Remember the length offset, and skip the length field (to be completed by WriteChunkEnd)
    long lPos = ftell(f_);
    if (fseek(f_, sizeof(DWORD), SEEK_CUR) != 0)
        return 0;

    // If we have a type, write that too
    if (pszType_ && fwrite(pszType_, 1, 4, f_) != 4)
        return 0;

    // Return the offset to the length, so it can be completed later using WriteChunkEnd
    return lPos;
}

static long WriteChunkEnd (FILE *f_, long lPos_)
{
    // Remember the current position, and calculate the chunk size (not including the length field)
    long lPos = ftell(f_);
    long lSize = lPos-lPos_-sizeof(DWORD);

    // Seek back to the length field
    if (lPos_ < 0 || fseek(f_, lPos_, SEEK_SET) != 0)
        return 0;

    // Write the chunk size
    WriteLittleEndianDWORD(lSize);

    // Restore original position (should always be end of file, but we'll use the value from earlier)
    if (lPos < 0 || fseek(f_, lPos, SEEK_SET) != 0)
        return 0;

    // If the length was odd, pad file position to even boundary
    if (lPos & 1)
    {
        fputc(0x00, f);
        lSize++;
    }

    // Return the on-disk size
    return lSize;
}

static bool WriteAVIHeader (FILE *f_)
{
    long lPos = WriteChunkStart(f_, "avih");

    // Should we include an audio stream?
    DWORD dwStreams = (nAudioReduce < 4) ? 2 : 1;

    WriteLittleEndianDWORD(19968);			// microseconds per frame: 1000000*TSTATES_PER_FRAME/REAL_TSTATES_PER_SECOND
    WriteLittleEndianDWORD((lVideoMax*EMULATED_FRAMES_PER_SECOND)+(lAudioMax*EMULATED_FRAMES_PER_SECOND));	// approximate max data rate
    WriteLittleEndianDWORD(0);				// reserved
    WriteLittleEndianDWORD((1<<8)|(1<<4));	// flags: bit 4 = has index(idx1), bit 5 = use index for AVI structure, bit 8 = interleaved file, bit 16 = optimized for live video capture, bit 17 = copyrighted data
    WriteLittleEndianDWORD(dwVideoFrames);	// total number of video frames
    WriteLittleEndianDWORD(0);				// initial frame number for interleaved files
    WriteLittleEndianDWORD(dwStreams);		// number of streams in the file (video+audio)
    WriteLittleEndianDWORD(0);				// suggested buffer size for reading the file
    WriteLittleEndianDWORD(width);			// pixel width
    WriteLittleEndianDWORD(height);			// pixel height
    WriteLittleEndianDWORD(0);				// 4 reserved DWORDs (must be zero)
    WriteLittleEndianDWORD(0);
    WriteLittleEndianDWORD(0);
    WriteLittleEndianDWORD(0);

    return WriteChunkEnd(f_, lPos) != 0;
}

static bool WriteVideoHeader (FILE *f_)
{
    long lPos = WriteChunkStart(f_, "strh", "vids");

    fwrite("mrle", 4, 1, f);				// 'mrle' = Microsoft Run Length Encoding Video Codec
    WriteLittleEndianDWORD(0);				// flags, unused
    WriteLittleEndianDWORD(0);				// priority and language, unused
    WriteLittleEndianDWORD(0);				// initial frames
    WriteLittleEndianDWORD(TSTATES_PER_FRAME); // scale
    WriteLittleEndianDWORD(REAL_TSTATES_PER_SECOND); // rate
    WriteLittleEndianDWORD(0);				// start time
    WriteLittleEndianDWORD(dwVideoFrames);	// total frames in stream
    WriteLittleEndianDWORD(lVideoMax);		// suggested buffer size
    WriteLittleEndianDWORD(10000);			// quality
    WriteLittleEndianDWORD(0);				// sample size
    WriteLittleEndianWORD(0);				// left
    WriteLittleEndianWORD(0);				// top
    WriteLittleEndianWORD(width);			// right
    WriteLittleEndianWORD(height);			// bottom

    WriteChunkEnd(f_, lPos);

    lPos = WriteChunkStart(f_, "strf");

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

    // Determine the appropriate brightness adjustment for scanlines
    int nScanAdjust = GetOption(scanlevel) - 100;
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

    return WriteChunkEnd(f_, lPos) != 0;
}

static bool WriteAudioHeader (FILE *f_)
{
    long lPos = WriteChunkStart(f_, "strh", "auds");

    // Default to normal sound parameters
    WORD wFreq = SAMPLE_FREQ;
    WORD wBits = SAMPLE_BITS;
    WORD wBlock = SAMPLE_BLOCK;
    WORD wChannels = SAMPLE_CHANNELS;

    // 8-bit?
    if (nAudioReduce >= 1)
        wBits /= 2, wBlock /= 2;

    // 22kHz?
    if (nAudioReduce >= 2)
        wFreq /= 2;

    // Mono?
    if (nAudioReduce >= 3)
        wChannels /= 2, wBlock /= 2;


    fwrite("\0\0\0\0", 4, 1, f);			// FOURCC not specified (PCM below)
    WriteLittleEndianDWORD(0);				// flags, unused
    WriteLittleEndianDWORD(0);				// priority and language, unused
    WriteLittleEndianDWORD(1);				// initial frames
    WriteLittleEndianDWORD(wBlock);			// scale
    WriteLittleEndianDWORD(wFreq*wBlock);	// rate
    WriteLittleEndianDWORD(0);				// start time
    WriteLittleEndianDWORD(dwAudioSamples);	// total samples in stream
    WriteLittleEndianDWORD(lAudioMax);		// suggested buffer size
    WriteLittleEndianDWORD(0xffffffff);		// quality
    WriteLittleEndianDWORD(wBlock);			// sample size
    WriteLittleEndianDWORD(0);				// two unused rect coords
    WriteLittleEndianDWORD(0);				// two more unused rect coords

    WriteChunkEnd(f_, lPos);

    lPos = WriteChunkStart(f_, "strf");

    WriteLittleEndianWORD(1);				// format tag (1 = WAVE_FORMAT_PCM)
    WriteLittleEndianWORD(wChannels);		// channels
    WriteLittleEndianDWORD(wFreq);			// samples per second
    WriteLittleEndianDWORD(wFreq*wBlock);	// average bytes per second
    WriteLittleEndianWORD(wBlock);			// block align
    WriteLittleEndianWORD(wBits);			// bits per sample
    WriteLittleEndianWORD(0);				// extra structure size

    return WriteChunkEnd(f_, lPos) != 0;
}

static bool WriteIndex (FILE *f_)
{
    // The chunk index is the final addition
    long lIdx1Pos = WriteChunkStart(f_, "idx1");
    if (fseek(f, -4, SEEK_CUR) != 0)
        return false;
    WriteLittleEndianDWORD(0);

    // Locate the start of the movi data (after the chunk header)
    lMoviPos += 2*sizeof(DWORD);

    // Calculate the number of entries in the index
    DWORD dwIndexEntries = dwVideoFrames + dwAudioFrames;
    DWORD dwVideoFrame = 0;

    // Loop through all frames in the file
    for (UINT u = 0 ; u < dwIndexEntries ; u++)
    {
        BYTE abType[4];

        if (fseek(f, lMoviPos, SEEK_SET) != 0)
            return false;

        // Read the type and size from the movi chunk
        if (fread(abType, 1, sizeof(abType), f) != sizeof(abType))
            return false;

        DWORD dwSize = ReadLittleEndianDWORD();
        if (fseek(f, 0, SEEK_END) != 0)
            return false;

        // Every 50th frame is a key frame
        bool fKeyFrame = (abType[1] == '0') && !(dwVideoFrame++ % EMULATED_FRAMES_PER_SECOND);

        // Write the type
        if (fwrite(abType, 1, sizeof(abType), f) != sizeof(abType))
            return false;

        // Write flags, offset and size to the index
        WriteLittleEndianDWORD(fKeyFrame ? 0x10 : 0x00);
        WriteLittleEndianDWORD(lMoviPos);
        WriteLittleEndianDWORD(dwSize);

        // Calculate next position, aligned to even boundary
        lMoviPos += 2*sizeof(DWORD) + ((dwSize+1) & ~1);
    }

    // Complete the index and riff chunks
    return WriteChunkEnd(f_, lIdx1Pos) != 0;
}

static bool WriteFileHeaders (FILE *f_)
{
    if (fseek(f_, 0, SEEK_SET) != 0)
        return false;

    lRiffPos = WriteChunkStart(f_, "RIFF", "AVI ");
    long lHdrlPos = WriteChunkStart(f_, "LIST", "hdrl");

    WriteAVIHeader(f_);

    long lPos = WriteChunkStart(f_, "LIST", "strl");
    WriteVideoHeader(f_);
    WriteChunkEnd(f_, lPos);

    if (nAudioReduce < 4)
    {
        lPos = WriteChunkStart(f_, "LIST", "strl");
        WriteAudioHeader(f_);
        WriteChunkEnd(f_, lPos);
    }

    lPos = WriteChunkStart(f_, "JUNK");

    // Align movi data to 2048-byte boundary
    if (fseek(f_, (-ftell(f)-3*sizeof(DWORD)) & 0x3ff, SEEK_CUR) != 0)
        return false;

    WriteChunkEnd(f_, lPos);
    WriteChunkEnd(f_, lHdrlPos);

    // Start of movie data
    lMoviPos = WriteChunkStart(f_, "LIST", "movi");

    // Check last write succeeded
    return lMoviPos != 0;
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
    pszFile = Util::GetUniqueFile("avi", szPath, sizeof(szPath));

    // Create the file
    f = fopen(szPath, "wb+");
    if (!f)
        return false;

    // Reset the frame counters
    dwVideoFrames = dwAudioFrames = dwAudioSamples = 0;
    lVideoMax = lAudioMax = 0;

    // Set the size and flag we want a video frame first
    fHalfSize = fHalfSize_;
    fWantVideo = true;

    // Set scanline mode for the recording
    fScanlines = GetOption(scanlines) && GetOption(aviscanlines);

#if SAMPLE_FREQ == 44100 && SAMPLE_BITS == 16 && SAMPLE_CHANNELS == 2
    // Set the audio reduction level
    nAudioReduce = GetOption(avireduce);
#endif

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
    WriteChunkEnd(f, lMoviPos);
    WriteIndex(f);
    WriteChunkEnd(f, lRiffPos);

    // Write the completed file headers
    WriteFileHeaders(f);
    fseek(f, 0, SEEK_END);

    // Close the recording
    fclose(f);
    f = NULL;

    // Free current frame data
    delete[] pbCurr, pbCurr = NULL;

    // Free resample buffer
    delete[] pbResample, pbResample = NULL;

    Frame::SetStatus("Saved %s", pszFile);
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

    // Old-style AVI has a 2GB size limit, so restart if we're within 1MB of that
    if (ftell(f) >= 0x7ff00000)
    {
        Stop();

        // Restart for a continuation volume
        if (!Start(fHalfSize))
            return;
    }

    // Start of file?
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

    // Set a key frame once per second, which encodes the full frame
    bool fKeyFrame = !(dwVideoFrames % EMULATED_FRAMES_PER_SECOND);

    // Start of frame chunk
    long lPos = WriteChunkStart(f, "00dc");

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
        if (fScanlines && !fHalfSize && (y&1))
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
    long lSize = WriteChunkEnd(f, lPos);
    dwVideoFrames++;

    // Track the maximum video data size
    if (lSize > lVideoMax)
        lVideoMax = lSize;

    // Want audio next, if enabled
    fWantVideo = (nAudioReduce >= 4);
}

// Add an audio frame to the file
void AVI::AddFrame (const BYTE *pb_, UINT uLen_)
{
    // Ignore if we're not recording or we're expecting video
    if (!f || fWantVideo)
        return;

    // Calculate the number of input samples
    UINT uBlock = SAMPLE_BLOCK;
    UINT uSamples = uLen_ / uBlock;

    // Do we need to reduce the audio size?
    if (nAudioReduce)
    {
        static bool fOddLast = false;

        // Allocate resample buffer if it doesn't already exist
        if (!pbResample && !(pbResample = new BYTE[uLen_]))
            return;

        BYTE *pbNew = pbResample;

        // 22kHz?
        if (nAudioReduce >= 2)
        {
            // If the last sample count was odd, skip the first sample
            if (fOddLast)
                pb_ += uBlock, uSamples--;

            // If the current sample count is odd, include the final sample
            if (uSamples & 1)
                fOddLast = true, uSamples++;

            // 22kHz drops half the samples
            uSamples /= 2;
            uBlock *= 2;
        }

        // Mono?
        if (nAudioReduce >= 3)
        {
            // Process every other sample
            for (UINT u = 0 ; u < uSamples ; u++, pb_ += uBlock)
            {
                // 16-bit stereo to 8-bit mono
                int left = static_cast<short>((pb_[1] << 8) | pb_[0]);
                int right = static_cast<short>((pb_[3] << 8) | pb_[2]);
                short combined = static_cast<short>((left+right)/2);

                *pbNew++ = static_cast<BYTE>((combined >> 8) ^ 0x80);
            }
        }
        // Stereo
        else
        {
            // Process the required number of samples
            for (UINT u = 0 ; u < uSamples ; u++, pb_ += uBlock)
            {
                // 16-bit to 8-bit
                short left = static_cast<short>((pb_[1] << 8) | pb_[0]);
                short right = static_cast<short>((pb_[3] << 8) | pb_[2]);

                *pbNew++ = static_cast<BYTE>((left >> 8) ^ 0x80);
                *pbNew++ = static_cast<BYTE>((right >> 8) ^ 0x80);
            }
        }

        pb_ = pbResample;
        uLen_ = static_cast<UINT>(pbNew - pbResample);
    }

    // Write the audio chunk
    long lPos = WriteChunkStart(f, "01wb");
    fwrite(pb_, uLen_, 1, f);
    long lSize = WriteChunkEnd(f, lPos);

    // Update counters
    dwAudioSamples += uSamples;
    dwAudioFrames++;

    // Track the maximum audio data size
    if (lSize > lAudioMax)
        lAudioMax = lSize;

    // Want video next
    fWantVideo = true;
}
