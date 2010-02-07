// Part of SimCoupe - A SAM Coupe emulator
//
// Clock.h: SAMBUS and DALLAS clock emulation
//
//  Copyright (c) 1999-2010  Simon Owen
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

    public:
        static void FrameUpdate ();

    protected:
        time_t m_tLast;
        SAMTIME m_st;
		bool m_fBCD;

        static time_t s_tEmulated;  // Holds the current time relative to the emulation speed
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
        BYTE m_abRegs[16];    // The 16 SamBus registers
};


class CDallasClock : public CClockDevice
{
    public:
        CDallasClock ();

    public:
        BYTE In (WORD wPort_);
        void Out (WORD wPort_, BYTE bVal_);
		bool Update ();

    protected:
        BYTE m_bReg;			// Currently selected DALLAS register
        BYTE m_abRegs[128+64];  // DALLAS RAM, including additional bank 1 locations
		BYTE m_abRAM[0x1000];	// 4K of extended RAM
};

#endif
