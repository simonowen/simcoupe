// Part of SimCoupe - A SAM Coupé emulator
//
// Sound.h: Win32 sound implementation using DirectSound
//
//  Copyright (c) 1999-2001  Simon Owen
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

namespace Sound
{
    bool Init (bool fFirstInit_=false);
    void Exit (bool fReInit_=false);

    void Out (WORD wPort_, BYTE bVal_);     // SAA chip port output
    void FrameUpdate ();

    void Stop ();
    void Play ();
    void Silence ();                        // Silence current output

    void OutputDACLeft (BYTE bVal_);        // Output to left channel
    void OutputDACRight (BYTE bVal_);       // Output to right channel
    void OutputDAC (BYTE bVal_);            // Output to both channels
};


////////////////////////////////////////////////////////////////////////////////


#ifdef SOUND_IMPLEMENTATION

class CStreamingSound
{
    public:
        CStreamingSound (int nFreq_=0, int nBits_=0, int nChannels_=0);
        virtual ~CStreamingSound ();

    public:
        virtual bool Init ();
        virtual void Generate (BYTE* pb_, int nSamples_) = 0;
        virtual void GenerateExtra (BYTE* pb_, int nSamples_);

    public:
        virtual bool Play () { return m_pdsb && SUCCEEDED(m_pdsb->Play(0, 0, DSBPLAY_LOOPING)); }
        virtual bool Stop()  { Silence(); return m_pdsb && SUCCEEDED(m_pdsb->Stop()); }
        virtual void Silence ();

        virtual DWORD GetSpaceAvailable ();
        virtual void Update (bool fFrameEnd_=false);
        virtual void AddData (BYTE* pbSampleData_, int nSamples_);

    protected:
        IDirectSoundBuffer* m_pdsb;
        int m_nFreq, m_nBits, m_nChannels;
        int m_nSamplesThisFrame;
        UINT m_uSampleSize;
        UINT m_uSamplesPerUnit, m_uCyclesPerUnit;
        UINT m_uPeriod, m_uOffsetPerUnit;

        DWORD m_dwWriteOffset;
        BYTE* m_pbFrameSample;
        UINT m_uSampleBufferSize, m_uFrameOffset;

        static int s_nUsage;
        static IDirectSound* s_pds;
        static IDirectSoundBuffer* s_pdsbPrimary;

    protected:
        bool InitDirectSound ();
        void ExitDirectSound ();
};


// The DirectX implementation of the sound driver
class CDirectXSAASound : public CStreamingSound
{
    public:
        CDirectXSAASound () : m_nUpdates(0) { }
        ~CDirectXSAASound () { }

    public:
        void Generate (BYTE* pb_, int nSamples_);
        void GenerateExtra (BYTE* pb_, int nSamples_);

        void Out (WORD wPort_, BYTE bVal_);
        void Update (bool fFrameEnd_=false);

    protected:
        int m_nUpdates;     // Counter to keep track of the number of sound changed in a frame, for sample playback detection
};


class CDAC : public CStreamingSound
{
    public:
        CDAC () : CStreamingSound(0, 8, 2)      // 8-bit stereo stream using same frequency as primary
        {
            m_uLeftTotal = m_uRightTotal = m_uPrevPeriod = 0;
            Output(0x80);
        }

    public:
        void OutputLeft (BYTE bVal_)            { Update(); m_bLeft = bVal_; }
        void OutputRight (BYTE bVal_)           { Update(); m_bRight = bVal_; }
        void Output (BYTE bVal_)                { Update(); m_bLeft = m_bRight = bVal_; }

    public:
        void Generate (BYTE* pb_, int nSamples_)
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
                *pb_++ = static_cast<BYTE>((m_uLeftTotal + m_bLeft * uPeriod) / m_uCyclesPerUnit);
                *pb_++ = static_cast<BYTE>((m_uRightTotal + m_bRight * uPeriod) / m_uCyclesPerUnit);
                nSamples_--;

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

    protected:
        BYTE m_bLeft, m_bRight;
        UINT m_uLeftTotal, m_uRightTotal;
        UINT m_uPrevPeriod;
};

#endif

#endif  // SOUND_H
