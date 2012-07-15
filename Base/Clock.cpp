// Part of SimCoupe - A SAM Coupe emulator
//
// Clock.cpp: SAMBUS and Dallas clock emulation
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

// Notes:
//  The SAMDOS clock seems to uses 4 bits for each digit, and invalid values
//  can be written and read back (as used for the QDOS clock check).  It
//  appears that only the digits affected by a tick are updated, so any
//  invalid values written will persist until the digits require updating.
//
//  The handling of invalid values during a clock update is a bit of a grey
//  area.  The current implementation acts on pairs of digits for each update,
//  treating out-of-range values to be the largest value that the pair can
//  store, i.e. 77 seconds will wrap over to 00 on the next tick.

#include "SimCoupe.h"

#include "Clock.h"
#include "Options.h"


CClockDevice::CClockDevice ()
    : m_fBCD(true)
{
    // Initialise the clock to the current date/time
    Reset();
}

// Initialise a SAMTIME structure with the current date/time
void CClockDevice::Reset ()
{
    // Get current local time
    m_tLast = time(NULL);

    // Break the current time into it's parts
    tm *ptm = localtime(&m_tLast);

    m_st.nCentury = Encode((1900+ptm->tm_year) / 100);
    m_st.nYear  = Encode(ptm->tm_year % 100);
    m_st.nMonth = Encode(ptm->tm_mon+1);        // Change month from zero-based to one-based
    m_st.nDay = Encode(ptm->tm_mday);

    m_st.nHour = Encode(ptm->tm_hour);
    m_st.nMinute  = Encode(ptm->tm_min);
    m_st.nSecond  = Encode(ptm->tm_sec);
}

int CClockDevice::Decode (int nValue_)
{
    return m_fBCD ? ((nValue_ & 0xf0) >> 4)*10 + (nValue_ & 0x0f) : nValue_;
}

int CClockDevice::Encode (int nValue_)
{
    return m_fBCD ? ((nValue_ / 10) << 4) | (nValue_ % 10) : nValue_;
}

int CClockDevice::DateAdd (int &nValue_, int nAdd_, int nMax_)
{
    if (!nAdd_)
        return 0;

    nValue_ = Decode(nValue_);

    // Limit the starting value to cause an immediate wrap
    if (nValue_ > nMax_)
        nValue_ = nMax_;

    // Add the difference, clipping to the maximum
    nValue_ += nAdd_;
    int nCarry = nValue_ / (nMax_+1);
    nValue_ %= (nMax_+1);

    nValue_ = Encode(nValue_);

    // Return the amount to carry
    return nCarry;
}

bool CClockDevice::Update ()
{
    // The clocks stays synchronised to real time
    time_t tNow = time(NULL);

    // Same time as before?
    if (tNow == m_tLast)
        return false;

    // Before the previous time?!
    if (tNow < m_tLast)
    {
        // Force a resync for negative differences (DST or manual change)
        Reset();
        return true;
    }

    // Work out how many seconds have passed since the last SAM time update
    int nDiff = static_cast<int>(tNow - m_tLast);
    m_tLast = tNow;

    // Update the time, clipping to the maximum values
    nDiff = DateAdd(m_st.nSecond, nDiff, 59);
    nDiff = DateAdd(m_st.nMinute, nDiff, 59);
    nDiff = DateAdd(m_st.nHour, nDiff, 23);

    // Any remaining time is in days and affects the date
    while (nDiff > 0)
    {
        // Limit the month so we know how many days are in the current month
        int nMonth  = Decode(m_st.nMonth);
        nMonth = min(nMonth ? nMonth : 1, 12);

        // Table for the number of days in each month
        static int anDays[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

        // Decode the year and determine whether it's a leap year
        int nYear = Decode(m_st.nCentury)*100 + Decode(m_st.nYear);
        anDays[2] = (!(nYear % 4) && ((nYear % 100) || !(nYear % 400))) ? 29 : 28;

        // Limit the day to between 1 and the maximum for the current month
        int nDay = Decode(m_st.nDay);
        nDay = min((nDay ? nDay : 1), anDays[nMonth]);

        // If there's not enough to complete the current month, add it on and finish
        if (nDay + nDiff <= anDays[nMonth])
        {
            // Update the day in the month
            DateAdd(m_st.nDay, nDiff, anDays[nMonth]);
            break;
        }

        // Complete the current month and set the day back to the first of the month
        nDiff -= anDays[nMonth] - nDay + 1;
        m_st.nDay = 1;

        // Advance to the next month
        DateAdd(m_st.nMonth, 1, 12);

        // If we've completed a year, move back to Jan and increment the year
        if (!m_st.nMonth)
        {
            m_st.nMonth = 1;
            int nCarry = DateAdd(m_st.nYear, 1, 99);
            DateAdd(m_st.nCentury, nCarry, 99);
        }
    }

    // Time updated
    return true;
}

// Get the day of the week for the current SAMTIME
int CClockDevice::GetDayOfWeek ()
{
    struct tm t, *ptm;

    // Set the date and hour, just in case daylight savings is important
    t.tm_year = Decode(m_st.nCentury)*100 + Decode(m_st.nYear);
    t.tm_mon  = Decode(m_st.nMonth) - 1;
    t.tm_mday = Decode(m_st.nDay);
    t.tm_hour = Decode(m_st.nHour);
    t.tm_min  = t.tm_sec = 0;

    // Create a ctime value from the date
    if (t.tm_year >= 1900) t.tm_year -= 1900;
    time_t tNow = mktime(&t);

    // Convert back to a tm structure to get the day of the week :-)
    return (ptm = localtime(&tNow)) ? ptm->tm_wday : 0;
}

////////////////////////////////////////////////////////////////////////////////

CSambusClock::CSambusClock ()
{
    // Clear the clock registers
    memset(m_abRegs, 0, sizeof(m_abRegs));
}

BYTE CSambusClock::In (WORD wPort_)
{
    // Strip off the bottom 8 bits (239 for CLOCK_PORT)
    wPort_ >>= 8;

    // Update the clock
    Update();

    // The register is in the top 4 bits
    BYTE bReg = (wPort_ >> 4) & 0x0f;
    return m_abRegs[bReg];
}

void CSambusClock::Out (WORD wPort_, BYTE bVal_)
{
    // Strip off the bottom 8 bits (always 239 for CLOCK_PORT)
    wPort_ >>= 8;

    // Update the clock
    Update();

    // Determine the register location and perform the write
    BYTE bReg = (wPort_ >> 4) & 0x0f;
    m_abRegs[bReg] = bVal_;

    // SAMBUS clock only appears to use the lower 4 bits of the value
    bVal_ &= 0x0f;

    // Post-write modifications
    switch (bReg)
    {
        // Set registers, assuming BCD to preserve all bits
        case 0x00:  m_st.nSecond = (m_st.nSecond & 0xf0) |  bVal_;       break; // Seconds (ones)
        case 0x01:  m_st.nSecond = (m_st.nSecond & 0x0f) | (bVal_ << 4); break; // Seconds (tens)
        case 0x02:  m_st.nMinute = (m_st.nMinute & 0xf0) |  bVal_;       break; // Minutes (ones)
        case 0x03:  m_st.nMinute = (m_st.nMinute & 0x0f) | (bVal_ << 4); break; // Minutes (tens)
        case 0x04:  m_st.nHour   = (m_st.nHour   & 0xf0) |  bVal_;       break; // Hours (ones)
        case 0x05:  m_st.nHour   = (m_st.nHour   & 0x0f) | (bVal_ << 4); break; // Hours (tens)
        case 0x06:  m_st.nDay    = (m_st.nDay    & 0xf0) |  bVal_;       break; // Days (ones)
        case 0x07:  m_st.nDay    = (m_st.nDay    & 0x0f) | (bVal_ << 4); break; // Days (tens)
        case 0x08:  m_st.nMonth  = (m_st.nMonth  & 0xf0) |  bVal_;       break; // Months (ones)
        case 0x09:  m_st.nMonth  = (m_st.nMonth  & 0x0f) | (bVal_ << 4); break; // Months (tens)
        case 0x0a:  m_st.nYear   = (m_st.nYear   & 0xf0) |  bVal_;       break; // Year (ones)
        case 0x0b:  m_st.nYear   = (m_st.nYear   & 0x0f) | (bVal_ << 4); break; // Year (tens)

        case 0x0c:  break;  // unknown

        // These appear to be control registers
        case 0x0d:
            m_abRegs[bReg] &= ~0x02;    // clear busy bit
            break;

        case 0x0f:  // bit 3 NZ for test mode, bit 2 NZ for 24hr, other bits unknown
            break;
    }
}

bool CSambusClock::Update ()
{
    // Call base to update time values
    if (!CClockDevice::Update())
        return false;

    // If the time update is disabled, do nothing more
    if (m_abRegs[0x0d] & 0x02)
        return false;

    // Update registers with current time
    m_abRegs[0x00] =  m_st.nSecond & 0x0f;          // Seconds (ones)
    m_abRegs[0x01] = (m_st.nSecond & 0xf0) >> 4;    // Seconds (tens)
    m_abRegs[0x02] =  m_st.nMinute & 0x0f;          // Minutes (ones)
    m_abRegs[0x03] = (m_st.nMinute & 0xf0) >> 4;    // Minutes (tens)
    m_abRegs[0x04] =  m_st.nHour   & 0x0f;          // Hours (ones)
    m_abRegs[0x05] = (m_st.nHour   & 0xf0) >> 4;    // Hours (tens)
    m_abRegs[0x06] =  m_st.nDay    & 0x0f;          // Days (ones)
    m_abRegs[0x07] = (m_st.nDay    & 0xf0) >> 4;    // Days (tens)
    m_abRegs[0x08] =  m_st.nMonth  & 0x0f;          // Months (ones)
    m_abRegs[0x09] = (m_st.nMonth  & 0xf0) >> 4;    // Months (tens)
    m_abRegs[0x0a] =  m_st.nYear   & 0x0f;          // Year (ones)
    m_abRegs[0x0b] = (m_st.nYear   & 0xf0) >> 4;    // Year (tens)
    m_abRegs[0x0c] = localtime(&m_tLast)->tm_wday;  // Day of week (unsupported)

    return true;
}

////////////////////////////////////////////////////////////////////////////////

#define BANK1 0x40  // Bank 1 register offset


CDallasClock::CDallasClock ()
    : m_bReg(0)
{
    // Clear register and RAM areas
    memset(m_abRegs, 0, sizeof(m_abRegs));
    memset(m_abRAM, 0, sizeof(m_abRAM));

    // Initialise control registers
    m_abRegs[0x0a] = 0x20;      // Oscillators enabled (b5 set), original register bank (b4 clear)
    m_abRegs[0x0b] = 0x02;      // Update enabled (b7 clear), BCD mode (b2 clear), 24 hour (b1 set)
    m_abRegs[0x0c] = 0x00;
    m_abRegs[0x0d] = 0x80;      // Valid RAM and Time (b7 set)

    // Set the model, dummy serial number, and CRC
    m_abRegs[0x40+BANK1] = 0x78;    // DS17887
    memcpy(m_abRegs+0x41+BANK1, "\x11\x22\x33\x44\x55\x66", 6);
    m_abRegs[0x47+BANK1] = 0x1e;    // p(x) = x^8 + x^5 + x^4 + x^0
}

BYTE CDallasClock::In (WORD wPort_)
{
    // Update the clock
    Update();

    // Determine the register location to read from
    BYTE bReg = m_bReg & 0x7f;
    if (bReg >= 0x40 && (m_abRegs[0x0a] & 0x10)) bReg += BANK1;

    // Pre-read processing
    switch (bReg)
    {
        // Extended RAM reads come from a separate data area
        case 0x53+BANK1:
        {
            // Determine the extended RAM offset
            WORD wOffset = (m_abRegs[0x51+BANK1] << 8) | m_abRegs[0x50+BANK1];

            // Update the extended RAM data port, using 0xff if out of RAM range
            m_abRegs[bReg] = (wOffset < sizeof(m_abRAM)) ? m_abRAM[wOffset] : 0xff;

            // Perform burst mode increment, if enabled
            if ((m_abRegs[0x4a+BANK1] & 0x20) && !++m_abRegs[0x50+BANK1])
                m_abRegs[0x51+BANK1]++;

            break;
        }
    }

    // Perform the read
    BYTE bRet = m_abRegs[bReg];

    // Post-read side-effects
    switch (bReg)
    {
        // Interrupt flags in register C are cleared when read
        case 0x0c:
            m_abRegs[bReg] = 0x00;
            break;
    }

    // Return what was read
    return bRet;
}

void CDallasClock::Out (WORD wPort_, BYTE bVal_)
{
    // Strip off the bottom 8 bits (always 239 for CLOCK_PORT)
    wPort_ >>= 8;

    // Update the clock
    Update();

    // Register select?
    if (!(wPort_ & 1))
    {
        m_bReg = bVal_;
        return;
    }

    // Determine the register location to write to
    BYTE bReg = m_bReg & 0x7f;
    if (bReg >= 0x40 && (m_abRegs[0x0a] & 0x10)) bReg += BANK1;

    // Pre-write processing
    switch (bReg)
    {
        // Control register A has b7 clear
        case 0x0a:
            bVal_ &= 0x7f;
            break;

        // Control registers C+D are read-only
        case 0x0c:
        case 0x0d:
            return;

        default:
            // Model and serial number are read-only
            if (bReg >= 0x40+BANK1 && bReg <= 0x47+BANK1)
                return;

            break;
    }

    // Perform the write
    m_abRegs[bReg] = bVal_;

    // Post-write side-effects
    switch (bReg)
    {
        case 0x00: m_st.nSecond = bVal_;    break;
        case 0x02: m_st.nMinute = bVal_;    break;
        case 0x04: m_st.nHour   = bVal_;    break;
        case 0x07: m_st.nDay    = bVal_;    break;
        case 0x08: m_st.nMonth  = bVal_;    break;
        case 0x09: m_st.nYear   = bVal_;    break;

        case 0x48+BANK1: m_st.nCentury = bVal_; break;

        // Control register B
        case 0x0b:
            m_fBCD = !(m_abRegs[0x0b] & 0x04);
            break;

        // Extended RAM writes are to a separate data area
        case 0x53+BANK1:
        {
            // Determine the extended RAM offset
            WORD wOffset = (m_abRegs[0x51+BANK1]<< 8) | m_abRegs[0x50+BANK1];

            // Write the byte, if it's within RAM range
            if (wOffset < sizeof(m_abRAM))
                m_abRAM[wOffset] = m_abRegs[bReg];

            // Perform burst mode increment, if enabled
            if ((m_abRegs[0x4a+BANK1] & 0x20) && !++m_abRegs[0x50+BANK1])
                m_abRegs[0x51+BANK1]++;

            break;
        }
    }
}

bool CDallasClock::Update ()
{
    // Call base to update time values
    if (!CClockDevice::Update())
        return false;

    // If the update or oscillators are disabled, do nothing more
    if ((m_abRegs[0x0b] & 0x80) || (m_abRegs[0x0a] & 0x70) != 0x20)
        return false;

    // Update registers with current time
    m_abRegs[0x00] = m_st.nSecond;          // Seconds
    m_abRegs[0x02] = m_st.nMinute;          // Minutes
    m_abRegs[0x04] = m_st.nHour;            // Hours
    m_abRegs[0x06] = 1+GetDayOfWeek();      // Day of week
    m_abRegs[0x07] = m_st.nDay;             // Day of the month
    m_abRegs[0x08] = m_st.nMonth;           // Month
    m_abRegs[0x09] = m_st.nYear;            // Year
    m_abRegs[0x48+BANK1] = m_st.nCentury;   // Century

    return true;
}


// Load NVRAM contents from file
void CDallasClock::LoadState (const char *pcszFile_)
{
    FILE *f = fopen(pcszFile_, "rb");
    if (f)
    {
        fread(m_abRegs+0x0e, 1, 0x80-0x0e, f) &&    // User RAM
        fread(m_abRAM, 1, sizeof(m_abRAM), f);      // Extended RAM

        fclose(f);
    }
}

// Save NVRAM contents to file
void CDallasClock::SaveState (const char *pcszFile_)
{
    FILE *f = fopen(pcszFile_, "wb");
    if (f)
    {
        fwrite(m_abRegs+0x0e, 1, 0x80-0x0e, f);	// User RAM
        fwrite(m_abRAM, 1, sizeof(m_abRAM), f); // Extended RAM
        fclose(f);
    }
}
