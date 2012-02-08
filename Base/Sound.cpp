// Part of SimCoupe - A SAM Coupe emulator
//
// Sound.cpp: Common sound generation
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
#include "Sound.h"

#include "Audio.h"
#include "CPU.h"

static BYTE *pbSampleBuffer;

static void MixAudio (BYTE *pDst_, const BYTE *pSrc_, int nLen_);

//////////////////////////////////////////////////////////////////////////////

bool Sound::Init (bool fFirstInit_/*=false*/)
{
    int nSamplesPerFrame = (SAMPLE_FREQ / EMULATED_FRAMES_PER_SECOND)+1;
    pbSampleBuffer = new BYTE[nSamplesPerFrame*SAMPLE_BLOCK];

    bool fRet = Audio::Init(fFirstInit_);
    Audio::Silence();
    return fRet;
}

void Sound::Exit (bool fReInit_/*=false*/)
{
    delete[] pbSampleBuffer, pbSampleBuffer = NULL;
    Audio::Exit(fReInit_);
}

void Sound::Silence ()
{
    Audio::Silence();
}

void Sound::FrameUpdate ()
{
    pDAC->FrameEnd();   // set the actual sample count
    pSAA->FrameEnd();   // catch-up to the DAC position

    // Use the DAC as the master clock for sample count
    int nSamples = pDAC->GetSampleCount();
    int nSize = nSamples*SAMPLE_BLOCK;

    // Copy in the DAC samples and mix the SAA
    memcpy(pbSampleBuffer, pDAC->GetSampleBuffer(), nSize);
    MixAudio(pbSampleBuffer, pSAA->GetSampleBuffer(), nSize);

    // Queue the data for playback
    if (!Audio::AddData(pbSampleBuffer, nSize))
        OSD::FrameSync();
}

////////////////////////////////////////////////////////////////////////////////

void CSAA::Update (bool fFrameEnd_=false)
{
    int nSamplesSoFar = fFrameEnd_ ? pDAC->GetSampleCount() : pDAC->GetSamplesSoFar();

    int nNeeded = nSamplesSoFar - m_nSamplesThisFrame;
    if (nNeeded <= 0)
        return;

    BYTE *pb = m_pbFrameSample + m_nSamplesThisFrame*SAMPLE_BLOCK;

    if (g_fReset)
        memset(pb, 0x00, nNeeded*SAMPLE_BLOCK); // no clock means no SAA output
    else
        m_pSAASound->GenerateMany(pb, nNeeded);

    m_nSamplesThisFrame = nSamplesSoFar;
}

void CSAA::FrameEnd ()
{
    Update(true);
    m_nSamplesThisFrame = 0;
}

void CSAA::Out (WORD wPort_, BYTE bVal_)
{
    Update();

    if ((wPort_ & SOUND_MASK) == SOUND_ADDR)
        m_pSAASound->WriteAddress(bVal_);
    else
        m_pSAASound->WriteData(bVal_);
}

////////////////////////////////////////////////////////////////////////////////

CDAC::CDAC ()
{
    buf_left.clock_rate(REAL_TSTATES_PER_SECOND);
    buf_right.clock_rate(REAL_TSTATES_PER_SECOND);
    buf_left.set_sample_rate(SAMPLE_FREQ);
    buf_right.set_sample_rate(SAMPLE_FREQ);

    synth_left.output(&buf_left);
    synth_right.output(&buf_right);
    synth_left.volume(1.0);
    synth_right.volume(1.0);
}

void CDAC::FrameEnd ()
{
    buf_left.end_frame(TSTATES_PER_FRAME);
    buf_right.end_frame(TSTATES_PER_FRAME);

    blip_sample_t *ps = reinterpret_cast<blip_sample_t*>(m_pbFrameSample);
    m_nSamplesThisFrame = static_cast<int>(buf_left.samples_avail());

    buf_left.read_samples(ps, m_nSamplesThisFrame, 1);
    buf_right.read_samples(ps+1, m_nSamplesThisFrame, 1);
}

void CDAC::OutputLeft (BYTE bVal_)
{
    synth_left.update(g_dwCycleCounter, (bVal_ - 0x80));
}

void CDAC::OutputRight (BYTE bVal_)
{
    synth_right.update(g_dwCycleCounter, (bVal_ - 0x80));
}

void CDAC::Output (BYTE bVal_)
{
    synth_left.update(g_dwCycleCounter, (bVal_ - 0x80));
    synth_right.update(g_dwCycleCounter, (bVal_ - 0x80));
}

int CDAC::GetSamplesSoFar ()
{
    UINT uCycles = min(g_dwCycleCounter, TSTATES_PER_FRAME);
    return static_cast<int>(buf_left.count_samples(uCycles));
}

////////////////////////////////////////////////////////////////////////////////

void CBeeperDevice::Out(WORD wPort_, BYTE bVal_)
{
    if (pDAC)
        pDAC->Output((bVal_ & 0x10) ? 0xa0 : 0x80);
}

////////////////////////////////////////////////////////////////////////////////

CSoundDevice::CSoundDevice ()
    : m_nSamplesThisFrame(0)
{
    int nSamplesPerFrame = (SAMPLE_FREQ / EMULATED_FRAMES_PER_SECOND)+1;
    int nSize = nSamplesPerFrame*SAMPLE_BLOCK;

    m_pbFrameSample = new BYTE[nSize];
    memset(m_pbFrameSample, 0x00, nSize);
}

////////////////////////////////////////////////////////////////////////////////

// Basic audio mixing
void MixAudio (BYTE *pDst_, const BYTE *pSrc_, int nLen_)
{
    for (nLen_ /= 2 ; nLen_-- > 0 ; pSrc_ += 2, pDst_ += 2)
    {
        // Add two 16-bit samples
        short s1 = (pSrc_[1] << 8) | pSrc_[0];
        short s2 = (pDst_[1] << 8) | pDst_[0];
        int samp = s1 + s2;

        // Clip to signed range
        samp = min(samp, 32767);
        samp = max(-32768, samp);

        // Write new sample
        pDst_[0] = samp & 0xff;
        pDst_[1] = samp >> 8;
    }
}
