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

typedef struct {
    uint8_t bRed, bGreen, bBlue;
} COLOUR;

enum { AUTOLOAD_NONE, AUTOLOAD_DISK, AUTOLOAD_TAPE };


namespace IO
{
bool Init(bool fFirstInit_ = false);
void Exit(bool fReInit_ = false);

uint8_t In(uint16_t /*port*/);
void Out(uint16_t port, uint8_t val);

void OutLmpr(uint8_t bVal_);
void OutHmpr(uint8_t bVal_);
void OutVmpr(uint8_t bVal_);
void OutLepr(uint8_t bVal_);
void OutHepr(uint8_t bVal_);

void OutClut(uint16_t wPort_, uint8_t bVal_);

void FrameUpdate();
void UpdateInput();
const COLOUR* GetPalette();
bool IsAtStartupScreen(bool fExit_ = false);
void AutoLoad(int nType_, bool fOnlyAtStartup_ = true);
void WakeAsic();

bool EiHook();
bool Rst8Hook();
bool Rst48Hook();
}


class CIoDevice
{
public:
    virtual ~CIoDevice() = default;

public:
    virtual void Reset() { }

    virtual uint8_t In(uint16_t /*port*/) { return 0xff; }
    virtual void Out(uint16_t /*port*/, uint8_t /*val*/) { }

    virtual void FrameEnd() { }

    virtual bool LoadState(const char* /*file*/) { return true; }  // preserve basic state (such as NVRAM)
    virtual bool SaveState(const char* /*file*/) { return true; }
};

enum { drvNone, drvFloppy, drvAtom, drvAtomLite, drvSDIDE };

class CDiskDevice : public CIoDevice
{
public:
    CDiskDevice() = default;

public:
    void FrameEnd() override { if (m_uActive) m_uActive--; }

public:
    virtual bool Insert(const char* /*image*/, bool /*autoload*/ = false) { return false; }
    virtual void Eject() { }
    virtual bool Save() { return true; }

public:
    virtual const char* DiskPath() const = 0;
    virtual const char* DiskFile() const = 0;

    virtual bool HasDisk() const { return false; }
    virtual bool DiskModified() const { return false; }
    virtual bool IsLightOn() const { return false; }
    virtual bool IsActive() const { return m_uActive != 0; }

    virtual void SetDiskModified(bool /*modified*/ = true) { }

protected:
    unsigned int m_uActive = 0; // active when non-zero, decremented by FrameEnd()
};

#define in_byte     IO::In
#define out_byte    IO::Out


#define LEPR_PORT           128
#define HEPR_PORT           129

#define CLOCK_PORT          239         // SAMBUS and DALLAS clocks

#define PEN_MASK            0x1f8
#define LPEN_PORT           248         // Input
#define HPEN_PORT           504         // Input
#define CLUT_BASE_PORT      248         // Output

#define STATUS_PORT         249         // Input
#define LINE_PORT           249         // Output

#define LMPR_PORT           250
#define HMPR_PORT           251
#define VMPR_PORT           252
#define MIDI_PORT           253

#define KEYBOARD_PORT       254         // Input
#define BORDER_PORT         254         // Output

#define SOUND_MASK          0x1ff
#define SOUND_DATA          255         // Output (register select)
#define SOUND_ADDR          511         // Output (data)
#define ATTR_PORT           255         // Input

#define KEMPSTON_PORT       31          // Kempston joystick

#define BLUE_ALPHA_PORT     127         // Blue Alpha Sampler and VoiceBox

#define QUAZAR_PORT         208         // Quazar Surround
#define SID_PORT            212         // Quazar SID interface at 0xD4xx

// Floppy drives or ATOM hard disk - 111d0srr : d = drive, s = side, r = register
#define FLOPPY_MASK         0xf8        // 11111000
#define FLOPPY1_BASE        224         // 224 to 231
#define FLOPPY2_BASE        240         // 240 to 247

#define PRINTL_MASK         0xfc        // 11111100
#define PRINTL_BASE         232         // 11101000
#define PRINTL1_DATA        232
#define PRINTL1_STAT        233
#define PRINTL2_DATA        234
#define PRINTL2_STAT        235

#define SERIAL_MASK         0xfe        // 11111110
#define SERIAL_BASE         236         // 11101100
#define SERIAL1             236
#define SERIAL2             237
#define SERIAL3             238
//#define SERIAL4           239         // disabled due to clock port clash

#define SDIDE_DATA          189
#define SDIDE_REG           191

#define LMPR_PAGE_MASK      0x1f
#define LMPR_PAGE           (lmpr & LMPR_PAGE_MASK)
#define LMPR_ROM0_OFF       0x20
#define LMPR_ROM1           0x40
#define LMPR_WPROT          0x80

#define HMPR_PAGE_MASK      0x1f
#define HMPR_MD3COL_MASK    0x60
#define HMPR_MCNTRL_MASK    0x80
#define HMPR_PAGE           (hmpr & HMPR_PAGE_MASK)
#define HMPR_MD3COL         (hmpr & HMPR_MD3_MASK)
#define HMPR_MCNTRL         (hmpr & HMPR_MCNTRL_MASK)

#define MODE_1              0x00
#define MODE_2              0x20
#define MODE_3              0x40
#define MODE_4              0x60

#define VMPR_PAGE_MASK      0x1f
#define VMPR_MODE_MASK      0x60
#define VMPR_MODE_SHIFT     5
#define VMPR_MDE0_MASK      0x20
#define VMPR_MDE1_MASK      0x40
#define VMPR_PAGE           (vmpr & VMPR_PAGE_MASK)
#define VMPR_MODE           (vmpr & VMPR_MODE_MASK)
#define VMPR_MODE_3_OR_4    (vmpr & VMPR_MDE1_MASK)

#define BORD_COLOUR_MASK    0x27
#define BORD_KEY_MASK       0x1f
#define BORD_MIC_MASK       0x08
#define BORD_BEEP_MASK      0x10
#define BORD_SPEN_MASK      0x20
#define BORD_EAR_MASK       0x40
#define BORD_SOFF_MASK      0x80
#define BORD_VAL(x)         ((((x) & 0x20 ) >> 2) | ((x) & 0x07))
#define BORD_COL(x)         ((x) & BORD_COLOUR_MASK)
#define BORD_SOFF           (border & BORD_SOFF_MASK)

#define LPEN_TXFMST         0x02    // Bit set if MIDI OUT is currently transmitting a byte


// Bits in the status register to RESET to signal the various interrupts
#define STATUS_INT_LINE     0x01
#define STATUS_INT_MOUSE    0x02    // Part of original SAM design, but never used
#define STATUS_INT_MIDIIN   0x04
#define STATUS_INT_FRAME    0x08
#define STATUS_INT_MIDIOUT  0x10
#define STATUS_INT_NONE     0x1f

// MIDI transfer rates are 31.25 Kbaud. Data has 1 start bit, 8 data bits, and 1 stop bit, for 320us serial byte.
#define MIDI_TRANSMIT_TIME      USECONDS_TO_TSTATES(320)
#define MIDI_INT_ACTIVE_TIME    USECONDS_TO_TSTATES(16)
#define MIDI_TXFMST_ACTIVE_TIME USECONDS_TO_TSTATES(32)

#define N_PALETTE_COLOURS   128     // 128 colours in the SAM palette
#define N_CLUT_REGS         16      // 16 CLUT entries
#define N_SAA_REGS          32      // 32 registers in the Philips SAA1099 sound chip

#define BASE_ASIC_PORT      0xf8    // Ports from this value require ASIC attention, and can cause contention delays


// Keyboard matrix buffer
extern uint8_t keybuffer[9];

// Last port read/written
extern uint16_t wPortRead, wPortWrite;
extern uint8_t bPortInVal, bPortOutVal;

// Paging ports for internal and external memory
extern uint8_t vmpr, hmpr, lmpr, lepr, hepr;
extern uint8_t vmpr_mode, vmpr_page1, vmpr_page2;

extern uint8_t keyboard, border;
extern uint8_t border_col;

// Write only ports
extern uint8_t line_int;
extern unsigned int clut[N_CLUT_REGS], mode3clut[4];

// Read only ports
extern uint8_t status_reg;
extern uint8_t lpen;

extern CDiskDevice* pFloppy1, * pFloppy2, * pBootDrive;
extern CIoDevice* pParallel1, * pParallel2;

extern int g_nAutoLoad;
extern bool display_changed;
