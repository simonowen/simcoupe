// Part of SimCoupe - A SAM Coupe emulator
//
// Clock.h: SAMBUS and Dallas clock emulation
//
//  Copyright (c) 1999-2014 Simon Owen
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

#pragma once

#include "SAMIO.h"

typedef struct tagSAMTIME
{
    int nSecond = 0;
    int nMinute = 0;
    int nHour = 0;

    int nDay = 0;
    int nMonth = 0;
    int nYear = 0;
    int nCentury = 0;
} SAMTIME;


class CClockDevice : public CIoDevice
{
public:
    CClockDevice();

public:
    void Reset() override;
    virtual bool Update();
    int GetDayOfWeek();

    int Decode(int nValue_);
    int Encode(int nValue_);
    int DateAdd(int& nValue_, int nAdd_, int nMax_);

protected:
    time_t m_tLast = 0;
    SAMTIME m_st{};
    bool m_fBCD = true;
};


class CSambusClock final : public CClockDevice
{
public:
    CSambusClock();

public:
    BYTE In(WORD wPort_) override;
    void Out(WORD wPort_, BYTE bVal_) override;
    bool Update() override;

protected:
    BYTE m_abRegs[16];    // 16 registers
};


class CDallasClock final : public CClockDevice
{
public:
    CDallasClock();

public:
    BYTE In(WORD wPort_) override;
    void Out(WORD wPort_, BYTE bVal_) override;
    bool Update() override;

    bool LoadState(const char* pcszFile_) override;
    bool SaveState(const char* pcszFile_) override;

protected:
    BYTE m_bReg = 0;             // Currently selected register
    BYTE m_abRegs[14 + 114 + 64];   // 14 bank 0 registers, 50+64=114 bytes user RAM, 64 bank 1 registers
    BYTE m_abRAM[0x2000];       // 8K of extended RAM
};
