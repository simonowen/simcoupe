// Part of SimCoupe - A SAM Coupé emulator
//
// Sound.cpp: Win32 sound implementation using DirectSound
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

#include "../Extern/SAASound.h"
#define SOUND_IMPLEMENTATION
#include "Sound.h"

#include "IO.h"
#include "Options.h"
#include "Util.h"
#include "Profile.h"

#ifndef DUMMY_SAASOUND
#pragma comment(lib, "SAASound")
#endif


extern HWND g_hwnd;
extern int g_nLine, g_nLineCycle;

// Direct sound, primary buffer and secondary buffer interface pointers
IDirectSound* CStreamingSound::s_pds = NULL;
IDirectSoundBuffer* CStreamingSound::s_pdsbPrimary;
int CStreamingSound::s_nUsage = 0;

UINT HCF (UINT x_, UINT y_);


namespace Sound
{
CDirectXSAASound* pSAA;     // Pointer to the current driver object for dealing with the sound chip
CDAC *pDAC;                 // DAC object used for parallel DAC devices and the Spectrum-style beeper

LPCSAASOUND pSAASound;      // SAASound.dll object - needs to exist as long as we do, to preseve subtle internal states

////////////////////////////////////////////////////////////////////////////////

bool Init (bool fFirstInit_/*=false*/)
{
    // Clear out any existing config before starting again
    Exit(true);
    TRACE("-> Sound::Init(%s)\n", fFirstInit_ ? "first" : "");

    // All sound disabled?
    if (!GetOption(sound))
        TRACE("Sound disabled, nothing to initialise\n");
    else
    {
        // If the SAA 1099 chip is enabled, create its driver object
        if (GetOption(saasound) && (pSAA = new CDirectXSAASound))
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
    if (pSAA) { pSAA->Stop(); pSAA->Silence(); }
    if (pDAC) { pDAC->Stop(); pDAC->Silence(); }
}

void Play ()
{
    if (pSAA) { pSAA->Silence(); pSAA->Play(); }
    if (pDAC) { pDAC->Silence(); pDAC->Play(); }
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




bool CStreamingSound::InitDirectSound ()
{
    HRESULT hr;
    bool fRet = false;

    // Initialise DirectX
    if (FAILED(hr = DirectSoundCreate(NULL, &s_pds, NULL)))
        TRACE("!!! DirectSoundCreate failed (%#08lx)\n", hr);

    // We want priority control over the sound format while we're active
    else if (FAILED(hr = s_pds->SetCooperativeLevel(g_hwnd, DSSCL_PRIORITY)))
        TRACE("!!! SetCooperativeLevel() failed (%#08lx)\n", hr);
    else
    {
        DSBUFFERDESC dsbd;
        ZeroMemory(&dsbd, sizeof dsbd);
        dsbd.dwSize = sizeof DSBUFFERDESC;
        dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;

        // Create the primary buffer
        if (FAILED(hr = s_pds->CreateSoundBuffer(&dsbd, &s_pdsbPrimary, NULL)))
            TRACE("!!! CreateSoundBuffer() failed with primary buffer (%#08lx)\n", hr);

        // Eliminate the mixing slowdown by setting the primary buffer to our required format
        else
        {
            // Set up the sound format according to the sound options
            WAVEFORMATEX wf;
            wf.cbSize = sizeof WAVEFORMATEX;
            wf.wFormatTag = WAVE_FORMAT_PCM;
            wf.nChannels = GetOption(stereo) ? 2 : 1;
            wf.wBitsPerSample = GetOption(bits) > 8 ? 16 : 8;
            wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
            wf.nSamplesPerSec = (GetOption(frequency) < 20000) ? 11025 : (GetOption(frequency) < 40000) ? 22050 : 44100;
            wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

            // Set the primary buffer format (closest match will be used if hardware doesn't support what we request)
            if (FAILED(hr = s_pdsbPrimary->SetFormat(&wf)))
                TRACE("!!! SetFormat() failed on primary buffer (%#08lx)\n", hr);

            // Play the primary buffer all the time to eliminate the mixer delays in stopping and starting
            else if (FAILED(hr = s_pdsbPrimary->Play(0, 0, DSBPLAY_LOOPING)))
                TRACE("!!! Play() failed on primary buffer (%#08lx)\n", hr);

            // Success :-)
            else
                fRet = true;
        }
    }

    return fRet;
}


void CStreamingSound::ExitDirectSound ()
{
    if (s_pdsbPrimary)
    {
        s_pdsbPrimary->Stop();
        s_pdsbPrimary->Release();
        s_pdsbPrimary = NULL;
    }

    if (s_pds) { s_pds->Release(); s_pds = NULL; }
}



CStreamingSound::CStreamingSound (int nFreq_/*=0*/, int nBits_/*=0*/, int nChannels_/*=0*/)
{
    m_pdsb = NULL;

    m_nFreq = nFreq_;
    m_nBits = nBits_;
    m_nChannels = nChannels_;

    m_pbFrameSample = NULL;
}

CStreamingSound::~CStreamingSound ()
{
    if (m_pdsb) { m_pdsb->Release(); m_pdsb = NULL; }
    if (m_pbFrameSample) { delete m_pbFrameSample; m_pbFrameSample = NULL; }

    // If this is the last object we have to close down DirectSound
    if (!--s_nUsage)
        ExitDirectSound();
}


bool CStreamingSound::Init ()
{
    bool fRet = true;

    // If this is the first object we have to set up DirectSound
    if (!s_nUsage++)
        fRet = InitDirectSound();

    if (fRet)
    {
        HRESULT hr;

        WAVEFORMATEX wf;
        s_pdsbPrimary->GetFormat(&wf,  sizeof wf, NULL);
        wf.cbSize = sizeof wf;

        if (!m_nFreq)
            m_nFreq = wf.nSamplesPerSec;

        if (!m_nBits)
            m_nBits = wf.wBitsPerSample;

        if (!m_nChannels)
            m_nChannels = wf.nChannels;

        wf.nChannels = m_nChannels;
        wf.wBitsPerSample = m_nBits;
        wf.nSamplesPerSec = m_nFreq;

        wf.nBlockAlign = m_nChannels * m_nBits / 8;
        wf.nAvgBytesPerSec = (m_nFreq) * wf.nBlockAlign;


        // Use some arbitrary units to keep the numbers manageably small...
        UINT uUnits = HCF(m_nFreq, EMULATED_TSTATES_PER_SECOND);
        m_uSamplesPerUnit = m_nFreq / uUnits;
        m_uCyclesPerUnit = EMULATED_TSTATES_PER_SECOND / uUnits;

        // Do this because 50Hz doesn't divide exactly in to 11025Hz...
        UINT uMaxSamplesPerFrame = m_nFreq / EMULATED_FRAMES_PER_SECOND;
        if ((m_nFreq % EMULATED_FRAMES_PER_SECOND) > 0)
            uMaxSamplesPerFrame++;
        m_uSampleSize = m_nChannels * m_nBits / 8;
        m_uSampleBufferSize = (uMaxSamplesPerFrame * m_uSampleSize * GetOption(latency));
//      m_uSampleBufferSize = (m_uSampleBufferSize + (16-1)) & ~(16-1);

        m_pbFrameSample = new BYTE[m_uSampleBufferSize];
        m_nSamplesThisFrame = 0;
        m_dwWriteOffset = 0;
        m_uOffsetPerUnit = 0;
        m_uPeriod = 0;


        DSBUFFERDESC dsbd;
        ZeroMemory(&dsbd, sizeof dsbd);
        dsbd.dwSize = sizeof DSBUFFERDESC;
        dsbd.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
        dsbd.dwBufferBytes = m_uSampleBufferSize;
        dsbd.lpwfxFormat = &wf;

        m_pdsb = NULL;
        if (FAILED(hr = s_pds->CreateSoundBuffer(&dsbd, &m_pdsb, NULL)))
        {
            TRACE("!!! CreateSoundBuffer failed (%#08lx)\n", hr);
            fRet = false;
        }
        else
        {
            // Make sure the buffer contents is silent before playing
            Silence();

            // Play for as long as we're running
            if (FAILED(hr = m_pdsb->Play(0, 0, DSBPLAY_LOOPING)))
                TRACE("!!! Play failed on DirectSound buffer (%#08lx)\n", hr);
        }
    }

    return fRet;
}


void CStreamingSound::Silence ()
{
    PVOID pvWrite1, pvWrite2;
    DWORD dwLength1, dwLength2;

    BYTE bSilence = (m_nBits == 16) ? 0x00 : 0x80;

    // Lock the entire buffer and fill will the values for silence
    if (m_pdsb)
    {
        FillMemory(m_pbFrameSample, bSilence, m_uSampleBufferSize);

        if (SUCCEEDED(m_pdsb->Lock(0, 0, &pvWrite1, &dwLength1, &pvWrite2, &dwLength2, DSBLOCK_ENTIREBUFFER)))
        {
            FillMemory(pvWrite1, dwLength1, bSilence);
            m_pdsb->Unlock(pvWrite1, dwLength1, pvWrite2, dwLength2);
        }

        // The new write offset will be the current write cursor position
        DWORD dwPlayCursor, dwWriteCursor;
        m_pdsb->GetCurrentPosition(&dwPlayCursor, &dwWriteCursor);
        m_dwWriteOffset = dwPlayCursor;
    }

    m_nSamplesThisFrame = 0;
}


DWORD CStreamingSound::GetSpaceAvailable ()
{
    HRESULT hr;
    DWORD dwPlayCursor, dwWriteCursor;

    // Get the current play cursor position
    if (FAILED(hr = m_pdsb->GetCurrentPosition(&dwPlayCursor, &dwWriteCursor)))
        return 0;

    // The amount of space free depends on where the last write postion is relative to the play cursor
    UINT uSpace = (m_dwWriteOffset <= dwPlayCursor) ? dwPlayCursor - m_dwWriteOffset :
                    m_uSampleBufferSize - (m_dwWriteOffset - dwPlayCursor);

    // Return the number of samples
    return uSpace / m_uSampleSize;
}


void CStreamingSound::Update (bool fFrameEnd_)
{
    ProfileStart(Snd);

    // Got a DirectSound buffer pointer?
    if (m_pdsb)
    {
        // Calculate the number of whole samples passed and the amount spanning in to the next sample
        UINT uSamplesCyclesPerUnit = ((g_nLine * TSTATES_PER_LINE) + g_nLineCycle) * m_uSamplesPerUnit + m_uOffsetPerUnit;
        int nSamplesSoFar = uSamplesCyclesPerUnit / m_uCyclesPerUnit;
        m_uPeriod = uSamplesCyclesPerUnit % m_uCyclesPerUnit;

        // Generate and append the the additional sample(s) to our temporary buffer
        m_nSamplesThisFrame = min(m_nSamplesThisFrame, nSamplesSoFar);
        Generate(m_pbFrameSample + (m_nSamplesThisFrame * m_uSampleSize), nSamplesSoFar - m_nSamplesThisFrame);
        m_nSamplesThisFrame = nSamplesSoFar;

        if (fFrameEnd_)
        {
            DWORD dwSpaceAvailable = GetSpaceAvailable();

            // Is there enough space for all this frame's data?
            if (dwSpaceAvailable >= m_nSamplesThisFrame)
            {
                // Add on the current frame's sample data
                AddData(m_pbFrameSample, m_nSamplesThisFrame);
                dwSpaceAvailable -= m_nSamplesThisFrame;

                // Have we fallen below the hover range?
                if (dwSpaceAvailable > m_nSamplesThisFrame)
                {
//                  OutputDebugString("Below hover range\n");

                    // Calculate the remaining space below the hover point
                    dwSpaceAvailable -= (m_nSamplesThisFrame >> 1);

                    // Add as many additional full frames as are needed to get close to the hover point (without exceeding it)
                    while (dwSpaceAvailable >= m_nSamplesThisFrame)
                    {
//                      OutputDebugString(" Added generated frame\n");
                        GenerateExtra(m_pbFrameSample, m_nSamplesThisFrame);
                        AddData(m_pbFrameSample, m_nSamplesThisFrame);
                        dwSpaceAvailable -= m_nSamplesThisFrame;
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
            else if (dwSpaceAvailable >= (m_nSamplesThisFrame >> 1))
            {
//              OutputDebugString("Above hover range\n");
//              OutputDebugString(" Adding part frame data\n");
                AddData(m_pbFrameSample, dwSpaceAvailable - (m_nSamplesThisFrame >> 1));
            }


            // Reset the sample counters for the next frame
            m_uOffsetPerUnit += (TSTATES_PER_FRAME * m_uSamplesPerUnit) - (m_nSamplesThisFrame * m_uCyclesPerUnit);
            m_nSamplesThisFrame = 0;
        }
    }

    ProfileEnd();
}


void CStreamingSound::AddData (BYTE* pbSampleData_, int nSamples_)
{
    LPVOID pvWrite1, pvWrite2;
    DWORD dwLength1, dwLength2;

    // We must have some samples or there's be nothing to do
    if (nSamples_ > 0)
    {
        // Lock the required buffer range so we can add the new data
        HRESULT hr = m_pdsb->Lock(m_dwWriteOffset, nSamples_ * m_uSampleSize, &pvWrite1, &dwLength1, &pvWrite2, &dwLength2, 0);
        if (FAILED(hr))
            TRACE("!!! Failed to lock sound buffer! (%#08lx)\n", hr);
        else
        {
            // Write the first part (maybe all)
            memcpy(pvWrite1, pbSampleData_, dwLength1);

            // Write the second block if necessary
            if (dwLength2)
                memcpy(pvWrite2, pbSampleData_+dwLength1, dwLength2);

            // Unlock the buffer
            hr = m_pdsb->Unlock(pvWrite1, dwLength1, pvWrite2, dwLength2);
            if (FAILED(hr))
                TRACE("!!! Failed to unlock sound buffer! (%#08lx)\n", hr);

            // Work out the offset for the next write in the circular buffer
            m_dwWriteOffset = (m_dwWriteOffset + dwLength1 + dwLength2) % m_uSampleBufferSize;
        }
    }
}


void CStreamingSound::GenerateExtra (BYTE* pb_, int nSamples_)
{
    // Re-use the specified amount from the previous sample, taking care of buffer overlaps
    if (pb_ != m_pbFrameSample)
        memmove(pb_, m_pbFrameSample, nSamples_*m_uSampleSize);
}

////////////////////////////////////////////////////////////////////////////////


void CDirectXSAASound::Generate (BYTE* pb_, int nSamples_)
{
    // Samples could now be zero, so check...
    if (nSamples_ > 0)
        Sound::pSAASound->GenerateMany(pb_, nSamples_);
}

void CDirectXSAASound::GenerateExtra (BYTE* pb_, int nSamples_)
{
    // If at least one sound update is done per screen line, it's being used for sample playback,
    // so generate the fill-in data from previous data to try and keep it sounding about right
    if (m_nUpdates > HEIGHT_LINES)
        CStreamingSound::GenerateExtra(pb_, nSamples_);

    // Normal SAA sound use, so generate more real samples to give a seamless join
    else
        Sound::pSAASound->GenerateMany(pb_, nSamples_);
}

void CDirectXSAASound::Out (WORD wPort_, BYTE bVal_)
{
    Update();

    if ((wPort_ & SOUND_MASK) == SOUND_ADDR)
        Sound::pSAASound->WriteAddress(bVal_);
    else
        Sound::pSAASound->WriteData(bVal_);
}

void CDirectXSAASound::Update (bool fFrameEnd_/*=false*/)
{
    m_nUpdates++;
    CStreamingSound::Update(fFrameEnd_);

    if (fFrameEnd_)
        m_nUpdates = 0;
}


////////////////////////////////////////////////////////////////////////////////

UINT HCF (UINT x_, UINT y_)
{
    UINT uHCF = 1, uMin = sqrt(min(x_, y_));

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
