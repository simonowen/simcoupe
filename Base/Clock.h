// Part of SimCoupe - A SAM Coupe emulator
//
// Clock.h: SAMBUS and DALLAS clock emulation
//
//  Copyright (c) 1999-2003  Simon Owen
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
    int nSecond10,  nSecond1;
    int nMinute10,  nMinute1;
    int nHour10,    nHour1;

    int nDay10,     nDay1;
    int nMonth10,   nMonth1;
    int nYear10,    nYear1;
}
SAMTIME;


class CClockDevice : public CIoDevice
{
    public:
        CClockDevice ();

    public:
        void Reset ();
        void Update ();
        int GetDayOfWeek ();

    public:
        static void FrameUpdate ();

    protected:
        time_t m_tLast;
        SAMTIME m_st;

        static time_t s_tEmulated;  // Holds the current time relative to the emulation speed
};


class CSambusClock : public CClockDevice
{
    public:
        CSambusClock ();

    public:
        BYTE In (WORD wPort_);
        void Out (WORD wPort_, BYTE bVal_);

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

    protected:
        BYTE DateValue (BYTE bH_, BYTE bL_);

    protected:
        BYTE m_bReg;          // Currently selected DALLAS register
        BYTE m_abRegs[128];   // DALLAS RAM
};

#endif
