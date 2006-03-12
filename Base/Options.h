// Part of SimCoupe - A SAM Coupe emulator
//
// Options.h: Option saving, loading and command-line processing
//
//  Copyright (c) 1999-2006  Simon Owen
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

#ifndef OPTION_H
#define OPTION_H

typedef struct
{
    int     cfgversion;             // Config compatability number (set defaults if mismatched)
    bool    firstrun;               // Non-zero if this is the first time the emulator has been run

    int     sync;                   // Syncronise the emulator to 50Hz
    int     frameskip;              // 0 for auto, otherwise 'mod frameskip' used to decide which to draw
    int     scale;                  // Window scaling mode
    bool    ratio5_4;               // Use 5:4 screen ratio?
    bool    scanlines;              // Show scanlines?
    int     scanlevel;              // Scanline brightness level
    int     mode3;                  // Which mode3 pixels to show on low-res displays?
    int     fullscreen;             // Start in full-screen mode?
    int     depth;                  // Screen depth for full-screen
    int     borders;                // How much of the borders to show
    bool    stretchtofit;           // Stretch screen image to fit target area?
    bool    overlay;                // Non-zero to use a video overlay surface, if available
    bool    hwaccel;                // Non-zero to use hardware accelerated video
    bool    greyscale;              // Non-zero to use greyscale instead of colour
    bool    filter;                 // Non-zero to filter the OpenGL image when stretching

    char    rom[MAX_PATH];          // SAM ROM image path
    bool    hdbootrom;              // Use HDBOOT ROM patches
    bool    fastreset;              // Fast SAM system reset?
    bool    asicdelay;              // ASIC startup delay of ~49ms
    int     mainmem;                // 256 or 512 for amount of main memory
    int     externalmem;            // Number of MB of external memory

    int     drive1;                 // Drive 1 type
    int     drive2;                 // Drive 2 type
    int     turboload;              // 0 for disabled, or sensitivity in number of frames
    bool    saveprompt;             // Non-zero to prompt before saving changes
    bool    autoboot;               // Autoboot drive 1 on first startup?
    bool    dosboot;                // True to automagically boot DOS from non-bootable disks
    char    dosdisk[MAX_PATH];      // Override DOS boot disk to use instead of the internal SAMDOS 2.2 image
    bool    stdfloppy;              // Assume real disks are standard format, initially

    char    disk1[MAX_PATH];        // Floppy disk image in drive 1
    char    disk2[MAX_PATH];        // Floppy disk image in drive 2
    char    atomdisk[MAX_PATH];     // Hard disk image for Atom
    char    sdidedisk[MAX_PATH];    // Hard disk image for SD IDE interface
    char    yatbusdisk[MAX_PATH];   // Hard disk image for YAMOD.ATBUS interface

    char    floppypath[MAX_PATH];   // Default floppy disk path
    char    hddpath[MAX_PATH];      // Default hard disk path
    char    rompath[MAX_PATH];      // Default ROM path
    char    datapath[MAX_PATH];     // Default data path
    char    mru0[MAX_PATH];         // Most recently used files
    char    mru1[MAX_PATH];         // Most recently used files
    char    mru2[MAX_PATH];         // Most recently used files
    char    mru3[MAX_PATH];         // Most recently used files
    char    mru4[MAX_PATH];         // Most recently used files
    char    mru5[MAX_PATH];         // Most recently used files

    int     keymapping;             // Keyboard mapping mode (raw/SAM/Spectrum)
    bool    altforcntrl;            // Non-zero if Left-Alt is used for SAM Cntrl
    bool    altgrforedit;           // Non-zero if Right-Alt is used for SAM Edit
    bool    keypadreset;            // Non-zero if Keypad-minus is used for Reset
    bool    samfkeys;               // Non-zero to use the PC function keys for the SAM keypad
    bool    mouse;                  // True to emulate the SAM mouse
    bool    mouseesc;               // True to allow Esc to release the mouse capture
    bool    swap23;                 // True to swap mouse buttons 2 and 3

    char    joydev1[128];           // Joystick 1 device
    char    joydev2[128];           // Joystick 2 device number
    int     deadzone1;              // Joystick 1 deadzone
    int     deadzone2;              // Joystick 2 deadzone

    int     parallel1;              // Parallel port 1 function
    int     parallel2;              // Parallel port 2 function
    char    printerdev[128];        // Printer device name

    int     serial1;                // Serial port 1 function
    int     serial2;                // Serial port 2 function
    char    serialdev1[128];        // Serial port 1 device
    char    serialdev2[128];        // Serial port 2 device

    int     midi;                   // MIDI port function
    char    midiindev[128];         // MIDI-In device
    char    midioutdev[128];        // MIDI-Out device
    int     networkid;              // Network station number

    bool    sambusclock;            // Non-zero if we want SAMBUS clock support
    bool    dallasclock;            // Non-zero if we want DALLAS clock support
    bool    clocksync;              // Non-zero if clock(s) advanced relative to real time

    bool    sound;                  // Sound enabled?
    bool    saasound;               // SAA 1099 sound chip enabled?
    bool    beeper;                 // Spectrum-style beeper?

    bool    stereo;                 // Stereo sound?
    int     latency;                // Amount of sound buffering

    int     drivelights;            // Show floppy drive LEDs
    int     profile;                // Show profile stats?
    bool    status;                 // Show status line?

    bool    pauseinactive;          // Pause when not the active app?

    char    fnkeys[256];            // Function key bindings
}
OPTIONS;


class Options
{
    public:
        static void SetDefaults (bool fForce_=true);
        static void* GetDefault (const char* pcszName_);

        static bool Load (int argc_, char* argv[]);
        static bool Save ();

        static OPTIONS s_Options;
};


// Helper macros for getting/setting options
#define GetOption(field)        (const_cast<const OPTIONS*>(&Options::s_Options)->field)
#define SetOption(field,value)  SetOption_(Options::s_Options.field, value)
#define SetDefault(field,value) SetDefault_(#field, value, Options::s_Options.field)

// inline functions so we can take advantage of function polymorphism
inline bool SetOption_(bool& rfOption_, bool fValue_)   { return rfOption_ = fValue_; }
inline int SetOption_(int& rnOption_, int nValue_)      { return rnOption_ = nValue_; }
inline const char* SetOption_(char* pszOption_, const char* pszValue_)  { return strcpy(pszOption_, pszValue_); }

inline void SetDefault_(const char* pcszOption_, bool fValue_, bool&) { *((bool*)Options::GetDefault(pcszOption_)) = fValue_; }
inline void SetDefault_(const char* pcszOption_, int nValue_, int&) { *((int*)Options::GetDefault(pcszOption_)) = nValue_; }
//inline void SetDefault_(const char* pcszOption_, char* pszValue_, char*&) { strcpy((char*)Options::GetDefault(pcszOption_), pszValue_); }

#endif  // OPTION_H
