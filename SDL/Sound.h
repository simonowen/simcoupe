// Part of SimCoupe - A SAM Coupe emulator
//
// Sound.h: SDL sound implementation
//
//  Copyright (c) 1999-2005  Simon Owen
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

class Sound
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

        static void Out (WORD wPort_, BYTE bVal_);     // SAA chip port output
        static void FrameUpdate ();

        static void Stop ();
        static void Play ();
        static void Silence ();                        // Silence current output

        static void OutputDACLeft (BYTE bVal_);        // Output to left channel
        static void OutputDACRight (BYTE bVal_);       // Output to right channel
        static void OutputDAC (BYTE bVal_);            // Output to both channels
};

////////////////////////////////////////////////////////////////////////////////

#define SOUND_STREAMS   2

class CStreamBuffer
{
    public:
        CStreamBuffer (int nChannels_=0);
        virtual ~CStreamBuffer ();

    public:
        virtual void Generate (Uint8* pb_, int nSamples_) = 0;
        virtual void GenerateExtra (Uint8* pb_, int nSamples_) = 0;

    public:
        virtual void Play () = 0;
        virtual void Stop () = 0;
        virtual void Silence (bool fFill_=false) = 0;

        virtual int GetSpaceAvailable () = 0;
        virtual void Update (bool fFrameEnd_=false);
        virtual void AddData (Uint8* pbSampleData_, int nSamples_) = 0;

    protected:
        int m_nChannels, m_nSampleSize, m_nSamplesThisFrame, m_nSamplesPerFrame;

        UINT m_uSamplesPerUnit, m_uCyclesPerUnit, m_uOffsetPerUnit;
        UINT m_uPeriod;

        Uint8 *m_pbFrameSample;
};


class CSoundStream : public CStreamBuffer
{
    public:
        CSoundStream (int nChannels_/*=0*/);
        ~CSoundStream ();

    // Overrides
    public:
        void Play ();
        void Stop ();
        void Silence (bool fFill_=false);

        int GetSpaceAvailable ();
        void AddData (Uint8* pbSampleData_, int nLength_);

        Uint8 *m_pbStart, *m_pbEnd, *m_pbNow;
        int m_nSampleBufferSize;

    public:
        static void SoundCallback (void *pvParam_, Uint8 *pbStream_, int nLen_);
};


class CSAA : public CSoundStream
{
    public:
        CSAA (int nChannels_/*=0*/) : CSoundStream(nChannels_), m_nUpdates(0) { }

    public:
        void Generate (Uint8* pb_, int nSamples_);
        void GenerateExtra (Uint8* pb_, int nSamples_);

        void Out (WORD wPort_, BYTE bVal_);
        void Update (bool fFrameEnd_=false);

    protected:
        int m_nUpdates;     // Counter of sound changes in a frame, for sample playback detection
};


class CDAC : public CSoundStream
{
    public:
        CDAC ();

    public:
        void Generate (Uint8* pb_, int nSamples_);
        void GenerateExtra (Uint8* pb_, int nSamples_);

        void OutputLeft (BYTE bVal_)            { Update(); m_bLeft = bVal_; }
        void OutputRight (BYTE bVal_)           { Update(); m_bRight = bVal_; }
        void Output (BYTE bVal_)                { Update(); m_bLeft = m_bRight = bVal_; }

    protected:
        BYTE m_bLeft, m_bRight;

        UINT m_uLeftTotal, m_uRightTotal;
        UINT m_uPrevPeriod;
};

#endif  // SOUND_H
