// Part of SimCoupe - A SAM Coupe emulator
//
// Sound.cpp: Allegro sound implementation
//
//  Copyright (c) 1999-2010  Simon Owen
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

#include "Sound.h"
#include "../Extern/SAASound.h"

#include "CPU.h"
#include "IO.h"
#include "Options.h"
#include "Profile.h"

#define SOUND_FREQ      44100
#define SOUND_BITS      16
#define FRAGMENT_SIZE   4096


CSoundStream* aStreams[SOUND_STREAMS];

CSoundStream*& pSAA = aStreams[0];     // SAA 1099 
CSoundStream*& pDAC = aStreams[1];     // DAC for parallel DACs and Spectrum-style beeper

LPCSAASOUND pSAASound;      // SAASound.dll object - needs to exist as long as we do, to preseve subtle internal states

////////////////////////////////////////////////////////////////////////////////

bool InitAllegroSound ()
{
    bool fRet = false;

    reserve_voices(2,1);

    if (install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, NULL) < 0)
        TRACE("install_sound() failed: %s\n", allegro_error);
    else
        fRet = true;

    return fRet;
}

void ExitAllegroSound ()
{
    remove_sound();
}



bool Sound::Init (bool fFirstInit_/*=false*/)
{
    // Clear out any existing config before starting again
    Exit(true);
    TRACE("-> Sound::Init(%s)\n", fFirstInit_ ? "first" : "");

    // All sound disabled?
    if (!GetOption(sound))
        TRACE("Sound disabled, nothing to initialise\n");
    else if (!InitAllegroSound())
        TRACE("Sound initialisation failed\n");
    else
    {
        // If the SAA 1099 chip is enabled, create its driver object
        bool fNeedSAA = GetOption(saasound);
        if (fNeedSAA && (pSAA = new CSAA(GetOption(stereo)?2:1)))
        {
            // Else, create the CSAASound object if it doesn't already exist
            if (pSAASound || (pSAASound = CreateCSAASound()))
            {
                // Set the DLL parameters from the options, so it matches the setup of the primary sound buffer
                pSAASound->SetSoundParameters(SAAP_NOFILTER | SAAP_44100 | SAAP_16BIT | (GetOption(stereo) ? SAAP_STEREO : SAAP_MONO));
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
            Message(msgWarning, "Sound initialisation failed");
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

    ExitAllegroSound();

    for (int i = 0 ; i < SOUND_STREAMS ; i++)
        delete aStreams[i], aStreams[i] = NULL;

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
        reinterpret_cast<CSAA*>(pSAA)->Out(wPort_, bVal_);
}

void Sound::FrameUpdate ()
{
    ProfileStart(Snd);

    if (!g_fTurbo)
    {
        for (int i = 0 ; i < SOUND_STREAMS ; i++)
            if (aStreams[i]) aStreams[i]->Update(true);
    }

    ProfileEnd();
}


void Sound::Silence ()
{
    for (int i = 0 ; i < SOUND_STREAMS ; i++)
        if (aStreams[i]) aStreams[i]->Silence();
}

void Sound::Stop ()
{
    for (int i = 0 ; i < SOUND_STREAMS ; i++)
        if (aStreams[i]) aStreams[i]->Stop();
}

void Sound::Play ()
{
    for (int i = 0 ; i < SOUND_STREAMS ; i++)
        if (aStreams[i]) aStreams[i]->Play();
}


void Sound::OutputDAC (BYTE bVal_)
{
    if (pDAC) reinterpret_cast<CDAC*>(pDAC)->Output(bVal_);
}

void Sound::OutputDACLeft (BYTE bVal_)
{
    if (pDAC) reinterpret_cast<CDAC*>(pDAC)->OutputLeft(bVal_);
}

void Sound::OutputDACRight (BYTE bVal_)
{
    if (pDAC) reinterpret_cast<CDAC*>(pDAC)->OutputRight(bVal_);
}

////////////////////////////////////////////////////////////////////////////////

CStreamBuffer::CStreamBuffer (int nChannels_/*=0*/)
    : m_nChannels(nChannels_), m_nSamplesThisFrame(0), m_uOffsetPerUnit(0), m_uPeriod(0)
{
    // Any values not supplied will be taken from the current options
    if (!m_nChannels) m_nChannels = GetOption(stereo) ? 2 : 1;

    // Use some arbitrary units to keep the numbers manageably small...
    UINT uUnits = Util::HCF(SOUND_FREQ, EMULATED_TSTATES_PER_SECOND);
    m_uSamplesPerUnit = SOUND_FREQ / uUnits;
    m_uCyclesPerUnit = EMULATED_TSTATES_PER_SECOND / uUnits;

    m_nSamplesPerFrame = SOUND_FREQ / EMULATED_FRAMES_PER_SECOND;
    m_nSampleSize = m_nChannels * SOUND_BITS / 8;

    m_pbFrameSample = new BYTE[m_nSamplesPerFrame * m_nSampleSize];
}

CStreamBuffer::~CStreamBuffer ()
{
    delete[] m_pbFrameSample;
}


void CStreamBuffer::Update (bool fFrameEnd_)
{
    ProfileStart(Snd);

    // Limit to a single frame's worth as the raster may be just into the next frame
    UINT uRasterPos = min(g_dwCycleCounter, TSTATES_PER_FRAME);

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
        // Add on the current frame's sample data
        AddData(m_pbFrameSample, m_nSamplesThisFrame*m_nSampleSize);

        // Reset the sample counters for the next frame
        m_uOffsetPerUnit += (TSTATES_PER_FRAME * m_uSamplesPerUnit) - (m_nSamplesThisFrame * m_uCyclesPerUnit);
        m_nSamplesThisFrame = 0;
    }

    ProfileEnd();
}

////////////////////////////////////////////////////////////////////////////////

CSoundStream::CSoundStream (int nChannels_/*=0*/)
    : CStreamBuffer(nChannels_), m_pStream(NULL)
{
    m_nSampleBufferSize = (FRAGMENT_SIZE + (m_nSamplesPerFrame * GetOption(latency))) * m_nSampleSize;
    TRACE("Sample buffer size = %d samples\n", m_nSampleBufferSize/m_nSampleSize);
    m_pbEnd = (m_pbNow = m_pbStart = new BYTE[m_nSampleBufferSize]) + m_nSampleBufferSize;

    Silence();
}

CSoundStream::~CSoundStream ()
{
    delete[] m_pbStart;
}

void CSoundStream::Play ()
{
    if (!m_pStream)
        m_pStream = play_audio_stream(FRAGMENT_SIZE, SOUND_BITS, (m_nChannels>1) ? 1 : 0, SOUND_FREQ, 255, 128);
}

void CSoundStream::Stop ()
{
    if (m_pStream)
    {
        stop_audio_stream(m_pStream);
        m_pStream = NULL;
    }
}

void CSoundStream::Silence (bool fFill_/*=false*/)
{
    memset(m_pbStart, 0x00 , m_pbEnd-m_pbStart);
    m_pbNow = fFill_ ? m_pbEnd-1 : m_pbStart;
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
        int nSpace = m_pbEnd - m_pbNow;

        // Overflow?  If so, discard all we've got to force the callback to correct it
        if (nLength_ > nSpace)
        {
//          m_pbNow = m_pbStart + (m_nSampleBufferSize >> 1);
            TRACE("Overflowed by %d samples\n", (nLength_-nSpace)/m_nSampleSize);
        }

        // Append the new block
        else
        {
            memcpy(m_pbNow, pbData_, nLength_);
            m_pbNow += nLength_;
        }
    }

    BYTE* pb = NULL;
    while (m_pStream && (pb = reinterpret_cast<BYTE*>(get_audio_stream_buffer(m_pStream))))
    {
        int nNeed = FRAGMENT_SIZE*m_nSampleSize, nData = m_pbNow-m_pbStart, nCopy = min(nNeed,nData);

        if (nCopy)
        {
            memcpy(pb, m_pbStart, nCopy);
            memmove(m_pbStart, m_pbStart+nCopy, nData-nCopy);
            m_pbNow = m_pbStart + nData-nCopy;

            nNeed -= nCopy;
        }

        if (nNeed)
        {
            TRACE("Short by %d samples\n", nNeed/m_nSampleSize);
            Generate(pb+nCopy, nNeed/m_nSampleSize);

            int nPad = (m_nSampleBufferSize >> 1);
            if (nCopy && nNeed != nPad)
            {
                Generate(m_pbStart, nPad/m_nSampleSize);
                m_pbNow = m_pbStart + nPad;
            }
        }

        free_audio_stream_buffer(m_pStream);
    }
}


////////////////////////////////////////////////////////////////////////////////


void CSAA::Generate (BYTE* pb_, int nSamples_)
{
    // Samples could now be zero, so check...
    if (nSamples_ > 0)
    {
        pSAASound->GenerateMany(pb_, nSamples_);

        // 16-bit samples need converting from signed to unsigned
        for (nSamples_ <<= (m_nChannels-1) ; nSamples_-- ; pb_[1] ^= 0x80, pb_ += 2);
    }
}

void CSAA::GenerateExtra (BYTE* pb_, int nSamples_)
{
    // If at least one sound update is done per screen line then it's being used for sample playback,
    // so generate the fill-in data from previous data to try and keep it sounding about right
    if (m_nUpdates > HEIGHT_LINES)
        memmove(pb_, m_pbFrameSample, nSamples_*m_nSampleSize);

    // Normal SAA sound use, so generate more real samples to give a seamless join
    else if (nSamples_ > 0)
    {
        pSAASound->GenerateMany(pb_, nSamples_);

        // 16-bit samples need converting from signed to unsigned
        for (nSamples_ <<= (m_nChannels-1) ; nSamples_-- ; pb_[1] ^= 0x80, pb_ += 2);
    }
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

CDAC::CDAC () : CSoundStream(0)
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

        // Mono
        if (m_nChannels == 1)
        {
            WORD *pw = reinterpret_cast<WORD*>(pb_), wSample = ((static_cast<WORD>(m_bLeft) + m_bRight) >> 1) * 0x0101;
            *pw++ = ((static_cast<WORD>(bFirstRight) + bFirstRight) >> 1) * 0x0101;
            while (nSamples_--)
                *pw++ = wSample;
        }

        // Stereo
        else
        {
            WORD wLeft = static_cast<WORD>(m_bLeft) << 8, wRight = static_cast<WORD>(m_bRight) << 8;

            DWORD *pdw = reinterpret_cast<DWORD*>(pb_), dwSample = (static_cast<DWORD>(wRight) << 16) | wLeft;
            *pdw++ = (static_cast<WORD>(bFirstRight) << 24) | (static_cast<WORD>(bFirstLeft) << 8);

            while (nSamples_--)
                *pdw++ = dwSample;
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
