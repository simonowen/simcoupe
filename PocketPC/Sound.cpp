// Part of SimCoupe - A SAM Coupe emulator
//
// Sound.cpp: WinCE sound implementation using WaveOut
//
//  Copyright (c) 1999-2003  Simon Owen
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


extern HWND g_hwnd;

// Direct sound, primary buffer and secondary buffer interface pointers
UINT HCF (UINT x_, UINT y_);

CStreamBuffer* aStreams[SOUND_STREAMS];

CStreamBuffer*& pSAA = aStreams[0];     // SAA 1099 
CStreamBuffer*& pDAC = aStreams[1];     // DAC for parallel DACs and Spectrum-style beeper

LPCSAASOUND pSAASound;  // SAASound.dll object - needs to exist as long as we do, to preseve subtle internal states

BYTE* buf;

HWAVEOUT hWaveOut;
WAVEHDR* pWaveHeaders;
BYTE* pbData;

UINT uTotalBuffers, uCurrentBuffer;

WAVEFORMATEX wf;

long uBuffered;
bool fSlow;

UINT uMaxSamplesPerFrame, uSampleSize, uSampleBufferSize;


void AddData (BYTE* pbData_, int nLength_);

////////////////////////////////////////////////////////////////////////////////

void CALLBACK WaveOutCallback (HANDLE hWaveOut_, UINT uMsg_, DWORD dwUser_, DWORD dw1_, DWORD dw2_)
{
    if (uMsg_ == WOM_DONE)
    {
        // Decrement the buffer count
        InterlockedDecrement(&uBuffered);

        // Flag we're running slow if we've running low on data
        fSlow |= (uBuffered <= (GetOption(latency)/2));
    }
}


bool InitWaveOut ()
{
    // Used the wave format specified in the options
    ZeroMemory(&wf, sizeof wf);
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nSamplesPerSec = GetOption(freq);
    wf.wBitsPerSample = GetOption(bits);
    wf.nChannels = GetOption(stereo) ? 2 : 1;
    wf.nBlockAlign = wf.nChannels * wf.wBitsPerSample / 8;
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;

    // Create a single sample buffer to hold all the data
    uSampleBufferSize = uMaxSamplesPerFrame * uSampleSize * uTotalBuffers;
    pbData = new BYTE[uSampleBufferSize];

    pWaveHeaders = new WAVEHDR[uTotalBuffers];
    ZeroMemory(pWaveHeaders, sizeof(WAVEHDR) * uTotalBuffers);

    // Open the single sound stream we use
    if (pbData && pWaveHeaders && waveOutOpen(&hWaveOut, WAVE_MAPPER, &wf,
        reinterpret_cast<DWORD>(WaveOutCallback), NULL, CALLBACK_FUNCTION) == MMSYSERR_NOERROR)
    {
        // Prepare all the headers, pointing at the relevant position in the sample buffer
        for (UINT u = 0 ; u < uTotalBuffers ; u++)
        {
            pWaveHeaders[u].dwBufferLength = uMaxSamplesPerFrame * wf.nBlockAlign;
            pWaveHeaders[u].lpData = reinterpret_cast<char*>(pbData + (pWaveHeaders[u].dwBufferLength * u));
            waveOutPrepareHeader(hWaveOut, &pWaveHeaders[u], sizeof WAVEHDR);
        }

        Sound::Play();
        return true;
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

    // Correct any approximate/bad option values before we use them
    UINT uBits = SetOption(bits, (GetOption(bits) > 8) ? 16 : 8);
    UINT uFreq = SetOption(freq, (GetOption(freq) < 20000) ? 11025 : (GetOption(freq) < 40000) ? 22050 : 44100);
    UINT uChannels = GetOption(stereo) ? 2 : 1;

    uTotalBuffers = GetOption(latency)+1;
    uCurrentBuffer = uBuffered = 0;

    // Do this because 50Hz doesn't divide exactly in to 11025Hz...
    uMaxSamplesPerFrame = (uFreq+EMULATED_FRAMES_PER_SECOND-1) / EMULATED_FRAMES_PER_SECOND;
    uSampleSize = uChannels * uBits / 8;


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
        bool fNeedSAA = GetOption(saasound);

#ifndef USE_SAASOUND
        fNeedSAA = false;
#endif

        // If the SAA 1099 chip is enabled, create its driver object
        if (fNeedSAA && (pSAA = new CSAA))
        {
            // Else, create the CSAASound object if it doesn't already exist
            if (pSAASound || (pSAASound = CreateCSAASound()))
            {
                // Set the DLL parameters from the options, so it matches the setup of the primary sound buffer
                pSAASound->SetSoundParameters(SAAP_NOFILTER |
                    ((uFreq == 11025) ? SAAP_11025 : (uFreq == 22050) ? SAAP_22050 : SAAP_44100) |
                    ((uBits == 8) ? SAAP_8BIT : SAAP_16BIT) | (GetOption(stereo) ? SAAP_STEREO : SAAP_MONO));
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
    ProfileStart(Snd);

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

    ProfileEnd();
}


void Sound::Silence ()
{
    // Reset all sound streams
    for (int i = 0 ; i < SOUND_STREAMS ; i++)
        if (aStreams[i]) aStreams[i]->Reset();

    if (pbData)
    {
        // Fill the entire sample buffer with silence
        BYTE bSilence = (GetOption(bits) == 16) ? 0x00 : 0x80;
        FillMemory(pbData, bSilence, uSampleBufferSize);
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
    Silence();

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

CStreamBuffer::CStreamBuffer (int nFreq_, int nBits_, int nChannels_)
    : m_nFreq(nFreq_), m_nBits(nBits_), m_nChannels(nChannels_), m_pbFrameSample(NULL),
      m_nSamplesThisFrame(0), m_uOffsetPerUnit(0), m_uPeriod(0), m_uUpdates(0)
{
    // Any values not supplied will be taken from the current options
    if (!m_nFreq) m_nFreq = GetOption(freq);
    if (!m_nBits) m_nBits = GetOption(bits);
    if (!m_nChannels) m_nChannels = GetOption(stereo) ? 2 : 1;

    // Use some arbitrary units to keep the numbers manageably small...
    UINT uUnits = HCF(m_nFreq, EMULATED_TSTATES_PER_SECOND);
    m_uSamplesPerUnit = m_nFreq / uUnits;
    m_uCyclesPerUnit = EMULATED_TSTATES_PER_SECOND / uUnits;

    m_pbFrameSample = new BYTE[uMaxSamplesPerFrame * uSampleSize];
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
    Generate(m_pbFrameSample + (m_nSamplesThisFrame * uSampleSize), nSamplesSoFar - m_nSamplesThisFrame);
    m_nSamplesThisFrame = nSamplesSoFar;

    if (!fFrameEnd_)
        m_uUpdates++;
    else
    {
        DWORD dwSpaceAvailable = (uTotalBuffers - uBuffered) * uMaxSamplesPerFrame;

        // Is there enough space for all this frame's data?
        if (dwSpaceAvailable >= static_cast<DWORD>(m_nSamplesThisFrame))
        {
            // Add on the current frame's sample data
            AddData(m_pbFrameSample, m_nSamplesThisFrame*uSampleSize);
            dwSpaceAvailable -= m_nSamplesThisFrame;

            if (fSlow && dwSpaceAvailable >= static_cast<DWORD>(uMaxSamplesPerFrame))
            {
                GenerateExtra(m_pbFrameSample, uMaxSamplesPerFrame);
                AddData(m_pbFrameSample, uMaxSamplesPerFrame*uSampleSize);
                fSlow = false;
            }
        }

        // Reset the sample counters for the next frame
        m_uOffsetPerUnit += (TSTATES_PER_FRAME * m_uSamplesPerUnit) - (m_nSamplesThisFrame * m_uCyclesPerUnit);
        m_nSamplesThisFrame = 0;
        m_uUpdates = 0;
    }

    ProfileEnd();
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

    // Copy the frame data into the buffer
    memcpy(pbData + (uMaxSamplesPerFrame*uSampleSize*uCurrentBuffer), pbData_, nLength_);

    // Write the block using a single header
    if (waveOutWrite(hWaveOut, &pWaveHeaders[uCurrentBuffer], sizeof WAVEHDR) == MMSYSERR_NOERROR)
    {
        // Move to the next buffer, wrapping round at the end
        uCurrentBuffer = (uCurrentBuffer + 1) % uTotalBuffers;

        // Increment the count of used buffers
        InterlockedIncrement(&uBuffered);
    }
    else
        TRACE("waveOutWrite failed!\n");
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
        memmove(pb_, m_pbFrameSample, nSamples_*uSampleSize);
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
