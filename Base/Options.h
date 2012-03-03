// Part of SimCoupe - A SAM Coupe emulator
//
// Options.h: Option saving, loading and command-line processing
//
//  Copyright (c) 1999-2012 Simon Owen
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

#ifndef OPTIONS_H
#define OPTIONS_H

#define OPTIONS_FILE  "SimCoupe.cfg"

typedef struct
{
    int     cfgversion;             // Config compatability number (set defaults if mismatched)
    bool    firstrun;               // First run of the emulator?

    int     sync;                   // 0=unsynced, 1=50Hz sync, [Win32 only: 2=vsync, 3=double-vsync]
    int     scale;                  // Window scaling mode
    bool    ratio5_4;               // Use 5:4 screen ratio?
    bool    scanlines;              // Show scanlines?
    int     scanlevel;              // Scanline brightness level
    int     mode3;                  // Which mode3 pixels to show on low-res displays?
    bool    fullscreen;             // Start in full-screen mode?
    int     borders;                // How much of the borders to show
    bool    stretchtofit;           // Stretch screen image to fit target area?
    bool    hwaccel;                // Use hardware accelerated video?
    bool    greyscale;              // Use greyscale instead of colour?
    bool    filter;                 // Filter the OpenGL image when stretching?

    char    rom[MAX_PATH];          // SAM ROM image path
    bool    hdbootrom;              // Use HDBOOT ROM patches?
    bool    fastreset;              // Fast SAM system reset?
    bool    asicdelay;              // Enforce ASIC startup delay (~49ms)?
    int     mainmem;                // 256 or 512 for amount of main memory
    int     externalmem;            // Number of MB of external memory
    bool    nmosz80;                // NMOS rather than CMOS Z80?

    int     drive1;                 // Drive 1 type
    int     drive2;                 // Drive 2 type
    int     turboload;              // 0 for disabled, or sensitivity in number of frames
    bool    saveprompt;             // Prompt before saving disk changes?
    bool    autoboot;               // Autoboot drive 1 on first startup?
    bool    dosboot;                // Automagically boot DOS from non-bootable disks?
    char    dosdisk[MAX_PATH];      // Override DOS boot disk to use instead of the internal SAMDOS 2.2 image
    bool    stdfloppy;              // Assume real disks are standard format, initially?
    int     nextfile;               // Next file number for auto-generated filenames

    char    disk1[MAX_PATH];        // Floppy disk image in drive 1
    char    disk2[MAX_PATH];        // Floppy disk image in drive 2
    char    atomdisk[MAX_PATH];     // Hard disk image for Atom
    char    sdidedisk[MAX_PATH];    // Hard disk image for SD IDE interface
    char    yatbusdisk[MAX_PATH];   // Hard disk image for YAMOD.ATBUS interface

    char    inpath[MAX_PATH];       // Override path for input files
    char    outpath[MAX_PATH];      // Override path for output files
    char    mru0[MAX_PATH];         // Most recently used files
    char    mru1[MAX_PATH];         // Most recently used files
    char    mru2[MAX_PATH];         // Most recently used files
    char    mru3[MAX_PATH];         // Most recently used files
    char    mru4[MAX_PATH];         // Most recently used files
    char    mru5[MAX_PATH];         // Most recently used files

    int     keymapping;             // Keyboard mapping mode (raw/SAM/Spectrum)
    bool    altforcntrl;            // Use Left-Alt for SAM Cntrl?
    bool    altgrforedit;           // Use Right-Alt for SAM Edit?
    bool    keypadreset;            // Use Keypad-minus for SAM Reset?
    bool    mouse;                  // Emulate the SAM mouse?
    bool    mouseesc;               // Allow Esc to release the mouse capture?
    bool    swap23;                 // Swap SAM mouse buttons 2 and 3?

    char    joydev1[128];           // Joystick 1 device
    char    joydev2[128];           // Joystick 2 device number
    int     deadzone1;              // Joystick 1 deadzone
    int     deadzone2;              // Joystick 2 deadzone

    int     parallel1;              // Parallel port 1 function
    int     parallel2;              // Parallel port 2 function
    char    printerdev[128];        // Printer device name
    bool    printeronline;          // Printer is online?
    int     flushdelay;             // Delay before auto-flushing print data

    int     serial1;                // Serial port 1 function
    int     serial2;                // Serial port 2 function
    char    serialdev1[128];        // Serial port 1 device
    char    serialdev2[128];        // Serial port 2 device

    int     midi;                   // MIDI port function
    char    midiindev[128];         // MIDI-In device
    char    midioutdev[128];        // MIDI-Out device
    int     networkid;              // Network station number

    bool    sambusclock;            // Enable SAMBUS clock support?
    bool    dallasclock;            // Enable DALLAS clock support?

    bool    sound;                  // Sound enabled?
    int     latency;                // Amount of sound buffering
    bool    bluealpha;              // Blue Alpha Sampler?
    int     samplerfreq;            // Blue Alpha Sampler clock frequency
    bool    samvox;                 // SAMVox 4-channel DAC?
    bool    paula;                  // Paula 4-bit dual-DAC?

    int     drivelights;            // Show floppy drive LEDs
    bool    profile;                // Show profile stats?
    bool    status;                 // Show status line?

    char    fnkeys[256];            // Function key bindings
    char    keymap[256];            // Custom keymap
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

#endif  // OPTIONS_H
