// Part of SimCoupe - A SAM Coupe emulator
//
// Sound.cpp: WinCE sound implementation using WaveOut
//
//  Copyright (c) 1999-2011  Simon Owen
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

#define SOUND_FREQ      22050   // If you change this, change SetSoundParameters() below
#define SOUND_BITS      8


// Direct sound, primary buffer and secondary buffer interface pointers
CStreamBuffer* aStreams[SOUND_STREAMS];

CStreamBuffer*& pSAA = aStreams[0];     // SAA 1099
CStreamBuffer*& pDAC = aStreams[1];     // DAC for parallel DACs and Spectrum-style beeper

LPCSAASOUND pSAASound;  // SAASound.dll object - needs to exist as long as we do, to preseve subtle internal states

BYTE* buf;

WAVEFORMATEX wf;

HWAVEOUT hWaveOut;
WAVEHDR* pWaveHeaders;
BYTE* pbData;

UINT uTotalBuffers;
UINT uSamplesPerFrame, uSampleSize, uSampleBufferSize;

void AddData (BYTE* pbData_, int nLength_);

////////////////////////////////////////////////////////////////////////////////

bool InitWaveOut ()
{
    // Used the wave format specified in the options
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nSamplesPerSec = SOUND_FREQ;
    wf.wBitsPerSample = SOUND_BITS;
    wf.nChannels = 2;
    wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    wf.cbSize = 0;

    // Create a single sample buffer to hold all the data
    uSampleBufferSize = uSamplesPerFrame * uSampleSize * uTotalBuffers;
    pbData = new BYTE[uSampleBufferSize];
    pWaveHeaders = new WAVEHDR[uTotalBuffers];

    if (!pbData || !pWaveHeaders)
    {
        TRACE("!!! Out of memory allocating sound buffers\n");
        return false;
    }

    ZeroMemory(pWaveHeaders, sizeof(WAVEHDR) * uTotalBuffers);

    // Loop through the available devices rather than relying on the MIDI mapper, just in case
    for (UINT id = 0; id < waveOutGetNumDevs(); id++)
    {
        // Attempt to open our single stream
        if (waveOutOpen(&hWaveOut, id, &wf, NULL, NULL, CALLBACK_NULL) == MMSYSERR_NOERROR)
        {
            // Prepare all the headers, pointing at the relevant position in the sample buffer
            for (UINT u = 0 ; u < uTotalBuffers ; u++)
            {
                pWaveHeaders[u].dwBufferLength = uSamplesPerFrame * wf.nBlockAlign;
                pWaveHeaders[u].lpData = reinterpret_cast<char*>(pbData + (pWaveHeaders[u].dwBufferLength * u));
                pWaveHeaders[u].dwFlags = WHDR_DONE;
                waveOutPrepareHeader(hWaveOut, &pWaveHeaders[u], sizeof WAVEHDR);
            }

            Sound::Play();
            return true;
        }
    }

    return false;
}

void ExitWaveOut ()
{
    if (hWaveOut)
    {
        // Reset the stream to flush out any data
        waveOutReset(hWaveOut);

        // Unprepare all headers
        for (UINT u = 0; u < uTotalBuffers; u++)
            waveOutUnprepareHeader(hWaveOut, &pWaveHeaders[u], sizeof WAVEHDR);

        // Close the stream and delete the header buffers
        waveOutClose(hWaveOut);
        delete[] pWaveHeaders;
    }

    // Delete the sample buffer
    delete[] pbData;
    pbData = NULL;
}


bool Sound::Init (bool fFirstInit_/*=false*/)
{
    // Clear out any existing config before starting again
    Exit(true);
    TRACE("-> Sound::Init(%s)\n", fFirstInit_ ? "first" : "");

    // Do this because 50Hz doesn't divide exactly in to 11025Hz...
    uSamplesPerFrame = SOUND_FREQ / EMULATED_FRAMES_PER_SECOND;
    uSamplesPerFrame *= 4;
    uSampleSize = 2 * SOUND_BITS / 8;

    uTotalBuffers = GetOption(latency)+1;

    // All sound disabled?
    if (!GetOption(sound))
        TRACE("Sound disabled, nothing to initialise\n");
    else if (!InitWaveOut())
    {
        TRACE("WaveOut initialisation failed\n");
        SetOption(sound,0);
    }
    else
    {
        // Create the SAA 1099 objects
        if ((pSAA = new CSAA) && (pSAASound || (pSAASound = CreateCSAASound())))
        {
            // Set the DLL parameters from the options, so it matches the setup of the primary sound buffer
            pSAASound->SetSoundParameters(SAAP_NOFILTER | SAAP_22050 | SAAP_16BIT | SAAP_STEREO);
        }

        // Create and initialise a DAC, for Spectrum beeper and parallel port DACs
        pDAC = new CDAC;

        // If anything failed, disable the sound
        if (!pSAA || !pSAASound || !pDAC)
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

    // Delete all the stream objects
    for (int i = 0 ; i < SOUND_STREAMS ; i++)
        delete aStreams[i], aStreams[i] = NULL;

    // Only close SAASound on the final exit, to preserve the internal state
    if (pSAASound && !fReInit_)
    {
        DestroyCSAASound(pSAASound);
        pSAASound = NULL;
    }

    // Close the sound stream
    ExitWaveOut();

    TRACE("<- Sound::Exit()\n");
}


void Sound::Out (WORD wPort_, BYTE bVal_)
{
    if (pSAA)
        reinterpret_cast<CSAA*>(pSAA)->Out(wPort_, bVal_);
}

void Sound::FrameUpdate ()
{
    if (!g_fTurbo)
    {
        // The code below is needed to share a single sound stream between both SAA and DAC/beeper
        // Only one can be heard (we don't mix), and DAC/beeper has priority if being used
        if (pDAC)
        {
            // Use the DAC if there's no SAA or the DAC is being driven
            if (!pSAA || pDAC->GetUpdates())
            {
                // Use the DAC data for this frame
                pDAC->Update(true);

                // Discard the SAA data for this frame
                if (pSAA)
                    pSAA->Reset();
            }

            // SAA available?
            else if (pSAA)
            {
                // Use the SAA data and discard the DAC data for this frame
                pSAA->Update(true);
                pDAC->Reset();
            }
        }

        // No DAC, so use SAA data if available
        else if (pSAA)
            pSAA->Update(true);
    }
}


void Sound::Silence ()
{
    // Reset all sound streams
    for (int i = 0 ; i < SOUND_STREAMS ; i++)
        if (aStreams[i]) aStreams[i]->Reset();

    if (pbData)
    {
        // Fill the entire sample buffer with silence
        FillMemory(pbData, 0x80, uSampleBufferSize);
    }
}

void Sound::Stop ()
{
    if (hWaveOut)
        waveOutPause(hWaveOut);

    Silence();
}

void Sound::Play ()
{
    if (hWaveOut)
        waveOutRestart(hWaveOut);
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

CStreamBuffer::CStreamBuffer ()
    : m_pbFrameSample(NULL), m_nSamplesThisFrame(0), m_uOffsetPerUnit(0), m_uPeriod(0), m_uUpdates(0)
{
/*
    // Test: tweak the frequency closer to measured values, to avoid over/under-runs
    if (m_nFreq == 44100) m_nFreq = 44300;
    if (m_nFreq == 22050) m_nFreq = 22150;
    if (m_nFreq == 11050) m_nFreq = 11075;
*/
    // Use some arbitrary units to keep the numbers manageably small...
    UINT uUnits = Util::HCF(SOUND_FREQ, EMULATED_TSTATES_PER_SECOND);
    m_uSamplesPerUnit = SOUND_FREQ / uUnits;
    m_uCyclesPerUnit = EMULATED_TSTATES_PER_SECOND / uUnits;

    m_pbFrameSample = new BYTE[uSamplesPerFrame * uSampleSize];
}

CStreamBuffer::~CStreamBuffer ()
{
    delete[] m_pbFrameSample;
}


void CStreamBuffer::Update (bool fFrameEnd_)
{
    // Limit to a single frame's worth as the raster may be just into the next frame
    UINT uRasterPos = min(g_dwCycleCounter, TSTATES_PER_FRAME);

    // Calculate the number of whole samples passed and the amount spanning in to the next sample
    UINT uSamplesCyclesPerUnit = uRasterPos * m_uSamplesPerUnit + m_uOffsetPerUnit;
    int nSamplesSoFar = uSamplesCyclesPerUnit / m_uCyclesPerUnit;
    m_uPeriod = uSamplesCyclesPerUnit % m_uCyclesPerUnit;

    // Generate the additional sample(s) to our frame sample buffer
    m_nSamplesThisFrame = min(m_nSamplesThisFrame, nSamplesSoFar);
    Generate(m_pbFrameSample + (m_nSamplesThisFrame * uSampleSize), nSamplesSoFar - m_nSamplesThisFrame);
    m_nSamplesThisFrame = nSamplesSoFar;

    if (!fFrameEnd_)
        m_uUpdates++;
    else
    {

        int uFreeBuffers = 0;
        for (UINT u = 0; u < uTotalBuffers ; u++)
        {
            if (pWaveHeaders[u].dwFlags & WHDR_DONE)
                uFreeBuffers++;
        }

        DWORD dwSpaceAvailable = uFreeBuffers * uSamplesPerFrame;
#if 0
        // Is there enough space for all this frame's data?
        if (dwSpaceAvailable >= static_cast<DWORD>(m_nSamplesThisFrame))
#endif
        {
            // Add on the current frame's sample data
            AddData(m_pbFrameSample, m_nSamplesThisFrame*uSampleSize);
#if 0
            dwSpaceAvailable -= m_nSamplesThisFrame;

            // Is the buffer only 1/4 full?
            if (uFreeBuffers > (GetOption(latency)*3/4))
            {
                // Top up until 3/4 full
                for ( ; uFreeBuffers > (GetOption(latency)/4) ; uFreeBuffers--)
                {
                    GenerateExtra(m_pbFrameSample, uSamplesPerFrame);
                    AddData(m_pbFrameSample, uSamplesPerFrame*uSampleSize);
                }
            }
#endif
        }

        // Reset the sample counters for the next frame
        m_uOffsetPerUnit += (TSTATES_PER_FRAME * m_uSamplesPerUnit) - (m_nSamplesThisFrame * m_uCyclesPerUnit);
        m_nSamplesThisFrame = 0;
        m_uUpdates = 0;
    }
}

void CStreamBuffer::Reset ()
{
    m_nSamplesThisFrame = 0;
    m_uOffsetPerUnit = m_uUpdates = 0;
}

////////////////////////////////////////////////////////////////////////////////

void AddData (BYTE* pbData_, int nLength_)
{
    if (nLength_ <= 0)
        return;

    for (UINT u = 0; u < uTotalBuffers; u++)
    {
        if (pWaveHeaders[u].dwFlags & WHDR_DONE)
        {
            // Copy the frame data into the buffer
            memcpy(pbData + (uSamplesPerFrame*uSampleSize*u), pbData_, nLength_);

            // Write the block using a single header
            pWaveHeaders[u].dwBufferLength = nLength_;
            if (waveOutWrite(hWaveOut, &pWaveHeaders[u], sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
                TRACE("!!! waveOutWrite failed!\n");

            break;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////

CSAA::CSAA ()
{
}

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
    if (GetUpdates() > HEIGHT_LINES)
        memmove(pb_, m_pbFrameSample, nSamples_*uSampleSize);

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

////////////////////////////////////////////////////////////////////////////////

CDAC::CDAC ()
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
        memmove(pb_, m_pbFrameSample, nSamples_*uSampleSize);
}
