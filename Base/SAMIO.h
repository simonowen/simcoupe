// Part of SimCoupe - A SAM Coupe emulator
//
// SAMIO.h: SAM I/O port handling
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

#pragma once

constexpr uint8_t KEMPSTON_PORT = 0x1f;

constexpr uint8_t BLUE_ALPHA_PORT = 0x7f;
constexpr uint16_t BA_VOICEBOX_PORT = 0xff7f;
constexpr uint16_t BA_SAMPLER_BASE = 0x7c7f;
constexpr uint16_t BA_SAMPLER_MASK = 0xfcff;

constexpr uint8_t LEPR_PORT = 0x80;
constexpr uint8_t HEPR_PORT = 0x81;

constexpr uint8_t SDIDE_DATA_PORT = 0xbd;
constexpr uint8_t SDIDE_REG_PORT = 0xbf;

constexpr uint8_t QUAZAR_PORT = 0xd0;
constexpr uint8_t SID_PORT = 0xd4;

constexpr uint8_t FLOPPY1_BASE = 0xe0;
constexpr uint8_t FLOPPY2_BASE = 0xf0;
constexpr uint8_t FLOPPY_MASK = 0xf8;

constexpr uint8_t PRINTL1_DATA_PORT = 0xe8;
constexpr uint8_t PRINTL1_STAT_PORT = 0xe9;
constexpr uint8_t PRINTL2_DATA_PORT = 0xea;
constexpr uint8_t PRINTL2_STAT_PORT = 0xeb;

constexpr uint8_t CLOCK_PORT = 0xef;

constexpr uint8_t BASE_ASIC_PORT = 0xf8;

constexpr uint8_t LPEN_PORT = 0xf8;
constexpr uint8_t LPEN_TXFMST = 0x02;
constexpr uint8_t LPEN_BORDER_BCD0 = 0x01;
constexpr uint16_t HPEN_PORT = 0x1f8;
constexpr uint16_t PEN_PORT_MASK = 0x1ff;

constexpr uint8_t CLUT_BASE_PORT = 0xf8;

constexpr uint8_t STATUS_PORT = 0xf9;
constexpr uint8_t STATUS_INT_LINE = 0x01;
constexpr uint8_t STATUS_INT_MOUSE = 0x02;    // Part of original SAM design, but never used
constexpr uint8_t STATUS_INT_MIDIIN = 0x04;
constexpr uint8_t STATUS_INT_FRAME = 0x08;
constexpr uint8_t STATUS_INT_MIDIOUT = 0x10;
constexpr uint8_t STATUS_INT_MASK = 0x1f;
constexpr uint8_t STATUS_KEY_MASK = 0xe0;

constexpr uint8_t LINE_PORT = 0xf9;

constexpr uint8_t LMPR_PORT = 0xfa;
constexpr uint8_t LMPR_PAGE_MASK = 0x1f;
constexpr uint8_t LMPR_ROM0_OFF = 0x20;
constexpr uint8_t LMPR_ROM1 = 0x40;
constexpr uint8_t LMPR_WPROT = 0x80;

constexpr uint8_t HMPR_PORT = 0xfb;
constexpr uint8_t HMPR_PAGE_MASK = 0x1f;
constexpr uint8_t HMPR_MD3COL_MASK = 0x60;
constexpr uint8_t HMPR_MCNTRL_MASK = 0x80;

constexpr uint8_t VMPR_PORT = 0xfc;
constexpr uint8_t VMPR_RXMIDI_MASK = 0x80;
constexpr uint8_t VMPR_MDE1_MASK = 0x40;
constexpr uint8_t VMPR_MDE0_MASK = 0x20;
constexpr uint8_t VMPR_PAGE_MASK = 0x1f;
constexpr uint8_t VMPR_MODE_MASK = 0x60;
constexpr uint8_t VMPR_MODE_SHIFT = 5;
constexpr uint8_t VMPR_MODE_1 = 0x00;
constexpr uint8_t VMPR_MODE_2 = 0x20;
constexpr uint8_t VMPR_MODE_3 = 0x40;
constexpr uint8_t VMPR_MODE_4 = 0x60;

constexpr uint8_t MIDI_PORT = 0xfd;
constexpr auto MIDI_TRANSMIT_TIME = usecs_to_tstates(320);  // 1 start + 8 data + 1 stop @31.25Kbps = 320us
constexpr auto MIDI_INT_ACTIVE_TIME = usecs_to_tstates(16);
constexpr auto MIDI_TXFMST_ACTIVE_TIME = usecs_to_tstates(32);

constexpr uint8_t KEYBOARD_PORT = 0xfe;
constexpr uint8_t KEYBOARD_KEY_MASK = 0x1f;
constexpr uint8_t KEYBOARD_SPEN_MASK = 0x20;
constexpr uint8_t KEYBOARD_EAR_MASK = 0x40;
constexpr uint8_t KEYBOARD_SOFF_MASK = 0x80;

constexpr uint8_t BORDER_PORT = 0xfe;
constexpr uint8_t BORDER_COLOUR_MASK = 0x27;
constexpr uint8_t BORDER_MIC_MASK = 0x08;
constexpr uint8_t BORDER_BEEP_MASK = 0x10;
constexpr uint8_t BORDER_SOFF_MASK = 0x80;
constexpr uint8_t BORDER_COLOUR(uint8_t x) { return ((((x) & 0x20) >> 2) | ((x) & 0x07)); }

constexpr uint8_t ATTR_PORT = 0xff;

constexpr uint8_t SAA_PORT = 0xff;
constexpr uint8_t SAA_DATA = 0xff;
constexpr uint16_t SAA_ADDR_PORT = 0x1ff;
constexpr uint16_t SAA_MASK = 0x1ff;

struct COLOUR
{
    uint8_t red, green, blue;
};

enum class AutoLoadType { None, Disk, Tape };

namespace IO
{
extern bool mid_frame_change;
extern bool flash_phase;
extern std::array<uint8_t, 9> key_matrix;

struct IoState
{
    uint8_t lepr = 0x00;
    uint8_t hepr = 0x00;
    uint8_t lpen = 0x00;
    uint8_t hpen = 0x00;
    uint8_t line = 0xff;
    uint8_t status = 0xff;
    uint8_t lmpr = 0x00;
    uint8_t hmpr = 0x00;
    uint8_t vmpr = 0x00;
    uint8_t keyboard = KEYBOARD_EAR_MASK;
    uint8_t border = 0x00;
    uint8_t attr = 0x00;
    uint8_t clut[NUM_CLUT_REGS]{};

    bool asic_asleep = false;
};

bool Init();
void Exit(bool fReInit_ = false);

IoState& State();

uint8_t In(uint16_t port);
void Out(uint16_t port, uint8_t val);

inline int WaitStates(uint32_t frame_cycles, uint16_t port)
{
    if ((port & 0xff) < BASE_ASIC_PORT)
        return 0;

    constexpr auto mask = 7;
    auto delay = mask - ((frame_cycles + 2) & mask);
    return delay;
}

void out_lmpr(uint8_t val);
void out_hmpr(uint8_t val);
void out_vmpr(uint8_t val);
void out_lepr(uint8_t val);
void out_hepr(uint8_t val);
void out_clut(uint16_t port, uint8_t val);
void out_border(uint8_t val);

bool ScreenDisabled();
bool ScreenMode3or4();
int ScreenMode();
int VisibleScreenPage();
uint8_t Mode3Clut(int index);

void FrameUpdate();
void UpdateInput();
void UpdateDrives();
std::vector<COLOUR> Palette();
bool TestStartupScreen(bool exit = false);
void QueueAutoLoad(AutoLoadType type);
void AutoLoad(AutoLoadType type);

void EiHook();
bool Rst8Hook();
void Rst48Hook();
}

////////////////////////////////////////////////////////////////////////////////

class IoDevice
{
public:
    virtual ~IoDevice() = default;

public:
    virtual void Reset() { }

    virtual uint8_t In(uint16_t /*port*/) { return 0xff; }
    virtual void Out(uint16_t /*port*/, uint8_t /*val*/) { }

    virtual void FrameEnd() { }

    virtual bool LoadState(const std::string&) { return true; }  // preserve basic state (such as NVRAM)
    virtual bool SaveState(const std::string&) { return true; }
};

enum { drvNone, drvFloppy, drvAtom, drvAtomLite, drvSDIDE };

class DiskDevice : public IoDevice
{
public:
    DiskDevice() = default;

public:
    void FrameEnd() override { if (m_uActive) m_uActive--; }

public:
    virtual bool Insert(const std::string& disk_path) { return false; }
    virtual bool Insert(const std::vector<uint8_t>& mem_file) { return false; }
    virtual void Eject() { }
    virtual void Flush() { }

public:
    virtual std::string DiskPath() const = 0;
    virtual std::string DiskFile() const = 0;

    virtual bool HasDisk() const { return false; }
    virtual bool IsLightOn() const { return false; }
    virtual bool IsActive() const { return m_uActive != 0; }

protected:
    unsigned int m_uActive = 0; // active when non-zero, decremented by FrameEnd()
};

////////////////////////////////////////////////////////////////////////////////

extern std::unique_ptr<DiskDevice> pFloppy1, pFloppy2, pBootDrive;
extern std::unique_ptr<IoDevice> pParallel1, pParallel2;

