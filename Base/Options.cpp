// Part of SimCoupe - A SAM Coupe emulator
//
// Options.cpp: Option saving, loading and command-line processing
//
//  Copyright (c) 1999-2014 Simon Owen
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

// Notes:
//  Options specified on the command-line override options in the file.
//  The settings are only and written back when it's closed.

#include "SimCoupe.h"
#include "Options.h"

#include "IO.h"
#include "OSD.h"
#include "Util.h"

const int CFG_VERSION = 4;      // increment to force a config reset, if incompatible changes are made

enum { OT_BOOL, OT_INT, OT_STRING };

typedef struct
{
    const char* pcszName;                                       // Option name used in config file
    int nType;                                                  // Option type

    union { void* pv;  char* ppsz;  bool* pf;  int* pn; };      // Address of config variable

    const char* pcszDefault;                                    // Default value of option, with only appropriate type used
    int nDefault;
    bool fDefault;

    bool fSpecified;
}
OPTION;

// Helper macros for structure definition below
#define OPT_S(o,v,s)        { o, OT_STRING, {&Options::s_Options.v}, (s), 0,  false }
#define OPT_N(o,v,n)        { o, OT_INT,    {&Options::s_Options.v}, "", (n), false }
#define OPT_F(o,v,f)        { o, OT_BOOL,   {&Options::s_Options.v}, "",  0,  (f) }

OPTIONS Options::s_Options;

OPTION aOptions[] =
{
    OPT_N("CfgVersion",   cfgversion,     0),         // Config compatability number
    OPT_F("FirstRun",     firstrun,       true),      // Non-zero if this is the first run
    OPT_S("WindowPos",    windowpos,      ""),        // Main window position, if supported

    OPT_N("Scale",        scale,          2),         // Windowed display is 150%
    OPT_F("Ratio5_4",     ratio5_4,       false),     // Don't use 5:4 screen ratio
    OPT_F("Scanlines",    scanlines,      true),      // TV scanlines
    OPT_N("ScanLevel",    scanlevel,      80),        // Scanlines are 80% brightness
    OPT_F("ScanHiRes",    scanhires,      true),      // Scanlines at PC resolution (if supported)
    OPT_N("Mode3",        mode3,          0),         // Show only odd mode3 pixels on low-res displays
    OPT_F("Fullscreen",   fullscreen,     false),     // Not full screen
    OPT_N("Borders",      borders,        2),         // Same amount of borders as previous version
    OPT_F("HWAccel",      hwaccel,        true),      // Use hardware accelerated video
    OPT_F("Greyscale",    greyscale,      false),     // Colour display
    OPT_F("Filter",       filter,         true),      // Filter the image when stretching
    OPT_F("FilterGUI",    filtergui,      false),     // Don't filter the image when the GUI is active
    OPT_N("Direct3D",     direct3d,       -1),        // Automatic use of D3D (currently, Vista or later)

    OPT_N("AviReduce",    avireduce,      1),         // Record 44kHz 8-bit stereo audio (50% saving)
    OPT_F("AviScanlines", aviscanlines,   false),     // Don't include scanlines in AVI recordings

    OPT_S("ROM",          rom,            ""),        // No custom ROM (use built-in)
    OPT_F("RomWrite",     romwrite,       false),     // ROM is read-only
    OPT_F("ALBootRom",    albootrom,      false),     // Don't use Atom Lite ROM patches
    OPT_F("FastReset",    fastreset,      true),      // Allow fast Z80 resets
    OPT_F("AsicDelay",    asicdelay,      true),      // ASIC startup delay of ~50ms
    OPT_N("MainMemory",   mainmem,        512),       // 512K main memory
    OPT_N("ExternalMem",  externalmem,    0),         // No external memory
    OPT_F("CMOSZ80",      cmosz80,        false),     // CMOS rather than NMOS Z80?
    OPT_N("Speed",        speed,          100),       // Default to 100% speed

    OPT_N("Drive1",       drive1,         1),         // Floppy drive 1 present
    OPT_N("Drive2",       drive2,         1),         // Floppy drive 2 present
    OPT_N("TurboDisk",    turbodisk,      true),      // Accelerated disk access
    OPT_F("SavePrompt",   saveprompt,     true),      // Prompt before saving changes
    OPT_F("DosBoot",      dosboot,        true),      // Automagically boot DOS from non-bootable disks
    OPT_S("DosDisk",      dosdisk,        ""),        // No override DOS disk, use internal SAMDOS 2.2
    OPT_F("StdFloppy",    stdfloppy,      true),      // Assume real disks are standard format, initially
    OPT_N("NextFile",     nextfile,       0),         // Start from 0000

    OPT_S("Disk1",        disk1,          ""),        // No disk in floppy drive 1
    OPT_S("Disk2",        disk2,          ""),        // No disk in floppy drive 2
    OPT_S("AtomDisk0",    atomdisk0,      ""),        // No Atom disk 0
    OPT_S("AtomDisk1",    atomdisk1,      ""),        // No Atom disk 1
    OPT_S("SDIDEDisk",    sdidedisk,      ""),        // No SD IDE hard disk
    OPT_S("Tape",         tape,           ""),        // No tape image
    OPT_F("AutoLoad",     autoload,       true),      // Auto-load media inserted at the startup screen
    OPT_F("AutoByteSwap", autobyteswap,   true),      // Byte-swap Atom [Lite] media as necessary

    OPT_F("TurboTape",    turbotape,      true),      // Accelerated tape loading
    OPT_F("TapeTraps",    tapetraps,      true),      // Short-circuit ROM loading for a speed boost

    OPT_S("InPath",       inpath,         ""),        // Default input path
    OPT_S("OutPath",      outpath,        ""),        // Default output path
    OPT_S("MRU0",         mru0,           ""),        // No recently used files
    OPT_S("MRU1",         mru1,           ""),
    OPT_S("MRU2",         mru2,           ""),
    OPT_S("MRU3",         mru3,           ""),
    OPT_S("MRU4",         mru4,           ""),
    OPT_S("MRU5",         mru5,           ""),

    OPT_N("KeyMapping",   keymapping,     1),         // SAM keyboard mapping
    OPT_F("AltForCntrl",  altforcntrl,    false),     // Left-Alt not used for SAM Cntrl
    OPT_F("AltGrForEdit", altgrforedit,   true),      // Right-Alt used for SAM Edit
    OPT_F("Mouse",        mouse,          true),      // Mouse interface connected
    OPT_F("MouseEsc",     mouseesc,       true),      // Allow Esc to release the mouse capture

    OPT_N("JoyType1",     joytype1,       1),         // Joystick 1 controls SAM joystick 1
    OPT_N("JoyType2",     joytype2,       2),         // Joystick 2 controls SAM joystick 2
    OPT_S("JoyDev1",      joydev1,        ""),        // Joystick 1 device
    OPT_S("JoyDev2",      joydev2,        ""),        // Joystick 2 device
    OPT_N("DeadZone1",    deadzone1,      20),        // Joystick 1 deadzone is 20% around central position
    OPT_N("DeadZone2",    deadzone2,      20),        // Joystick 2 deadzone is 20% around central position

    OPT_N("Parallel1",    parallel1,      0),         // Nothing on parallel port 1
    OPT_N("Parallel2",    parallel2,      0),         // Nothing on parallel port 2
    OPT_S("PrinterDev",   printerdev,     ""),        // No printer device (save to file)
    OPT_F("PrinterOnline",printeronline,  true),      // Printer is online
    OPT_N("FlushDelay",   flushdelay,     2),         // Auto-flush printer data after 2 seconds

    OPT_S("SerialDev1",   serialdev1,     ""),        // Serial port 1 device
    OPT_S("SerialDev2",   serialdev2,     ""),        // Serial port 2 device

    OPT_N("Midi",         midi,           0),         // Nothing on MIDI port
    OPT_S("MidiInDev",    midiindev,      ""),        // MIDI-In device
    OPT_S("MidiOutDev",   midioutdev,     ""),        // MIDI-Out device
//  OPT_N("NetworkId",    networkid,      1),         // Network station number, or something, eventually

    OPT_F("SambusClock",  sambusclock,    true),      // SAMBUS clock present
    OPT_F("DallasClock",  dallasclock,    false),     // DALLAS clock not present

    OPT_F("Sound",        sound,          true),      // Sound enabled
    OPT_N("Latency",      latency,        5),         // Sound latency of 5 frames
    OPT_N("DAC7C",        dac7c,          1),         // Blue Alpha Sampler on port &7c
    OPT_N("SamplerFreq",  samplerfreq,    18000),     // Blue Alpha clock frequency (default=18KHz)
    OPT_N("SID",          sid,            1),         // SID interface with MOS6581

    OPT_N("DriveLights",  drivelights,    1),         // Show drive activity lights
    OPT_F("Profile",      profile,        true),      // Show only emulation speed and framerate
    OPT_F("Status",       status,         true),      // Show status line for changed options, etc.

    OPT_F("BreakOnExec",  breakonexec,    false),     // Don't break on code auto-execute

    OPT_S("FnKeys",       fnkeys,
     "F1=1,SF1=2,AF1=0,CF1=3,F2=5,SF2=6,AF2=4,CF2=7,F3=50,SF3=49,F4=11,SF4=12,AF4=8,F5=25,SF5=23,F6=26,F7=27,SF7=21,F8=22,F9=14,SF9=13,F10=9,SF10=10,F11=16,F12=15,CF12=8"),
    OPT_S("KeyMap",       keymap,         "76,68,65,66,48,16,17,56,56"),  // Pocket PC keymap: left,right,up,down,enter,q,w,space,space

    { NULL, 0 }
};

inline bool IsTrue (const char* pcsz_)
{
    return pcsz_ && (!strcasecmp(pcsz_, "true") || !strcasecmp(pcsz_, "on") || !strcasecmp(pcsz_, "enabled") ||
                    !strcasecmp(pcsz_, "yes") || !strcasecmp(pcsz_, "1"));
}

// Find a named option in the options list above
static OPTION* FindOption (const char* pcszName_)
{
    // Convert AutoBoot to AutoLoad, for backwards compatibility
    if (!strcasecmp(pcszName_, "AutoBoot"))
        pcszName_ = "AutoLoad";

    for (OPTION* p = aOptions ; p->pcszName ; p++)
    {
        if (!strcasecmp(pcszName_, p->pcszName))
            return p;
    }

    return NULL;
}

// Set (optionally unspecified) options to their default values
void Options::SetDefaults (bool fForce_/*=true*/)
{
    // Process the full options list
    for (OPTION* p = aOptions ; p->pcszName ; p++)
    {
        // Set the default if forcing defaults, or if we've not already
        if (fForce_ || !p->fSpecified)
        {
            switch (p->nType)
            {
                case OT_BOOL:       *p->pf = p->fDefault;   break;
                case OT_INT:        *p->pn = p->nDefault;   break;
                case OT_STRING:     strcpy(p->ppsz, p->pcszDefault);   break;
            }
        }
    }

    // Force the current compatability number
    SetOption(cfgversion,CFG_VERSION);
}

// Find the address of the variable holding the specified option default
void* Options::GetDefault (const char* pcszName_)
{
    OPTION* p = FindOption(pcszName_);

    if (p)
    {
        switch (p->nType)
        {
            case OT_BOOL:       return &p->fDefault;
            case OT_INT:        return &p->nDefault;
//          case OT_STRING:     return &p->pcszDefault;     // Don't use - points to read-only string table!
        }
    }

    // This should never happen, thanks to a compile-time check in the header
    static void* pv = NULL;
    return &pv;
}

bool Options::Load (int argc_, char* argv_[])
{
    FILE* hfOptions = fopen(OSD::MakeFilePath(MFP_SETTINGS, OPTIONS_FILE), "r");
    if (hfOptions)
    {
        char szLine[256];
        while (fgets(szLine, sizeof(szLine), hfOptions))
        {
            char *pszValue = strchr(szLine, '=');
            char *pszName = strtok(szLine, " \t=");

            if (!pszName || !pszValue)
                continue;

            // Skip delimiters up to the value, and take the value up to a <CR> or <LF>
            pszValue++;
            strtok(pszValue += strspn(pszValue, " \t=\r\n"), "\r\n");

            // Look for the option in the list
            for (OPTION* p = aOptions ; p->pcszName ; p++)
            {
                if (!strcasecmp(pszName, p->pcszName))
                {
                    // Remember that a value has been found for this option
                    p->fSpecified = true;

                    // Extract the appropriate value type from the string
                    switch (p->nType)
                    {
                        case OT_BOOL:       *p->pf = *pszValue ? IsTrue(pszValue) : p->fDefault;    break;
                        case OT_INT:        *p->pn = *pszValue ? atoi(pszValue) : p->nDefault;      break;
                        case OT_STRING:     strcpy(p->ppsz, *pszValue ? pszValue : p->pcszDefault); break;
                    }
                }
            }
        }

        // We're done with the file
        fclose(hfOptions);
    }

    // Set the default values for any missing options, or all if the config version has changed
    bool fIncompatible = GetOption(cfgversion) != CFG_VERSION;
    SetDefaults(fIncompatible);

    // Process any commmand-line arguments to look for options
    while (argc_ && --argc_)
    {
        const char* pcszOption = *++argv_;
        if (*pcszOption == '-')
        {
            // Find the option in the list of known options
            OPTION* p = FindOption(pcszOption+1);

            if (p)
            {
                switch (p->nType)
                {
                    case OT_BOOL:   *p->pf = (argv_[1] && *argv_[1] == '-') || (argc_-- && IsTrue(*++argv_)); continue;
                    case OT_INT:    *p->pn = atoi(*++argv_);    break;
                    case OT_STRING: strcpy(p->ppsz, *++argv_);  break;
                }

                argc_--;
            }
            else
                TRACE("Unknown command-line option: %s\n", pcszOption);
        }
        else
        {
            static int nDrive = 1;

            // Bare filenames will be inserted into drive 1 then 2
            switch (nDrive++)
            {
                case 1:
                    SetOption(disk1, pcszOption);
                    SetOption(drive1,drvFloppy);
                    g_nAutoLoad = AUTOLOAD_DISK;
                    break;

                case 2:
                    SetOption(disk2, pcszOption);
                    SetOption(drive2,drvFloppy);
                    break;

                default:
                    TRACE("Unexpected command-line parameter: %s\n", pcszOption);
                    break;
            }
        }
    }

    return true;
}


bool Options::Save ()
{
    const char *pcszPath = OSD::MakeFilePath(MFP_SETTINGS, OPTIONS_FILE);

    // Open the options file for writing, fail if we can't
    FILE* hfOptions = fopen(pcszPath, "wb");
    if (!hfOptions)
        return false;

    // Some settings shouldn't be saved
    SetOption(speed, 100);

    // Loop through each option to write out
    for (OPTION* p = aOptions ; p->pcszName ; p++)
    {
        switch (p->nType)
        {
            case OT_BOOL:       fprintf(hfOptions, "%s=%s\r\n", p->pcszName, *p->pf ? "Yes" : "No");  break;
            case OT_INT:        fprintf(hfOptions, "%s=%d\r\n", p->pcszName, *p->pn);                 break;
            case OT_STRING:     fprintf(hfOptions, "%s=%s\r\n", p->pcszName, p->ppsz);                break;
        }
    }

    fclose(hfOptions);
    return true;
}
