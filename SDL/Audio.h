// Part of SimCoupe - A SAM Coupe emulator
//
// Audio.h: SDL sound implementation
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef AUDIO_H
#define AUDIO_H

class Audio
{
public:
    static bool Init(bool fFirstInit_ = false);
    static void Exit(bool fReInit_ = false);

    static bool IsAvailable() { return SDL_GetAudioStatus() == SDL_AUDIO_PLAYING; }
    static bool AddData(Uint8* pbData_, int nLength_);
    static void Silence();
};

////////////////////////////////////////////////////////////////////////////////

class CSoundStream
{
public:
    CSoundStream();
    CSoundStream(const CSoundStream&) = delete;
    void operator= (const CSoundStream&) = delete;
    virtual ~CSoundStream();

public:
    void Silence();
    void AddData(Uint8* pbSampleData_, int nLength_);

    Uint8* m_pbStart = nullptr;
    Uint8* m_pbEnd = nullptr;
    Uint8* m_pbNow = nullptr;
    int m_nSampleBufferSize = 0;
};

#endif  // AUDIO_H
