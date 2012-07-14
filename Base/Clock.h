// Part of SimCoupe - A SAM Coupe emulator
//
// Clock.h: SAMBUS and Dallas clock emulation
//
//  Copyright (c) 1999-2012  Simon Owen
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

#ifndef CLOCK_H
#define CLOCK_H

#include "IO.h"


typedef struct tagSAMTIME
{
    int nSecond;
    int nMinute;
    int nHour;

    int nDay;
    int nMonth;
    int nYear;
    int nCentury;
}
SAMTIME;


class CClockDevice : public CIoDevice
{
    public:
        CClockDevice ();

    public:
        void Reset ();
        virtual bool Update ();
        int GetDayOfWeek ();

        int Decode (int nValue_);
        int Encode (int nValue_);
        int DateAdd (int &nValue_, int nAdd_, int nMax_);

    protected:
        time_t m_tLast;
        SAMTIME m_st;
        bool m_fBCD;
};


class CSambusClock : public CClockDevice
{
    public:
        CSambusClock ();

    public:
        BYTE In (WORD wPort_);
        void Out (WORD wPort_, BYTE bVal_);
        bool Update ();

    protected:
        BYTE m_abRegs[16];    // 16 registers
};


class CDallasClock : public CClockDevice
{
    public:
        CDallasClock ();

    public:
        BYTE In (WORD wPort_);
        void Out (WORD wPort_, BYTE bVal_);
        bool Update ();

        void LoadState (const char *pcszFile_);
        void SaveState (const char *pcszFile_);

    protected:
        BYTE m_bReg;                // Currently selected register
        BYTE m_abRegs[14+114+64];   // 14 bank 0 registers, 50+64=114 bytes user RAM, 64 bank 1 registers
        BYTE m_abRAM[0x2000];       // 8K of extended RAM
};

#endif
