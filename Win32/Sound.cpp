// Part of SimCoupe - A SAM Coupe emulator
//
// Sound.cpp: Win32 sound implementation using DirectSound
//
//  Copyright (c) 1999-2005  Simon Owen
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
#include "Util.h"
#include "Profile.h"

#define SOUND_FREQ      44100
#define SOUND_BITS      16

extern HWND g_hwnd;

// Direct sound, primary buffer and secondary buffer interface pointers
IDirectSound* g_pds = NULL;
IDirectSoundBuffer* g_pdsbPrimary;

CSoundStream* aStreams[SOUND_STREAMS];

CSoundStream*& pSAA = aStreams[0];     // SAA 1099 
CSoundStream*& pDAC = aStreams[1];     // DAC for parallel DACs and Spectrum-style beeper

LPCSAASOUND pSAASound;  // SAASound.dll object - needs to exist as long as we do, to preseve subtle internal states

////////////////////////////////////////////////////////////////////////////////

bool InitDirectSound (bool fFirstInit_)
{
    HRESULT hr;
    bool fRet = false;

    // Initialise DirectX
    if (FAILED(hr = pfnDirectSoundCreate(NULL, &g_pds, NULL)))
        TRACE("!!! DirectSoundCreate failed (%#08lx)\n", hr);

    // We want priority control over the sound format while we're active
    else if (FAILED(hr = g_pds->SetCooperativeLevel(g_hwnd, DSSCL_PRIORITY)))
        TRACE("!!! SetCooperativeLevel() failed (%#08lx)\n", hr);
    else
    {
        DSBUFFERDESC dsbd = { sizeof DSBUFFERDESC };
        dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;

        // Create the primary buffer
        if (FAILED(hr = g_pds->CreateSoundBuffer(&dsbd, &g_pdsbPrimary, NULL)))
            TRACE("!!! CreateSoundBuffer() failed with primary buffer (%#08lx)\n", hr);

        // Eliminate the mixing slowdown by setting the primary buffer to our required format
        else
        {
            // Set up the sound format according to the sound options
            WAVEFORMATEX wf = {0};
            wf.wFormatTag = WAVE_FORMAT_PCM;
            wf.nSamplesPerSec = SOUND_FREQ;
            wf.wBitsPerSample = SOUND_BITS;
            wf.nChannels = GetOption(stereo) ? 2 : 1;
            wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
            wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

            // Set the primary buffer format (closest match will be used if hardware doesn't support what we request)
            if (FAILED(hr = g_pdsbPrimary->SetFormat(&wf)))
                TRACE("!!! SetFormat() failed on primary buffer (%#08lx)\n", hr);

            // Play the primary buffer all the time to eliminate the mixer delays in stopping and starting
            else if (FAILED(hr = g_pdsbPrimary->Play(0, 0, DSBPLAY_LOOPING)))
                TRACE("!!! Play() failed on primary buffer (%#08lx)\n", hr);
            else
                fRet = true;    // Success :-)
        }
    }

    return fRet;
}

void ExitDirectSound (bool fReInit_)
{
    if (g_pdsbPrimary)
    {
        g_pdsbPrimary->Stop();
        g_pdsbPrimary->Release();
        g_pdsbPrimary = NULL;
    }

    if (g_pds) { g_pds->Release(); g_pds = NULL; }
}


bool Sound::Init (bool fFirstInit_/*=false*/)
{
    // Clear out any existing config before starting again
    Exit(true);
    TRACE("-> Sound::Init(%s)\n", fFirstInit_ ? "first" : "");

    // All sound disabled?
    if (!GetOption(sound))
        TRACE("Sound disabled, nothing to initialise\n");
    else if (!InitDirectSound(fFirstInit_))
        TRACE("DirectSound initialisation failed\n");
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

#ifdef USE_TESTHW
        TestHW::SoundInit(fFirstInit_);
#endif
    }

    Play();

    // Sound initialisation failure isn't fatal, so always return success
    TRACE("<- Sound::Init()\n");
    return true;
}

void Sound::Exit (bool fReInit_/*=false*/)
{
    TRACE("-> Sound::Exit(%s)\n", fReInit_ ? "reinit" : "");

    ExitDirectSound(fReInit_);

    for (int i = 0 ; i < SOUND_STREAMS ; i++)
        delete aStreams[i], aStreams[i] = NULL;

#ifdef USE_TESTHW
    TestHW::SoundExit(fReInit_);
#endif

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

    Silence();
}

void Sound::Play ()
{
    Silence();

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

CStreamBuffer::CStreamBuffer (int nChannels_)
    : m_nChannels(nChannels_), m_pbFrameSample(NULL), m_nSamplesThisFrame(0), m_uOffsetPerUnit(0), m_uPeriod(0)
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
        DWORD dwSpaceAvailable = GetSpaceAvailable(), dwHoverSize = m_nSamplesPerFrame * (GetOption(latency)+1)/2;

        // Is there enough space for all this frame's data?
        if (dwSpaceAvailable >= static_cast<DWORD>(m_nSamplesThisFrame))
        {
            // Add on the current frame's sample data
            AddData(m_pbFrameSample, m_nSamplesThisFrame*m_nSampleSize);
            dwSpaceAvailable -= m_nSamplesThisFrame;

            // Have we fallen below the hover range?
            if (dwSpaceAvailable > dwHoverSize)
            {
//              TRACE("Below hover range\n");

                // Calculate the remaining space below the hover point
                dwSpaceAvailable -= (dwHoverSize >> 1);

                // Add as many additional full frames as are needed to get close to the hover point (without exceeding it)
                while (m_nSamplesThisFrame && dwSpaceAvailable >= static_cast<DWORD>(m_nSamplesThisFrame))
                {
//                  TRACE(" Added generated frame\n");
                    GenerateExtra(m_pbFrameSample, m_nSamplesThisFrame);
                    AddData(m_pbFrameSample, m_nSamplesThisFrame*m_nSampleSize);
                    dwSpaceAvailable -= m_nSamplesThisFrame;
                }

                // Top up the buffer to the hover point
                if (dwSpaceAvailable)
                {
//                  TRACE(" Added part generated frame\n");
                    GenerateExtra(m_pbFrameSample, dwSpaceAvailable);
                    AddData(m_pbFrameSample, dwSpaceAvailable*m_nSampleSize);
                }
            }
        }

        // Else there's not enough space for the full frame of data, but we'll add what we can to leave us at the hover point
        else if (dwSpaceAvailable >= (dwHoverSize >> 1))
        {
//          TRACE("Above hover range\n Adding part frame data\n");
            AddData(m_pbFrameSample, (dwSpaceAvailable - (dwHoverSize >> 1)) * m_nSampleSize);
        }

        // Reset the sample counters for the next frame
        m_uOffsetPerUnit += (TSTATES_PER_FRAME * m_uSamplesPerUnit) - (m_nSamplesThisFrame * m_uCyclesPerUnit);
        m_nSamplesThisFrame = 0;
    }

    ProfileEnd();
}

////////////////////////////////////////////////////////////////////////////////

CSoundStream::CSoundStream (int nChannels_/*=0*/)
    : CStreamBuffer(nChannels_), m_pdsb(NULL), m_dwWriteOffset(0)
{
    m_nSampleBufferSize = m_nSamplesPerFrame * m_nSampleSize * (GetOption(latency)+1);

    WAVEFORMATEX wf;
    g_pdsbPrimary->GetFormat(&wf,  sizeof wf, NULL);
    wf.cbSize = sizeof wf;

    wf.nChannels = m_nChannels;
    wf.wBitsPerSample = SOUND_BITS;
    wf.nSamplesPerSec = SOUND_FREQ;

    wf.nBlockAlign = m_nChannels * SOUND_BITS / 8;
    wf.nAvgBytesPerSec = SOUND_FREQ * wf.nBlockAlign;


    DSBUFFERDESC dsbd = { sizeof DSBUFFERDESC };
    dsbd.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
    dsbd.dwBufferBytes = m_nSampleBufferSize;
    dsbd.lpwfxFormat = &wf;

    m_pdsb = NULL;
    HRESULT hr = g_pds->CreateSoundBuffer(&dsbd, &m_pdsb, NULL);

    if (FAILED(hr))
        TRACE("!!! CreateSoundBuffer failed (%#08lx)\n", hr);
    else
    {
        // Make sure the buffer contents is silence before playing
        Silence();
        Play();
    }
}

CSoundStream::~CSoundStream ()
{
}



bool CSoundStream::Play ()
{
    return m_pdsb && SUCCEEDED(m_pdsb->Play(0, 0, DSBPLAY_LOOPING));
}

bool CSoundStream::Stop ()
{
    return m_pdsb && SUCCEEDED(m_pdsb->Stop());
}

void CSoundStream::Silence (bool fFill_/*=false*/)
{
    PVOID pvWrite1, pvWrite2;
    DWORD dwLength1, dwLength2;

    // Silence the stored buffer
    memset(m_pbFrameSample, 0x00, m_nSamplesPerFrame*m_nSampleSize);

    // Lock the buffer to obtain pointers and lengths for data writes
    if (SUCCEEDED(m_pdsb->Lock(0, 0, &pvWrite1, &dwLength1, &pvWrite2, &dwLength2, DSBLOCK_ENTIREBUFFER)))
    {
        // Silence the hardware buffer
        memset(pvWrite1, 0x00, dwLength1);
        m_pdsb->Unlock(pvWrite1, dwLength1, pvWrite2, dwLength2);
    }

    // The new write offset will be the current write cursor position
    DWORD dwPlayCursor, dwWriteCursor;
    m_pdsb->GetCurrentPosition(&dwPlayCursor, &dwWriteCursor);
    m_dwWriteOffset = dwPlayCursor;

    m_nSamplesThisFrame = 0;
}


int CSoundStream::GetSpaceAvailable ()
{
    HRESULT hr;
    DWORD dwPlayCursor, dwWriteCursor;

    // Get the current play cursor position
    if (FAILED(hr = m_pdsb->GetCurrentPosition(&dwPlayCursor, &dwWriteCursor)))
        return 0;

    // The amount of space free depends on where the last write postion is relative to the play cursor
    int nSpace = (m_dwWriteOffset <= dwPlayCursor) ? dwPlayCursor - m_dwWriteOffset :
                    m_nSampleBufferSize - (m_dwWriteOffset - dwPlayCursor);

    // Return the number of samples
    return nSpace / m_nSampleSize;
}

void CSoundStream::AddData (BYTE* pbData_, int nLength_)
{
    LPVOID pvWrite1, pvWrite2;
    DWORD dwLength1, dwLength2;

    if (nLength_ <= 0)
        return;

    // Lock the required buffer range so we can add the new data
    HRESULT hr = m_pdsb->Lock(m_dwWriteOffset, nLength_, &pvWrite1, &dwLength1, &pvWrite2, &dwLength2, 0);
    if (FAILED(hr))
        TRACE("!!! Failed to lock sound buffer! (%#08lx)\n", hr);
    else
    {
        // Write the first part (maybe all)
        memcpy(pvWrite1, pbData_, dwLength1);

        // Write the second block if necessary
        if (dwLength2)
            memcpy(pvWrite2, pbData_+dwLength1, dwLength2);

        // Unlock the buffer
        hr = m_pdsb->Unlock(pvWrite1, dwLength1, pvWrite2, dwLength2);
        if (FAILED(hr))
            TRACE("!!! Failed to unlock sound buffer! (%#08lx)\n", hr);

        // Work out the offset for the next write in the circular buffer
        m_dwWriteOffset = (m_dwWriteOffset + dwLength1 + dwLength2) % m_nSampleBufferSize;
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

CDAC::CDAC () : CSoundStream(2)
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
