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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#pragma once

constexpr auto OPTIONS_FILE = "SimCoupe.cfg";
constexpr auto ConfigVersion = 4;       // increment to force a config reset if incompatible changes are made

struct Config
{
    int cfgversion = ConfigVersion;     // Config compatability number (set defaults if mismatched)
    bool firstrun = true;               // First run of the emulator?
    std::string windowpos;              // Main window position (client area)

    bool tvaspect = true;               // TV pixel aspect ratio?
    bool fullscreen = false;            // Start in full-screen mode?
    int borders = 2;                    // How much of the borders to show (2=TV Visible)
    bool smooth = true;                 // Smooth image when stretching? (disables integer scaling)
    bool motionblur = false;            // Motion blur to reduce animation flicker?
    int blurpercent = 25;               // Percentage of previous frame retained with motion blur enabled
    int maxintensity = 255;             // Maximum colour channel intensity (0-255)
    bool blackborder = false;           // Black border around emulated screen?

    int avireduce = 1;                  // Reduce AVI audio size (0=lossless, 1=44KHz 8-bit, ..., 4=muted)

    std::string rom;                    // Custom SAM ROM path (blank for built-in v3.0)
    bool romwrite = false;              // Enable writes to ROM?
    bool atombootrom = true;            // Use Atom boot ROM if Atom/AtomLite device is connected?
    bool fastreset = true;              // Run at turbo speed during SAM ROM memory test?
    bool asicdelay = true;              // Enforce ASIC startup delay (~49ms)?
    int mainmem = 512;                  // Main memory size in K (256 or 512)
    int externalmem = 1;                // External memory size in MB (0-4)
    bool cmosz80 = false;               // CMOS rather than NMOS Z80? (affects OUT (C),X)
    int speed = 100;                    // Emulation speed (50-1000%)

    int drive1 = 1;                     // Drive 1 type (0=none, 1=floppy, 2=Atom, 3=AtomLite, 4=SDIDE)
    int drive2 = 1;                     // Drive 2 type
    bool turbodisk = true;              // Run at turbo speed during disk access?
    bool dosboot = true;                // Automagically boot DOS from non-bootable disks?
    std::string dosdisk;                // Custom DOS boot disk path (blank for built-in SAMDOS 2.2)
    bool stdfloppy = true;              // Assume real disks are standard format, initially?
    int nextfile = 0;                   // Next file number for auto-generated filenames

    bool turbotape = true;              // Run at turn speed during tape loading?
    bool tapetraps = true;              // Instant loading of ROM tape blocks?

    std::string disk1;                  // Floppy disk image in drive 1
    std::string disk2;                  // Floppy disk image in drive 2
    std::string atomdisk0;              // Atom disk 0
    std::string atomdisk1;              // Atom disk 1
    std::string sdidedisk;              // Hard disk image for SD IDE interface
    std::string tape;                   // Tape image file
    bool autoload = true;               // Auto-load media inserted at the startup screen?
    bool autoboot = true;               // Auto-boot disks passed on command-line? (not saved)

    std::string inpath;                 // Default path for input files
    std::string outpath;                // Default path for output files
    std::string mru0;                   // Most recently used files
    std::string mru1;
    std::string mru2;
    std::string mru3;
    std::string mru4;
    std::string mru5;

    int keymapping = 1;                 // Keyboard mapping mode (0=raw, 1=Auto-detect, 2=SAM, 3=Spectrum)
    bool altforcntrl = false;           // Use Left-Alt for SAM Cntrl key?
    bool altgrforedit = true;           // Use Right-Alt for SAM Edit key?
    bool mouse = true;                  // Mouse interface connected?
    bool mouseesc = true;               // Relase mouse capture if Esc is pressed?

    std::string joydev1;                // Joystick 1 device
    std::string joydev2;                // Joystick 2 device number
    int joytype1 = 1;                   // Joystick 1 mapping (0=None, 1=Joystick1, 2=Joystick2, 3=Kempston)
    int joytype2 = 2;                   // Joystick 2 mapping
    int deadzone1 = 20;                 // Joystick 1 deadzone
    int deadzone2 = 20;                 // Joystick 2 deadzone

    int parallel1 = 0;                  // Parallel port 1 function
    int parallel2 = 0;                  // Parallel port 2 function
    bool printeronline = true;          // Printer is online?
    int flushdelay = 2;                 // Delay (in seconds) before auto-flushing print data

    int midi = 0;                       // MIDI port function (0=none, 1=device)
    std::string midiindev;              // MIDI-In device
    std::string midioutdev;             // MIDI-Out device

    bool sambusclock = true;            // Enable SAMBUS clock support?
    bool dallasclock = false;           // Enable DALLAS clock support?

    bool audiosync = false;             // Forced audio sync? (seamless but jittery)
    int latency = 3;                    // Amount of sound buffering
    int dac7c = 1;                      // DAC device on shared port &7c? (0=none, 1=BlueAlpha Sampler, 2=SAMVox, 3=Paula)
    int samplerfreq = 18000;            // Blue Alpha Sampler clock frequency (default=18KHz)
    bool voicebox = true;               // Blue Alpha VoiceBox connected?
    int sid = 1;                        // SID chip type (0=none, 1=MOS6581, 2=MOS8580)

    int drivelights = 1;                // Show floppy drive LEDs (0=none, 1=top-left, 2=bottom-left)
    bool profile = true;                // Show current emulation speed?
    bool status = true;                 // Show status messages?

    bool breakonexec = false;           // Break on code auto-execute?
    bool rasterdebug = true;            // Raster-accurate debugger display

    std::string fkeys =                 // Function key bindings
        "F1=InsertDisk1,SF1=EjectDisk1,AF1=NewDisk1,CF1=SaveDisk1,"
        "F2=InsertDisk2,SF2=EjectDisk2,AF2=NewDisk2,CF2=SaveDisk2,"
        "F3=TapeBrowser,SF3=EjectTape,"
        "F4=ImportData,SF4=ExportData,AF4=ExitApp,"
        "F5=Toggle54,"
        "F6=ToggleSmoothing,SF6=ToggleMotionBlur,"
        ""
        "F8=ToggleFullscreen,"
        "F9=Debugger,SF9=SavePNG,"
        "F10=Options,"
        "F11=Nmi,"
        "F12=Reset,CF12=ExitApp";
};


namespace Options
{
bool Load(int argc_, char* argv[]);
bool Save();

extern Config g_config;
}

#define GetOption(field)        (Options::g_config.field)
#define SetOption(field,value)  (Options::g_config.field = (value))
