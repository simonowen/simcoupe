// Part of SimCoupe - A SAM Coupé emulator
//
// IO.cpp: SAM I/O port handling
//
//  Copyright (c) 1996-2001  Allan Skillman
//  Copyright (c) 1999-2001  Simon Owen
//  Copyright (c) 2000-2001  Dave Laundon
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

// Changes 1999-2001 by Simon Owen
//  - hooked up newly supported hardware from other modules
//  - most ports now correctly initialised to zero on reset (allow reset screens)
//  - palette only pre-initialised on power-on
//  - HPEN now returns the current line number
//  - LPEN now treats right-border as part of following line
//  - ATTR port now allows on/off-screen detection (onscreen always zero tho)
//  - clut values now cached, to allow runtime changes in pixel format

// Changes 2000-2001 by Dave Laundon
//  - mode change done in two steps, with mode changed one block after page
//  - line interrupt now correctly activated/deactivated on port writes

// ToDo:
//  - tidy up a bit, and handle drive reinitialiseation in IO::Init() better

#include "SimCoupe.h"
#include "IO.h"

#include "Atom.h"
#include "CDrive.h"
#include "Clock.h"
#include "CPU.h"
#include "Floppy.h"
#include "Frame.h"
#include "GUI.h"
#include "Memory.h"
#include "MIDI.h"
#include "Mouse.h"
#include "Options.h"
#include "Parallel.h"
#include "Serial.h"
#include "Sound.h"
#include "Util.h"
#include "Video.h"

extern int g_nLine;
extern int g_nLineCycle;

CDiskDevice *pDrive1, *pDrive2;
CIoDevice *pParallel1, *pParallel2;
CIoDevice *pSerial1, *pSerial2;
CIoDevice *pMidi;
CIoDevice *pBeeper;


// Paging ports for internal and external memory
BYTE vmpr, hmpr, lmpr, lepr, hepr;
BYTE vmpr_mode, vmpr_page1, vmpr_page2;

BYTE border, border_col;

BYTE keyboard;
BYTE status_reg;
BYTE line_int;

BYTE hpen, lpen;

UINT clut[N_CLUT_REGS], clutval[N_CLUT_REGS], mode3clutval[4];

BYTE keyports[9];       // 8 rows of keys (+ 1 row for unscanned keys)

bool fASICStartup = true;

static inline void out_vmpr(BYTE bVal_);
static inline void out_hmpr(BYTE bVal_);
static inline void out_lmpr(BYTE bVal_);
static inline void out_lepr(BYTE bVal_);
static inline void out_hepr(BYTE bVal_);


bool IO::Init (bool fFirstInit_/*=false*/)
{
    bool fRet = true;
    Exit(true);

    // Any sort of reset requires the paging to be reinitialised
    out_vmpr(0);                    // Video in page 0, screen mode 1
    out_hmpr(0);                    // Page 0 in section A, page 1 in section B, ROM0 on, ROM off
    out_lmpr(0);                    // Page 0 in section C, page 1 in section D

    // Initialise external memory
    out_lepr(0);
    out_hepr(0);

    // Border black, speaker off
    out_byte(BORDER_PORT, 0);


    // No extended keys pressed, no active (or visible) interrupts
    status_reg = 0xff;
    line_int = 0xff;

    // Clear lightpen/scan position
    hpen = lpen = 0x00;


    // Also, if this is a power-on initialisation, set up the clut, etc.
    if (fFirstInit_)
    {
        // No keys pressed initially
        ReleaseAllSamKeys();

        // Initialise all the devices
        fRet &= (InitDrives () && InitParallel() && InitSerial() && InitMidi() && InitBeeper() && Clock::Init());
    }

    // Return true only if everything
    return fRet;
}

void IO::Exit (bool fReInit_/*=false*/)
{
    if (!fReInit_)
    {
        InitDrives(false, fReInit_);
        InitParallel(false, fReInit_);
        InitSerial(false, fReInit_);
        InitMidi(false, fReInit_);
        InitBeeper(false, fReInit_);

        Clock::Exit();
    }
}


bool IO::InitDrives (bool fInit_/*=true*/, bool fReInit_/*=true*/)
{
    if (pDrive1 && ((!fInit_ && !fReInit_) || (pDrive1->GetType() != GetOption(drive1))))
    {
        if (pDrive1->GetType() == dskImage)
            SetOption(disk1, pDrive1->GetImage());

        delete pDrive1;
        pDrive1 = NULL;
    }

    if (pDrive2 && ((!fInit_ && !fReInit_) || (pDrive2->GetType() != GetOption(drive2))))
    {
        if (pDrive2->GetType() == dskImage)
            SetOption(disk2, pDrive2->GetImage());

        delete pDrive2;
        pDrive2 = NULL;
    }

    Floppy::Exit(fReInit_);

    if (fInit_)
    {
        Floppy::Init();

        if (!pDrive1)
        {
            switch (GetOption(drive1))
            {
                case dskImage:
                    if (!(pDrive1 = new CDrive)->Insert(GetOption(disk1)))
                        SetOption(disk1, "");
                    break;

                default:
                    pDrive1 = new CDiskDevice(dskNone);
                    break;
            }
        }

        if (!pDrive2)
        {
            switch (GetOption(drive2))
            {
                case dskImage:
                    if (!(pDrive2 = new CDrive)->Insert(GetOption(disk2)))
                        SetOption(disk2, "");
                    break;

                case dskAtom:
                    pDrive2 = new CAtomDiskDevice;
                    break;

                default:
                    pDrive2 = new CDiskDevice(dskNone);
                    break;
            }
        }
    }

    return !fInit_ || (pDrive1 && pDrive2);
}

bool IO::InitParallel (bool fInit_/*=true*/, bool fReInit_/*=true*/)
{
    if (pParallel1) { delete pParallel1; pParallel1 = NULL; }
    if (pParallel2) { delete pParallel2; pParallel2 = NULL; }

    if (fInit_)
    {
        switch (GetOption(parallel1))
        {
            case 1:     pParallel1 = new CPrinterDevice;    break;
            case 2:     pParallel1 = new CMonoDACDevice;    break;
            case 3:     pParallel1 = new CStereoDACDevice;  break;
            default:    pParallel1 = new CIoDevice;         break;
        }

        switch (GetOption(parallel2))
        {
            case 1:     pParallel2 = new CPrinterDevice;    break;
            case 2:     pParallel2 = new CMonoDACDevice;    break;
            case 3:     pParallel2 = new CStereoDACDevice;  break;
            default:    pParallel2 = new CIoDevice;         break;
        }
    }

    return !fInit_ || (pParallel1 && pParallel2);
}

bool IO::InitSerial (bool fInit_/*=true*/, bool fReInit_/*=true*/)
{
    if (pSerial1) { delete pSerial1; pSerial1 = NULL; }
    if (pSerial2) { delete pSerial2; pSerial2 = NULL; }

    if (fInit_)
    {
        // Serial ports are dummy for now
        pSerial1 = new CModemDevice;
        pSerial2 = new CModemDevice;
    }

    return !fInit_ || (pSerial1 && pSerial2);
}

bool IO::InitMidi (bool fInit_/*=true*/, bool fReInit_/*=true*/)
{
    if (pMidi) { delete pMidi; pMidi = NULL; }

    if (fInit_)
    {
        switch (GetOption(midi))
        {
            case 1:     pMidi = new CMidiDevice;    break;
            default:    pMidi = new CIoDevice;      break;
        }
    }

    return !fInit_ || pMidi;
}

bool IO::InitBeeper (bool fInit_/*=true*/, bool fReInit_/*=true*/)
{
    if (pBeeper) { delete pBeeper; pBeeper = NULL; }

    if (fInit_)
        pBeeper = GetOption(beeper) ? new CBeeperDevice : new CIoDevice;

    return fInit_ || pBeeper;
}

////////////////////////////////////////////////////////////////////////////////

static inline void PaletteChange (BYTE bHMPR_)
{
    // Update the 4 colours available to mode 3 (note: the middle colours are switched)
    BYTE mode3_bcd48 = (bHMPR_ & HMPR_MD3COL_MASK) >> 3;
    mode3clutval[0] = clutval[mode3_bcd48 | 0];
    mode3clutval[1] = clutval[mode3_bcd48 | 2];
    mode3clutval[2] = clutval[mode3_bcd48 | 1];
    mode3clutval[3] = clutval[mode3_bcd48 | 3];
}


static inline void out_lepr (BYTE bVal_)
{
    lepr = bVal_;

    if (HMPR_MCNTRL)
        PageIn(SECTION_C, N_PAGES_MAIN + bVal_);
}

static inline void out_hepr (BYTE bVal_)
{
    hepr = bVal_;

    if (HMPR_MCNTRL)
        PageIn(SECTION_D, N_PAGES_MAIN + bVal_);
}

static inline void out_vmpr (BYTE bVal_)
{
    // The ASIC changes mode before page, so consider an on-screen artifact from the mode change
    Frame::ChangeMode(bVal_);

    vmpr = (bVal_ & (VMPR_MODE_MASK|VMPR_PAGE_MASK));
    vmpr_mode = vmpr & VMPR_MODE_MASK;

    // Extract the page number(s) for faster access by the memory writing functions
    vmpr_page1 = VMPR_PAGE;
    vmpr_page2 = (vmpr_page1+1) & VMPR_PAGE_MASK;
}

static inline void out_hmpr (BYTE bVal_)
{
    // Have the mode3 BCD4/8 bits changed?
    if ((hmpr ^ bVal_) & HMPR_MD3COL_MASK)
    {
        // The changes are effective immediately in mode 3
        if (vmpr_mode == MODE_3)
            Frame::Update();

        // Update the mode 3 colours
        PaletteChange(bVal_);
    }

    // Update the HMPR
    hmpr = bVal_;

    // Put the relevant RAM bank into section C
    PageIn(SECTION_C, HMPR_PAGE);

    // Put either RAM or ROM1 into section D
    if (lmpr & LMPR_ROM1)
        PageIn(SECTION_D, ROM1);
    else
        PageIn(SECTION_D,(HMPR_PAGE + 1) & HMPR_PAGE_MASK);

    // Perform changes to external memory
    if (HMPR_MCNTRL)
    {
        out_hepr(hepr);
        out_lepr(lepr);
    }
}


static inline void out_lmpr(BYTE val)
{
    lmpr = val;

    // Put either RAM or ROM 0 into section A
    if (lmpr & LMPR_ROM0_OFF)
        PageIn(SECTION_A, LMPR_PAGE);
    else
        PageIn(SECTION_A, ROM0);

    // Put the relevant bank into section B
    PageIn(SECTION_B, (LMPR_PAGE + 1) & LMPR_PAGE_MASK);

    // Put either RAM or ROM 1 into section D
    if (lmpr & LMPR_ROM1)
        PageIn(SECTION_D, ROM1);
    else
        PageIn(SECTION_D, (HMPR_PAGE + 1) & HMPR_PAGE_MASK);
}

static inline void out_clut(WORD wPort_, BYTE bVal_)
{
    wPort_ &= (N_CLUT_REGS-1);          // 16 clut registers, so only the bottom 4 bits are significant
    bVal_ &= (N_PALETTE_COLOURS-1);     // 128 colours, so only the bottom 7 bits are significant

    // Has the clut value actually changed?
    if (clut[wPort_] != aulPalette[bVal_])
    {
        // Draw up to the current point with the previous settings
        Frame::Update();

        // Update the clut entry and the mode 3 palette
        clut[wPort_] = static_cast<DWORD>(aulPalette[clutval[wPort_] = bVal_]);
        PaletteChange(hmpr);
    }
}


BYTE IO::In (WORD wPort_)
{
    BYTE bPortLow = wPort_ & 0xff;

    // The ASIC doesn't respond to I/O immediately after power-on
    if (fASICStartup)
    {
        fASICStartup = g_dwCycleCounter < ASIC_STARTUP_DELAY;
        if (bPortLow >= BASE_ASIC_PORT)
            return 0x00;
    }


    switch (bPortLow)
    {
        // keyboard 1 / mouse
        case KEYBOARD_PORT:
        {
            int highbyte = wPort_ >> 8, res = 0x1f;

            if (highbyte == 0xff)
            {
                res = keyports[8] & 0x1f;

                if (GetOption(mouse) && res == 0x1f)
                    res = Mouse::Read(g_dwCycleCounter) & 0x1f;
            }
            else
            {
                if (!(highbyte & 0x80)) res &= keyports[7] & 0x1f;
                if (!(highbyte & 0x40)) res &= keyports[6] & 0x1f;
                if (!(highbyte & 0x20)) res &= keyports[5] & 0x1f;
                if (!(highbyte & 0x10)) res &= keyports[4] & 0x1f;
                if (!(highbyte & 0x08)) res &= keyports[3] & 0x1f;
                if (!(highbyte & 0x04))
                {
                    res &= keyports[2] & 0x1f;

                    // Once the keyboard is being read we must have finished booted, so turn off the fast startup
                    if (g_nFastBooting)
                        g_nFastBooting = 0;

                    // With auto-booting enabled we'll tap F9 a few times (it's released by the normal key scan)
                    static int nFastBoot = 10 * GetOption(autoboot);
                    if (nFastBoot && (--nFastBoot & 1) && pDrive1->IsInserted())
                        PressSamKey(SK_F9);
                }

                if (!(highbyte & 0x02)) res &= keyports[1] & 0x1f;
                if (!(highbyte & 0x01)) res &= keyports[0] & 0x1f;
            }

            return (keyboard & 0xe0) | res;
        }

        // keyboard 2
        case STATUS_PORT:
        {
            // Add on the instruction time to the line counter and the global counter
            // This will make sure the value in status_reg is up to date
            CheckCpuEvents();

            int highbyte = wPort_ >> 8, res = 0xe0;

            if (!(highbyte & 0x80)) res &= keyports[7] & 0xe0;
            if (!(highbyte & 0x40)) res &= keyports[6] & 0xe0;
            if (!(highbyte & 0x20)) res &= keyports[5] & 0xe0;
            if (!(highbyte & 0x10)) res &= keyports[4] & 0xe0;
            if (!(highbyte & 0x08)) res &= keyports[3] & 0xe0;
            if (!(highbyte & 0x04)) res &= keyports[2] & 0xe0;
            if (!(highbyte & 0x02)) res &= keyports[1] & 0xe0;
            if (!(highbyte & 0x01)) res &= keyports[0] & 0xe0;

            return (res & 0xe0) | (status_reg & 0x1f);
        }


        // Banked memory management
        case VMPR_PORT:     return vmpr | 0x80;     // RXMIDI bit always one for now
        case HMPR_PORT:     return hmpr;
        case LMPR_PORT:     return lmpr;

        // SAMBUS and DALLAS clock ports
        case CLOCK_PORT:    return Clock::In(wPort_);

        // HPEN and LPEN ports
        case LPEN_PORT:
            if ((wPort_ & PEN_MASK) == LPEN_PORT)
                return lpen;
            else
            {
                // Return 192 for the top/bottom border areas, or the real line number
                // Note: the right-border area is treated as part of the following line
                return (g_nLine < TOP_BORDER_LINES || g_nLine >= (TOP_BORDER_LINES+SCREEN_LINES)) ? static_cast<BYTE>(SCREEN_LINES) :
                        g_nLine - TOP_BORDER_LINES + (g_nLineCycle > (BORDER_PIXELS+SCREEN_PIXELS));
            }

        // Spectrum ATTR port
        case ATTR_PORT:
        {
            // Border lines return 0xff
            if (g_nLine < TOP_BORDER_LINES && g_nLine >= (TOP_BORDER_LINES+SCREEN_LINES))
                return 0xff;

            // Strictly we need to check the mode and return the appropriate value, but for now we'll return zero
            return 0x00;
        }

        // External memory
        case HEPR_PORT:     return hepr;
        case LEPR_PORT:     return lepr;

        // Parallel ports
        case PRINTL1_STAT:
        case PRINTL1_DATA:
            return pParallel1->In(wPort_);

        case PRINTL2_STAT:
        case PRINTL2_DATA:
            return pParallel2->In(wPort_);

        // Serial ports
        case SERIAL1:
            return pSerial1->In(wPort_);

        case SERIAL2:
            return pSerial2->In(wPort_);

        // MIDI IN/Network
        case MIDI_PORT:
            return pMidi->In(wPort_);

        default:
        {
            // Dunno if there's a special reason these return zero!  Anyone?
            if (bPortLow < 0x10)
                return 0;

            // Floppy drive 1
            else if ((wPort_ & FLOPPY_MASK) == FLOPPY1_BASE)
            {
                // Read from floppy drive 1, if present
                if (GetOption(drive1))
                    return pDrive1->In(wPort_);
            }

            // Floppy drive 2 *OR* the ATOM hard disk
            else if ((wPort_ & FLOPPY_MASK) == FLOPPY2_BASE)
            {
                // Read from floppy drive 2 or Atom hard disk, if present
                if (GetOption(drive2))
                    return pDrive2->In(wPort_);
            }

            // Only unsupported hardware should reach here
            else
                TRACE("*** Unhandled read: %#04x (%d)\n", wPort_, wPort_&0xff);
        }
    }

    // Default to returning 0xff
    return 0xff;
}


// The actual port input and output routines
void IO::Out (WORD wPort_, BYTE bVal_)
{
    BYTE bPortLow = wPort_ & 0xff;

    // The ASIC doesn't respond to I/O immediately after power-on
    if (fASICStartup)
    {
        fASICStartup = g_dwCycleCounter < ASIC_STARTUP_DELAY;
        if (bPortLow >= BASE_ASIC_PORT)
            return;
    }

    switch (bPortLow)
    {
        case BORDER_PORT:
        {
            bool fScreenOffChange = ((border ^ bVal_) & BORD_SOFF_MASK) && VMPR_MODE_3_OR_4;

            // Has the border colour has changed colour or the screen been enabled/disabled?
            if (fScreenOffChange || ((border ^ bVal_) & BORD_COLOUR_MASK))
                Frame::Update();

            // If the speaker bit has been toggled, generate a click
            if ((border ^ bVal_) & BORD_BEEP_MASK)
                pBeeper->Out(wPort_, bVal_);

            // Store the new border value and extract the border colour for faster access by the video routines
            border = bVal_;
            border_col = BORD_VAL(bVal_);
            keyboard = (keyboard & ~BORD_SOFF_MASK) | BORD_SOFF;

            // If the screen state has changed, we need to reconsider memory contention changes
            if (fScreenOffChange)
                CPU::UpdateContention();
        }
        break;

        // Banked memory management
        case VMPR_PORT:
        {
            // Changes to screen mode and screen page are visible at different times

            // Has the screen mode changed?
            if (vmpr_mode != (bVal_ & VMPR_MODE_MASK))
            {
                // Are either the current mode or the new mode 3 or 4?  i.e. bit MDE1 is set
                if ((bVal_ | vmpr) & VMPR_MDE1_MASK)
                {
                    // Changes to the screen MODE are visible straight away
                    Frame::Update();

                    // Change only the screen MODE for the transition block
                    out_vmpr((bVal_ & VMPR_MODE_MASK) | (vmpr & ~VMPR_MODE_MASK));
                }
                // Otherwise both modes are 1 or 2
                else
                {
                    // There are no visible changes in the transition block
                    g_nLineCycle += VIDEO_DELAY;
                    Frame::Update();
                    g_nLineCycle -= VIDEO_DELAY;

                    // Do the whole change here - the check below will not be triggered
                    out_vmpr(bVal_);
                }

                // Video mode change may have affected memory contention
                CPU::UpdateContention();
            }

            // Has the screen page changed?
            if (vmpr_page1 != (bVal_ & VMPR_PAGE_MASK))
            {
                // Changes to screen PAGE aren't visible until 8 tstates later
                // as the memory has been read by the ASIC already
                g_nLineCycle += VIDEO_DELAY;
                Frame::Update();
                g_nLineCycle -= VIDEO_DELAY;

                out_vmpr(bVal_);
            }
        }
        break;

        case HMPR_PORT:
            if (hmpr != bVal_)
                out_hmpr(bVal_);
            break;

        case LMPR_PORT:
            if (lmpr != bVal_)
                out_lmpr(bVal_);
            break;

        // SAMBUS and DALLAS clock ports
        case CLOCK_PORT:
            Clock::Out(wPort_, bVal_);
            break;

        case CLUT_BASE_PORT:
            out_clut(wPort_ >> 8, bVal_);
            break;

        // External memory
        case HEPR_PORT:
            out_hepr(bVal_);
            break;

        case LEPR_PORT:
            out_lepr(bVal_);
            break;

        case LINE_PORT:
            // Line changed?
            if (line_int != bVal_)
            {
                // Set the new value
                line_int = bVal_;

                // Because of this change, does the LINE interrupt signal now need to be active, or not?
                if (line_int < SCREEN_LINES)
                {
                    // Offset Line and LineCycle from the point in the scan line that interrupts occur
                    int nLine_ = g_nLine, nLineCycle_ = g_nLineCycle - INT_START_TIME;
                    if (nLineCycle_ < 0)
                        nLineCycle_ += TSTATES_PER_LINE;
                    else
                        nLine_++;

                    // Make sure timing and events are right up-to-date
                    CheckCpuEvents();

                    // Does the interrupt need to be active?
                    if ((nLine_ == (line_int + TOP_BORDER_LINES)) && (nLineCycle_ < INT_ACTIVE_TIME))
                    {
                        // If the interrupt is already active then we have only just passed into the right border
                        // and the CheckCpuEvents above has taken care of it, so do nothing
                        if (status_reg & STATUS_INT_LINE)
                        {
                            // Otherwise, set the interrupt ourselves and create an event to end it
                            status_reg &= ~STATUS_INT_LINE;
                            AddCpuEvent(evtStdIntEnd, g_dwCycleCounter - nLineCycle_ + INT_ACTIVE_TIME);
                        }
                    }
                    else
                        // Else make sure it isn't active
                        status_reg |= STATUS_INT_LINE;
                }
                else
                    // Else make sure it isn't active
                    status_reg |= STATUS_INT_LINE;
            }
            break;

        case SOUND_DATA:
            Sound::Out(wPort_, bVal_);
            break;


        // Parallel ports 1 and 2
        case PRINTL1_STAT:
        case PRINTL1_DATA:
            pParallel1->Out(wPort_, bVal_);
            break;

        case PRINTL2_STAT:
        case PRINTL2_DATA:
            pParallel2->Out(wPort_, bVal_);
            break;


        // Serial ports 1 and 2
        case SERIAL1:
            pSerial1->Out(wPort_, bVal_);
            break;

        case SERIAL2:
            pSerial2->Out(wPort_, bVal_);
            break;


        // MIDI OUT/Network
        case MIDI_PORT:
        {
            // Add on the instruction time to the line counter and the global counter
            // This will make sure the value in lpen is up to date
            CheckCpuEvents();

            // Only transmit a new byte if one isn't already being sent
            if (!(lpen & LPEN_TXFMST))
            {
                // Set the TXFMST bit in LPEN to show that we're transmitting something
                lpen |= LPEN_TXFMST;

                // Create an event to begin an interrupt at the required time
                AddCpuEvent(evtMidiOutIntStart, g_dwCycleCounter +
                            A_ROUND(MIDI_TRANSMIT_TIME + 16, 32) - 16 - 32 - MIDI_INT_ACTIVE_TIME + 2);

                // Output the byte using the platform specific implementation
                pMidi->Out(wPort_, bVal_);
            }
            break;
        }

        default:
            // Floppy drive 1
            if ((wPort_ & FLOPPY_MASK) == FLOPPY1_BASE)
            {
                // Write to floppy drive 1, if present
                if (GetOption(drive1))
                    pDrive1->Out(wPort_, bVal_);
            }

            // Floppy drive 2 *OR* the ATOM hard disk
            else if ((wPort_ & FLOPPY_MASK) == FLOPPY2_BASE)
            {
                // Write to floppy drive 2 or Atom hard disk, if present...
                if (GetOption(drive2))
                    pDrive2->Out(wPort_, bVal_);
            }

            // Only unsupported hardware should reach here
            else
                TRACE("*** Unhandled write: %#04x (LSB=%d), %#02x (%d)\n", wPort_, bPortLow, bVal_, bVal_);
    }
}

void IO::FrameUpdate ()
{
    pDrive1->FrameEnd();
    pDrive2->FrameEnd();

    Clock::FrameUpdate();

    Sound::FrameUpdate();
}

const RGBA* IO::GetPalette (bool fDimmed_/*=false*/)
{
    static RGBA asPalette[N_PALETTE_COLOURS];
    static bool fPrepared = false, fDimmed;

    // If we've already got what's needed, return the current setup
    if (fPrepared && fDimmed_ == fDimmed)
        return asPalette;

    // Look-up table for an even intensity spread, used to map SAM colours to RGB
    static const BYTE abIntensities[] = { 0x00, 0x24, 0x49, 0x6d, 0x92, 0xb6, 0xdb, 0xff };

    // Build the full palette: SAM's 128 colours and the extra colours for the GUI
    for (int i = 0; i < N_PALETTE_COLOURS ; i++)
    {
        // Convert from SAM palette position to 8-bit RGB
        BYTE bRed   = abIntensities[(i&0x02)     | ((i&0x20) >> 3) | ((i&0x08) >> 3)];
        BYTE bGreen = abIntensities[(i&0x04) >> 1| ((i&0x40) >> 4) | ((i&0x08) >> 3)];
        BYTE bBlue  = abIntensities[(i&0x01) << 1| ((i&0x10) >> 2) | ((i&0x08) >> 3)];

        // Dim if required
        if (fDimmed_)
        {
            bRed   = (bRed   << 1) / 3;
            bGreen = (bGreen << 1) / 3;
            bBlue  = (bBlue  << 1) / 3;
        }

        // Store the calculated values for the entry
        asPalette[i].bRed = bRed;
        asPalette[i].bGreen = bGreen;
        asPalette[i].bBlue = bBlue;
        asPalette[i].bAlpha = 0xff;
    }

    // Remember the current state to cache the palette for future calls if possible
    fPrepared = true;
    fDimmed = fDimmed_;

    // Return the freshly prepared palette
    return asPalette;
}
