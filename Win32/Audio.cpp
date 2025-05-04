// Part of SimCoupe - A SAM Coupe emulator
//
// Audio.cpp: Win32 sound implementation using XAudio2
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

#ifdef HAVE_XAUDIO2REDIST
#include <xaudio2redist/xaudio2redist.h>
#elif (_WIN32_WINNT < _WIN32_WINNT_WIN8)
#error Windows 7 support requires xaudio2redist from vcpkg
#else
#include <xaudio2.h>
#endif

#include "Audio.h"
#include "Options.h"
#include "Sound.h"

using XAUDIO2CREATEPROC = HRESULT(__stdcall*)(_Outptr_ IXAudio2**, UINT32, XAUDIO2_PROCESSOR);

constexpr auto SOUND_BUFFERS = 32;
constexpr auto MIN_LATENCY_FRAMES = 3;

static ComPtr<IXAudio2> pXAudio2;
static IXAudio2MasteringVoice* pMasteringVoice;
static IXAudio2SourceVoice* pSourceVoice;

static HANDLE hEvent;
static MMRESULT hTimer;
static DWORD dwTimerPeriod;

////////////////////////////////////////////////////////////////////////////////

bool Audio::Init()
{
    Exit();

#ifdef HAVE_XAUDIO2REDIST
    auto hinstXAudio2 = LoadLibrary("xaudio2_9redist.dll");
#elif (_WIN32_WINNT < _WIN32_WINNT_WIN8)
    auto hinstXAudio2 = LoadLibrary("XAudio2_7.dll");
#else
    auto hinstXAudio2 = LoadLibrary("XAudio2_9.dll");
#endif
    if (!hinstXAudio2)
    {
        Message(MsgType::Error, "XAudio2 DLL not found.");
        return false;
    }

    auto pfnXAudio2Create = reinterpret_cast<XAUDIO2CREATEPROC>(
        GetProcAddress(hinstXAudio2, "XAudio2Create"));
    if (!pfnXAudio2Create)
    {
        Message(MsgType::Error, "XAudio2Create not found.");
        return false;
    }

    auto hr = pfnXAudio2Create(pXAudio2.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);

#ifdef _DEBUG
    if (SUCCEEDED(hr))
    {
        XAUDIO2_DEBUG_CONFIGURATION debug{};
        debug.TraceMask = XAUDIO2_LOG_ERRORS | XAUDIO2_LOG_WARNINGS;
        debug.BreakMask = XAUDIO2_LOG_ERRORS;
        pXAudio2->SetDebugConfiguration(&debug);
    }
#endif

    if (SUCCEEDED(hr))
        hr = pXAudio2->CreateMasteringVoice(&pMasteringVoice, 2, SAMPLE_FREQ);

    WAVEFORMATEX wfx{};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nSamplesPerSec = SAMPLE_FREQ;
    wfx.wBitsPerSample = SAMPLE_BITS;
    wfx.nChannels = SAMPLE_CHANNELS;
    wfx.nBlockAlign = BYTES_PER_SAMPLE;
    wfx.nAvgBytesPerSec = SAMPLE_FREQ * BYTES_PER_SAMPLE;

    if (SUCCEEDED(hr))
        hr = pXAudio2->CreateSourceVoice(&pSourceVoice, &wfx);

    if (SUCCEEDED(hr))
        hr = pSourceVoice->Start();

    if (FAILED(hr))
        hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    return SUCCEEDED(hr) || hEvent;
}

void Audio::Exit()
{
    if (pSourceVoice)
    {
        pSourceVoice->Stop();
        pSourceVoice->DestroyVoice();
        pSourceVoice = nullptr;
    }

    if (pMasteringVoice)
    {
        pMasteringVoice->DestroyVoice();
        pMasteringVoice = nullptr;
    }

    pXAudio2.Reset();

    if (hTimer)
    {
        timeKillEvent(hTimer);
        hTimer = 0;
        dwTimerPeriod = 0;
    }

    if (hEvent)
    {
        CloseHandle(hEvent);
        hEvent = nullptr;
    }
}

static void CALLBACK TimeCallback(UINT, UINT, DWORD_PTR, DWORD_PTR, DWORD_PTR)
{
    SetEvent(hEvent);
}

float Audio::AddData(uint8_t* pData, int len_bytes)
{
    XAUDIO2_VOICE_STATE state{};

    if (hEvent)
    {
        DWORD frame_time = 1000 / (EMULATED_FRAMES_PER_SECOND * GetOption(speed) / 100);
        if (frame_time != dwTimerPeriod)
        {
            if (hTimer)
                timeKillEvent(hTimer);

            hTimer = timeSetEvent(dwTimerPeriod = frame_time, 0, TimeCallback, 0, TIME_PERIODIC | TIME_CALLBACK_FUNCTION);
        }

        WaitForSingleObject(hEvent, INFINITE);
        return 1.0f;
    }

    static std::vector<uint8_t> data;
    data.insert(data.end(), pData, pData + len_bytes);

    auto buffer_frames = std::max(GetOption(latency), MIN_LATENCY_FRAMES);
    size_t buffer_size = SAMPLES_PER_FRAME * buffer_frames / SOUND_BUFFERS * BYTES_PER_SAMPLE;
    while (data.size() >= buffer_size)
    {
        for (;;)
        {
#if (_WIN32_WINNT < _WIN32_WINNT_WIN8)
            pSourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
#else
            pSourceVoice->GetState(&state);
#endif
            if (state.BuffersQueued < SOUND_BUFFERS)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        static std::array<std::vector<uint8_t>, SOUND_BUFFERS> buffers;
        static size_t buffer_index = 0;

        auto& current_buffer = buffers[buffer_index];
        current_buffer.assign(data.begin(), data.begin() + buffer_size);
        data.erase(data.begin(), data.begin() + buffer_size);

        XAUDIO2_BUFFER buffer{};
        buffer.pAudioData = current_buffer.data();
        buffer.AudioBytes = static_cast<UINT32>(current_buffer.size());
        pSourceVoice->SubmitSourceBuffer(&buffer);

        buffer_index = (buffer_index + 1) % SOUND_BUFFERS;
    }

    return static_cast<float>(state.BuffersQueued) / SOUND_BUFFERS;
}
