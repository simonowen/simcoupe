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
#include "Events.h"
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

auto auto_load = AutoLoadType::None;
bool mid_frame_change = false;
bool flash_phase = false;

std::array<uint8_t, 9> key_matrix;

#ifdef _DEBUG
std::array<uint8_t, 32> unhandled_ports{};
bool is_unhandled_port(uint16_t port) { return (unhandled_ports[(port >> 3) & 0x1f] & (1U << (port & 7))) == 0; }
void mark_unhandled_port(uint16_t port) { unhandled_ports[(port >> 3) & 0x1f] |= (1U << (port & 7)); }
#endif

constexpr uint32_t A_ROUND(uint32_t frame_cycles, int add_cycles, int power_of_2) {
    return Round(frame_cycles + add_cycles, power_of_2) - frame_cycles;
}

bool Init()
{
    bool fRet = true;
    Exit(true);

    m_state.lpen = 0;
    m_state.keyboard = KEYBOARD_EAR_MASK;
    m_state.status = 0xff;

    out_lepr(0);
    out_hepr(0);
    out_lmpr(0);
    out_hmpr(0);
    out_vmpr(0);
    out_border(0);

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

        UpdateDrives();
        Tape::Insert(GetOption(tape));
    }

    if (GetOption(asicdelay))
    {
        m_state.asic_asleep = true;
        AddEvent(EventType::AsicReady, CPU::frame_cycles + CPU_CYCLES_ASIC_STARTUP);
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

#ifdef _DEBUG
    mark_unhandled_port(0); // KEDisk bug
    mark_unhandled_port(QUAZAR_PORT);
#endif

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

IoState& State()
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
        auto line = CPU::frame_cycles / CPU_CYCLES_PER_LINE;
        auto line_cycle = CPU::frame_cycles % CPU_CYCLES_PER_LINE;

        if (IsScreenLine(line) && line_cycle >= (CPU_CYCLES_PER_SIDE_BORDER + CPU_CYCLES_PER_SIDE_BORDER))
        {
            auto [b0, b1, b2, b3] = Frame::GetAsicData();

            uint8_t clut_bcd1{};
            switch (ScreenMode())
            {
            case 1:
            case 2:
            {
                uint8_t ink_bit = (b0 & 0x40) ? 1 : 0;
                uint8_t flash_reverse = ((b2 & 0x80) && flash_phase) ? 1 : 0;
                uint8_t clut_idx = (b2 >> ((ink_bit ^ flash_reverse) ? 0 : 3)) & 7;
                clut_bcd1 = clut_idx & 1;
                break;
            }
            case 3:
                clut_bcd1 = ((b0 >> 1) | (b0 >> 3)) & 1;
                break;
            case 4:
                clut_bcd1 = b0 & 1;
                break;
            }

            auto xpos = static_cast<uint8_t>(line_cycle - (CPU_CYCLES_PER_SIDE_BORDER + CPU_CYCLES_PER_SIDE_BORDER));
            auto bcd1 = (line_cycle < (CPU_CYCLES_PER_SIDE_BORDER + CPU_CYCLES_PER_SIDE_BORDER)) ? (m_state.border & 1) : (clut_bcd1 & 1);
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
        auto line = CPU::frame_cycles / CPU_CYCLES_PER_LINE;
        auto line_cycle = CPU::frame_cycles % CPU_CYCLES_PER_LINE;

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
        auto [line, line_cycle] = Frame::GetRasterPos(CPU::frame_cycles);
        mid_frame_change |= IsScreenLine(line);
    }

    m_state.vmpr = val & (VMPR_MODE_MASK | VMPR_PAGE_MASK);
    Memory::UpdateContention();
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
        auto [line, line_cycle] = Frame::GetRasterPos(CPU::frame_cycles);
        if (IsScreenLine(line))
            mid_frame_change = true;

        Frame::Update();
        m_state.clut[clut_index] = palette_index;
    }
}

void out_border(uint8_t val)
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
        pBeeper->Out(BORDER_PORT, val);

    m_state.border = val;

    if (soff_change)
        Memory::UpdateContention();
}

uint8_t In(uint16_t port)
{
    uint8_t port_low = port & 0xff;
    uint8_t port_high = port >> 8;

    CheckEvents(CPU::frame_cycles);

    if (port_low >= BASE_ASIC_PORT && m_state.asic_asleep)
        return 0x00;

    switch (port_low)
    {
    case KEYBOARD_PORT:
    {
        Tape::InFEHook();

        auto keys = KEYBOARD_KEY_MASK;
        if (port_high == 0xff)
        {
            keys &= key_matrix[8];
            if (GetOption(mouse))
                keys &= pMouse->In(port);
        }
        else
        {
            if (!(port_high & 0x80)) keys &= key_matrix[7];
            if (!(port_high & 0x40)) keys &= key_matrix[6];
            if (!(port_high & 0x20)) keys &= key_matrix[5];
            if (!(port_high & 0x10)) keys &= key_matrix[4];
            if (!(port_high & 0x08)) keys &= key_matrix[3];
            if (!(port_high & 0x04)) keys &= key_matrix[2];
            if (!(port_high & 0x02)) keys &= key_matrix[1];
            if (!(port_high & 0x01)) keys &= key_matrix[0];
        }

        return keys |
            (m_state.border & BORDER_SOFF_MASK) |
            (m_state.keyboard & (KEYBOARD_EAR_MASK | KEYBOARD_SPEN_MASK));
    }

    case STATUS_PORT:
    {
        auto keys = STATUS_KEY_MASK;
        if (!(port_high & 0x80)) keys &= key_matrix[7];
        if (!(port_high & 0x40)) keys &= key_matrix[6];
        if (!(port_high & 0x20)) keys &= key_matrix[5];
        if (!(port_high & 0x10)) keys &= key_matrix[4];
        if (!(port_high & 0x08)) keys &= key_matrix[3];
        if (!(port_high & 0x04)) keys &= key_matrix[2];
        if (!(port_high & 0x02)) keys &= key_matrix[1];
        if (!(port_high & 0x01)) keys &= key_matrix[0];

        return keys | (m_state.status & 0x1f);
    }

    case LMPR_PORT:
        return m_state.lmpr;

    case HMPR_PORT:
        return m_state.hmpr;

    case VMPR_PORT:
        return VMPR_RXMIDI_MASK | m_state.vmpr;

    case CLOCK_PORT:
        if (port < 0xfe00 && GetOption(sambusclock))
            return pSambus->In(port);
        else if (port >= 0xfe00 && GetOption(dallasclock))
            return pDallas->In(port);
        break;

    case LPEN_PORT:
        if ((port & PEN_PORT_MASK) == LPEN_PORT)
            return update_lpen();

        return update_hpen();

    case ATTR_PORT:
        if (!ScreenDisabled())
        {
            auto [b0, b1, b2, b3] = Frame::GetAsicData();
            m_state.attr = b2;
        }
        return m_state.attr;

    case PRINTL1_STAT_PORT:
    case PRINTL1_DATA_PORT:
        switch (GetOption(parallel1))
        {
        case 1: return pPrinterFile->In(port); break;
        case 2: return pMonoDac->In(port); break;
        case 3: return pStereoDac->In(port); break;
        }
        break;

    case PRINTL2_STAT_PORT:
    case PRINTL2_DATA_PORT:
        switch (GetOption(parallel2))
        {
        case 1: return pPrinterFile->In(port); break;
        case 2: return pMonoDac->In(port); break;
        case 3: return pStereoDac->In(port); break;
        }
        break;

    case MIDI_PORT:
        if (GetOption(midi) == 1)
            return pMidi->In(port);
        break;

    case SDIDE_REG_PORT:
    case SDIDE_DATA_PORT:
        return pSDIDE->In(port);

    case KEMPSTON_PORT:
    {
        auto kempston = 0xff;
        if (GetOption(joytype1) == jtKempston) kempston &= ~Joystick::ReadKempston(0);
        if (GetOption(joytype2) == jtKempston) kempston &= ~Joystick::ReadKempston(1);
        return kempston;
    }

    case BLUE_ALPHA_PORT:
        if (GetOption(voicebox) && port == BA_VOICEBOX_PORT)
            return pVoiceBox->In(port);

        if (GetOption(dac7c) == 1 && (port & BA_SAMPLER_MASK) == BA_SAMPLER_BASE)
            return pSampler->In(port_high & 0x03);
        break;

    default:
        if ((port & FLOPPY_MASK) == FLOPPY1_BASE)
        {
            switch (GetOption(drive1))
            {
            case drvFloppy: return (pBootDrive ? pBootDrive : pFloppy1)->In(port); break;
            default: break;
            }
        }

        else if ((port & FLOPPY_MASK) == FLOPPY2_BASE)
        {
            switch (GetOption(drive2))
            {
            case drvFloppy:     return pFloppy2->In(port); break;
            case drvAtom:       return pAtom->In(port); break;
            case drvAtomLite:   return pAtomLite->In(port); break;
            }
        }
    }

#ifdef _DEBUG
    if (is_unhandled_port(port))
    {
        Message(MsgType::Warning, "Unhandled read from port {:04x}\n", port);
        mark_unhandled_port(port);
        debug_break = true;
    }
#endif

    auto line = CPU::frame_cycles / CPU_CYCLES_PER_LINE;
    auto line_cycle = CPU::frame_cycles % CPU_CYCLES_PER_LINE;
    if (IsScreenLine(line) && line_cycle >= (CPU_CYCLES_PER_SIDE_BORDER + CPU_CYCLES_PER_SIDE_BORDER))
    {
        auto [b0, b1, b2, b3] = Frame::GetAsicData();
        return b2;
    }

    return 0xff;
}


void Out(uint16_t port, uint8_t val)
{
    auto port_low = port & 0xff;
    auto port_high = port >> 8;

    CheckEvents(CPU::frame_cycles);

    if (port_low >= BASE_ASIC_PORT && m_state.asic_asleep)
        return;

    switch (port_low)
    {
    case BORDER_PORT:
        if (m_state.border != val)
            out_border(val);
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
                CPU::frame_cycles += CPU_CYCLES_PER_CELL;
                Frame::Update();
                CPU::frame_cycles -= CPU_CYCLES_PER_CELL;

                out_vmpr(val);
            }
        }

        if (vmpr_changes & VMPR_PAGE_MASK)
        {
            CPU::frame_cycles += CPU_CYCLES_PER_CELL;
            Frame::Update();
            CPU::frame_cycles -= CPU_CYCLES_PER_CELL;

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
        if (m_state.line != val)
        {
            if (m_state.line < GFX_SCREEN_LINES)
            {
                CancelEvent(EventType::LineInterrupt);
                CancelEvent(EventType::LineInterruptEnd);
                m_state.status |= STATUS_INT_LINE;
            }

            m_state.line = val;

            if (m_state.line < GFX_SCREEN_LINES)
            {
                auto line_int_time = (m_state.line + TOP_BORDER_LINES) * CPU_CYCLES_PER_LINE;
                AddEvent(EventType::LineInterrupt, line_int_time);
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
            auto midi_int_time = CPU::frame_cycles +
                A_ROUND(CPU::frame_cycles, MIDI_TRANSMIT_TIME + 16, 32) - 16 - 32 - MIDI_INT_ACTIVE_TIME + 1;
            AddEvent(EventType::MidiOutStart, midi_int_time);

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
        else if (is_unhandled_port(port))
        {
            Message(MsgType::Warning, "Unhandled write to port {:04x}, value = {:02x}\n", port, val);
            mark_unhandled_port(port);
            debug_break = true;
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
    IO::mid_frame_change = false;

    static uint8_t flash_frame = 0;
    if (!(++flash_frame % MODE12_FLASH_FRAMES))
        flash_phase = !flash_phase;

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

void UpdateDrives()
{
    pFloppy1->Eject();
    pFloppy2->Eject();
    pAtom->Detach();
    pAtomLite->Detach();
    pSDIDE->Detach();

    switch (GetOption(drive1))
    {
    case drvFloppy:
        if (!pFloppy1->Insert(GetOption(disk1)))
            Message(MsgType::Warning, "Failed to insert disk 1:\n\n{}", GetOption(disk1));
        break;
    default:
        break;
    }

    switch (GetOption(drive2))
    {
    case drvFloppy:
        if (!pFloppy2->Insert(GetOption(disk2)))
            Message(MsgType::Warning, "Failed to insert disk 2:\n\n{}", GetOption(disk2));
        break;
    case drvAtom:
        if (!pAtom->Attach(GetOption(atomdisk0), 0))
            Message(MsgType::Warning, "Failed to attach Atom disk:\n\n{}", GetOption(atomdisk0));
        if (!pAtom->Attach(GetOption(atomdisk1), 1))
            Message(MsgType::Warning, "Failed to attach Atom disk:\n\n{}", GetOption(atomdisk1));
        break;
    case drvAtomLite:
        if (!pAtomLite->Attach(GetOption(atomdisk0), 0))
            Message(MsgType::Warning, "Failed to attach AtomLite disk:\n\n{}", GetOption(atomdisk0));
        if (!pAtomLite->Attach(GetOption(atomdisk1), 1))
            Message(MsgType::Warning, "Failed to attach AtomLite disk:\n\n{}", GetOption(atomdisk1));
        break;
    default:
        break;
    }

    pSDIDE->Attach(GetOption(sdidedisk), 0);
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
    constexpr auto max_stack_slots = 15;
    const auto wtfk_addr = rom_hook_addr(RomHook::WTFK);

    for (int i = 0; i < max_stack_slots; ++i)
    {
        auto stack_addr = cpu.get_sp() + i * 2;
        auto addr = read_word(stack_addr);

        if (addr == wtfk_addr)
        {
            // Optionally skip JR to exit WTFK loop at copyright message
            if (skip_startup)
                write_word(stack_addr, addr + 2);

            return true;
        }
    }

    return false;
}

void QueueAutoLoad(AutoLoadType type)
{
    auto_load = type;
}

void AutoLoad(AutoLoadType type)
{
    auto_load = AutoLoadType::None;

    if (!GetOption(autoload) || type == AutoLoadType::None || !TestStartupScreen())
    {
        Keyin::Stop();
        return;
    }

    if (type == AutoLoadType::Disk)
        Keyin::String("\xc9", false);
    else if (type == AutoLoadType::Tape)
        Keyin::String("\xc7", false);
}

void EiHook()
{
    // If we're leaving the ROM interrupt handler, inject any auto-typing input
    if (cpu.get_pc() == rom_hook_addr(RomHook::IMEXIT))
    {
        if (Keyin::IsTyping())
        {
            TestStartupScreen(true);
            Keyin::Next();
        }
    }

    Tape::EiHook();
}

bool Rst8Hook()
{
    if (AddrPage(cpu.get_pc()) != ROM0 && AddrPage(cpu.get_pc()) != ROM1)
        return false;

    // If a drive object exists, clean up after our boot attempt, whether or not it worked
    pBootDrive.reset();

    switch (read_byte(cpu.get_pc()))
    {
    // No error
    case 0x00:
        break;

    // "NO DOS" or "Loading error"
    case 0x35:
    case 0x13:
        if (GetOption(dosboot))
        {
            const auto bootnr = rom_hook_addr(RomHook::BOOTNR);
            if (!bootnr)
                return false;

            auto dosdisk = GetOption(dosdisk);
            if (dosdisk.empty())
                dosdisk = OSD::MakeFilePath(PathType::Resource, "samdos2.sbt");

            pBootDrive = std::make_unique<Drive>();
            if (!pBootDrive->Insert(dosdisk))
            {
                pBootDrive.reset();
                break;
            }

            // Jump back to BOOTEX to try again
            auto bootex = read_word(*bootnr + 1);
            cpu.set_pc(bootex);
            return true;
        }
        break;

    // Copyright message
    case 0x50:
        g_nTurbo &= ~TURBO_BOOT;
        break;

    default:
        Keyin::Stop();
        break;
    }

    return false;
}

void Rst48Hook()
{

    // Are we at READKEY in ROM0?
    if (cpu.get_pc() == rom_hook_addr(RomHook::READKEY))
    {
        if (auto_load != AutoLoadType::None)
            AutoLoad(auto_load);
    }
}

} // namespace IO
