// Part of SimCoupe - A SAM Coupe emulator
//
// Clock.cpp: SAMBUS and DALLAS clock emulation
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

// ToDo:
//  - derive separate clock devices from CIoDevice?

#include "SimCoupe.h"

#include "Clock.h"
#include "Options.h"


time_t CClockDevice::s_tEmulated;

CClockDevice::CClockDevice ()
{
    // Initialise the clock to the current date/time
    Reset();
}

// Initialise a SAMTIME structure with the current date/time
void CClockDevice::Reset ()
{
    // Get current local time, setting the class value if not yet set
    m_tLast = time(NULL);
    if (!s_tEmulated)
        s_tEmulated = m_tLast;

    // Break the current time into it's parts
    tm* ptm = localtime(&m_tLast);

    m_st.nYear10 = ptm->tm_year / 10;
    m_st.nYear1  = ptm->tm_year % 10;

    m_st.nMonth10 = ++ptm->tm_mon / 10;    // Change month from zero-based to one-based
    m_st.nMonth1  = ptm->tm_mon % 10;

    m_st.nDay10 = ptm->tm_mday / 10;
    m_st.nDay1  = ptm->tm_mday % 10;

    m_st.nHour10 = ptm->tm_hour / 10;
    m_st.nHour1  = ptm->tm_hour % 10;

    m_st.nMinute10 = ptm->tm_min / 10;
    m_st.nMinute1  = ptm->tm_min % 10;

    m_st.nSecond10 = ptm->tm_sec / 10;
    m_st.nSecond1  = ptm->tm_sec % 10;
}

void CClockDevice::Update ()
{
    // Get the time in more normal components
    int nSecond = m_st.nSecond10 * 10 + m_st.nSecond1;
    int nMinute = m_st.nMinute10 * 10 + m_st.nMinute1;
    int nHour   = m_st.nHour10   * 10 + m_st.nHour1;
    int nDay    = m_st.nDay10    * 10 + m_st.nDay1;
    int nMonth  = m_st.nMonth10  * 10 + m_st.nMonth1;
    int nYear   = m_st.nYear10   * 10 + m_st.nYear1;

    // The clocks are either always synchronised with real time, or stay relative to emulated time
    time_t tNow = GetOption(clocksync) ? time(NULL) : s_tEmulated;

    // Work out how many seconds have passed since the last SAM time update
    int nDiff = tNow - m_tLast;
    m_tLast = tNow;

    // Add on the change in number of seconds, reducing invalid values to 59 before-hand
    nDiff = (nSecond = min(nSecond, 59) + nDiff) / 60;
    m_st.nSecond1 = (nSecond %= 60) % 10;
    m_st.nSecond10 = nSecond / 10;

    // If there's any time left, consider updating the minutes
    if (nDiff)
    {
        // Add on the change in number of minutes, reducing invalid values to 59 before-hand
        nDiff = (nMinute = min(nMinute, 59) + nDiff) / 60;
        m_st.nMinute1 = (nMinute %= 60) % 10;
        m_st.nMinute10 = nMinute / 10;

        // If there's any time left, consider updating the hours
        if (nDiff)
        {
            // Add on the change in number of hours, reducing invalid values to 23 before-hand
            nDiff = (nHour = min(nHour, 23) + nDiff) / 24;
            m_st.nHour1 = (nHour %= 24) % 10;
            m_st.nHour10 = nHour / 10;

            // Any remaining time is in days and affects the date
            if (nDiff)
            {
                // Limit the month so we know how many days are in the current month
                nMonth = min(nMonth ? nMonth : 1, 12);

                while (1)
                {
                    // Table for the number of days in each month
                    static int anDays[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

                    // The years are relative to 1900, so add that on before considering leap years
                    nYear += 1900;
                    anDays[2] = (!(nYear % 4) && ((nYear % 100) || !(nYear % 400))) ? 29 : 28;

                    // Limit the day to between 1 and the maximum for the current month
                    nDay = min((nDay ? nDay : 1), anDays[nMonth]);

                    // If there's not enough to complete the current month, add it on and finish
                    if (nDay + nDiff <= anDays[nMonth])
                    {
                        m_st.nDay1 = (nDay += nDiff) % 10;
                        m_st.nDay10 = nDay / 10;
                        break;
                    }

                    // Complete the current month and set the day back to the first of the month
                    nDiff -= anDays[nMonth] - nDay + 1;
                    nDay = 1;

                    // If we've completed a year, move back to Jan and increment the year
                    if (++nMonth > 12)
                    {
                        nMonth = 1;

                        m_st.nYear1 = ++nYear % 10;
                        m_st.nYear10 = nYear / 10;
                    }

                    // Update the month digits as they've changed
                    m_st.nMonth1 = nMonth % 10;
                    m_st.nMonth10 = nMonth / 10;
                }
            }
        }
    }
}

// Get the day of the week for the current SAMTIME
int CClockDevice::GetDayOfWeek ()
{
    struct tm t, *ptm;

    // Set the date and hour (just in case daylight savings is important)
    t.tm_year = (m_st.nYear10  * 10) + m_st.nYear1;
    t.tm_mon  = (m_st.nMonth10 * 10) + m_st.nMonth1 - 1;
    t.tm_mday = (m_st.nDay10   * 10) + m_st.nDay1;
    t.tm_hour = (m_st.nHour10  * 10) + m_st.nHour1;
    t.tm_min  = t.tm_sec = 0;

    // Create a ctime value from the date
    time_t tNow = mktime(&t);

    // Convert back to a tm structure to get the day of the week :-)
    return (ptm = localtime(&tNow)) ? ptm->tm_wday : 0;
}


/*static*/ void CClockDevice::FrameUpdate ()
{
    static int nFrames = 0;

    // Every one second we advance the emulation relative time
    if (!(++nFrames %= EMULATED_FRAMES_PER_SECOND))
        s_tEmulated++;
}

////////////////////////////////////////////////////////////////////////////////

CSambusClock::CSambusClock ()
{
    // Clear the clock registers
    memset(&m_abRegs, 0, sizeof m_abRegs);
}

BYTE CSambusClock::In (WORD wPort_)
{
    // Strip off the bottom 8 bits (239 for CLOCK_PORT)
    wPort_ >>= 8;

    // Update the SAMBUS time, unless the clock update is disabled
    if (!(m_abRegs[0x0d] & 0x02))
        Update();
    else
        time(&m_tLast);

    // The register is in the top 4 bits
    switch (wPort_ &= 0xf0)
    {
        case 0x00:  return m_st.nSecond1;   // Seconds (ones)
        case 0x10:  return m_st.nSecond10;  // Seconds (tens)
        case 0x20:  return m_st.nMinute1;   // Minutes (ones)
        case 0x30:  return m_st.nMinute10;  // Minutes (tens)
        case 0x40:  return m_st.nHour1;     // Hours (ones)
        case 0x50:  return m_st.nHour10;    // Hours (tens)
        case 0x60:  return m_st.nDay1;      // Days (ones)
        case 0x70:  return m_st.nDay10;     // Days (tens)
        case 0x80:  return m_st.nMonth1;    // Months (ones)
        case 0x90:  return m_st.nMonth10;   // Months (tens)
        case 0xa0:  return m_st.nYear1;     // Year (ones)
        case 0xb0:  return m_st.nYear10;    // Year (tens)
        case 0xc0:  return localtime(&m_tLast)->tm_wday;   // Day of week (unsupported)

        // These appear to be control registers
        case 0xd0:  // bit 1 NZ for busy, bit 0 NZ for hold clock
        case 0xe0:
        case 0xf0:  // bit 3 NZ for test mode, bit 2 NZ for 24hr, other bits unknown
            return m_abRegs[wPort_ >> 4];
    }

    // Unreachable
    return 0xff;
}

void CSambusClock::Out (WORD wPort_, BYTE bVal_)
{
    // Strip off the bottom 8 bits (always 239 for CLOCK_PORT)
    wPort_ >>= 8;

    // Get the current SAM time, unless the clock update is on hold
    if (!(m_abRegs[0x0d] & 0x02))
        Update();
    else
        time(&m_tLast);

    // SAMBUS clock only appears to use the lower 4 bits of the value
    bVal_ &= 0x0f;

    // Make the modification depending on which port was written to
    switch (wPort_ &= 0xf0)
    {
        // SAMBUS clock ports
        case 0x00:  m_st.nSecond1 = bVal_;  break;  // Seconds (ones)
        case 0x10:  m_st.nSecond10 = bVal_; break;  // Seconds (tens)
        case 0x20:  m_st.nMinute1 = bVal_;  break;  // Minutes (ones)
        case 0x30:  m_st.nMinute10 = bVal_; break;  // Minutes (tens)
        case 0x40:  m_st.nHour1 = bVal_;    break;  // Hours (ones)
        case 0x50:  m_st.nHour10 = bVal_;   break;  // Hours (tens)
        case 0x60:  m_st.nDay1 = bVal_;     break;  // Days (ones)
        case 0x70:  m_st.nDay10 = bVal_;    break;  // Days (tens)
        case 0x80:  m_st.nMonth1 = bVal_;   break;  // Months (ones)
        case 0x90:  m_st.nMonth10 = bVal_;  break;  // Months (tens)
        case 0xa0:  m_st.nYear1 = bVal_;    break;  // Year (ones)
        case 0xb0:  m_st.nYear10 = bVal_;   break;  // Year (tens)

        case 0xc0:  break;  // unknown

        // These appear to be control registers
        case 0xd0:  bVal_ &= ~0x02;     // bit 1 NZ for busy
        case 0xe0:
        case 0xf0:  // bit 3 NZ for test mode, bit 2 NZ for 24hr, other bits unknown
            m_abRegs[wPort_ >> 4] = bVal_;
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////

CDallasClock::CDallasClock ()
    : m_bReg(0)
{
    // Clear the RAM area
    memset(&m_abRegs, 0, sizeof m_abRegs);

    // Initialise some registers
    m_abRegs[0x0a] = 0x20;      // Oscillators enabled
    m_abRegs[0x0b] = 0x00;      // Update enabled
    m_abRegs[0x0c] = 0x00;
    m_abRegs[0x0d] = 0x80;      // Valid RAM and Time
}

BYTE CDallasClock::In (WORD wPort_)
{
    // Strip off the bottom 8 bits (239 for CLOCK_PORT)
    wPort_ >>= 8;

    // Update the current SAM time if the update bit and oscillators are enabled
    if (!(m_abRegs[0x0b] & 0x80) && ((m_abRegs[0x0a] & 0x70) == 0x20))
        Update();
    else
        time(&m_tLast);

    switch (m_bReg)
    {
        // Time registers
        case 0x00:  return DateValue(m_st.nSecond10, m_st.nSecond1);  // Seconds
        case 0x02:  return DateValue(m_st.nMinute10, m_st.nMinute1);  // Minutes
        case 0x04:  return DateValue(m_st.nHour10, m_st.nHour1);      // Hours
        case 0x06:  return 1+GetDayOfWeek();                          // Day of week
        case 0x07:  return DateValue(m_st.nDay10, m_st.nDay1);        // Day of the month
        case 0x08:  return DateValue(m_st.nMonth10, m_st.nMonth1);    // Month
        case 0x09:  return DateValue(m_st.nYear10, m_st.nYear1);      // Year

        // Control registers that require special handling
        case 0x0c:
        {
            BYTE bRet = m_abRegs[m_bReg];
            m_abRegs[m_bReg] = 0x00;
            return bRet;
        }

        default:
            return m_abRegs[m_bReg & 0x7f];
    }
}

void CDallasClock::Out (WORD wPort_, BYTE bVal_)
{
    // Strip off the bottom 8 bits (always 239 for CLOCK_PORT)
    wPort_ >>= 8;

    // Get the current SAM time, unless the clock update has been disabled
    if (!(m_abRegs[0x0b] & 0x80) && ((m_abRegs[0x0a] & 0x70) == 0x20))
        Update();
    else
        time(&m_tLast);

    switch (wPort_ & 1)
    {
        // Select a register
        case 0:
            m_bReg = bVal_ & 0x7f;
            break;

        // Write a register value
        case 1:
        {
            switch (m_bReg)
            {
                // Some control registers need special handling, as not all bits are writeable
                case 0x0a:  m_abRegs[m_bReg] = (m_abRegs[m_bReg] & 0x80) | (bVal_ & ~0x80); break;
                case 0x0c:  break;      // All bits read-only
                case 0x0d:  break;      // All bits read-only

                default:
                    m_abRegs[m_bReg] = bVal_;
                    break;
            }
            break;
        }
    }
}


// The format SAMBUS dates are returned in (BCD or binary) depends on bit 2 of register B
BYTE CDallasClock::DateValue (BYTE bH_, BYTE bL_)
{
    // Bit is set for binary, reset for BCD
    return (m_abRegs[0x0b] & 0x02) ? (bH_ * 10) + bL_ : (bH_ << 4) | bL_;
}
