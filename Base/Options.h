// Part of SimCoupe - A SAM Coupe emulator
//
// Options.h: Option saving, loading and command-line processing
//
//  Copyright (c) 1999-2002  Simon Owen
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
    char logfile[MAX_PATH]; // log filename

    bool    fastreset;              // Fast SAM system reset?

    int     sync;                   // Syncronise the emulator to 50Hz
    int     frameskip;              // 0 for auto, otherwise 'mod frameskip' used to decide which to draw
    int     scale;                  // Window scaling mode
    bool    ratio5_4;               // Use 5:4 screen ratio?
    bool    scanlines;              // Show scanlines?
    bool    fullscreen;             // Start in full-screen mode?
    int     depth;                  // Screen depth for full-screen
    int     borders;                // How much of the borders to show
    bool    stretchtofit;           // Stretch screen image to fit target area?
    int     surface;                // Surface type to use

    char    rom[MAX_PATH];          // SAM ROM image path
    int     mainmem;                // 256 or 512 for amount of main memory
    int     externalmem;            // Number of MB of external memory

    int     drive1;                 // Drive 1 type
    int     drive2;                 // Drive 2 type
    char    disk1[MAX_PATH];        // Floppy disk image in drive 1
    char    disk2[MAX_PATH];        // Floppy disk image in drive 2
    bool    autoboot;               // Autoboot drive 1 on first startup?
    int     turboload;              // 0 for disabled, or sensitivity in number of frames

    int     keymapping;             // Keyboard mapping mode (raw/SAM/Spectrum)
    bool    altforcntrl;            // Non-zero if Left-Alt is used for SAM Cntrl
    bool    altgrforedit;           // Non-zero if Right-Alt is used for SAM Edit
    bool    mouse;                  // True to emulate the SAM mouse

    char    joydev1[128];           // Joystick 1 device
    char    joydev2[128];           // Joystick 2 device number
    int     deadzone1;              // Joystick 1 deadzone
    int     deadzone2;              // Joystick 2 deadzone

    int     parallel1;              // Parallel port 1 function
    int     parallel2;              // Parallel port 2 function
    char    printerdev[128];        // Printer device name
    bool    printeronline;          // True if the printer is ready

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

    int     freq;                   // Sound frequency
    int     bits;                   // Bits per sample per channel
    bool    stereo;                 // Stereo?
    int     latency;                // Amount of sound buffering

    int     drivelights;            // Show floppy drive LEDs
    int     profile;                // Show profile stats?
    bool    status;                 // Show status line?

    char    fnkeys[256];            // Function key bindings

    bool    pauseinactive;          // Pause when not the active app?
}
OPTIONS;


class Options
{
    public:
        static bool Options::Load (int argc_, char* argv[]);
        static bool Options::Save ();

        static OPTIONS s_Options;
};


// Helper macros for getting/setting options
#define GetOption(field)        (const_cast<const OPTIONS*>(&Options::s_Options)->field)
#define SetOption(field,value)  SetOption_(Options::s_Options.field, value)

// inline functions so we can take advantage of function polymorphism
inline bool SetOption_(bool& rfOption_, bool fValue_)   { return rfOption_ = fValue_; }
inline int SetOption_(int& rnOption_, int nValue_)      { return rnOption_ = nValue_; }
inline const char* SetOption_(char* pszOption_, const char* pszValue_)  { return strcpy(pszOption_, pszValue_); }

#endif  // OPTION_H
