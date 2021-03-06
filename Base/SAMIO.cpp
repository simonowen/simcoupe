// Part of SimCoupe - A SAM Coupe emulator
//
// SAMIO.cpp: SAM I/O port handling
//
//  Copyright (c) 1999-2015 Simon Owen
//  Copyright (c) 1996-2001 Allan Skillman
//  Copyright (c) 2000-2001 Dave Laundon
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

#include "SimCoupe.h"
#include "SAMIO.h"

#include "Atom.h"
#include "AtomLite.h"
#include "BlueAlpha.h"
#include "Clock.h"
#include "CPU.h"
#include "Drive.h"
#include "Floppy.h"
#include "Frame.h"
#include "GUI.h"
#include "HardDisk.h"
#include "Input.h"
#include "Joystick.h"
#include "Keyboard.h"
#include "Keyin.h"
#include "Memory.h"
#include "MIDI.h"
#include "Mouse.h"
#include "Options.h"
#include "Parallel.h"
#include "Paula.h"
#include "SAMDOS.h"
#include "SAMVox.h"
#include "SDIDE.h"
#include "SID.h"
#include "Sound.h"
#include "Tape.h"
#include "Video.h"
#include "VoiceBox.h"

std::unique_ptr<DiskDevice> pFloppy1;
std::unique_ptr<DiskDevice> pFloppy2;
std::unique_ptr<DiskDevice> pBootDrive;
std::unique_ptr<AtaAdapter> pAtom;
std::unique_ptr<AtaAdapter> pAtomLite;
std::unique_ptr<AtaAdapter> pSDIDE;

std::unique_ptr<PrintBuffer> pPrinterFile;
std::unique_ptr<MonoDACDevice> pMonoDac;
std::unique_ptr<StereoDACDevice> pStereoDac;

std::unique_ptr<ClockDevice> pSambus;
std::unique_ptr<DallasClock> pDallas;
std::unique_ptr<MouseDevice> pMouse;

std::unique_ptr<MidiDevice> pMidi;
std::unique_ptr<BeeperDevice> pBeeper;
std::unique_ptr<BASamplerDevice> pSampler;
std::unique_ptr<VoiceBoxDevice> pVoiceBox;
std::unique_ptr<SAMVoxDevice> pSAMVox;
std::unique_ptr<PaulaDevice> pPaula;
std::unique_ptr<DAC> pDAC;
std::unique_ptr<SAADevice> pSAA;
std::unique_ptr<SIDDevice> pSID;

// Port read/write addresses for I/O breakpoints
uint16_t wPortRead, wPortWrite;
uint8_t bPortInVal, bPortOutVal;

// Paging ports for internal and external memory
uint8_t vmpr, hmpr, lmpr, lepr, hepr;
uint8_t vmpr_mode, vmpr_page1, vmpr_page2;

uint8_t border, border_col;

uint8_t keyboard;
uint8_t status_reg;
uint8_t line_int;
uint8_t lpen;
uint8_t hpen;
uint8_t attr;

unsigned int clut[N_CLUT_REGS], mode3clut[4];

uint8_t keyports[9];       // 8 rows of keys (+ 1 row for unscanned keys)
uint8_t keybuffer[9];      // working buffer for key changed, activated mid-frame

bool fASICStartup;      // If set, the ASIC will be unresponsive shortly after first power-on
bool display_changed;   // Mid-frame main display change using VMPR or CLUT

int g_nAutoLoad = AUTOLOAD_NONE;    // don't auto-load on startup

#ifdef _DEBUG
static uint8_t abUnhandled[32];    // track unhandled port access in debug mode
#endif

//////////////////////////////////////////////////////////////////////////////

namespace IO
{

bool Init(bool fFirstInit_/*=false*/)
{
    bool fRet = true;
    Exit(true);

    // Forget any automatic input after reset
    Keyin::Stop();

    // Reset ASIC registers
    lmpr = hmpr = vmpr = lepr = hepr = lpen = border = 0;
    keyboard = BORD_EAR_MASK;

    OutLmpr(lmpr);  // Page 0 in section A, page 1 in section B, ROM0 on, ROM1 off
    OutHmpr(hmpr);  // Page 0 in section C, page 1 in section D
    OutVmpr(vmpr);  // Video in page 0, screen mode 1

    // No extended keys pressed, no active interrupts
    status_reg = STATUS_INT_NONE;

    // Also, if this is a power-on initialisation, set up the clut, etc.
    if (fFirstInit_)
    {
        // Line interrupts aren't cleared by a reset
        line_int = 0xff;

        // Release all keys
        memset(keyports, 0xff, sizeof(keyports));

        pDAC = std::make_unique<DAC>();
        pSAA = std::make_unique<SAADevice>();
        pSID = std::make_unique<SIDDevice>();
        pBeeper = std::make_unique<BeeperDevice>();
        pSampler = std::make_unique<BASamplerDevice>();
        pVoiceBox = std::make_unique<VoiceBoxDevice>();
        pSAMVox = std::make_unique<SAMVoxDevice>();
        pPaula = std::make_unique<PaulaDevice>();
        pMidi = std::make_unique<MidiDevice>();

        pSambus = std::make_unique<SambusClock>();
        pDallas = std::make_unique<DallasClock>();
        pMouse = std::make_unique<MouseDevice>();

        pPrinterFile = std::make_unique<PrinterFile>();
        pMonoDac = std::make_unique<MonoDACDevice>();
        pStereoDac = std::make_unique<StereoDACDevice>();

        pFloppy1 = std::make_unique<Drive>();
        pFloppy2 = std::make_unique<Drive>();
        pAtom = std::make_unique<AtomDevice>();
        pAtomLite = std::make_unique<AtomLiteDevice>();

        pSDIDE = std::make_unique<SDIDEDevice>();

        pDallas->LoadState(OSD::MakeFilePath(PathType::Settings, "dallas"));

        pFloppy1->Insert(GetOption(disk1));
        pFloppy2->Insert(GetOption(disk2));

        Tape::Insert(GetOption(tape));

        auto& pActiveAtom = (GetOption(drive2) == drvAtom) ? pAtom : pAtomLite;
        pActiveAtom->Attach(GetOption(atomdisk0), 0);
        pActiveAtom->Attach(GetOption(atomdisk1), 1);

        pSDIDE->Attach(GetOption(sdidedisk), 0);
    }

    // The ASIC is unresponsive during the first ~49ms on production SAM units
    if (GetOption(asicdelay))
    {
        fASICStartup = true;
        AddCpuEvent(EventType::AsicReady, g_dwCycleCounter + CPU_CYCLES_ASIC_STARTUP);
    }

    // Reset the sound hardware
    pDAC->Reset();
    pSID->Reset();
    pSampler->Reset();
    pVoiceBox->Reset();

    // Reset the disk hardware
    pFloppy1->Reset();
    pFloppy2->Reset();
    pAtom->Reset();
    pAtomLite->Reset();
    pSDIDE->Reset();

    // Stop the tape on reset
    Tape::Stop();

    // Return true only if everything
    return fRet;
}

void Exit(bool fReInit_/*=false*/)
{
    if (!fReInit_)
    {
        if (pPrinterFile)
            pPrinterFile->Flush();

        if (pFloppy1)
            SetOption(disk1, pFloppy1->DiskPath());

        if (pFloppy2)
            SetOption(disk2, pFloppy2->DiskPath());

        if (pDallas)
            pDallas->SaveState(OSD::MakeFilePath(PathType::Settings, "dallas"));

        SetOption(tape, Tape::GetPath());
        Tape::Eject();

        pMidi.reset();
        pPaula.reset();
        pSAMVox.reset();
        pSampler.reset();
        pVoiceBox.reset();
        pBeeper.reset();
        pSID.reset();
        pSAA.reset();
        pDAC.reset();

        pSambus.reset();
        pDallas.reset();
        pMouse.reset();

        pPrinterFile.reset();
        pMonoDac.reset();
        pStereoDac.reset();

        pFloppy1.reset();
        pFloppy2.reset();
        pBootDrive.reset();

        pAtom.reset();
        pAtomLite.reset();
        pSDIDE.reset();
    }
}

////////////////////////////////////////////////////////////////////////////////

static inline void PaletteChange(uint8_t bHMPR_)
{
    // Update the 4 colours available to mode 3 (note: the middle colours are switched)
    uint8_t mode3_bcd48 = (bHMPR_ & HMPR_MD3COL_MASK) >> 3;
    mode3clut[0] = clut[mode3_bcd48 | 0];
    mode3clut[1] = clut[mode3_bcd48 | 2];
    mode3clut[2] = clut[mode3_bcd48 | 1];
    mode3clut[3] = clut[mode3_bcd48 | 3];
}


static inline void UpdatePaging()
{
    // ROM0 or internal RAM in section A
    if (!(lmpr & LMPR_ROM0_OFF))
        PageIn(Section::A, ROM0);
    else
        PageIn(Section::A, LMPR_PAGE);

    // Internal RAM in section B
    PageIn(Section::B, (LMPR_PAGE + 1) & LMPR_PAGE_MASK);

    // External RAM or internal RAM in section C
    if (hmpr & HMPR_MCNTRL_MASK)
        PageIn(Section::C, EXTMEM + lepr);
    else
        PageIn(Section::C, HMPR_PAGE);

    // External RAM, ROM1, or internal RAM in section D
    if (hmpr & HMPR_MCNTRL_MASK)
        PageIn(Section::D, EXTMEM + hepr);
    else if (lmpr & LMPR_ROM1)
        PageIn(Section::D, ROM1);
    else
        PageIn(Section::D, (HMPR_PAGE + 1) & HMPR_PAGE_MASK);
}

static uint8_t update_lpen()
{
    if (!VMPR_MODE_3_OR_4 || !BORD_SOFF)
    {
        int line = g_dwCycleCounter / CPU_CYCLES_PER_LINE, line_cycle = g_dwCycleCounter % CPU_CYCLES_PER_LINE;

        if (IsScreenLine(line) && line_cycle >= (CPU_CYCLES_PER_SIDE_BORDER + CPU_CYCLES_PER_SIDE_BORDER))
        {
            auto [b0, b1, b2, b3] = Frame::GetAsicData();

            auto xpos = static_cast<uint8_t>(line_cycle - (CPU_CYCLES_PER_SIDE_BORDER + CPU_CYCLES_PER_SIDE_BORDER));
            auto bcd1 = (line_cycle < (CPU_CYCLES_PER_SIDE_BORDER + CPU_CYCLES_PER_SIDE_BORDER)) ? (border & 1) : (b0 & 1);
            lpen = (xpos & 0xfc) | (lpen & LPEN_TXFMST) | bcd1;
        }
        else
        {
            lpen = (lpen & LPEN_TXFMST) | (border & 1);
        }
    }

    return lpen;
}

static uint8_t update_hpen()
{
    if (!VMPR_MODE_3_OR_4 || !BORD_SOFF)
    {
        int line = g_dwCycleCounter / CPU_CYCLES_PER_LINE, line_cycle = g_dwCycleCounter % CPU_CYCLES_PER_LINE;

        if (IsScreenLine(line) && (line != TOP_BORDER_LINES || line_cycle >= (CPU_CYCLES_PER_SIDE_BORDER + CPU_CYCLES_PER_SIDE_BORDER)))
            hpen = line - TOP_BORDER_LINES;
        else
            hpen = GFX_SCREEN_LINES;
    }

    return hpen;
}

void OutLmpr(uint8_t val)
{
    // Update LMPR and paging
    lmpr = val;
    UpdatePaging();
}

void OutHmpr(uint8_t bVal_)
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

    // Update HMPR and paging
    hmpr = bVal_;
    UpdatePaging();
}

void OutVmpr(uint8_t bVal_)
{
    // The ASIC changes mode before page, so consider an on-screen artifact from the mode change
    Frame::ChangeMode(bVal_);

    if ((vmpr ^ bVal_) & (VMPR_MODE_MASK | VMPR_PAGE_MASK))
    {
        auto [line, line_cycle] = Frame::GetRasterPos(g_dwCycleCounter);
        if (IsScreenLine(line))
            display_changed = true;
    }

    vmpr = bVal_ & (VMPR_MODE_MASK | VMPR_PAGE_MASK);
    vmpr_mode = VMPR_MODE;

    // Extract the page number for faster access by the memory writing functions
    vmpr_page1 = VMPR_PAGE;

    // The second page is only used by modes 3+4
    vmpr_page2 = VMPR_MODE_3_OR_4 ? ((vmpr_page1 + 1) & VMPR_PAGE_MASK) : 0xff;
}

void OutLepr(uint8_t bVal_)
{
    lepr = bVal_;
    UpdatePaging();
}

void OutHepr(uint8_t bVal_)
{
    hepr = bVal_;
    UpdatePaging();
}


void OutClut(uint16_t wPort_, uint8_t bVal_)
{
    wPort_ &= (N_CLUT_REGS - 1);          // 16 clut registers, so only the bottom 4 bits are significant
    bVal_ &= (N_PALETTE_COLOURS - 1);     // 128 colours, so only the bottom 7 bits are significant

    // Has the clut value actually changed?
    if (clut[wPort_] != bVal_)
    {
        auto [line, line_cycle] = Frame::GetRasterPos(g_dwCycleCounter);
        if (IsScreenLine(line))
            display_changed = true;

        // Draw up to the current point with the previous settings
        Frame::Update();

        // Update the clut entry and the mode 3 palette
        clut[wPort_] = bVal_;
        PaletteChange(hmpr);
    }
}


uint8_t In(uint16_t wPort_)
{
    uint8_t bPortLow = (wPortRead = wPort_) & 0xff, bPortHigh = (wPort_ >> 8);

    // Default port result if not handled
    uint8_t bRet = 0xff;

    // The ASIC doesn't respond to I/O immediately after power-on
    if (bPortLow >= BASE_ASIC_PORT && fASICStartup)
        return bPortInVal = 0x00;

    // Ensure state is up-to-date
    CheckCpuEvents();

    switch (bPortLow)
    {
        // keyboard 1 / mouse / tape
    case KEYBOARD_PORT:
    {
        // Consider a tape read first
        Tape::InFEHook();

        // Disable fast boot on the first keyboard read
        g_nTurbo &= ~TURBO_BOOT;

        if (bPortHigh == 0xff)
        {
            bRet = keyports[8];

            if (GetOption(mouse))
                bRet &= pMouse->In(wPort_);
        }
        else
        {
            if (!(bPortHigh & 0x80)) bRet &= keyports[7];
            if (!(bPortHigh & 0x40)) bRet &= keyports[6];
            if (!(bPortHigh & 0x20)) bRet &= keyports[5];
            if (!(bPortHigh & 0x10)) bRet &= keyports[4];
            if (!(bPortHigh & 0x08)) bRet &= keyports[3];
            if (!(bPortHigh & 0x04)) bRet &= keyports[2];
            if (!(bPortHigh & 0x02)) bRet &= keyports[1];
            if (!(bPortHigh & 0x01)) bRet &= keyports[0];
        }

        bRet = (keyboard & ~BORD_KEY_MASK) | (bRet & BORD_KEY_MASK);
        break;
    }

    // keyboard 2
    case STATUS_PORT:
    {
        if (!(bPortHigh & 0x80)) bRet &= keyports[7];
        if (!(bPortHigh & 0x40)) bRet &= keyports[6];
        if (!(bPortHigh & 0x20)) bRet &= keyports[5];
        if (!(bPortHigh & 0x10)) bRet &= keyports[4];
        if (!(bPortHigh & 0x08)) bRet &= keyports[3];
        if (!(bPortHigh & 0x04)) bRet &= keyports[2];
        if (!(bPortHigh & 0x02)) bRet &= keyports[1];
        if (!(bPortHigh & 0x01)) bRet &= keyports[0];

        bRet = (bRet & 0xe0) | (status_reg & 0x1f);
        break;
    }


    // Low memory page register
    case LMPR_PORT:
        bRet = lmpr;
        break;

    // High memory page register
    case HMPR_PORT:
        bRet = hmpr;
        break;


    // Video memory page register
    case VMPR_PORT:
        bRet = vmpr;
        bRet |= 0x80;   // RXMIDI bit always one for now
        break;

        // SAMBUS and DALLAS clock ports
    case CLOCK_PORT:
        if (wPort_ < 0xfe00 && GetOption(sambusclock))
            bRet = pSambus->In(wPort_);
        else if (wPort_ >= 0xfe00 && GetOption(dallasclock))
            bRet = pDallas->In(wPort_);
        break;

    case LPEN_PORT:
        if ((wPort_ & PEN_MASK) == LPEN_PORT)
            bRet = update_lpen();
        else
            bRet = update_hpen();
        break;

    // Spectrum ATTR port
    case ATTR_PORT:
    {
        // If the display is enabled, update the attribute port value
        if (!(VMPR_MODE_3_OR_4 && BORD_SOFF))
        {
            // Determine the 4 ASIC display bytes and return the 3rd, as documented
            auto [b0, b1, b2, b3] = Frame::GetAsicData();
            attr = b2;
        }

        // Return the current attribute port value
        bRet = attr;

        break;
    }

    // Parallel ports
    case PRINTL1_STAT:
    case PRINTL1_DATA:
    {
        switch (GetOption(parallel1))
        {
        case 1: bRet = pPrinterFile->In(wPort_); break;
        case 2: bRet = pMonoDac->In(wPort_); break;
        case 3: bRet = pStereoDac->In(wPort_); break;
        }
        break;
    }

    case PRINTL2_STAT:
    case PRINTL2_DATA:
    {
        switch (GetOption(parallel2))
        {
        case 1: bRet = pPrinterFile->In(wPort_); break;
        case 2: bRet = pMonoDac->In(wPort_); break;
        case 3: bRet = pStereoDac->In(wPort_); break;
        }
        break;
    }

    // Serial ports (currently unsupported)
    case SERIAL1:
    case SERIAL2:
        break;

        // MIDI IN/Network
    case MIDI_PORT:
        if (GetOption(midi) == 1) // MIDI device
            bRet = pMidi->In(wPort_);
        break;

        // S D Software IDE interface
    case SDIDE_REG:
    case SDIDE_DATA:
        bRet = pSDIDE->In(wPort_);
        break;

        // SID interface (reads not implemented by current hardware)
    case SID_PORT:
        break;

        // Quazar Surround (unsupported)
    case QUAZAR_PORT:
        break;

        // Kempston joystick interface
    case KEMPSTON_PORT:
        if (GetOption(joytype1) == jtKempston) bRet &= ~Joystick::ReadKempston(0);
        if (GetOption(joytype2) == jtKempston) bRet &= ~Joystick::ReadKempston(1);
        break;

    case BLUE_ALPHA_PORT:
        if (wPort_ == BA_VOICEBOX_PORT)
        {
            bRet = pVoiceBox->In(wPort_);
        }
        else if (GetOption(dac7c) == 1 && (wPort_ & BA_SAMPLER_MASK) == BA_SAMPLER_BASE)
        {
            bRet = pSampler->In(bPortHigh & 0x03);
        }
        break;

    default:
    {
        // Floppy drive 1
        if ((wPort_ & FLOPPY_MASK) == FLOPPY1_BASE)
        {
            switch (GetOption(drive1))
            {
            case drvFloppy: bRet = (pBootDrive ? pBootDrive : pFloppy1)->In(wPort_); break;
            default: break;
            }
        }

        // Floppy drive 2 *OR* the ATOM hard disk
        else if ((wPort_ & FLOPPY_MASK) == FLOPPY2_BASE)
        {
            switch (GetOption(drive2))
            {
            case drvFloppy:     bRet = pFloppy2->In(wPort_); break;
            case drvAtom:       bRet = pAtom->In(wPort_); break;
            case drvAtomLite:   bRet = pAtomLite->In(wPort_); break;
            }
        }

#ifdef _DEBUG
        // Only unsupported hardware should reach here
        else
        {
            int nEntry = bPortLow >> 3, nBit = 1 << (bPortLow & 7);

            if (!(abUnhandled[nEntry] & nBit))
            {
                Message(MsgType::Warning, "Unhandled read from port {:04x}\n", wPort_);
                abUnhandled[nEntry] |= nBit;
                g_fDebug = true;
            }
        }
#endif
    }
    }

    // Store the value for breakpoint use, then return it
    return bPortInVal = bRet;
}


// The actual port input and output routines
void Out(uint16_t wPort_, uint8_t bVal_)
{
    uint8_t bPortLow = (wPortWrite = wPort_) & 0xff, bPortHigh = (wPort_ >> 8);
    bPortOutVal = bVal_;

    // The ASIC doesn't respond to I/O immediately after power-on
    if (bPortLow >= BASE_ASIC_PORT && fASICStartup)
        return;

    // Ensure state is up-to-date
    CheckCpuEvents();

    switch (bPortLow)
    {
    case BORDER_PORT:
    {
        bool fScreenOffChange = ((border ^ bVal_) & BORD_SOFF_MASK) && VMPR_MODE_3_OR_4;

        // Has the border changed colour or the screen been enabled/disabled?
        if (fScreenOffChange || ((border ^ bVal_) & BORD_COLOUR_MASK))
            Frame::Update();

        // Change of screen enable state?
        if (fScreenOffChange)
        {
            // If the display is now enabled, consider a border change artefact
            if (BORD_SOFF)
                Frame::ChangeScreen(bVal_);

            // Otherwise determine the current ATTR value to return whilst disabled
            else
            {
                update_lpen();
                update_hpen();

                auto [b0, b1, b2, b3] = Frame::GetAsicData();
                attr = b2;
            }
        }

        // If the speaker bit has been toggled, generate a click
        if ((border ^ bVal_) & BORD_BEEP_MASK)
            pBeeper->Out(wPort_, bVal_);

        // Store the new border value and extract the border colour for faster access by the video routines
        border = bVal_;
        border_col = BORD_VAL(bVal_);
        lpen = (lpen & 0xfe) | (border & 1);

        // Update the port read value, including the screen-off status
        keyboard = (border & BORD_SOFF_MASK) | (keyboard & (BORD_EAR_MASK | BORD_KEY_MASK));

        // If the screen state has changed, update the active memory contention.
        // (unless we're running in debugger timing mode with minimal contention).
        if (fScreenOffChange)
            CPU::UpdateContention(CPU::IsContentionActive());
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
                OutVmpr((bVal_ & VMPR_MODE_MASK) | (vmpr & ~VMPR_MODE_MASK));
            }
            // Otherwise both modes are 1 or 2
            else
            {
                // There are no visible changes in the transition block
                g_dwCycleCounter += CPU_CYCLES_PER_CELL;
                Frame::Update();
                g_dwCycleCounter -= CPU_CYCLES_PER_CELL;

                // Do the whole change here - the check below will not be triggered
                OutVmpr(bVal_);
            }

            // The video mode has changed so update the active memory contention.
            // (unless we're running in debugger timing mode with minimal contention).
            CPU::UpdateContention(CPU::IsContentionActive());
        }

        // Has the screen page changed?
        if (vmpr_page1 != (bVal_ & VMPR_PAGE_MASK))
        {
            // Changes to screen PAGE aren't visible until 8 tstates later
            // as the memory has been read by the ASIC already
            g_dwCycleCounter += CPU_CYCLES_PER_CELL;
            Frame::Update();
            g_dwCycleCounter -= CPU_CYCLES_PER_CELL;

            OutVmpr(bVal_);
        }
    }
    break;

    case HMPR_PORT:
        if (hmpr != bVal_)
            OutHmpr(bVal_);
        break;

    case LMPR_PORT:
        if (lmpr != bVal_)
            OutLmpr(bVal_);
        break;

        // SAMBUS and DALLAS clock ports
    case CLOCK_PORT:
        if (wPort_ < 0xfe00 && GetOption(sambusclock))
            pSambus->Out(wPort_, bVal_);
        else if (wPort_ >= 0xfe00 && GetOption(dallasclock))
            pDallas->Out(wPort_, bVal_);
        break;

    case CLUT_BASE_PORT:
        OutClut(bPortHigh, bVal_);
        break;

        // External memory
    case HEPR_PORT:
        OutHepr(bVal_);
        break;

    case LEPR_PORT:
        OutLepr(bVal_);
        break;

    case LINE_PORT:
    // Line changed?
    if (line_int != bVal_)
    {
        // Cancel any existing line interrupt
        if (line_int < GFX_SCREEN_LINES)
        {
            CancelCpuEvent(EventType::LineInterrupt);
            status_reg |= STATUS_INT_LINE;
        }

        // Set the new value
        line_int = bVal_;

        // Valid line interrupt set?
        if (line_int < GFX_SCREEN_LINES)
        {
            uint32_t dwLineTime = (line_int + TOP_BORDER_LINES) * CPU_CYCLES_PER_LINE;

            // Schedule the line interrupt (could be active now, or already passed this frame)
            AddCpuEvent(EventType::LineInterrupt, dwLineTime);
        }
    }
    break;

    case SOUND_DATA:
        pSAA->Out(wPort_, bVal_);
        break;

        // Parallel ports 1 and 2
    case PRINTL1_STAT:
    case PRINTL1_DATA:
        switch (GetOption(parallel1))
        {
        case 1: return pPrinterFile->Out(wPort_, bVal_);
        case 2: return pMonoDac->Out(wPort_, bVal_);
        case 3: return pStereoDac->Out(wPort_, bVal_);
        }
        break;

    case PRINTL2_STAT:
    case PRINTL2_DATA:
        switch (GetOption(parallel2))
        {
        case 1: return pPrinterFile->Out(wPort_, bVal_);
        case 2: return pMonoDac->Out(wPort_, bVal_);
        case 3: return pStereoDac->Out(wPort_, bVal_);
        }
        break;

        // Serial ports 1 and 2 (currently unsupported)
    case SERIAL1:
    case SERIAL2:
        break;


        // MIDI OUT/Network
    case MIDI_PORT:
    {
        // Only transmit a new byte if one isn't already being sent
        if (!(lpen & LPEN_TXFMST))
        {
            // Set the TXFMST bit in LPEN to show that we're transmitting something
            lpen |= LPEN_TXFMST;

            // Create an event to begin an interrupt at the required time
            AddCpuEvent(EventType::MidiOutStart, g_dwCycleCounter +
                A_ROUND(MIDI_TRANSMIT_TIME + 16, 32) - 16 - 32 - MIDI_INT_ACTIVE_TIME + 1);

            // Output the byte to the appropriate device
            if (GetOption(midi) == 1)
                pMidi->Out(wPort_, bVal_);
        }
        break;
    }

    // S D Software IDE interface
    case SDIDE_REG:
    case SDIDE_DATA:
        pSDIDE->Out(wPort_, bVal_);
        break;

        // SID interface
    case SID_PORT:
        if (GetOption(sid))
            pSID->Out(wPort_, bVal_);
        break;

        // Quazar Surround (unsupported)
    case QUAZAR_PORT:
        break;

    default:
    {
        // Floppy drive 1
        if ((wPort_ & FLOPPY_MASK) == FLOPPY1_BASE)
        {
            switch (GetOption(drive1))
            {
            case drvFloppy: (pBootDrive ? pBootDrive : pFloppy1)->Out(wPort_, bVal_); break;
            default: break;
            }
        }

        // Floppy drive 2 *OR* the ATOM hard disk
        else if ((wPort_ & FLOPPY_MASK) == FLOPPY2_BASE)
        {
            TRACE("PORT OUT({:02x}) wrote {:02x}\n", wPort_, bVal_);

            switch (GetOption(drive2))
            {
            case drvFloppy:     pFloppy2->Out(wPort_, bVal_);  break;
            case drvAtom:       pAtom->Out(wPort_, bVal_);     break;
            case drvAtomLite:   pAtomLite->Out(wPort_, bVal_); break;
            }
        }
        else if (wPort_ == BA_VOICEBOX_PORT)
        {
            pVoiceBox->Out(0, bVal_);
        }

        // Blue Alpha, SAMVox and Paula ports overlap!
        else if ((bPortLow & 0xfc) == 0x7c)
        {
            // Determine which one device is connected
            switch (GetOption(dac7c))
            {
                // Blue Alpha Sampler
            case 1:
                // Blue Alpha only uses a single port
                if ((wPort_ & BA_SAMPLER_MASK) == BA_SAMPLER_BASE)
                {
                    pSampler->Out(bPortHigh & 0x03, bVal_);
                }
                break;

                // SAMVox
            case 2:
                pSAMVox->Out(bPortLow & 0x03, bVal_);
                break;

                // Paula
            case 3:
                pPaula->Out(bPortLow & 0x01, bVal_);
                break;
            }
        }

#ifdef _DEBUG
        // Only unsupported hardware should reach here
        else
        {
            int nEntry = bPortLow >> 3, nBit = 1 << (bPortLow & 7);

            if (!(abUnhandled[nEntry] & nBit))
            {
                Message(MsgType::Warning, "Unhandled write to port {:04x}, value = {:02x}\n", wPort_, bVal_);
                abUnhandled[nEntry] |= nBit;
                g_fDebug = true;
            }
        }
#endif
    }
    }
}

void FrameUpdate()
{
    pFloppy1->FrameEnd();
    pFloppy2->FrameEnd();
    pAtom->FrameEnd();
    pAtomLite->FrameEnd();
    pPrinterFile->FrameEnd();

    Input::Update();

    if (!Frame::TurboMode())
        Sound::FrameUpdate();
}

void UpdateInput()
{
    // To avoid accidents, purge keyboard input during accelerated disk access
    if (GetOption(turbodisk) && (pFloppy1->IsActive() || pFloppy2->IsActive()))
        Input::Purge();

    // Copy the working buffer to the live port buffer
    memcpy(keyports, keybuffer, sizeof(keyports));
}

std::vector<COLOUR> Palette()
{
    std::vector<COLOUR> palette(N_PALETTE_COLOURS);

    for (size_t i = 0; i < palette.size(); ++i)
    {
        auto r = (((i & 0x02) << 0) | ((i & 0x20) >> 3) | ((i & 0x08) >> 3)) / 7.0f;
        auto g = (((i & 0x04) >> 1) | ((i & 0x40) >> 4) | ((i & 0x08) >> 3)) / 7.0f;
        auto b = (((i & 0x01) << 1) | ((i & 0x10) >> 2) | ((i & 0x08) >> 3)) / 7.0f;

#if 0 // TODO
        if (srgb)
        {
            r = RGB2sRGB(r);
            g = RGB2sRGB(g);
            b = RGB2sRGB(b);
        }
#endif

        auto max_intensity = static_cast<float>(GetOption(maxintensity));
        palette[i].red = static_cast<uint8_t>(std::lround(r * max_intensity));
        palette[i].green = static_cast<uint8_t>(std::lround(g * max_intensity));
        palette[i].blue = static_cast<uint8_t>(std::lround(b * max_intensity));
    }

    return palette;
}

// Check if we're at the striped SAM startup screen
bool IsAtStartupScreen(bool fExit_)
{
    // Search the top 10 stack entries
    for (int i = 0; i < 20; i += 2)
    {
        // Check for 0f78 on stack, with previous location pointing at JR Z,-5
        if (read_word(REG_SP + i + 2) == 0x0f78 && read_word(read_word(REG_SP + i)) == 0xfb28)
        {
            // Optionally skip JR to exit WTFK loop at copyright message
            if (fExit_)
                write_word(REG_SP + i, read_word(REG_SP + i) + 2);

            return true;
        }
    }

    // Not found
    return false;
}

void AutoLoad(int nType_, bool fOnlyAtStartup_/*=true*/)
{
    // Ignore if auto-load is disabled, or we need to be at the startup screen but we're not
    if (!GetOption(autoload) || (fOnlyAtStartup_ && !IsAtStartupScreen()))
        return;

    // For disk booting press F9
    if (nType_ == AUTOLOAD_DISK)
        Keyin::String("\xc9", false);

    // For tape loading press F7
    else if (nType_ == AUTOLOAD_TAPE)
        Keyin::String("\xc7", false);
}

void WakeAsic()
{
    // No longer in ASIC startup phase
    fASICStartup = false;
}


bool EiHook()
{
    // If we're leaving the ROM interrupt handler, inject any auto-typing input
    if (REG_PC == 0x005a && GetSectionPage(Section::A) == ROM0)
        Keyin::Next();

    Tape::EiHook();

    // Continue EI processing
    return false;
}

bool Rst8Hook()
{
    // Return for normal processing if we're not executing ROM code
    if ((REG_PC < 0x4000 && GetSectionPage(Section::A) != ROM0) ||
        (REG_PC >= 0xc000 && GetSectionPage(Section::D) != ROM1))
        return false;

    // If a drive object exists, clean up after our boot attempt, whether or not it worked
    pBootDrive.reset();

    // Read the error code after the RST 8 opcode
    uint8_t bErrCode = read_byte(REG_PC);

    switch (bErrCode)
    {
        // No error
    case 0x00:
        break;

        // Copyright message
    case 0x50:
        // Forced boot on startup?
        if (g_nAutoLoad != AUTOLOAD_NONE)
        {
            AutoLoad(g_nAutoLoad, false);
            g_nAutoLoad = AUTOLOAD_NONE;
        }
        break;

        // "NO DOS" or "Loading error"
    case 0x35:
    case 0x13:
        // Is automagical DOS booting enabled?
        if (GetOption(dosboot))
        {
            // If there's a custom boot disk, load it read-only
            auto disk = Disk::Open(GetOption(dosdisk), true);

            // Fall back on the built-in SAMDOS2 image
            if (!disk)
                disk = Disk::Open(abSAMDOS, sizeof(abSAMDOS), "mem:SAMDOS.sbt");

            if (disk)
            {
                // Create a private drive for the DOS disk
                pBootDrive = std::make_unique<Drive>(std::move(disk));

                // Jump back to BOOTEX to try again
                REG_PC = 0xd8e5;
                return true;
            }
        }
        break;

    default:
        // Stop auto-typing on any other error code
        Keyin::Stop();
        break;
    }

    // Continue with RST
    return false;
}

bool Rst48Hook()
{
    // Are we at READKEY in ROM0?
    if (REG_PC == 0x1cb2 && GetSectionPage(Section::A) == ROM0)
    {
        // If we have auto-type input, skip the startup screen
        if (Keyin::IsTyping())
            IsAtStartupScreen(true);
    }

    // Continue with RST
    return false;
}

} // namespace IO
