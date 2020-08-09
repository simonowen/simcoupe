// Part of SimCoupe - A SAM Coupe emulator
//
// Audio.cpp: SDL sound implementation
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

#include "Audio.h"
#include "Options.h"
#include "Sound.h"

constexpr auto MIN_LATENCY_FRAMES = 4;

static SDL_AudioDeviceID dev;

////////////////////////////////////////////////////////////////////////////////

bool Audio::Init()
{
    Exit();

    SDL_AudioSpec desired{};
    desired.freq = SAMPLE_FREQ;
    desired.format = AUDIO_S16LSB;
    desired.channels = SAMPLE_CHANNELS;
    desired.samples = 512;

    dev = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
    if (!dev)
    {
        TRACE("SDL_OpenAudio failed: {}\n", SDL_GetError());
        return false;
    }

    SDL_PauseAudioDevice(dev, 0);
    return true;
}

void Audio::Exit()
{
    if (dev)
    {
        SDL_CloseAudioDevice(dev);
        dev = 0;
    }
}

float Audio::AddData(uint8_t* pData_, int len_bytes)
{
    SDL_QueueAudio(dev, pData_, len_bytes);

    auto buffer_frames = std::max(GetOption(latency), MIN_LATENCY_FRAMES);
    Uint32 buffer_size = SAMPLES_PER_FRAME * buffer_frames * BYTES_PER_SAMPLE;

#if 1
    while (SDL_GetQueuedAudioSize(dev) >= buffer_size)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
#endif

    return static_cast<float>(SDL_GetQueuedAudioSize(dev)) / buffer_size;
}
