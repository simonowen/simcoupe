// Part of SimCoupe - A SAM Coupe emulator
//
// Sound.cpp: Common sound generation
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

#include "SimCoupe.h"
#include "Sound.h"

#include "Audio.h"
#include "AVI.h"
#include "CPU.h"
#include "Frame.h"
#include "Options.h"
#include "SID.h"
#include "WAV.h"

static uint8_t* pbSampleBuffer;

static void MixAudio(uint8_t* pDst_, const uint8_t* pSrc_, int nLen_);
static int AdjustSpeed(uint8_t* pb_, int nSize_, int nSpeed_);

//////////////////////////////////////////////////////////////////////////////

bool Sound::Init()
{
    Exit();

    int nMaxFrameSamples = 2; // Needed for 50% running speed
    int nSamplesPerFrame = (SAMPLE_FREQ / EMULATED_FRAMES_PER_SECOND) + 1;
    pbSampleBuffer = new uint8_t[nSamplesPerFrame * BYTES_PER_SAMPLE * nMaxFrameSamples];

    bool fRet = Audio::Init();
    Audio::Silence();
    return fRet;
}

void Sound::Exit()
{
    // Stop any recording
    WAV::Stop();
    AVI::Stop();

    delete[] pbSampleBuffer; pbSampleBuffer = nullptr;
    Audio::Exit();
}

void Sound::Silence()
{
    Audio::Silence();
}

void Sound::FrameUpdate()
{
    static bool fSidUsed = false;

    // Track whether SID has been used, to avoid unnecessary sample generation+mixing
    fSidUsed |= pSID->GetSampleCount() != 0;

    pDAC->FrameEnd();   // set the actual sample count
    pSAA->FrameEnd();   // catch-up to the DAC position
    if (fSidUsed) pSID->FrameEnd();

    // Use the DAC as the master clock for sample count
    int nSamples = pDAC->GetSampleCount();
    int nSize = nSamples * BYTES_PER_SAMPLE;

    // Copy in the DAC samples, then mix SAA and possibly SID too
    memcpy(pbSampleBuffer, pDAC->GetSampleBuffer(), nSize);
    MixAudio(pbSampleBuffer, pSAA->GetSampleBuffer(), nSize);
    if (fSidUsed && GetOption(sid)) MixAudio(pbSampleBuffer, pSID->GetSampleBuffer(), nSize);

    // Add the frame to any recordings
    WAV::AddFrame(pbSampleBuffer, nSize);
    AVI::AddFrame(pbSampleBuffer, nSize);

    // Scale the audio to fit the require running speed
    if (SAMPLE_FREQ == 44100 && SAMPLE_BITS == 16 && SAMPLE_CHANNELS == 2)
        nSize = AdjustSpeed(pbSampleBuffer, nSize, GetOption(speed));

    // Queue the data for playback
    Audio::AddData(pbSampleBuffer, nSize);
}

////////////////////////////////////////////////////////////////////////////////

void SAADevice::Update(bool fFrameEnd_ = false)
{
    int nSamplesSoFar = fFrameEnd_ ? pDAC->GetSampleCount() : pDAC->GetSamplesSoFar();

    int nNeeded = nSamplesSoFar - m_samples_this_frame;
    if (nNeeded <= 0)
        return;

    auto pb = m_sample_buffer.data() + m_samples_this_frame * BYTES_PER_SAMPLE;

    if (g_fReset)
        memset(pb, 0x00, nNeeded * BYTES_PER_SAMPLE); // no clock means no SAA output
    else
        m_pSAASound->GenerateMany(pb, nNeeded);

    m_samples_this_frame = nSamplesSoFar;
}

void SAADevice::FrameEnd()
{
    Update(true);
    m_samples_this_frame = 0;
}

void SAADevice::Out(uint16_t wPort_, uint8_t bVal_)
{
    Update();

    if ((wPort_ & SOUND_MASK) == SOUND_ADDR)
        m_pSAASound->WriteAddress(bVal_);
    else
        m_pSAASound->WriteData(bVal_);
}

////////////////////////////////////////////////////////////////////////////////

DAC::DAC()
{
    buf_left.clock_rate(CPU_CLOCK_HZ);
    buf_right.clock_rate(CPU_CLOCK_HZ);
    buf_left.set_sample_rate(SAMPLE_FREQ);
    buf_right.set_sample_rate(SAMPLE_FREQ);

    synth_left.output(&buf_left);
    synth_left2.output(&buf_left);
    synth_right.output(&buf_right);
    synth_right2.output(&buf_right);

    synth_left.volume(1.0);
    synth_left2.volume(1.0);
    synth_right.volume(1.0);
    synth_right2.volume(1.0);

    Reset();
}

void DAC::Reset()
{
    Output(0);
    Output2(0);
}

void DAC::FrameEnd()
{
    buf_left.end_frame(CPU_CYCLES_PER_FRAME);
    buf_right.end_frame(CPU_CYCLES_PER_FRAME);

    m_samples_this_frame = static_cast<int>(buf_left.samples_avail());

    auto ps = reinterpret_cast<blip_sample_t*>(m_sample_buffer.data());
    buf_left.read_samples(ps, m_samples_this_frame, 1);
    buf_right.read_samples(ps + 1, m_samples_this_frame, 1);
}

void DAC::OutputLeft(uint8_t bVal_)
{
    synth_left.update(g_dwCycleCounter, bVal_);
}

void DAC::OutputLeft2(uint8_t bVal_)
{
    synth_left2.update(g_dwCycleCounter, bVal_);
}

void DAC::OutputRight(uint8_t bVal_)
{
    synth_right.update(g_dwCycleCounter, bVal_);
}

void DAC::OutputRight2(uint8_t bVal_)
{
    synth_right2.update(g_dwCycleCounter, bVal_);
}

void DAC::Output(uint8_t bVal_)
{
    synth_left.update(g_dwCycleCounter, bVal_);
    synth_right.update(g_dwCycleCounter, bVal_);
}

void DAC::Output2(uint8_t bVal_)
{
    synth_left2.update(g_dwCycleCounter, bVal_);
    synth_right2.update(g_dwCycleCounter, bVal_);
}

int DAC::GetSamplesSoFar()
{
    auto uCycles = std::min(g_dwCycleCounter, static_cast<uint32_t>(CPU_CYCLES_PER_FRAME));
    return static_cast<int>(buf_left.count_samples(uCycles));
}

////////////////////////////////////////////////////////////////////////////////

void BeeperDevice::Out(uint16_t /*wPort_*/, uint8_t bVal_)
{
    if (pDAC)
        pDAC->Output((bVal_ & 0x10) ? 0xa0 : 0x80);
}

////////////////////////////////////////////////////////////////////////////////

// Basic audio mixing
static void MixAudio(uint8_t* pDst_, const uint8_t* pSrc_, int nLen_)
{
    for (nLen_ /= 2; nLen_-- > 0; pSrc_ += 2, pDst_ += 2)
    {
        // Add two 16-bit samples
        short s1 = (pSrc_[1] << 8) | pSrc_[0];
        short s2 = (pDst_[1] << 8) | pDst_[0];
        int samp = s1 + s2;

        // Clip to signed range
        samp = std::min(samp, 32767);
        samp = std::max(-32768, samp);

        // Write new sample
        pDst_[0] = samp & 0xff;
        pDst_[1] = samp >> 8;
    }
}


// Scale audio data to fit the current emulator speed setting
static int AdjustSpeed(uint8_t* pb_, int nSize_, int nSpeed_)
{
    // Limit speed range
    nSpeed_ = std::max(nSpeed_, 50);
    nSpeed_ = std::min(nSpeed_, 1000);

    // Slow?
    if (nSpeed_ < 100)
    {
        auto pdwS = reinterpret_cast<uint32_t*>(pb_ + nSize_) - 1;
        auto pdwD = reinterpret_cast<uint32_t*>(pb_ + nSize_ * 2) - 1;

        // Double samples in reverse order
        for (int i = 0; i < nSize_; i += BYTES_PER_SAMPLE, pdwS--)
        {
            *pdwD-- = *pdwS;
            *pdwD-- = *pdwS;
        }

        nSize_ *= 2;
    }
    else if (nSpeed_ == 100)
    {
        // Nothing to do
    }
    // Fast?
    else if (nSpeed_ > 100)
    {
        int nScale = nSpeed_ / 100;
        nSize_ = (nSize_ / nScale) & ~(BYTES_PER_SAMPLE - 1);

        auto pdwS = reinterpret_cast<uint32_t*>(pb_);
        auto pdwD = pdwS;

        // Skip the required number of samples
        for (int i = 0; i < nSize_; i += BYTES_PER_SAMPLE, pdwS += nScale)
            *pdwD++ = *pdwS;
    }

    return nSize_;
}
