// Part of SimCoupe - A SAM Coupe emulator
//
// Sound.h: Common sound generation
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

#ifndef SOUND_H
#define SOUND_H

#include "IO.h"
#include "SAA1099.h"
#include "BlipBuffer.h"

#define SAMPLE_FREQ			44100
#define SAMPLE_BITS			16
#define SAMPLE_CHANNELS		2
#define SAMPLE_BLOCK		(SAMPLE_BITS*SAMPLE_CHANNELS/8)


class Sound
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

        static void Silence ();
        static void FrameUpdate ();
};

class CSoundDevice : public CIoDevice
{
    public:
        CSoundDevice ();
        CSoundDevice (const CSoundDevice &) = delete;
        void operator= (const CSoundDevice &) = delete;
        virtual ~CSoundDevice () { delete[] m_pbFrameSample; }

    public:
        int GetSampleCount () { return m_nSamplesThisFrame; }
        BYTE *GetSampleBuffer () { return m_pbFrameSample; }

    protected:
        int m_nSamplesThisFrame = 0;
        BYTE *m_pbFrameSample = nullptr;
};

class CSAA final : public CSoundDevice
{
    public:
        CSAA () { m_pSAASound = new CSAASound(SAMPLE_FREQ); }
        CSAA (const CSAA &) = delete;
        void operator= (const CSAA &) = delete;
        ~CSAA () { delete m_pSAASound; }

    public:
        void Update (bool fFrameEnd_);
        void FrameEnd () override;

        void Out (WORD wPort_, BYTE bVal_) override;

    protected:
        CSAASound *m_pSAASound = nullptr;
};


class CDAC final : public CSoundDevice
{
    public:
        CDAC ();

    public:
        void Reset () override;

        void Update (bool fFrameEnd_);
        void FrameEnd () override;

        void OutputLeft (BYTE bVal_);
        void OutputRight (BYTE bVal_);
        void OutputLeft2 (BYTE bVal_);
        void OutputRight2 (BYTE bVal_);
        void Output (BYTE bVal_);
        void Output2 (BYTE bVal_);

        int GetSamplesSoFar ();

    protected:
        Blip_Buffer buf_left {}, buf_right {};
        Blip_Synth<blip_med_quality,256> synth_left {}, synth_right {}, synth_left2 {}, synth_right2 {};
};

// Spectrum-style BEEPer
class CBeeperDevice final : public CIoDevice
{
    public:
        void Out (WORD wPort_, BYTE bVal_) override;
};


extern CSAA *pSAA;
extern CDAC *pDAC;

#endif  // SOUND_H
