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

//////////////////////////////////////////////////////////////////////////////

namespace IO
{
IoState m_state{};

uint16_t last_in_port, last_out_port;
uint8_t last_in_val, last_out_val;

auto auto_load = AutoLoadType::None;
bool mid_frame_change;

std::array<uint8_t, 9> key_matrix;

#ifdef _DEBUG
std::array<uint8_t, 32> unhandled_ports;
#endif


bool Init()
{
    bool fRet = true;
    Exit(true);

    m_state.lepr = m_state.hepr = m_state.lpen = m_state.border = 0;
    m_state.keyboard = KEYBOARD_EAR_MASK;
    m_state.status_reg = 0xff;

    out_lmpr(0);
    out_hmpr(0);
    out_vmpr(0);

    key_matrix.fill(0xff);

    if (!pFloppy1)
    {
        pFloppy1 = std::make_unique<Drive>();
        pFloppy2 = std::make_unique<Drive>();
        pAtom = std::make_unique<AtomDevice>();
        pAtomLite = std::make_unique<AtomLiteDevice>();
        pSDIDE = std::make_unique<SDIDEDevice>();

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

        pDallas->LoadState(OSD::MakeFilePath(PathType::Settings, "dallas"));

        pFloppy1->Insert(GetOption(disk1));
        pFloppy2->Insert(GetOption(disk2));

        Tape::Insert(GetOption(tape));

        auto& pActiveAtom = (GetOption(drive2) == drvAtom) ? pAtom : pAtomLite;
        pActiveAtom->Attach(GetOption(atomdisk0), 0);
        pActiveAtom->Attach(GetOption(atomdisk1), 1);

        pSDIDE->Attach(GetOption(sdidedisk), 0);
    }

    if (GetOption(asicdelay))
    {
        m_state.asic_asleep = true;
        AddCpuEvent(EventType::AsicReady, g_dwCycleCounter + CPU_CYCLES_ASIC_STARTUP);
    }

    pDAC->Reset();
    pSID->Reset();
    pSampler->Reset();
    pVoiceBox->Reset();

    pFloppy1->Reset();
    pFloppy2->Reset();
    pAtom->Reset();
    pAtomLite->Reset();
    pSDIDE->Reset();

    return fRet;
}

void Exit(bool reinit)
{
    if (!reinit)
    {
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

constexpr IoState& State()
{
    return m_state;
}

////////////////////////////////////////////////////////////////////////////////

static inline void UpdatePaging()
{
    // ROM0 or internal RAM in section A
    if (!(m_state.lmpr & LMPR_ROM0_OFF))
        PageIn(Section::A, ROM0);
    else
        PageIn(Section::A, m_state.lmpr & LMPR_PAGE_MASK);

    // Internal RAM in section B
    PageIn(Section::B, (m_state.lmpr + 1) & LMPR_PAGE_MASK);

    // External RAM or internal RAM in section C
    if (m_state.hmpr & HMPR_MCNTRL_MASK)
        PageIn(Section::C, EXTMEM + m_state.lepr);
    else
        PageIn(Section::C, m_state.hmpr & HMPR_PAGE_MASK);

    // External RAM, ROM1, or internal RAM in section D
    if (m_state.hmpr & HMPR_MCNTRL_MASK)
        PageIn(Section::D, EXTMEM + m_state.hepr);
    else if (m_state.lmpr & LMPR_ROM1)
        PageIn(Section::D, ROM1);
    else
        PageIn(Section::D, (m_state.hmpr + 1) & HMPR_PAGE_MASK);
}

static uint8_t update_lpen()
{
    if (!ScreenDisabled())
    {
        int line = g_dwCycleCounter / CPU_CYCLES_PER_LINE, line_cycle = g_dwCycleCounter % CPU_CYCLES_PER_LINE;

        if (IsScreenLine(line) && line_cycle >= (CPU_CYCLES_PER_SIDE_BORDER + CPU_CYCLES_PER_SIDE_BORDER))
        {
            auto [b0, b1, b2, b3] = Frame::GetAsicData();

            auto xpos = static_cast<uint8_t>(line_cycle - (CPU_CYCLES_PER_SIDE_BORDER + CPU_CYCLES_PER_SIDE_BORDER));
            auto bcd1 = (line_cycle < (CPU_CYCLES_PER_SIDE_BORDER + CPU_CYCLES_PER_SIDE_BORDER)) ? (m_state.border & 1) : (b0 & 1);
            m_state.lpen = (xpos & 0xfc) | (m_state.lpen & LPEN_TXFMST) | bcd1;
        }
        else
        {
            m_state.lpen = (m_state.lpen & LPEN_TXFMST) | (m_state.border & 1);
        }
    }
    else
    {
        m_state.lpen = (m_state.lpen & ~LPEN_BORDER_BCD0) | (m_state.border & 1);
    }

    return m_state.lpen;
}

static uint8_t update_hpen()
{
    if (!ScreenDisabled())
    {
        int line = g_dwCycleCounter / CPU_CYCLES_PER_LINE, line_cycle = g_dwCycleCounter % CPU_CYCLES_PER_LINE;

        if (IsScreenLine(line) && (line != TOP_BORDER_LINES || line_cycle >= (CPU_CYCLES_PER_SIDE_BORDER + CPU_CYCLES_PER_SIDE_BORDER)))
            m_state.hpen = line - TOP_BORDER_LINES;
        else
            m_state.hpen = GFX_SCREEN_LINES;
    }

    return m_state.hpen;
}

void out_lmpr(uint8_t val)
{
    m_state.lmpr = val;
    UpdatePaging();
}

void out_hmpr(uint8_t val)
{
    if ((m_state.vmpr & VMPR_MODE_MASK) == VMPR_MODE_3 &&
        ((m_state.hmpr ^ val) & HMPR_MD3COL_MASK))
    {

        Frame::Update();
    }

    m_state.hmpr = val;
    UpdatePaging();
}

void out_vmpr(uint8_t val)
{
    Frame::ModeChanged(val);

    if ((m_state.vmpr ^ val) & (VMPR_MODE_MASK | VMPR_PAGE_MASK))
    {
        auto [line, line_cycle] = Frame::GetRasterPos(g_dwCycleCounter);
        mid_frame_change |= IsScreenLine(line);
    }

    m_state.vmpr = val & (VMPR_MODE_MASK | VMPR_PAGE_MASK);
}

void out_lepr(uint8_t val)
{
    m_state.lepr = val;
    UpdatePaging();
}

void out_hepr(uint8_t val)
{
    m_state.hepr = val;
    UpdatePaging();
}


void out_clut(uint16_t port, uint8_t val)
{
    auto clut_index = port & (NUM_CLUT_REGS - 1);
    auto palette_index = val & (NUM_PALETTE_COLOURS - 1);

    if (m_state.clut[clut_index] != palette_index)
    {
        auto [line, line_cycle] = Frame::GetRasterPos(g_dwCycleCounter);
        if (IsScreenLine(line))
            mid_frame_change = true;

        Frame::Update();
        m_state.clut[clut_index] = palette_index;
    }
}


uint8_t In(uint16_t port)
{
    uint8_t port_low = port & 0xff;
    uint8_t port_high = port >> 8;
    last_in_port = port;

    CheckCpuEvents();

    if (port_low >= BASE_ASIC_PORT && m_state.asic_asleep)
        return last_in_val = 0x00;

    uint8_t ret = 0xff;
    switch (port_low)
    {
    case KEYBOARD_PORT:
    {
        Tape::InFEHook();

        g_nTurbo &= ~TURBO_BOOT;

        if (port_high == 0xff)
        {
            ret = key_matrix[8];

            if (GetOption(mouse))
                ret &= pMouse->In(port);
        }
        else
        {
            if (!(port_high & 0x80)) ret &= key_matrix[7];
            if (!(port_high & 0x40)) ret &= key_matrix[6];
            if (!(port_high & 0x20)) ret &= key_matrix[5];
            if (!(port_high & 0x10)) ret &= key_matrix[4];
            if (!(port_high & 0x08)) ret &= key_matrix[3];
            if (!(port_high & 0x04)) ret &= key_matrix[2];
            if (!(port_high & 0x02)) ret &= key_matrix[1];
            if (!(port_high & 0x01)) ret &= key_matrix[0];
        }

        ret &= KEYBOARD_KEY_MASK;
        ret |= (m_state.border & BORDER_SOFF_MASK);
        ret |= (m_state.keyboard & (KEYBOARD_EAR_MASK | KEYBOARD_SPEN_MASK));
        break;
    }

    case STATUS_PORT:
    {
        if (!(port_high & 0x80)) ret &= key_matrix[7];
        if (!(port_high & 0x40)) ret &= key_matrix[6];
        if (!(port_high & 0x20)) ret &= key_matrix[5];
        if (!(port_high & 0x10)) ret &= key_matrix[4];
        if (!(port_high & 0x08)) ret &= key_matrix[3];
        if (!(port_high & 0x04)) ret &= key_matrix[2];
        if (!(port_high & 0x02)) ret &= key_matrix[1];
        if (!(port_high & 0x01)) ret &= key_matrix[0];

        ret = (ret & 0xe0) | (m_state.status_reg & 0x1f);
        break;
    }

    case LMPR_PORT:
        ret = m_state.lmpr;
        break;

    case HMPR_PORT:
        ret = m_state.hmpr;
        break;

    case VMPR_PORT:
        ret = VMPR_RXMIDI_MASK | m_state.vmpr;
        break;

    case CLOCK_PORT:
        if (port < 0xfe00 && GetOption(sambusclock))
            ret = pSambus->In(port);
        else if (port >= 0xfe00 && GetOption(dallasclock))
            ret = pDallas->In(port);
        break;

    case LPEN_PORT:
        if ((port & PEN_PORT_MASK) == LPEN_PORT)
            ret = update_lpen();
        else
            ret = update_hpen();
        break;

    case ATTR_PORT:
    {
        if (!ScreenDisabled())
        {
            auto [b0, b1, b2, b3] = Frame::GetAsicData();
            m_state.attr = b2;
        }

        ret = m_state.attr;
        break;
    }

    case PRINTL1_STAT_PORT:
    case PRINTL1_DATA_PORT:
    {
        switch (GetOption(parallel1))
        {
        case 1: ret = pPrinterFile->In(port); break;
        case 2: ret = pMonoDac->In(port); break;
        case 3: ret = pStereoDac->In(port); break;
        }
        break;
    }

    case PRINTL2_STAT_PORT:
    case PRINTL2_DATA_PORT:
    {
        switch (GetOption(parallel2))
        {
        case 1: ret = pPrinterFile->In(port); break;
        case 2: ret = pMonoDac->In(port); break;
        case 3: ret = pStereoDac->In(port); break;
        }
        break;
    }

    case MIDI_PORT:
        if (GetOption(midi) == 1)
            ret = pMidi->In(port);
        break;

    case SDIDE_REG_PORT:
    case SDIDE_DATA_PORT:
        ret = pSDIDE->In(port);
        break;

    case KEMPSTON_PORT:
        if (GetOption(joytype1) == jtKempston) ret &= ~Joystick::ReadKempston(0);
        if (GetOption(joytype2) == jtKempston) ret &= ~Joystick::ReadKempston(1);
        break;

    case BLUE_ALPHA_PORT:
        if (GetOption(voicebox) && port == BA_VOICEBOX_PORT)
        {
            ret = pVoiceBox->In(port);
        }
        else if (GetOption(dac7c) == 1 && (port & BA_SAMPLER_MASK) == BA_SAMPLER_BASE)
        {
            ret = pSampler->In(port_high & 0x03);
        }
        break;

    case SID_PORT:
    case QUAZAR_PORT:
        break;

    default:
        if ((port & FLOPPY_MASK) == FLOPPY1_BASE)
        {
            switch (GetOption(drive1))
            {
            case drvFloppy: ret = (pBootDrive ? pBootDrive : pFloppy1)->In(port); break;
            default: break;
            }
        }

        else if ((port & FLOPPY_MASK) == FLOPPY2_BASE)
        {
            switch (GetOption(drive2))
            {
            case drvFloppy:     ret = pFloppy2->In(port); break;
            case drvAtom:       ret = pAtom->In(port); break;
            case drvAtomLite:   ret = pAtomLite->In(port); break;
            }
        }

#ifdef _DEBUG
        else
        {
            auto index = port_low >> 3;
            auto bitmask = 1 << (port_low & 7);

            if (!(unhandled_ports[index] & bitmask))
            {
                Message(MsgType::Warning, "Unhandled read from port {:04x}\n", port);
                unhandled_ports[index] |= bitmask;
                debug_break = true;
            }
        }
#endif
    }

    last_in_val = ret;
    return ret;
}


void Out(uint16_t port, uint8_t val)
{
    auto port_low = port & 0xff;
    auto port_high = port >> 8;

    last_out_port = port;
    last_out_val = val;

    CheckCpuEvents();

    if (port_low >= BASE_ASIC_PORT && m_state.asic_asleep)
        return;

    switch (port_low)
    {
    case BORDER_PORT:
    {
        bool soff_change = ((m_state.border ^ val) & BORDER_SOFF_MASK) && (m_state.vmpr & VMPR_MDE1_MASK);
        bool colour_change = ((m_state.border ^ val) & BORDER_COLOUR_MASK) != 0;

        if (soff_change || colour_change)
            Frame::Update();

        if (soff_change)
        {
            if (m_state.border & BORDER_SOFF_MASK)
            {
                Frame::BorderChanged(val);
            }
            else
            {
                update_lpen();
                update_hpen();

                auto [b0, b1, b2, b3] = Frame::GetAsicData();
                m_state.attr = b2;
            }
        }

        if ((m_state.border ^ val) & BORDER_BEEP_MASK)
            pBeeper->Out(port, val);

        m_state.border = val;

        if (soff_change)
            CPU::UpdateContention(CPU::IsContentionActive());
    }
    break;

    case VMPR_PORT:
    {
        auto vmpr_changes = m_state.vmpr ^ val;

        if (vmpr_changes & VMPR_MODE_MASK)
        {
            if ((m_state.vmpr | val) & VMPR_MDE1_MASK)
            {
                Frame::Update();
                out_vmpr((val & VMPR_MODE_MASK) | (m_state.vmpr & ~VMPR_MODE_MASK));
            }
            else
            {
                g_dwCycleCounter += CPU_CYCLES_PER_CELL;
                Frame::Update();
                g_dwCycleCounter -= CPU_CYCLES_PER_CELL;

                out_vmpr(val);
            }

            CPU::UpdateContention(CPU::IsContentionActive());
        }

        if (vmpr_changes & VMPR_PAGE_MASK)
        {
            g_dwCycleCounter += CPU_CYCLES_PER_CELL;
            Frame::Update();
            g_dwCycleCounter -= CPU_CYCLES_PER_CELL;

            out_vmpr(val);
        }
    }
    break;

    case HMPR_PORT:
        if (m_state.hmpr != val)
            out_hmpr(val);
        break;

    case LMPR_PORT:
        if (m_state.lmpr != val)
            out_lmpr(val);
        break;

    case CLOCK_PORT:
        if (port < 0xfe00 && GetOption(sambusclock))
            pSambus->Out(port, val);
        else if (port >= 0xfe00 && GetOption(dallasclock))
            pDallas->Out(port, val);
        break;

    case CLUT_BASE_PORT:
        out_clut(port_high, val);
        break;

    case HEPR_PORT:
        out_hepr(val);
        break;

    case LEPR_PORT:
        out_lepr(val);
        break;

    case LINE_PORT:
        if (m_state.line_int != val)
        {
            if (m_state.line_int < GFX_SCREEN_LINES)
            {
                CancelCpuEvent(EventType::LineInterrupt);
                m_state.status_reg |= STATUS_INT_LINE;
            }

            m_state.line_int = val;

            if (m_state.line_int < GFX_SCREEN_LINES)
            {
                uint32_t dwLineTime = (m_state.line_int + TOP_BORDER_LINES) * CPU_CYCLES_PER_LINE;

                // Schedule the line interrupt (could be active now, or already passed this frame)
                AddCpuEvent(EventType::LineInterrupt, dwLineTime);
            }
        }
        break;

    case SAA_PORT:
        pSAA->Out(port, val);
        break;

    case PRINTL1_STAT_PORT:
    case PRINTL1_DATA_PORT:
        switch (GetOption(parallel1))
        {
        case 1: return pPrinterFile->Out(port, val);
        case 2: return pMonoDac->Out(port, val);
        case 3: return pStereoDac->Out(port, val);
        }
        break;

    case PRINTL2_STAT_PORT:
    case PRINTL2_DATA_PORT:
        switch (GetOption(parallel2))
        {
        case 1: return pPrinterFile->Out(port, val);
        case 2: return pMonoDac->Out(port, val);
        case 3: return pStereoDac->Out(port, val);
        }
        break;

    case MIDI_PORT:
        if (!(m_state.lpen & LPEN_TXFMST))
        {
            m_state.lpen |= LPEN_TXFMST;
            AddCpuEvent(EventType::MidiOutStart, g_dwCycleCounter +
                A_ROUND(MIDI_TRANSMIT_TIME + 16, 32) - 16 - 32 - MIDI_INT_ACTIVE_TIME + 1);

            if (GetOption(midi) == 1)
                pMidi->Out(port, val);
        }
        break;

    case SDIDE_REG_PORT:
    case SDIDE_DATA_PORT:
        pSDIDE->Out(port, val);
        break;

    case SID_PORT:
        if (GetOption(sid))
            pSID->Out(port, val);
        break;

    case QUAZAR_PORT:
        break;

    default:
    {
        if ((port & FLOPPY_MASK) == FLOPPY1_BASE)
        {
            switch (GetOption(drive1))
            {
            case drvFloppy: (pBootDrive ? pBootDrive : pFloppy1)->Out(port, val); break;
            default: break;
            }
        }

        else if ((port & FLOPPY_MASK) == FLOPPY2_BASE)
        {
            TRACE("PORT OUT({:02x}) wrote {:02x}\n", port, val);

            switch (GetOption(drive2))
            {
            case drvFloppy:     pFloppy2->Out(port, val);  break;
            case drvAtom:       pAtom->Out(port, val);     break;
            case drvAtomLite:   pAtomLite->Out(port, val); break;
            }
        }
        else if (port == BA_VOICEBOX_PORT)
        {
            pVoiceBox->Out(0, val);
        }

        // Blue Alpha, SAMVox and Paula ports overlap!
        else if ((port_low & 0xfc) == 0x7c)
        {
            switch (GetOption(dac7c))
            {
            case 1:
                if ((port & BA_SAMPLER_MASK) == BA_SAMPLER_BASE)
                {
                    pSampler->Out(port_high & 0x03, val);
                }
                break;

            case 2:
                pSAMVox->Out(port_low & 0x03, val);
                break;

            case 3:
                pPaula->Out(port_low & 0x01, val);
                break;
            }
        }

#ifdef _DEBUG
        else
        {
            auto index = port_low >> 3;
            auto bitmask = 1 << (port_low & 7);

            if (!(unhandled_ports[index] & bitmask))
            {
                Message(MsgType::Warning, "Unhandled write to port {:04x}, value = {:02x}\n", port, val);
                unhandled_ports[index] |= bitmask;
                debug_break = true;
            }
        }
#endif
    }
    }
}

bool ScreenDisabled()
{
    return (m_state.border & BORDER_SOFF_MASK) && ScreenMode3or4();
}

bool ScreenMode3or4()
{
    return (m_state.vmpr & VMPR_MDE1_MASK) != 0;
}

int ScreenMode()
{
    return ((m_state.vmpr & VMPR_MODE_MASK) >> VMPR_MODE_SHIFT) + 1;
}

int VisibleScreenPage()
{
    if ((m_state.vmpr & VMPR_MODE_MASK) >= VMPR_MODE_3)
        return m_state.vmpr & (VMPR_PAGE_MASK & ~1);

    return m_state.vmpr & VMPR_PAGE_MASK;
}

uint8_t Mode3Clut(int index)
{
    static std::array<int, 4> mode3_mapping{ 0, 2, 1, 3 };
    return m_state.clut[mode3_mapping[index]];
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

    key_matrix = Keyboard::key_matrix;
}

std::vector<COLOUR> Palette()
{
    std::vector<COLOUR> palette(NUM_PALETTE_COLOURS);

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

bool TestStartupScreen(bool skip_startup)
{
    for (int i = 0; i < 20; i += 2)
    {
        // Check for 0f78 on stack, with previous location pointing at JR Z,-5
        if (read_word(REG_SP + i + 2) == 0x0f78 && read_word(read_word(REG_SP + i)) == 0xfb28)
        {
            // Optionally skip JR to exit WTFK loop at copyright message
            if (skip_startup)
                write_word(REG_SP + i, read_word(REG_SP + i) + 2);

            return true;
        }
    }

    return false;
}

void SetAutoLoad(AutoLoadType type)
{
    auto_load = type;
}

void AutoLoad(AutoLoadType type, bool fOnlyAtStartup_/*=true*/)
{
    if (!GetOption(autoload) || (fOnlyAtStartup_ && !TestStartupScreen()))
        return;

    if (type == AutoLoadType::Disk)
        Keyin::String("\xc9", false);
    else if (type == AutoLoadType::Tape)
        Keyin::String("\xc7", false);
}

bool EiHook()
{
    // If we're leaving the ROM interrupt handler, inject any auto-typing input
    if (REG_PC == 0x005a && GetSectionPage(Section::A) == ROM0)
        Keyin::Next();

    Tape::EiHook();
    return false;
}

bool Rst8Hook()
{
    if ((REG_PC < 0x4000 && GetSectionPage(Section::A) != ROM0) ||
        (REG_PC >= 0xc000 && GetSectionPage(Section::D) != ROM1))
    {
        return false;
    }

    // If a drive object exists, clean up after our boot attempt, whether or not it worked
    pBootDrive.reset();

    switch (read_byte(REG_PC))
    {
    // No error
    case 0x00:
        break;

    // Copyright message
    case 0x50:
        // Forced boot on startup?
        if (auto_load != AutoLoadType::None)
        {
            AutoLoad(auto_load, false);
            auto_load = AutoLoadType::None;
        }
        break;

    // "NO DOS" or "Loading error"
    case 0x35:
    case 0x13:
        if (GetOption(dosboot))
        {
            auto disk = Disk::Open(GetOption(dosdisk), true);
            if (!disk)
                disk = Disk::Open(abSAMDOS, sizeof(abSAMDOS), "mem:SAMDOS.sbt");

            if (disk)
            {
                pBootDrive = std::make_unique<Drive>(std::move(disk));

                // Jump back to BOOTEX to try again
                REG_PC = 0xd8e5;
                return true;
            }
        }
        break;

    default:
        Keyin::Stop();
        break;
    }

    return false;
}

bool Rst48Hook()
{
    // Are we at READKEY in ROM0?
    if (REG_PC == 0x1cb2 && GetSectionPage(Section::A) == ROM0)
    {
        if (Keyin::IsTyping())
            TestStartupScreen(true);
    }

    return false;
}

} // namespace IO
