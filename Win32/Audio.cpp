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
#include <xaudio2redist.h>
#elif (_WIN32_WINNT < _WIN32_WINNT_WIN8)
// Using XAudio 2.7 requires the DirectX SDK (install to default location):
//  https://www.microsoft.com/en-gb/download/details.aspx?id=6812
#include <C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)\Include\xaudio2.h>
#else
#include <xaudio2.h>
#endif

#include "Audio.h"
#include "Sound.h"

constexpr auto SOUND_BUFFERS = 2;

static ComPtr<IXAudio2> pXAudio2;
static IXAudio2MasteringVoice* pMasteringVoice;
static IXAudio2SourceVoice* pSourceVoice;

static std::array<std::vector<uint8_t>, SOUND_BUFFERS> buffers;
static size_t buffer_index = 0;

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

    auto pfnXAudio2Create = reinterpret_cast<decltype(&XAudio2Create)>(
        GetProcAddress(hinstXAudio2, "XAudio2Create"));
    if (!pfnXAudio2Create)
    {
        Message(MsgType::Error, "XAudio2Create not found.");
        return false;
    }

    auto hr = pfnXAudio2Create(pXAudio2.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR);
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

    return SUCCEEDED(hr);
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
}

void Audio::Silence()
{
}

bool Audio::AddData(uint8_t* pData, int len_bytes)
{
    XAUDIO2_VOICE_STATE state{};

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

    auto& data = buffers[buffer_index];
    data.insert(data.end(), pData, pData + len_bytes);

    size_t samples_per_frame = SAMPLE_FREQ * BYTES_PER_SAMPLE / EMULATED_FRAMES_PER_SECOND;
    if (data.size() >= samples_per_frame / 2)
    {
        XAUDIO2_BUFFER buffer{};
        buffer.pAudioData = data.data();
        buffer.AudioBytes = static_cast<UINT32>(data.size());
        pSourceVoice->SubmitSourceBuffer(&buffer);

        buffer_index = (buffer_index + 1) % SOUND_BUFFERS;
        buffers[buffer_index].clear();
    }

    return true;
}
