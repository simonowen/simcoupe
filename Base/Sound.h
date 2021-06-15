// Part of SimCoupe - A SAM Coupe emulator
//
// Sound.h: Common sound generation
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

#pragma once

#include "SAMIO.h"
#include "BlipBuffer.h"

#include "SAASound.h"

constexpr auto SAMPLE_FREQ = 44100;
constexpr auto SAMPLE_BITS = 16;
constexpr auto SAMPLE_CHANNELS = 2;
constexpr auto BYTES_PER_SAMPLE = SAMPLE_BITS * SAMPLE_CHANNELS / 8;
constexpr auto SAMPLES_PER_FRAME = SAMPLE_FREQ / EMULATED_FRAMES_PER_SECOND;


class Sound
{
public:
    static bool Init();
    static void Exit();
    static void FrameUpdate();
};

class SoundDevice : public IoDevice
{
public:
    SoundDevice()
    {
        int nSamplesPerFrame = (SAMPLE_FREQ / EMULATED_FRAMES_PER_SECOND) + 1;
        m_sample_buffer.resize(nSamplesPerFrame * BYTES_PER_SAMPLE);
    }

    int GetSampleCount() const { return m_samples_this_frame; }
    const uint8_t* GetSampleBuffer() const { return m_sample_buffer.data(); }

protected:
    int m_samples_this_frame = 0;
    std::vector<uint8_t> m_sample_buffer;
};


struct CSAASoundDeleter { void operator()(LPCSAASOUND saasound) { DestroyCSAASound(saasound); } };
using unique_saasound = unique_resource<LPCSAASOUND, nullptr, CSAASoundDeleter>;

class SAADevice final : public SoundDevice
{
public:
    SAADevice()
    {
        m_pSAASound = CreateCSAASound();
        m_pSAASound->SetSoundParameters(SAAP_NOFILTER | SAAP_16BIT | SAAP_STEREO);
        m_pSAASound->SetSampleRate(SAMPLE_FREQ);
        static_assert(SAMPLE_BITS == 16 && SAMPLE_CHANNELS == 2, "SAA parameter mismatch");
    }

public:
    void Update(bool fFrameEnd_);
    void FrameEnd() override;

    void Out(uint16_t wPort_, uint8_t bVal_) override;

protected:
    unique_saasound m_pSAASound;
};


class DAC final : public SoundDevice
{
public:
    DAC();

public:
    void Reset() override;

    void FrameEnd() override;

    void OutputLeft(uint8_t bVal_);
    void OutputRight(uint8_t bVal_);
    void OutputLeft2(uint8_t bVal_);
    void OutputRight2(uint8_t bVal_);
    void Output(uint8_t bVal_);
    void Output2(uint8_t bVal_);

    int GetSamplesSoFar();

protected:
    Blip_Buffer buf_left{}, buf_right{};
    Blip_Synth<blip_med_quality, 256> synth_left{}, synth_right{}, synth_left2{}, synth_right2{};
};

// Spectrum-style BEEPer
class BeeperDevice final : public IoDevice
{
public:
    void Out(uint16_t wPort_, uint8_t bVal_) override;
};


extern std::unique_ptr<SAADevice> pSAA;
extern std::unique_ptr<DAC> pDAC;
