// Part of SimCoupe - A SAM Coupé emulator
//
// IO.h: SAM I/O port handling
//
//  Copyright (c) 1999-2003  Simon Owen
//  Copyright (c) 1996-2001  Allan Skillman
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

#ifndef IO_H
#define IO_H

#include "Sound.h"

typedef struct
{
    BYTE    bRed;
    BYTE    bGreen;
    BYTE    bBlue;
    BYTE    bAlpha;     // Likely to be only needed for 3D surfaces
}
RGBA;


class IO
{
    public:
        static bool Init (bool fFirstInit_=false);
        static void Exit (bool fReInit_=false);

        static bool InitDrives (bool fInit_=true, bool fReInit_=true);
        static bool InitParallel (bool fInit_=true, bool fReInit_=true);
        static bool InitSerial (bool fInit_=true, bool fReInit_=true);
        static bool InitMidi (bool fInit_=true, bool fReInit_=true);
        static bool InitBeeper (bool fInit_=true, bool fReInit_=true);
        static bool InitSDIDE (bool fInit_=true, bool fReInit_=true);

        static BYTE In (WORD wPort_);
        static void Out (WORD wPort_, BYTE bVal_);

        static void FrameUpdate ();
        static void UpdateInput();
        static const RGBA* GetPalette (bool fDimmed_=false);
};


class CIoDevice
{
    public:
        virtual ~CIoDevice () { }

    public:
        virtual BYTE In (WORD wPort_) { return 0x00; }
        virtual void Out (WORD wPort_, BYTE bVal_) { }

        virtual void FrameEnd () { }
};

enum { dskNone, dskImage, dskAtom };

class CDiskDevice :  public CIoDevice
{
    public:
        CDiskDevice (int nType_) : m_nType(nType_) { }
        virtual ~CDiskDevice () { }

    public:
        virtual bool Insert (const char* pcszImage_, bool fReadOnly_=false) { return false; }
        virtual bool Eject () { return false; }
        virtual bool Flush () { return false; }

    public:
        virtual int GetType () const { return m_nType; }
        virtual const char* GetImage () const { return ""; }
        virtual bool IsInserted () const { return false; }
        virtual bool IsWriteable () const { return false; }
        virtual bool IsModified () const { return false; }
        virtual bool IsLightOn () const { return false; }
        virtual bool IsActive () const { return IsLightOn(); }

    protected:
        int m_nType;
};


// Spectrum-style BEEPer
class CBeeperDevice : public CIoDevice
{
    public:
        void Out (WORD wPort_, BYTE bVal_) { Sound::OutputDAC((bVal_ & 0x10) ? 0x60 : 0x80); }
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
// Did I hear 238 and 239 SERIAL3 and SERIAL4?  what about the clock port?

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
#define VMPR_MDE0_MASK      0x20
#define VMPR_MDE1_MASK      0x40
#define VMPR_PAGE           (vmpr & VMPR_PAGE_MASK)
#define VMPR_MODE           (vmpr & VMPR_MODE_MASK)
#define VMPR_MODE_3_OR_4    (vmpr & VMPR_MDE1_MASK)

#define BORD_COLOUR_MASK    0x27
#define BORD_MIC_MASK       0x08
#define BORD_BEEP_MASK      0x10
#define BORD_SOFF_MASK      0x80
#define BORD_VAL(x)         ((((x) & 0x20 ) >> 2) | ((x) & 0x07))
#define BORD_COL(x)         ((x) & BORD_COLOUR_MASK)
#define BORD_SOFF           (border & BORD_SOFF_MASK)

#define LPEN_TXFMST         0x02    // Bit set if MIDI OUT is currently transmitting a byte


// Bits in the status register to RESET to signal the various interrupts
#define STATUS_INT_LINE     0x01
#define STATUS_INT_MOUSE    0x02        // Not currently used in SimCoupe as everything seems to poll the mouse
#define STATUS_INT_MIDIIN   0x04
#define STATUS_INT_FRAME    0x08
#define STATUS_INT_MIDIOUT  0x10
#define STATUS_INT_NONE     0xff

// MIDI transfer rates are 31.25 Kbaud. Data has 1 start bit, 8 data bits, and 1 stop bit, for 320us serial byte.
#define MIDI_TRANSMIT_TIME      USECONDS_TO_TSTATES(320)
#define MIDI_INT_ACTIVE_TIME    USECONDS_TO_TSTATES(16)



// Key constants used with the key macros below
enum eSamKey
{
    SK_MIN=0, SK_SHIFT=SK_MIN, SK_Z, SK_X, SK_C, SK_V, SK_F1, SK_F2, SK_F3,
    SK_A, SK_S, SK_D, SK_F, SK_G, SK_F4, SK_F5, SK_F6,
    SK_Q, SK_W, SK_E, SK_R, SK_T, SK_F7, SK_F8, SK_F9,
    SK_1, SK_2, SK_3, SK_4, SK_5, SK_ESCAPE, SK_TAB, SK_CAPS,
    SK_0, SK_9, SK_8, SK_7, SK_6, SK_MINUS, SK_PLUS, SK_DELETE,
    SK_P, SK_O, SK_I, SK_U, SK_Y, SK_EQUALS, SK_QUOTES, SK_F0,
    SK_RETURN, SK_L, SK_K, SK_J, SK_H, SK_SEMICOLON, SK_COLON, SK_EDIT,
    SK_SPACE, SK_SYMBOL, SK_M, SK_N, SK_B, SK_COMMA, SK_PERIOD, SK_INV,
    SK_CONTROL, SK_UP, SK_DOWN, SK_LEFT, SK_RIGHT, SK_NONE, SK_MAX=SK_NONE
};

extern BYTE keyboard, keyports[9], keybuffer[9];
extern bool fInputDirty;

// Helper macros for SAM keyboard matrix manipulation
inline bool IsSamKeyPressed (int k) { return !(keybuffer[(k) >> 3] & (1 << ((k) & 7))); }
inline void PressSamKey (int k)     { keybuffer[(k) >> 3] &= ~(1 << ((k) & 7)); fInputDirty = true; }
inline void ReleaseSamKey (int k)   { keybuffer[(k) >> 3] |=  (1 << ((k) & 7)); fInputDirty = true; }
inline void ToggleSamKey (int k)    { keybuffer[(k) >> 3] ^=  (1 << ((k) & 7)); fInputDirty = true; }
inline void ReleaseAllSamKeys ()    { memset(keybuffer, 0xff, sizeof keyports); keyboard = 0x5f; fInputDirty = true; }

// The SAM ports

// Read / Write ports

// Last port read/written
extern WORD wPortRead, wPortWrite;

// Paging ports for internal and external memory
extern BYTE vmpr, hmpr, lmpr, lepr, hepr;
extern BYTE vmpr_mode, vmpr_page1, vmpr_page2;

extern BYTE border;
extern BYTE border_col;

// Write only ports
extern BYTE line_int;
extern UINT clut[N_CLUT_REGS], clutval[N_CLUT_REGS], mode3clutval[4];

// Read only ports
extern BYTE status_reg;
extern BYTE lpen, hpen;

extern CDiskDevice *pDrive1, *pDrive2;
extern CIoDevice *pParallel1, *pParallel2;

#endif
