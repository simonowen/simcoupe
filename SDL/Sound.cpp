// Part of SimCoupe - A SAM Coupé emulator
//
// Sound.cpp: SDL sound implementation
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

// Notes:
//  This module relies on Dave Hooper's SAASound library for the Philips
//  SAA 1099 sound chip emulation. See the SAASound.txt licence file
//  for details.
//
//  DACs and BEEPer output are done using a single DAC buffer, which is
//  mixed into the SAA output.

// Changes 2000-2001 by Dave Laundon
//  - interpolation of DAC output to improve high frequencies
//  - buffering tweaks to help with sample block joins

// ToDo:
//  - implement stream mixing so DAC output can be heard

#include "SimCoupe.h"
#include <math.h>

#include "../Extern/SAASound.h"
#define SOUND_IMPLEMENTATION
#include "Sound.h"

#include "IO.h"
#include "Options.h"
#include "Util.h"
#include "Profile.h"

extern int g_nLine, g_nLineCycle;

UINT HCF (UINT x_, UINT y_);


namespace Sound
{
CSDLSAASound* pSAA;     // Pointer to the current driver object for dealing with the sound chip
CDAC *pDAC;                 // DAC object used for parallel DAC devices and the Spectrum-style beeper

LPCSAASOUND pSAASound;      // SAASOUND.DLL object - needs to exist as long as we do, to preseve subtle internal states

////////////////////////////////////////////////////////////////////////////////

bool Init (bool fFirstInit_/*=false*/)
{
    Exit(true);
    TRACE("-> Sound::Init(%s)\n", fFirstInit_ ? "first" : "");

    // All sound disabled?
    if (!GetOption(sound))
        TRACE("Sound disabled, nothing to initialise\n");
    else
    {
        // If the SAA 1099 chip is enabled, create its driver object
        if (GetOption(saasound) && (pSAA = new CSDLSAASound))
        {

            // Initialise the driver, disabling it if it fails
            if (!pSAA->Init())
            {
                delete pSAA;
                pSAA = NULL;
            }

            // Else, create the CSAASound object if it doesn't already exist
            else if (pSAASound || (pSAASound = CreateCSAASound()))
            {
                // Set the DLL parameters from the options, so it matches the setup of the primary sound buffer
                pSAASound->SetSoundParameters(
                    (GetOption(filter) ? SAAP_FILTER : SAAP_NOFILTER) |
                    (GetOption(frequency) < 20000 ? SAAP_11025 : GetOption(frequency) < 40000 ? SAAP_22050 : SAAP_44100) |
                    (GetOption(bits) < 12 ? SAAP_8BIT : SAAP_16BIT) |
                    (GetOption(stereo) ? SAAP_STEREO : SAAP_MONO));
            }
        }

/*
        // If a DAC is connected to a parallel port or the Spectrum-style beeper is enabled, we need a CDAC object
        bool fNeedDAC = (GetOption(parallel1) >= 2 || GetOption(parallel2) >= 2 || GetOption(beeper));

        // Create and initialise a DAC, if required
        if (fNeedDAC && (pDAC = new CDAC) && !pDAC->Init())
        {
            delete pDAC;
            pDAC = NULL;
        }

        // If anything failed, disable the sound
        if ((GetOption(saasound) && !pSAA) || (fNeedDAC && !pDAC))
        {
            Message(msgWarning, "Sound initialisation failed, disabling...");
            SetOption(sound,0);
            Exit();
        }
*/
        Play();
    }

    // Sound initialisation failure isn't fatal, so always return success
    TRACE("<- Sound::Init()\n");
    return true;
}

void Exit (bool fReInit_/*=false*/)
{
    TRACE("-> Sound::Exit(%s)\n", fReInit_ ? "reinit" : "");

    if (pSAA) { delete pSAA; pSAA = NULL; }
    if (pDAC) { delete pDAC; pDAC = NULL; }

    if (pSAASound && !fReInit_)
    {
        DestroyCSAASound(pSAASound);
        pSAASound = NULL;
    }

    TRACE("<- Sound::Exit()\n");
}

void Out (WORD wPort_, BYTE bVal_)
{
    if (pSAA)
        pSAA->Out(wPort_, bVal_);
}

void FrameUpdate ()
{
    ProfileStart(Snd);

    if (!GetOption(turbo))
    {
        if (pSAA) pSAA->Update(true);
        if (pDAC) pDAC->Update(true);
    }

    ProfileEnd();
}


void Silence ()
{
    if (pSAA) pSAA->Silence();
    if (pDAC) pDAC->Silence();
}

void Stop ()
{
    SDL_PauseAudio(1);

    if (pSAA) { pSAA->Stop(); pSAA->Silence(); }
    if (pDAC) { pDAC->Stop(); pDAC->Silence(); }
}

void Play ()
{
    if (pSAA) { pSAA->Silence(); pSAA->Play(); }
    if (pDAC) { pDAC->Silence(); pDAC->Play(); }

    SDL_PauseAudio(0);
}


void OutputDAC (BYTE bVal_)
{
    if (pDAC) pDAC->Output(bVal_);
}

void OutputDACLeft (BYTE bVal_)
{
    if (pDAC) pDAC->OutputLeft(bVal_);
}

void OutputDACRight (BYTE bVal_)
{
    if (pDAC) pDAC->OutputRight(bVal_);
}


};  // namespace Sound


bool CStreamingSound::InitSDLSound ()
{
    bool fRet = false;

    SDL_AudioSpec sDesired = { 0 };
    sDesired.freq = (GetOption(frequency) < 20000 ? 11025 : GetOption(frequency) < 40000 ? 22050 : 44100);
    sDesired.format = (GetOption(bits) > 8) ? AUDIO_S16 : AUDIO_U8;
    sDesired.channels = GetOption(stereo) ? 2 : 1;
    sDesired.samples = (GetOption(latency) <= 6) ? 1024 : (GetOption(latency) <= 11) ? 4096 : 4096;
    sDesired.callback = SoundCallback;
    sDesired.userdata = reinterpret_cast<void*>(reinterpret_cast<CStreamingSound*>(this));

    if (!(fRet = (SDL_OpenAudio(&sDesired, &m_sObtained) >= 0)))
        TRACE("SDL_OpenAudio failed: %s\n", SDL_GetError());

    return fRet;
}


void CStreamingSound::ExitSDLSound ()
{
    SDL_CloseAudio();
}


CStreamingSound::CStreamingSound (int nFreq_/*=0*/, int nBits_/*=0*/, int nChannels_/*=0*/)
{
    m_nFreq = nFreq_;
    m_nBits = nBits_;
    m_nChannels = nChannels_;

    m_pbFrameSample = NULL;
}

CStreamingSound::~CStreamingSound ()
{
    if (m_pbFrameSample) { delete m_pbFrameSample; m_pbFrameSample = NULL; }

    ExitSDLSound();
}


bool CStreamingSound::Init ()
{
    bool fRet = InitSDLSound();

    if (fRet)
    {
        if (!m_nFreq)
            m_nFreq = m_sObtained.freq;

        if (!m_nBits)
            m_nBits = m_sObtained.format & 0xff;

        if (!m_nChannels)
            m_nChannels = m_sObtained.channels;

        // Use some arbitrary units to keep the numbers manageably small...
        UINT uUnits = HCF(m_nFreq, EMULATED_TSTATES_PER_SECOND);
        m_uSamplesPerUnit = m_nFreq / uUnits;
        m_uCyclesPerUnit = EMULATED_TSTATES_PER_SECOND / uUnits;

        // Do this because 50Hz doesn't divide exactly in to 11025Hz...
        UINT uMaxSamplesPerFrame = (m_nFreq + EMULATED_FRAMES_PER_SECOND - 1) / EMULATED_FRAMES_PER_SECOND;
        m_uSampleSize = m_nChannels * m_nBits / 8;
        m_uSampleBufferSize = (uMaxSamplesPerFrame * m_uSampleSize * 2);
//      m_uSampleBufferSize = (m_uSampleBufferSize + (16-1)) & ~(16-1);

        m_pbFrameSample = new BYTE[m_uSampleBufferSize];
        m_uSamplesThisFrame = 0;
        m_dwWriteOffset = 0;
        m_uOffsetPerUnit = 0;
        m_uPeriod = 0;

        m_pbEnd = (m_pbNow = m_pbStart = new BYTE[m_uSampleBufferSize << 2]) + (m_uSampleBufferSize << 2);

        Stop();
    }

    return fRet;
}


void CStreamingSound::Silence ()
{
    SDL_LockAudio();
    memset(m_pbStart, m_sObtained.silence, m_pbEnd-m_pbStart);
    SDL_UnlockAudio();
}


void CStreamingSound::Update (bool fFrameEnd_)
{
    ProfileStart(Snd);

    SDL_LockAudio();

    {
        // Calculate the number of whole samples passed and the amount spanning in to the next sample
        UINT uSamplesCyclesPerUnit = ((g_nLine * TSTATES_PER_LINE) + g_nLineCycle) * m_uSamplesPerUnit + m_uOffsetPerUnit;
        UINT uSamplesSoFar = uSamplesCyclesPerUnit / m_uCyclesPerUnit;
        m_uPeriod = uSamplesCyclesPerUnit % m_uCyclesPerUnit;

        // Generate and append the the additional sample(s) to our temporary buffer
        Generate(m_pbFrameSample + (m_uSamplesThisFrame * m_uSampleSize), uSamplesSoFar - m_uSamplesThisFrame);
        m_uSamplesThisFrame = uSamplesSoFar;

        if (fFrameEnd_)
        {
/*
            DWORD dwSpaceAvailable = GetSpaceAvailable();

            // Is there enough space for all this frame's data?
            if (dwSpaceAvailable >= m_uSamplesThisFrame)
            {
*/
                // Add on the current frame's sample data
                AddData(m_pbFrameSample, m_uSamplesThisFrame);
/*
                dwSpaceAvailable -= m_uSamplesThisFrame;

                // Have we fallen below the hover range?
                if (dwSpaceAvailable > m_uSamplesThisFrame)
                {
//                  OutputDebugString("Below hover range\n");

                    // Calculate the remaining space below the hover point
                    dwSpaceAvailable -= (m_uSamplesThisFrame >> 1);

                    // Add as many additional full frames as are needed to get close to the hover point (without exceeding it)
                    while (dwSpaceAvailable >= m_uSamplesThisFrame)
                    {
//                      OutputDebugString(" Added generated frame\n");
                        GenerateExtra(m_pbFrameSample, m_uSamplesThisFrame);
                        AddData(m_pbFrameSample, m_uSamplesThisFrame);
                        dwSpaceAvailable -= m_uSamplesThisFrame;
                    }

                    // Top up the buffer to the hover point
                    if (dwSpaceAvailable)
                    {
//                      OutputDebugString(" Added part generated frame\n");
                        GenerateExtra(m_pbFrameSample, dwSpaceAvailable);
                        AddData(m_pbFrameSample, dwSpaceAvailable);
                    }
                }
            }

            // Else there's not enough space for the full frame of data, but we'll add what we can to leave us at the hover point
            else if (dwSpaceAvailable >= (m_uSamplesThisFrame >> 1))
            {
//              OutputDebugString("Above hover range\n");
//              OutputDebugString(" Adding part frame data\n");
                AddData(m_pbFrameSample, dwSpaceAvailable - (m_uSamplesThisFrame >> 1));
            }
*/

            // Reset the sample counters for the next frame
            m_uOffsetPerUnit += (TSTATES_PER_FRAME * m_uSamplesPerUnit) - (m_uSamplesThisFrame * m_uCyclesPerUnit);
            m_uSamplesThisFrame = 0;
        }
    }

    SDL_UnlockAudio();

    ProfileEnd();
}


inline void CStreamingSound::AddData (BYTE* pbSampleData_, UINT uSamples_)
{
    // We must have some samples or there's be nothing to do
    if (uSamples_)
    {
        UINT uSpace = (m_pbEnd - m_pbNow) / m_uSampleSize;

        if (uSpace < uSamples_)
            TRACE("!!! AddData: too full - only %d of %d bytes added\n", uSpace*m_uSampleSize, uSamples_*m_uSampleSize);

        UINT uAdd = min(uSpace, uSamples_) * m_uSampleSize;
        memcpy(m_pbNow, pbSampleData_, uAdd);
        m_pbNow += uAdd;
    }
}


void CStreamingSound::GenerateExtra (BYTE* pb_, UINT uSamples_)
{
    // Re-use the specified amount from the previous sample, taking care of buffer overlaps
    if (pb_ != m_pbFrameSample)
        memmove(pb_, m_pbFrameSample, uSamples_*m_uSampleSize);
}


void CStreamingSound::SoundCallback (void *pvParam_, Uint8 *pbStream_, int nLen_)
{
    reinterpret_cast<CStreamingSound*>(pvParam_)->Callback(pbStream_, nLen_);
}

void CStreamingSound::Callback (Uint8 *pbStream_, int nLen_)
{
    int nData = m_pbNow - m_pbStart, nCopy = min(nLen_, nData);

    memcpy(pbStream_, m_pbStart, nCopy);
    pbStream_ += nCopy;
    nData -= nCopy;

    memmove(m_pbStart, m_pbStart + nCopy, nData);
    m_pbNow = m_pbStart + nData;

    int nShort = (nLen_ - nCopy) / m_uSampleSize;

    if (nShort)
    {
        TRACE("!!! Callback: short by %d samples\n", nShort);
        Sound::pSAASound->GenerateMany(pbStream_, nShort);

        nShort <<= 1;
        Sound::pSAASound->GenerateMany(m_pbStart, nShort);
        m_pbNow = m_pbStart + (nShort * m_uSampleSize);
    }
}

////////////////////////////////////////////////////////////////////////////////


void CSDLSAASound::Generate (BYTE* pb_, UINT uSamples_)
{
    // Samples could now be zero, so check...
    if (uSamples_ > 0)
        Sound::pSAASound->GenerateMany(pb_, uSamples_);
}

void CSDLSAASound::GenerateExtra (BYTE* pb_, UINT uSamples_)
{
    // If at least one sound update is done per screen line, it's being used for sample playback,
    // so generate the fill-in data from previous data to try and keep it sounding about right
    if (m_nUpdates > HEIGHT_LINES)
        CStreamingSound::GenerateExtra(pb_, uSamples_);

    // Normal SAA sound use, so generate more real samples to give a seamless join
    else
        Sound::pSAASound->GenerateMany(pb_, uSamples_);
}

void CSDLSAASound::Out (WORD wPort_, BYTE bVal_)
{
    Update();

    if ((wPort_ & SOUND_MASK) == SOUND_ADDR)
        Sound::pSAASound->WriteAddress(bVal_);
    else
        Sound::pSAASound->WriteData(bVal_);
}

void CSDLSAASound::Update (bool fFrameEnd_/*=false*/)
{
    m_nUpdates++;
    CStreamingSound::Update(fFrameEnd_);

    if (fFrameEnd_)
        m_nUpdates = 0;
}

////////////////////////////////////////////////////////////////////////////////


UINT HCF (UINT x_, UINT y_)
{
    UINT uHCF = 1, uMin = static_cast<UINT>(sqrt(min(x_, y_)));

    for (UINT uFactor = 2 ; uFactor <= uMin ; uFactor++)
    {
        while (!(x_ % uFactor) && !(y_ % uFactor))
        {
            uHCF *= uFactor;
            x_ /= uFactor;
            y_ /= uFactor;
        }
    }

    return uHCF;
}
