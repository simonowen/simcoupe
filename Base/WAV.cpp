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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "SimCoupe.h"
#include "WAV.h"

#include "Frame.h"
#include "Options.h"
#include "Sound.h"

namespace WAV
{

static std::string wav_path;
static unique_FILE file;
static int nFrames, nSilent = 0;
static bool fSegment;


// RIFF header must be byte-packed
#pragma pack(1)

struct tagRIFF
{
    uint8_t abRiffHeader[4];   // 'R','I','F','F'
    uint8_t abWaveLen[4];

    struct
    {
        uint8_t fmtheader[8];  // 'W','A','V','E','f','m','t',' '
        uint8_t fmtlen[4];

        struct
        {
            uint8_t FormatTag[2];      // WAVE_FORMAT_PCM = 1
            uint8_t Channels[2];
            uint8_t SamplesPerSec[4];
            uint8_t AvgBytesPerSec[4];
            uint8_t BlockAlign[2];
            uint8_t BitsPerSample[2];
        } fmt;

        struct
        {
            uint8_t dataheader[4];     // 'd','a','t','a'
            uint8_t datalen[4];

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

static void WriteWaveValue(long lVal_, uint8_t* pb_, int nSize_)
{
    for (int i = 0; i < nSize_; i++)
        *pb_++ = static_cast<uint8_t>((lVal_ >> (i << 3)) & 0xff);
}

//////////////////////////////////////////////////////////////////////////////

bool Start(bool fSegment_)
{
    // Fail if we're already recording
    if (file)
        return false;

    wav_path = Util::UniqueOutputPath("wav");
    file = fopen(wav_path.c_str(), "wb");
    if (!file)
    {
        Frame::SetStatus("Save failed: {}", wav_path);
        return false;
    }

    // Write the RIFF header
    WriteWaveValue(SAMPLE_CHANNELS, riff.wave.fmt.Channels, sizeof(riff.wave.fmt.Channels));
    WriteWaveValue(SAMPLE_FREQ, riff.wave.fmt.SamplesPerSec, sizeof(riff.wave.fmt.SamplesPerSec));
    WriteWaveValue(SAMPLE_FREQ * BYTES_PER_SAMPLE, riff.wave.fmt.AvgBytesPerSec, sizeof(riff.wave.fmt.AvgBytesPerSec));
    WriteWaveValue(BYTES_PER_SAMPLE, riff.wave.fmt.BlockAlign, sizeof(riff.wave.fmt.BlockAlign));
    WriteWaveValue(SAMPLE_BITS, riff.wave.fmt.BitsPerSample, sizeof(riff.wave.fmt.BitsPerSample));
    fwrite(&riff, sizeof(riff), 1, file);

    // Reset the frame counters and store the fragment flag
    nFrames = nSilent = 0;
    fSegment = fSegment_;

    Frame::SetStatus("Recording WAV{}", fSegment_ ? " segment" : "");
    return true;
}

void Stop()
{
    // Ignore if we're not recording
    if (!file)
        return;

    // Update the data length in the header
    long lDataSize = ftell(file) - sizeof(riff);
    WriteWaveValue(lDataSize, riff.wave.pcmdata.datalen, sizeof(int32_t));
    WriteWaveValue(lDataSize + sizeof(riff.wave), riff.abWaveLen, sizeof(int32_t));

    // Rewrite the completed file header
    if (fseek(file, 0, SEEK_SET) == 0)
    {
        if (fwrite(&riff, 1, sizeof(riff), file) != sizeof(riff))
            TRACE("!!! WAV::Stop(): Failed to write RIFF header\n");
    }

    file.reset();

    if (nFrames)
    {
        Frame::SetStatus("Saved {}", wav_path);
    }
    else
    {
        Frame::SetStatus("WAV save cancelled");
        std::error_code error{};
        fs::remove(wav_path, error);
    }
}

void Toggle(bool fSegment_)
{
    if (!file)
        Start(fSegment_);
    else
        Stop();
}

bool IsRecording()
{
    return file != nullptr;
}


void AddFrame(const uint8_t* pb_, int nLen_)
{
    // Fail if we're not recording
    if (!file)
        return;

    // Check for a full frame of repeated samples (silence)
    if (!memcmp(pb_, pb_ + BYTES_PER_SAMPLE, nLen_ - BYTES_PER_SAMPLE))
    {
        ++nSilent;

        // If we're recording a segment, stop if the silence threshold has been exceeded
        if (fSegment && nFrames && nSilent > 2 * EMULATED_FRAMES_PER_SECOND)
            Stop();
    }
    else
    {
        // Do we have any frames of buffered silence?
        if (nSilent)
        {
            // Add the accumulated silence, unless we're at the start of the recording
            if (nFrames && fseek(file, nLen_ * nSilent, SEEK_CUR) == 0)
                nFrames += nSilent;

            nSilent = 0;
        }

        // Write the new frame data
        if (fwrite(pb_, nLen_, 1, file))
            nFrames++;
        else
            Stop();
    }
}

} // namespace WAV
