// Part of SimCoupe - A SAM Coupe emulator
//
// WAV.cpp: WAV audio recording
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
#include "WAV.h"

#include "Frame.h"
#include "Options.h"
#include "Sound.h"

static char szPath[MAX_PATH];
static FILE *f;
static int nFrames, nSilent = 0;
static int nNext;
static bool fSegment;


// RIFF header must be byte-packed
#pragma pack(1)

struct tagRIFF
{
    BYTE abRiffHeader[4];	// 'R','I','F','F'
    BYTE abWaveLen[4];

    struct
    {
        BYTE fmtheader[8];		// 'W','A','V','E','f','m','t',' '
        BYTE fmtlen[4];

        struct
        {
            BYTE FormatTag[2];		// WAVE_FORMAT_PCM = 1
            BYTE Channels[2];
            BYTE SamplesPerSec[4];
            BYTE AvgBytesPerSec[4];
            BYTE BlockAlign[2];
            BYTE BitsPerSample[2];
        } fmt;

        struct
        {
            BYTE dataheader[4];		// 'd','a','t','a'
            BYTE datalen[4];

            // PCM data starts here...
        } pcmdata;
    } wave;
} riff = 
{
    { 'R','I','F','F' }, { sizeof(riff.wave) },
        {  { 'W','A','V','E','f','m','t',' ' }, { sizeof(riff.wave.fmt) },
            { {1}, {0},{0},{0},{0},{0} } ,
            { { 'd','a','t','a' }, {0,0,0,0} }
    }
};

#pragma pack()

////////////////////////////////////////////////////////////////////////////////

static void WriteWaveValue (long lVal_, BYTE *pb_, int nSize_)
{
    for (int i = 0 ; i < nSize_ ; i++)
        *pb_++ = static_cast<BYTE>((lVal_ >> (i<<3)) & 0xff);
}

//////////////////////////////////////////////////////////////////////////////

bool WAV::Start (bool fSegment_)
{
    // Fail if we're already recording
    if (f)
        return false;

    // Find a unique filename to use, in the format sndNNNN.wav
    char szTemplate[MAX_PATH];
    snprintf(szTemplate, MAX_PATH, "%ssnd%%04d.wav", OSD::GetDirPath(GetOption(datapath)));
    nNext = Util::GetUniqueFile(szTemplate, nNext, szPath, sizeof(szPath));

    // Create the file
    f = fopen(szPath, "wb");
    if (!f)
        return false;

    // Write the RIFF header
    WriteWaveValue(SAMPLE_CHANNELS, riff.wave.fmt.Channels, sizeof(riff.wave.fmt.Channels));
    WriteWaveValue(SAMPLE_FREQ, riff.wave.fmt.SamplesPerSec, sizeof(riff.wave.fmt.SamplesPerSec));
    WriteWaveValue(SAMPLE_FREQ*SAMPLE_BLOCK, riff.wave.fmt.AvgBytesPerSec, sizeof(riff.wave.fmt.AvgBytesPerSec));
    WriteWaveValue(SAMPLE_BLOCK, riff.wave.fmt.BlockAlign, sizeof(riff.wave.fmt.BlockAlign));
    WriteWaveValue(SAMPLE_BITS, riff.wave.fmt.BitsPerSample, sizeof(riff.wave.fmt.BitsPerSample));
    fwrite(&riff, sizeof(riff), 1, f);

    // Reset the frame counters and store the fragment flag
    nFrames = nSilent = 0;
    fSegment = fSegment_;

    Frame::SetStatus("Recording WAV%s", fSegment_ ? " segment" : "");
    return true;
}

void WAV::Stop ()
{
    // Ignore if we're not recording
    if (!f)
        return;

    // Update the data length in the header
    long lDataSize = ftell(f) - sizeof(riff);
    WriteWaveValue(lDataSize, riff.wave.pcmdata.datalen, sizeof(long));
    WriteWaveValue(lDataSize+sizeof(riff.wave), riff.abWaveLen, sizeof(long));

    // Rewrite the header and close the file
    fseek(f, 0, SEEK_SET);
    fwrite(&riff, sizeof(riff), 1, f);
    fclose(f);
    f = NULL;

    // Report what happened
    if (nFrames)
        Frame::SetStatus("Saved snd%04d.wav", nNext-1);
    else
    {
        Frame::SetStatus("WAV cancelled");
        unlink(szPath);
    }
}

void WAV::Toggle (bool fSegment_)
{
    if (!f)
        Start(fSegment_);
    else
        Stop();
}

bool WAV::IsRecording ()
{
    return f != NULL;
}


void WAV::AddFrame (const BYTE* pb_, int nLen_)
{
    // Fail if we're not recording
    if (!f)
        return;

    // Check for a full frame of repeated samples (silence)
    if (!memcmp(pb_, pb_+SAMPLE_BLOCK, nLen_-SAMPLE_BLOCK))
    {
        // If we're recording a segment, stop if the silence threshold has been exceeded
        if (fSegment && nFrames && ++nSilent > 2*EMULATED_FRAMES_PER_SECOND)
            Stop();
    }
    else
    {
        // Do we have any frames of buffered silence?
        if (nSilent)
        {
            // Add the accumulated silence, unless we're at the start of the recording
            if (nFrames)
            {
                fseek(f, nLen_*nSilent, SEEK_CUR);
                nFrames += nSilent;
            }

            nSilent = 0;
        }

        // Write the new frame data
        if (fwrite(pb_, nLen_, 1, f))
            nFrames++;
        else
            Stop();
    }
}
