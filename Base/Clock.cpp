// Part of SimCoupe - A SAM Coupé emulator
//
// Clock.cpp: SAMBUS and DALLAS clock emulation
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


// SAMBUS data
static SAMTIME stSamBus;            // Last SAMBUS time value
static time_t tSamBusLast;          // Last time we updated the SAM clock
static BYTE abSambusRegs[16];       // The 16 SamBus registers

// DALLAS data
static SAMTIME stDallas;            // Last SAMBUS time value
static time_t tDallasLast;          // Last time we updated the SAM clock
static BYTE abDallasRegs[128];      // DALLAS RAM
static BYTE bDallasReg;             // Currently selected DALLAS register

static time_t tEmulated;            // Holds the current time relative to the emulation speed

static time_t InitTime (SAMTIME* pst_);
static int GetDayOfWeek (SAMTIME* pst_);
static BYTE DallasDateValue (BYTE bH_, BYTE bL_);
static void Update (SAMTIME* pst_, time_t* ptLapst_);

////////////////////////////////////////////////////////////////////////////////

bool Clock::Init ()
{
    // Set the time on the 2 clocks to the current time
    tSamBusLast = tDallasLast = InitTime(&stSamBus);
    stDallas = stSamBus;

    // Clear the RAM areas
    memset(&abSambusRegs, 0, sizeof abSambusRegs);

    memset(&abDallasRegs, 0, sizeof abDallasRegs);
    abDallasRegs[0x0a] = 0x20;                      // Oscillators enabled
    abDallasRegs[0x0b] = 0x00;                      // Update enabled
    abDallasRegs[0x0c] = 0x00;
    abDallasRegs[0x0d] = 0x80;                      // Valid RAM and Time

    return true;
}

void Clock::Exit ()
{
}


BYTE Clock::In (WORD wPort_)
{
    // Strip off the bottom 8 bits (239 for CLOCK_PORT)
    wPort_ >>= 8;

    // SAMBUS clock support?
    if (GetOption(sambusclock) && wPort_ < 0xfe)
    {
        // Get the update the SAMBUS time, unless the clock update is disabled
        if (!(abSambusRegs[0x0d] & 0x02))
            Update(&stSamBus, &tSamBusLast);
        else
            time(&tSamBusLast);

        switch (wPort_ &= 0xf0)
        {
            case 0x00:  return stSamBus.nSecond1;                   // Seconds (ones)
            case 0x10:  return stSamBus.nSecond10;                  // Seconds (tens)
            case 0x20:  return stSamBus.nMinute1;                   // Minutes (ones)
            case 0x30:  return stSamBus.nMinute10;                  // Minutes (tens)
            case 0x40:  return stSamBus.nHour1;                     // Hours (ones)
            case 0x50:  return stSamBus.nHour10;                    // Hours (tens)
            case 0x60:  return stSamBus.nDay1;                      // Days (ones)
            case 0x70:  return stSamBus.nDay10;                     // Days (tens)
            case 0x80:  return stSamBus.nMonth1;                    // Months (ones)
            case 0x90:  return stSamBus.nMonth10;                   // Months (tens)
            case 0xa0:  return stSamBus.nYear1;                     // Year (ones)
            case 0xb0:  return stSamBus.nYear10;                    // Year (tens)
            case 0xc0:  return localtime(&tSamBusLast)->tm_wday;    // Day of week (unsupported - uses current day of week)

            // These appear to be control registers
            case 0xd0:                          // bit 1 NZ for busy, bit 0 NZ for hold clock
            case 0xe0:
            case 0xf0:                          // bit 3 NZ for test mode, bit 2 NZ for 24hr, other bits unknown
                return abSambusRegs[wPort_ >> 4];
        }
    }

    // DALLAS data read
    else if (GetOption(dallasclock) && wPort_ == 0xff)
    {
        // Update the current SAM time if the update bit and oscillators are enabled
        if (!(abDallasRegs[0x0b] & 0x80) && ((abDallasRegs[0x0a] & 0x70) == 0x20))
            Update(&stDallas, &tDallasLast);
        else
            time(&tDallasLast);

        switch (bDallasReg)
        {
            // Time registers
            case 0x00:      return DallasDateValue(stDallas.nSecond10, stDallas.nSecond1);// Seconds
            case 0x02:      return DallasDateValue(stDallas.nMinute10, stDallas.nMinute1);// Minutes
            case 0x04:      return DallasDateValue(stDallas.nHour10, stDallas.nHour1);  // Hours
            case 0x06:      return 1+GetDayOfWeek(&stDallas);                           // Day of week
            case 0x07:      return DallasDateValue(stDallas.nDay10, stDallas.nDay1);        // Day of the month
            case 0x08:      return DallasDateValue(stDallas.nMonth10, stDallas.nMonth1);    // Month
            case 0x09:      return DallasDateValue(stDallas.nYear10, stDallas.nYear1);  // Year

            // Control registers that require special handling
            case 0x0c:      { BYTE bRet = abDallasRegs[bDallasReg]; abDallasRegs[bDallasReg] = 0x00; return bRet; }

            default:        return abDallasRegs[bDallasReg & 0x7f];
        }
    }

    // Not present
    return 0xff;
}

void Clock::Out (WORD wPort_, BYTE bVal_)
{
    // Strip off the bottom 8 bits (always 239 for CLOCK_PORT)
    wPort_ >>= 8;

    if (GetOption(sambusclock) && wPort_ < 0xfe)
    {
        // Get the current SAM time, unless the clock update is on hold
        if (!(abSambusRegs[0x0d] & 0x02))
            Update(&stSamBus, &tSamBusLast);
        else
            time(&tSamBusLast);

        // SAMBUS clock only appears to use the lower 4 bits of the value
        bVal_ &= 0x0f;

        // Make the modification depending on which port was written to
        switch (wPort_ &= 0xf0)
        {
            // SAMBUS clock ports
            case 0x00:  stSamBus.nSecond1 = bVal_;  break;      // Seconds (ones)
            case 0x10:  stSamBus.nSecond10 = bVal_; break;      // Seconds (tens)
            case 0x20:  stSamBus.nMinute1 = bVal_;  break;      // Minutes (ones)
            case 0x30:  stSamBus.nMinute10 = bVal_; break;      // Minutes (tens)
            case 0x40:  stSamBus.nHour1 = bVal_;    break;      // Hours (ones)
            case 0x50:  stSamBus.nHour10 = bVal_;   break;      // Hours (tens)
            case 0x60:  stSamBus.nDay1 = bVal_;     break;      // Days (ones)
            case 0x70:  stSamBus.nDay10 = bVal_;    break;      // Days (tens)
            case 0x80:  stSamBus.nMonth1 = bVal_;   break;      // Months (ones)
            case 0x90:  stSamBus.nMonth10 = bVal_;  break;      // Months (tens)
            case 0xa0:  stSamBus.nYear1 = bVal_;    break;      // Year (ones)
            case 0xb0:  stSamBus.nYear10 = bVal_;   break;      // Year (tens)

            // These appear to be control registers
            case 0xd0:  bVal_ &= ~0x02;         // bit 1 NZ for busy
            case 0xe0:
            case 0xf0:      // bit 3 NZ for test mode, bit 2 NZ for 24hr, other bits unknown
                abSambusRegs[wPort_ >> 4] = bVal_;
                break;

            default:
                TRACE("Clock Out(): unhandled SAMBUS clock write (port=%#02x, val=%#02x)\n", wPort_ & 0xf0, bVal_);
        }
    }

    else if (GetOption(dallasclock) && wPort_ >= 0xfe)
    {
        // Get the current SAM time, unless the clock update has been disabled
        if (!(abDallasRegs[0x0b] & 0x80) && ((abDallasRegs[0x0a] & 0x70) == 0x20))
            Update(&stDallas, &tDallasLast);
        else
            time(&tDallasLast);

        switch (wPort_ & 1)
        {
            // Select a register
            case 0:
                bDallasReg = bVal_ & 0x7f;
                break;

            // Write a register value
            case 1:
            {
                switch (bDallasReg)
                {
                    // Some control registers need special handling, as not all bits are writeable
                    case 0x0a:  abDallasRegs[bDallasReg] = (abDallasRegs[bDallasReg] & 0x80) | (bVal_ & ~0x80); break;
                    case 0x0c:  break;      // All bits read-only
                    case 0x0d:  break;      // All bits read-only

                    default:
                        abDallasRegs[bDallasReg] = bVal_;
                        break;
                }

                TRACE("DALLAS reg %#02x now %#02x  (%#02x written)\n", bDallasReg, abDallasRegs[bDallasReg], bVal_);

                break;
            }
        }
    }
}

void Clock::FrameUpdate ()
{
    static int nFrames = 0;

    // Every one second we advance the emulation relative time
    if (!(++nFrames %= EMULATED_FRAMES_PER_SECOND))
        tEmulated++;
}


////////////////////////////////////////////////////////////////////////////////


// Initialise a SAMTIME structure with the current date/time
static time_t InitTime (SAMTIME* pst_)
{
    // Get current local time
    time_t tNow = tEmulated = time(NULL);
    tm* ptm = localtime(&tNow);

    pst_->nYear10 = ptm->tm_year / 10;
    pst_->nYear1  = ptm->tm_year % 10;

    pst_->nMonth10 = ++ptm->tm_mon / 10;    // Change month from zero-based to one-based
    pst_->nMonth1  = ptm->tm_mon % 10;

    pst_->nDay10 = ptm->tm_mday / 10;
    pst_->nDay1  = ptm->tm_mday % 10;

    pst_->nHour10 = ptm->tm_hour / 10;
    pst_->nHour1  = ptm->tm_hour % 10;

    pst_->nMinute10 = ptm->tm_min / 10;
    pst_->nMinute1  = ptm->tm_min % 10;

    pst_->nSecond10 = ptm->tm_sec / 10;
    pst_->nSecond1  = ptm->tm_sec % 10;

    // Return the time that the structure has been filled with
    return tNow;
}

static void Update (SAMTIME* pst_, time_t* ptLapst_)
{
    // Get the time in more normal components
    int nSecond = pst_->nSecond10 * 10 + pst_->nSecond1;
    int nMinute = pst_->nMinute10 * 10 + pst_->nMinute1;
    int nHour   = pst_->nHour10 * 10 + pst_->nHour1;
    int nDay    = pst_->nDay10 * 10 + pst_->nDay1;
    int nMonth  = pst_->nMonth10 * 10 + pst_->nMonth1;
    int nYear   = pst_->nYear10 * 10 + pst_->nYear1;

    // The clocks are either always synchronised with real time, or stay relative to emulated time
    time_t tNow = GetOption(clocksync) ? time(NULL) : tEmulated;

    // Work out how many seconds have passed since the last SAM time update
    int nDiff = tNow - *ptLapst_;
    *ptLapst_ = tNow;

    // Add on the change in number of seconds, reducing invalid values to 59 before-hand
    nDiff = (nSecond = min(nSecond, 59) + nDiff) / 60;
    pst_->nSecond1 = (nSecond %= 60) % 10;
    pst_->nSecond10 = nSecond / 10;

    // If there's any time left, consider updating the minutes
    if (nDiff)
    {
        // Add on the change in number of minutes, reducing invalid values to 59 before-hand
        nDiff = (nMinute = min(nMinute, 59) + nDiff) / 60;
        pst_->nMinute1 = (nMinute %= 60) % 10;
        pst_->nMinute10 = nMinute / 10;

        // If there's any time left, consider updating the hours
        if (nDiff)
        {
            // Add on the change in number of hours, reducing invalid values to 23 before-hand
            nDiff = (nHour = min(nHour, 23) + nDiff) / 24;
            pst_->nHour1 = (nHour %= 24) % 10;
            pst_->nHour10 = nHour / 10;

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
                        pst_->nDay1 = (nDay += nDiff) % 10;
                        pst_->nDay10 = nDay / 10;
                        break;
                    }

                    // Complete the current month and set the day back to the first of the month
                    nDiff -= anDays[nMonth] - nDay + 1;
                    nDay = 1;

                    // If we've completed a year, move back to Jan and increment the year
                    if (++nMonth > 12)
                    {
                        nMonth = 1;

                        pst_->nYear1 = ++nYear % 10;
                        pst_->nYear10 = nYear / 10;
                    }

                    // Update the month digits as they've changed
                    pst_->nMonth1 = nMonth % 10;
                    pst_->nMonth10 = nMonth / 10;
                }
            }
        }
    }
}

// Get the day of the week for the a supplied SAMTIME
static int GetDayOfWeek (SAMTIME* pst_)
{
    struct tm t, *ptm;

    t.tm_year = (pst_->nYear10 * 10) + pst_->nYear1;
    t.tm_mon = (pst_->nMonth10 * 10) + pst_->nMonth1 - 1;
    t.tm_mday = (pst_->nDay10 * 10) + pst_->nDay1;
    t.tm_hour = (pst_->nHour10 * 10) + pst_->nHour1;
    t.tm_min = (pst_->nMinute10 * 10) + pst_->nMinute1;
    t.tm_sec = (pst_->nSecond10 * 10) + pst_->nSecond1;

    // Create a ctime value from the current date
    time_t tNow = mktime(&t);

    // Convert back to a tm structure to get the day of the week :-)
    return (ptm = localtime(&tNow)) ? ptm->tm_wday : 0;
}

// The format SAMBUS dates are returned in (BCD or binary) depends on bit 2 of register B
static BYTE DallasDateValue (BYTE bH_, BYTE bL_)
{
    // Bit is set for binary, reset for BCD
    return (abDallasRegs[0x0b] & 0x02) ? (bH_ * 10) + bL_ : (bH_ << 4) | bL_;
}
