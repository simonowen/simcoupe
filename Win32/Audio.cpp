// Part of SimCoupe - A SAM Coupe emulator
//
// Audio.cpp: Win32 sound implementation using DirectSound
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
#include <mmsystem.h>
#define DIRECTSOUND_VERSION     0x0300
#include <dsound.h>

typedef HRESULT(WINAPI* PFNDIRECTSOUNDCREATE) (LPGUID, LPDIRECTSOUND*, LPUNKNOWN);

#include "Audio.h"
#include "Sound.h"

#include "CPU.h"
#include "SAMIO.h"
#include "Options.h"
#include "Util.h"
#include "UI.h"

static PFNDIRECTSOUNDCREATE pfnDirectSoundCreate;
static IDirectSound* pds;
static IDirectSoundBuffer* pdsb;
static int nSampleBufferSize;
static DWORD dwWriteOffset;

static HANDLE hEvent;
static MMRESULT hTimer;
static DWORD dwTimer;

static bool InitDirectSound();
static void ExitDirectSound();
static void CALLBACK TimeCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR);

////////////////////////////////////////////////////////////////////////////////


bool Audio::Init(bool fFirstInit_/*=false*/)
{
    Exit(true);
    TRACE("-> Audio::Init(%s)\n", fFirstInit_ ? "first" : "");

    auto hinstDSound = LoadLibrary("DSOUND.DLL");
    if (!hinstDSound)
    {
        Message(msgError, "DSOUND.DLL not found.");
        return false;
    }

    pfnDirectSoundCreate =
        reinterpret_cast<PFNDIRECTSOUNDCREATE>(
            GetProcAddress(hinstDSound, "DirectSoundCreate"));

    if (!pfnDirectSoundCreate)
    {
        Message(msgError, "DirectSoundCreate failed.");
        return false;
    }

    // All sound disabled?
    if (!GetOption(sound))
        TRACE("Sound disabled, nothing to initialise\n");
    else if (!InitDirectSound())
        TRACE("DirectSound initialisation failed\n");

    // If we've no sound, fall back on a timer for running speed
    if (!pdsb)
    {
        // Create an event to trigger when the next frame is due
        hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

    // Sound initialisation failure isn't fatal, so always return success
    TRACE("<- Audio::Init()\n");
    return true;
}

void Audio::Exit(bool fReInit_/*=false*/)
{
    TRACE("-> Audio::Exit(%s)\n", fReInit_ ? "reinit" : "");

    ExitDirectSound();

    if (hTimer) timeKillEvent(hTimer), hTimer = 0;
    if (hEvent) CloseHandle(hEvent), hEvent = nullptr;
    pfnDirectSoundCreate = nullptr;

    TRACE("<- Audio::Exit()\n");
}

void Audio::Silence()
{
    PVOID pvWrite;
    DWORD dwLength, dwPlayCursor;

    if (!pdsb)
        return;

    // Lock the entire audio buffer
    if (SUCCEEDED(pdsb->Lock(0, 0, &pvWrite, &dwLength, nullptr, nullptr, DSBLOCK_ENTIREBUFFER)))
    {
        // Silence it to prevent unwanted sound looping
        memset(pvWrite, 0x00, dwLength);
        pdsb->Unlock(pvWrite, dwLength, nullptr, 0);
    }

    // For a seamless join, set the write offset to the current _play_ cursor position
    pdsb->GetCurrentPosition(&dwPlayCursor, nullptr);
    dwWriteOffset = dwPlayCursor;
}

bool Audio::AddData(uint8_t* pbData_, int nLength_)
{
    LPVOID pvWrite1, pvWrite2;
    DWORD dwLength1, dwLength2;
    HRESULT hr;

    // No DirectSound buffer?
    if (!pdsb)
    {
        // Determine the time between frames in ms
        DWORD dwTime = 1000 / (EMULATED_FRAMES_PER_SECOND * GetOption(speed) / 100);
        if (!dwTime) dwTime = 1;

        // Has the frame time changed?
        if (dwTime != dwTimer)
        {
            // Kill any existing timer
            if (hTimer) timeKillEvent(hTimer);

            // Start a new one running
            if (!(hTimer = timeSetEvent(dwTimer = dwTime, 0, TimeCallback, 0, TIME_PERIODIC | TIME_CALLBACK_FUNCTION)))
                Message(msgWarning, "Failed to start frame timer (%#08lx)", GetLastError());
        }

        // Wait for the timer event
        WaitForSingleObject(hEvent, INFINITE);
        return false;
    }

    // Loop while there's still data to write
    while (nLength_ > 0)
    {
        DWORD dwPlayCursor, dwWriteCursor;

        // Fetch current play and write cursor positions
        if (FAILED(hr = pdsb->GetCurrentPosition(&dwPlayCursor, &dwWriteCursor)))
        {
            TRACE("!!! Failed to get sound position! (%#08lx)\n", hr);
            break;
        }

        // Determine the available space at the write cursor
        int nSpace = (nSampleBufferSize + dwPlayCursor - dwWriteOffset) % nSampleBufferSize;
        nSpace = std::min(nSpace, nLength_);

        if (nSpace)
        {
            // Lock the available space
            if (FAILED(hr = pdsb->Lock(dwWriteOffset, nSpace, &pvWrite1, &dwLength1, &pvWrite2, &dwLength2, 0)))
                TRACE("!!! Failed to lock sound buffer! (%#08lx)\n", hr);
            else
            {
                dwLength1 = std::min(dwLength1, static_cast<DWORD>(nLength_));

                // Write the first part (maybe all)
                if (dwLength1)
                {
                    memcpy(pvWrite1, pbData_, dwLength1);
                    pbData_ += dwLength1;
                    nLength_ -= dwLength1;
                }

                dwLength2 = std::min(dwLength2, static_cast<DWORD>(nLength_));

                // Write any second part
                if (dwLength2)
                {
                    memcpy(pvWrite2, pbData_, dwLength2);
                    pbData_ += dwLength2;
                    nLength_ -= dwLength2;
                }

                // Unlock the sound buffer to commit the data
                pdsb->Unlock(pvWrite1, dwLength1, pvWrite2, dwLength2);

                // Work out the offset for the next write in the circular buffer
                dwWriteOffset = (dwWriteOffset + dwLength1 + dwLength2) % nSampleBufferSize;
            }
        }

        // All written?
        if (!nLength_)
            break;

        // Wait for more space
        Sleep(2);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////

static bool InitDirectSound()
{
    HRESULT hr;
    bool fRet = false;

    // Initialise DirectSound
    if (FAILED(hr = pfnDirectSoundCreate(nullptr, &pds, nullptr)))
        TRACE("!!! DirectSoundCreate failed (%#08lx)\n", hr);

    // We want priority control over the sound format while we're active
    else if (FAILED(hr = pds->SetCooperativeLevel(g_hwnd, DSSCL_PRIORITY)))
        TRACE("!!! SetCooperativeLevel() failed (%#08lx)\n", hr);
    else
    {
        // Set up the sound format according to the sound options
        WAVEFORMATEX wf = {};
        wf.wFormatTag = WAVE_FORMAT_PCM;
        wf.nSamplesPerSec = SAMPLE_FREQ;
        wf.wBitsPerSample = SAMPLE_BITS;
        wf.nChannels = SAMPLE_CHANNELS;
        wf.nBlockAlign = SAMPLE_BLOCK;
        wf.nAvgBytesPerSec = SAMPLE_FREQ * SAMPLE_BLOCK;

        int nSamplesPerFrame = (SAMPLE_FREQ / EMULATED_FRAMES_PER_SECOND) + 1;
        nSampleBufferSize = nSamplesPerFrame * SAMPLE_BLOCK * (1 + GetOption(latency));

        DSBUFFERDESC dsbd = { sizeof(DSBUFFERDESC) };
        dsbd.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLFREQUENCY | DSBCAPS_GETCURRENTPOSITION2;
        dsbd.dwBufferBytes = nSampleBufferSize;
        dsbd.lpwfxFormat = &wf;

        HRESULT hr = pds->CreateSoundBuffer(&dsbd, &pdsb, nullptr);
        if (FAILED(hr))
            TRACE("!!! CreateSoundBuffer failed (%#08lx)\n", hr);
        else if (FAILED(hr = pdsb->Play(0, 0, DSBPLAY_LOOPING)))
            TRACE("!!! Play failed on secondary sound buffer (%#08lx)\n", hr);
        else
            fRet = true;
    }

    return fRet;
}

static void ExitDirectSound()
{
    if (pdsb) pdsb->Release(), pdsb = nullptr;
    if (pds) pds->Release(), pds = nullptr;
}


static void CALLBACK TimeCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    // Signal next frame due
    SetEvent(hEvent);
}
