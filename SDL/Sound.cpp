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

#include "SimCoupe.h"
#include <math.h>

#include "SAASound.h"
#define SOUND_IMPLEMENTATION
#include "Sound.h"

#include "CPU.h"
#include "IO.h"
#include "Options.h"
#include "Util.h"
#include "Profile.h"

SDL_AudioSpec sObtained;

UINT HCF (UINT x_, UINT y_);


CSAA* pSAA;     // Pointer to the current driver object for dealing with the sound chip
CDAC *pDAC;     // DAC object used for parallel DAC devices and the Spectrum-style beeper

LPCSAASOUND pSAASound;      // SAASound.dll object - needs to exist as long as we do, to preseve subtle internal states

////////////////////////////////////////////////////////////////////////////////

// Callback used by SDL to request more sound data to play
void CSoundStream::SoundCallback (void *pvParam_, Uint8 *pbStream_, int nLen_)
{
    if (g_fTurbo)
        return;

    ProfileStart(Snd);

    if (pDAC)
    {
        int nData = pDAC->m_pbNow - pDAC->m_pbStart, nCopy = min(nLen_, nData), nLeft = nData-nCopy, nShort = nLen_-nCopy;

        SDL_MixAudio(pbStream_, pDAC->m_pbStart, nCopy, SDL_MIX_MAXVOLUME);

        pDAC->m_pbNow = pDAC->m_pbStart + nLeft;
        memmove(pDAC->m_pbStart, pDAC->m_pbStart + nCopy, nLeft);

        if (nShort)
        {
            TRACE("DAC short by %d bytes of %d (%d copied)\n", nShort, nLen_, nCopy);
            pDAC->Generate(pDAC->m_pbStart, nShort/pDAC->m_nSampleSize);
            SDL_MixAudio(pbStream_+nCopy, pDAC->m_pbStart, nShort, SDL_MIX_MAXVOLUME);

            int nPad = (nLen_ >> 1);
            if (nCopy && nShort != nPad)
            {
                pDAC->Generate(pDAC->m_pbStart, nPad/pDAC->m_nSampleSize);
                pDAC->m_pbNow = pDAC->m_pbStart + nPad;
            }
        }
    }

    if (pSAA)
    {
        int nData = pSAA->m_pbNow - pSAA->m_pbStart, nCopy = min(nLen_, nData), nLeft = nData-nCopy, nShort = nLen_-nCopy;

        SDL_MixAudio(pbStream_, pSAA->m_pbStart, nCopy, SDL_MIX_MAXVOLUME);

        pSAA->m_pbNow = pSAA->m_pbStart + nLeft;
        memmove(pSAA->m_pbStart, pSAA->m_pbStart + nCopy, nLeft);

        if (nShort)
        {
            TRACE("SAA short by %d bytes of %d (%d copied)\n", nShort, nLen_, nCopy);
            pSAA->GenerateExtra(pSAA->m_pbStart, nShort/pSAA->m_nSampleSize);
            SDL_MixAudio(pbStream_+nCopy, pSAA->m_pbStart, nShort, SDL_MIX_MAXVOLUME);

            int nPad = (nLen_ >> 1);
            if (nCopy && nShort != nPad)
            {
                pSAA->Generate(pSAA->m_pbStart, nPad/pSAA->m_nSampleSize);
                pSAA->m_pbNow = pSAA->m_pbStart + nPad;
            }
        }
    }

    ProfileEnd();
}


bool InitDirectSound ()
{
    bool fRet = false;

    SDL_AudioSpec sDesired = { 0 };
    sDesired.freq = GetOption(freq);
    sDesired.format = (GetOption(bits) == 8) ? AUDIO_U8 : AUDIO_S16;
    sDesired.channels = GetOption(stereo) ? 2 : 1;
    sDesired.samples = (GetOption(latency) <= 11) ? 2048 : (GetOption(latency) <= 16) ? 4096 : 8192;
    sDesired.callback = CSoundStream::SoundCallback;

    if (!(fRet = (SDL_OpenAudio(&sDesired, &sObtained) >= 0)))
        TRACE("SDL_OpenAudio failed: %s\n", SDL_GetError());

    return fRet;
}

void ExitDirectSound ()
{
    SDL_CloseAudio();
}



bool Sound::Init (bool fFirstInit_/*=false*/)
{
    // Clear out any existing config before starting again
    Exit(true);
    TRACE("-> Sound::Init(%s)\n", fFirstInit_ ? "first" : "");

    // Correct any approximate/bad option values before we use them
    int nBits = SetOption(bits, (GetOption(bits) > 8) ? 16 : 8);
    int nFreq = SetOption(freq, (GetOption(freq) < 20000) ? 11025 : (GetOption(freq) < 40000) ? 22050 : 44100);
    int nChannels = GetOption(stereo) ? 2 : 1;


    // All sound disabled?
    if (!GetOption(sound))
        TRACE("Sound disabled, nothing to initialise\n");
    else if (!InitDirectSound())
    {
        TRACE("DirectX initialisation failed\n");
        SetOption(sound,0);
    }
    else
    {
        // If the SAA 1099 chip is enabled, create its driver object
        bool fNeedSAA = GetOption(saasound);
        if (fNeedSAA && (pSAA = new CSAA(nFreq,nBits,nChannels)))
        {
            // Else, create the CSAASound object if it doesn't already exist
            if (pSAASound || (pSAASound = CreateCSAASound()))
            {
                // Set the DLL parameters from the options, so it matches the setup of the primary sound buffer
                pSAASound->SetSoundParameters((GetOption(filter) ? SAAP_FILTER : SAAP_NOFILTER) |
                    ((nFreq == 11025) ? SAAP_11025 : (nFreq == 22050) ? SAAP_22050 : SAAP_44100) |
                    ((nBits == 8) ? SAAP_8BIT : SAAP_16BIT) | (GetOption(stereo) ? SAAP_STEREO : SAAP_MONO));
            }
        }

        // If a DAC is connected to a parallel port or the Spectrum-style beeper is enabled, we need a CDAC object
        bool fNeedDAC = GetOption(parallel1) >= 2 || GetOption(parallel2) >= 2 || GetOption(beeper);

        // Create and initialise a DAC, if required
        if (fNeedDAC)
            pDAC = new CDAC;

        // If anything failed, disable the sound
        if ((fNeedSAA && !pSAA) || (fNeedDAC && !pDAC))
        {
            Message(msgWarning, "Sound initialisation failed, disabling...");
            SetOption(sound,0);
            Exit();
        }
    }

    Play();

    // Sound initialisation failure isn't fatal, so always return success
    TRACE("<- Sound::Init()\n");
    return true;
}

void Sound::Exit (bool fReInit_/*=false*/)
{
    TRACE("-> Sound::Exit(%s)\n", fReInit_ ? "reinit" : "");

    ExitDirectSound();

    if (pSAA) { delete pSAA; pSAA = NULL; }
    if (pDAC) { delete pDAC; pDAC = NULL; }

    if (pSAASound && !fReInit_)
    {
        DestroyCSAASound(pSAASound);
        pSAASound = NULL;
    }

    TRACE("<- Sound::Exit()\n");
}


void Sound::Out (WORD wPort_, BYTE bVal_)
{
    if (pSAA)
        pSAA->Out(wPort_, bVal_);
}

void Sound::FrameUpdate ()
{
    ProfileStart(Snd);

    if (!g_fTurbo)
    {
        if (pSAA) pSAA->Update(true);
        if (pDAC) pDAC->Update(true);
    }

    ProfileEnd();
}


void Sound::Silence ()
{
    if (pSAA) pSAA->Silence();
    if (pDAC) pDAC->Silence();
}

void Sound::Stop ()
{
    SDL_PauseAudio(1);
}

void Sound::Play ()
{
    SDL_PauseAudio(0);
}


void Sound::OutputDAC (BYTE bVal_)
{
    if (pDAC) pDAC->Output(bVal_);
}

void Sound::OutputDACLeft (BYTE bVal_)
{
    if (pDAC) pDAC->OutputLeft(bVal_);
}

void Sound::OutputDACRight (BYTE bVal_)
{
    if (pDAC) pDAC->OutputRight(bVal_);
}

////////////////////////////////////////////////////////////////////////////////


CStreamBuffer::CStreamBuffer (int nFreq_, int nBits_, int nChannels_)
    : m_nFreq(nFreq_), m_nBits(nBits_), m_nChannels(nChannels_),
      m_nSamplesThisFrame(0), m_uOffsetPerUnit(0), m_uPeriod(0),
      m_pbFrameSample(NULL)
{
    // Any values not supplied will be taken from the current options
    if (!m_nFreq) m_nFreq = GetOption(freq);
    if (!m_nBits) m_nBits = GetOption(bits);
    if (!m_nChannels) m_nChannels = GetOption(stereo) ? 2 : 1;

    // Use some arbitrary units to keep the numbers manageably small...
    UINT uUnits = HCF(m_nFreq, EMULATED_TSTATES_PER_SECOND);
    m_uSamplesPerUnit = m_nFreq / uUnits;
    m_uCyclesPerUnit = EMULATED_TSTATES_PER_SECOND / uUnits;

    // Do this because 50Hz doesn't divide exactly in to 11025Hz...
    m_nMaxSamplesPerFrame = (m_nFreq+EMULATED_FRAMES_PER_SECOND-1) / EMULATED_FRAMES_PER_SECOND;
    m_nSampleSize = m_nChannels * m_nBits / 8;

    m_pbFrameSample = new BYTE[m_nMaxSamplesPerFrame * m_nSampleSize];
}

CStreamBuffer::~CStreamBuffer ()
{
    delete m_pbFrameSample;
}


void CStreamBuffer::Update (bool fFrameEnd_)
{
    ProfileStart(Snd);

    // Limit to a single frame's worth as the raster may be just into the next frame
    UINT uRasterPos = ((g_nLine * TSTATES_PER_LINE) + g_nLineCycle);
    uRasterPos = min(uRasterPos, TSTATES_PER_FRAME);

    // Calculate the number of whole samples passed and the amount spanning in to the next sample
    UINT uSamplesCyclesPerUnit = uRasterPos * m_uSamplesPerUnit + m_uOffsetPerUnit;
    int nSamplesSoFar = uSamplesCyclesPerUnit / m_uCyclesPerUnit;
    m_uPeriod = uSamplesCyclesPerUnit % m_uCyclesPerUnit;

    // Generate and append the the additional sample(s) to our temporary buffer
    m_nSamplesThisFrame = min(m_nSamplesThisFrame, nSamplesSoFar);
    Generate(m_pbFrameSample + (m_nSamplesThisFrame * m_nSampleSize), nSamplesSoFar - m_nSamplesThisFrame);
    m_nSamplesThisFrame = nSamplesSoFar;

    if (fFrameEnd_)
    {
//      DWORD dwSpaceAvailable = GetSpaceAvailable();

        // Is there enough space for all this frame's data?
//      if (dwSpaceAvailable >= m_nSamplesThisFrame)
        {
            // Add on the current frame's sample data
            AddData(m_pbFrameSample, m_nSamplesThisFrame*m_nSampleSize);
//          dwSpaceAvailable -= m_nSamplesThisFrame;
        }

        // Reset the sample counters for the next frame
        m_uOffsetPerUnit += (TSTATES_PER_FRAME * m_uSamplesPerUnit) - (m_nSamplesThisFrame * m_uCyclesPerUnit);
        m_nSamplesThisFrame = 0;
    }

    ProfileEnd();
}

////////////////////////////////////////////////////////////////////////////////

CSoundStream::CSoundStream (int nFreq_/*=0*/, int nBits_/*=0*/, int nChannels_/*=0*/)
    : CStreamBuffer(nFreq_, nBits_, nChannels_)
{
    m_nSampleBufferSize = sObtained.size * 2 + (m_nMaxSamplesPerFrame * m_nSampleSize);

    m_pbEnd = (m_pbNow = m_pbStart = new BYTE[m_nSampleBufferSize]) + m_nSampleBufferSize;
    Silence();
}

CSoundStream::~CSoundStream ()
{
    delete m_pbStart;
}


void CSoundStream::Silence (bool fFill_/*=false*/)
{
    SDL_LockAudio();
    memset(m_pbStart, sObtained.silence, m_pbEnd-m_pbStart);
    m_pbNow = fFill_ ? m_pbEnd-1 : m_pbStart;
    SDL_UnlockAudio();
}


int CSoundStream::GetSpaceAvailable ()
{
    return m_pbEnd-m_pbNow;
}

void CSoundStream::AddData (BYTE* pbData_, int nLength_)
{
    // We must have some samples or there's be nothing to do
    if (nLength_ > 0)
    {
        SDL_LockAudio();

        // How many samples do we have space for?
        int nSpace = m_pbEnd - m_pbNow;

        // Add as much of it as we can
        int nAdd = min(nSpace, nLength_);
        memcpy(m_pbNow, pbData_, nAdd);
        m_pbNow += nAdd;

//      TRACE("Adding %d of %d bytes, %d bytes free\n", nAdd, nLength_, m_pbNow - m_pbStart);

        // Did we have too much data?
        if (nSpace < nLength_)
        {
            // Discard some data to get us back to a safe point where we won't overflow again immediately
            m_pbNow = m_pbStart + sObtained.samples;
            TRACE("!!! Sound over-flow: %d samples too many\n", (nLength_-nSpace)/m_nSampleSize);
        }

        SDL_UnlockAudio();
    }
}


////////////////////////////////////////////////////////////////////////////////


void CSAA::Generate (BYTE* pb_, int nSamples_)
{
    // Samples could now be zero, so check...
    if (nSamples_ > 0)
        pSAASound->GenerateMany(pb_, nSamples_);
}

void CSAA::GenerateExtra (BYTE* pb_, int nSamples_)
{
    // If at least one sound update is done per screen line then it's being used for sample playback,
    // so generate the fill-in data from previous data to try and keep it sounding about right
    if (m_nUpdates > HEIGHT_LINES)
        memmove(pb_, m_pbFrameSample, nSamples_*m_nSampleSize);

    // Normal SAA sound use, so generate more real samples to give a seamless join
    else if (nSamples_ > 0)
        pSAASound->GenerateMany(pb_, nSamples_);
}

void CSAA::Out (WORD wPort_, BYTE bVal_)
{
    Update();

    if ((wPort_ & SOUND_MASK) == SOUND_ADDR)
        pSAASound->WriteAddress(bVal_);
    else
        pSAASound->WriteData(bVal_);
}

void CSAA::Update (bool fFrameEnd_/*=false*/)
{
    // Count the updates in the current frame, to watch for sample playback
    if (!fFrameEnd_)
        m_nUpdates++;
    else
        m_nUpdates = 0;

    CStreamBuffer::Update(fFrameEnd_);
}

////////////////////////////////////////////////////////////////////////////////

CDAC::CDAC () : CSoundStream(0, 0, 0)
{
    m_uLeftTotal = m_uRightTotal = m_uPrevPeriod = 0;
    m_bLeft = m_bRight = 0x80;
}


void CDAC::Generate (BYTE* pb_, int nSamples_)
{
    if (!nSamples_)
    {
        // If we are still on the same sample then update the mean level that spans it
        UINT uPeriod = m_uPeriod - m_uPrevPeriod;
        m_uLeftTotal += m_bLeft * uPeriod;
        m_uRightTotal += m_bRight * uPeriod;
    }
    else if (nSamples_ > 0)
    {
        // Otherwise output the mean level spanning the completed sample
        UINT uPeriod = m_uCyclesPerUnit - m_uPrevPeriod;
        BYTE bFirstLeft = static_cast<BYTE>((m_uLeftTotal + m_bLeft * uPeriod) / m_uCyclesPerUnit);
        BYTE bFirstRight = static_cast<BYTE>((m_uRightTotal + m_bRight * uPeriod) / m_uCyclesPerUnit);
        nSamples_--;

        // 8-bit?
        if (m_nBits == 8)
        {
            // Mono?
            if (m_nChannels == 1)
            {
                *pb_++ = (bFirstLeft >> 1) + (bFirstRight >> 1);
                memset(pb_, (m_bLeft >> 1) + (m_bRight >> 1), nSamples_);
            }

            // Stereo
            else
            {
                *pb_++ = bFirstLeft;
                *pb_++ = bFirstRight;

                // Fill in the block of complete samples at this level
                if (m_bLeft == m_bRight)
                    memset(pb_, m_bLeft, nSamples_ << 1);
                else
                {
                    WORD *pw = reinterpret_cast<WORD*>(pb_), wSample = (static_cast<WORD>(m_bRight) << 8) | m_bLeft;
                    while (nSamples_--)
                        *pw++ = wSample;
                }
            }
        }

        // 16-bit
        else
        {
            // Mono
            if (m_nChannels == 1)
            {
                WORD *pw = reinterpret_cast<WORD*>(pb_), wSample = ((static_cast<WORD>(m_bLeft) + m_bRight - 0x100) / 2) * 0x101;
                *pw++ = ((static_cast<WORD>(bFirstRight) + bFirstRight - 0x100) / 2) * 0x101;
                while (nSamples_--)
                    *pw++ = wSample;
            }

            // Stereo
            else
            {
                WORD wLeft = static_cast<WORD>(m_bLeft-0x80) << 8, wRight = static_cast<WORD>(m_bRight-0x80) << 8;

                DWORD *pdw = reinterpret_cast<DWORD*>(pb_), dwSample = (static_cast<DWORD>(wRight) << 16) | wLeft;
                *pdw++ = (static_cast<WORD>(bFirstRight-0x80) << 24) | (static_cast<WORD>(bFirstLeft-0x80) << 8);

                while (nSamples_--)
                    *pdw++ = dwSample;
            }
        }

        // Initialise the mean level for the next sample
        m_uLeftTotal = m_bLeft * m_uPeriod;
        m_uRightTotal = m_bRight * m_uPeriod;
    }

    // Store the positon spanning the current sample
    m_uPrevPeriod = m_uPeriod;
}

void CDAC::GenerateExtra (BYTE* pb_, int nSamples_)
{
    // Re-use the specified amount from the previous sample,
    if (pb_ != m_pbFrameSample)
        memmove(pb_, m_pbFrameSample, nSamples_*m_nSampleSize);
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
