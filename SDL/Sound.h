// Part of SimCoupe - A SAM Coupé emulator
//
// Sound.h: SDL sound implementation
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


class CStreamingSound
{
    public:
        CStreamingSound (int nFreq_=0, int nBits_=0, int nChannels_=0);
        virtual ~CStreamingSound ();

    public:
        virtual bool Init ();
        virtual void Generate (BYTE* pb_, UINT uSamples_) = 0;
        virtual void GenerateExtra (BYTE* pb_, UINT uSamples_);

    public:
        virtual bool Play () { return true; }
        virtual bool Stop()  { Silence(); return true; }
        virtual void Silence ();

        virtual void Update (bool fFrameEnd_=false);
        virtual void AddData (BYTE* pbSampleData_, UINT uSamples_);
        virtual void Callback (Uint8 *pbStream_, int nLen_);

    protected:
        int m_nFreq, m_nBits, m_nChannels;
        UINT m_uSampleSize, m_uSamplesThisFrame;
        UINT m_uSamplesPerUnit, m_uCyclesPerUnit;
        UINT m_uPeriod, m_uOffsetPerUnit;

        DWORD m_dwWriteOffset;
        BYTE* m_pbFrameSample;
        UINT m_uSampleBufferSize, m_uFrameOffset;

        BYTE *m_pbStart, *m_pbNow, *m_pbEnd;
        SDL_AudioSpec   m_sObtained;

    protected:
        static void SoundCallback (void *pvParam_, Uint8 *pbStream_, int nLen_);

        bool InitSDLSound ();
        void ExitSDLSound ();
};


// The SDL implementation of the sound driver
class CSDLSAASound : public CStreamingSound
{
    public:
        CSDLSAASound () : m_nUpdates(0) { }
        ~CSDLSAASound () { }

    public:
        void Generate (BYTE* pb_, UINT uSamples_);
        void GenerateExtra (BYTE* pb_, UINT uSamples_);

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
        void Generate (BYTE* pb_, UINT uSamples_)
        {
            if (uSamples_ == 0)
            {
                // If we are still on the same sample then update the mean level that spans it
                UINT uPeriod = m_uPeriod - m_uPrevPeriod;
                m_uLeftTotal += m_bLeft * uPeriod;
                m_uRightTotal += m_bRight * uPeriod;
            }
            else
            {
                // Otherwise output the mean level spanning the completed sample
                UINT uPeriod = m_uCyclesPerUnit - m_uPrevPeriod;
                *pb_++ = static_cast<BYTE>((m_uLeftTotal + m_bLeft * uPeriod) / m_uCyclesPerUnit);
                *pb_++ = static_cast<BYTE>((m_uRightTotal + m_bRight * uPeriod) / m_uCyclesPerUnit);
                uSamples_--;

                // Fill in the block of complete samples at this level
                if (m_bLeft == m_bRight)
                    memset(pb_, m_bLeft, uSamples_ << 1);
                else
                {
                    WORD *pw = reinterpret_cast<WORD*>(pb_), wSample = (static_cast<WORD>(m_bRight) << 8) | m_bLeft;

                    while (uSamples_--)
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

#endif  // SOUND_H
