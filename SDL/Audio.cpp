// Part of SimCoupe - A SAM Coupe emulator
//
// Audio.cpp: SDL sound implementation
//
//  Copyright (c) 1999-2012 Simon Owen
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

#include "SimCoupe.h"

#include "Audio.h"
#include "Sound.h"

#include "CPU.h"
#include "IO.h"
#include "Options.h"
#include "Util.h"
#include "UI.h"

#define SAMPLE_BUFFER_SIZE	2048

static Uint8 *m_pbStart, *m_pbEnd, *m_pbNow;
static int m_nSampleBufferSize;
static Uint32 uLastTime;

static bool InitSDLSound ();
static void ExitSDLSound ();
static void SoundCallback (void *pvParam_, Uint8 *pbStream_, int nLen_);

////////////////////////////////////////////////////////////////////////////////


bool Audio::Init (bool fFirstInit_/*=false*/)
{
    // Clear out any existing config before starting again
    Exit(true);
    TRACE("-> Audio::Init(%s)\n", fFirstInit_ ? "first" : "");

    // All sound disabled?
    if (!GetOption(sound))
        TRACE("Sound disabled, nothing to initialise\n");
    else if (!InitSDLSound())
        TRACE("Sound initialisation failed\n");
    else
    {
        int nSamplesPerFrame = (SAMPLE_FREQ / EMULATED_FRAMES_PER_SECOND)+1;
        int nBufferedFrames = (SAMPLE_BUFFER_SIZE/nSamplesPerFrame) + 1 + GetOption(latency);

        m_nSampleBufferSize = nSamplesPerFrame * SAMPLE_BLOCK * nBufferedFrames;
        m_pbEnd = (m_pbNow = m_pbStart = new Uint8[m_nSampleBufferSize]) + m_nSampleBufferSize;

        TRACE("Sample buffer size = %d samples\n", m_nSampleBufferSize/SAMPLE_BLOCK);
    }

    // Sound initialisation failure isn't fatal, so always return success
    TRACE("<- Audio::Init()\n");
    return true;
}

void Audio::Exit (bool fReInit_/*=false*/)
{
    TRACE("-> Audio::Exit(%s)\n", fReInit_ ? "reinit" : "");

    ExitSDLSound();

    TRACE("<- Audio::Exit()\n");
}

bool Audio::AddData (Uint8* pbData_, int nLength_)
{
    bool fWaited = false;
    int nSpace = 0;

    if (!IsAvailable())
        return false;

    // Calculate the frame time (in ms) from the sample data length
    UINT uFrameTime = ((nLength_*1000/SAMPLE_BLOCK) + (SAMPLE_FREQ/2)) / SAMPLE_FREQ;

    // Loop until everything has been written
    while (nLength_ > 0)
    {
        SDL_LockAudio();

        // Determine the available space
        nSpace = static_cast<int>(m_pbEnd-m_pbNow);
        int nAdd = min(nSpace, nLength_);

        // Copy as much as we can
        memcpy(m_pbNow, pbData_, nAdd);

        // Adjust for what was added
        m_pbNow += nAdd;
        pbData_ += nAdd;
        nLength_ -= nAdd;

        SDL_UnlockAudio();

        // If we've written it all or frame sync is disabled, we're done
        if (!nLength_ || !GetOption(sync))
            break;

        // Wait for more space
        SDL_Delay(1);
        fWaited = true;
    }

    // How long since the frame?
    Uint32 uNow = SDL_GetTicks();
    Sint32 nElapsed = static_cast<Sint32>(uNow - uLastTime);

    // If we're too far behind, re-sync
    if (nElapsed > uFrameTime*2)
    {
        uLastTime = uNow;
    }
    else
    {
        // If we're falling behind, reduce the delay by 1ms
        if (nSpace > (SAMPLE_BUFFER_SIZE*SAMPLE_BLOCK))
            uFrameTime--;

        do
        {
            // How long since the last frame?
            Sint32 nElapsed = static_cast<Sint32>(SDL_GetTicks() - uLastTime);

            // Have we waited long enough?
            if (nElapsed >= uFrameTime)
            {
                // Adjust for the next frame
                uLastTime += uFrameTime;
                break;
            }

            // Sleep a short time before checking again
            SDL_Delay(1);

        } while (GetOption(sync));
    }

    return true;
}

void Audio::Silence ()
{
    if (!IsAvailable())
        return;

    SDL_LockAudio();
    memset(m_pbStart, 0x00, m_pbEnd-m_pbStart);
    m_pbNow = m_pbEnd;
    SDL_UnlockAudio();
}

////////////////////////////////////////////////////////////////////////////////

bool InitSDLSound ()
{
    SDL_AudioSpec sDesired = { };
    sDesired.freq = SAMPLE_FREQ;
    sDesired.format = AUDIO_S16LSB;
    sDesired.channels = SAMPLE_CHANNELS;
    sDesired.samples = SAMPLE_BUFFER_SIZE;
    sDesired.callback = SoundCallback;

    if (SDL_OpenAudio(&sDesired, NULL) < 0)
    {
        TRACE("SDL_OpenAudio failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_PauseAudio(0);

    return true;
}

void ExitSDLSound ()
{
    SDL_CloseAudio();
}

// Callback used by SDL to request more sound data to play
void SoundCallback (void *pvParam_, Uint8 *pbStream_, int nLen_)
{
    // Determine how much data we have available, how much to copy, and what is left over
    int nData = static_cast<int>(m_pbNow - m_pbStart);
    int nCopy = min(nData, nLen_), nLeft = nData-nCopy;

    // Update the sound stream with what we have
    memcpy(pbStream_, m_pbStart, nCopy);

    // Move any remaining data to the start of our buffer
    m_pbNow = m_pbStart + nLeft;
    memmove(m_pbStart, m_pbStart+nCopy, nLeft);
}
