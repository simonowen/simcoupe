// Part of SimCoupe - A SAM Coupe emulator
//
// Options.cpp: Option saving, loading and command-line processing
//
//  Copyright (c) 1999-2004  Simon Owen
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

const char* const OPTIONS_FILE = "SimCoupe.cfg";
const int CFG_VERSION = 2;      // increment to force a config reset, if incompatible changes are made

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

    OPT_N("Sync",         sync,           1),         // Sync to 50Hz
    OPT_N("FrameSkip",    frameskip,      0),         // Auto frame-skipping
    OPT_N("Scale",        scale,          2),         // Windowed display is 2x2
    OPT_F("Ratio5_4",     ratio5_4,       false),     // Don't use 5:4 screen ratio
    OPT_F("Scanlines",    scanlines,      false),     // Don't use scanlines
    OPT_N("Mode3",        mode3,          0),         // Show only odd mode3 pixels on low-res displays
    OPT_N("Fullscreen",   fullscreen,     0),         // Not full screen
    OPT_N("Depth",        depth,          16),        // Full screen mode uses 16-bit colour
    OPT_N("Borders",      borders,        2),         // Same amount of borders as previous version
    OPT_F("StretchToFit", stretchtofit,   true),      // Stretch image to fit the display area
    OPT_F("Filter",       filter,         true),      // Filter the stretched image (OpenGL only)
    OPT_F("Overlay",      overlay,        true),      // Use a video overlay surface, if available
    OPT_N("Surface",      surface,        999),       // Try for the best possible by default

    OPT_S("ROM",          rom,            ""),        // No custom ROM (use built-in)
    OPT_F("HDBootRom",    hdbootrom,      false),     // Don't use HDBOOT ROM patches
    OPT_F("FastReset",    fastreset,      true),      // Allow fast Z80 resets
    OPT_F("AsicDelay",    asicdelay,      false),     // No ASIC startup delay of ~50ms
    OPT_N("MainMemory",   mainmem,        512),       // 512K main memory
    OPT_N("ExternalMem",  externalmem,    0),         // No external memory

    OPT_N("Drive1",       drive1,         1),         // Floppy drive 1 present
    OPT_N("Drive2",       drive2,         1),         // Floppy drive 2 present
    OPT_F("AutoBoot",     autoboot,       true),      // Autoboot disks inserted at the startup screen
    OPT_N("TurboLoad",    turboload,      15),        // Accelerate disk access (medium sensitivity)

    OPT_S("Disk1",        disk1,          ""),        // No disk in floppy drive 1
    OPT_S("Disk2",        disk2,          ""),        // No disk in floppy drive 2
    OPT_S("AtomDisk",     atomdisk,       ""),        // No Atom hard disk
    OPT_S("SDIDEDisk",    sdidedisk,      ""),        // No SD IDE hard disk
    OPT_S("YATBusDisk",   yatbusdisk,     ""),        // No YAMOD.ATBUS disk

    OPT_S("FloppyPath",   floppypath,     ""),        // Default floppy path
    OPT_S("HDDPath",      hddpath,        ""),        // Default hard disk path
    OPT_S("ROMPath",      rompath,        ""),        // Default ROM path
    OPT_S("DataPath",     datapath,       ""),        // Default data path

    OPT_N("KeyMapping",   keymapping,     1),         // SAM keyboard mapping
    OPT_F("AltForCntrl",  altforcntrl,    false),     // Left-Alt not used for SAM Cntrl
    OPT_F("AltGrForEdit", altgrforedit,   true),      // Right-Alt used for SAM Edit
    OPT_F("KeypadReset",  keypadreset,    true),      // Keypad-minus for Reset
    OPT_F("SAMFKeys",     samfkeys,       false),     // PC function keys not mapped to SAM keypad
    OPT_F("Mouse",        mouse,          false),     // Mouse not connected

    OPT_S("JoyDev1",      joydev1,        ""),        // Joystick 1 device
    OPT_S("JoyDev2",      joydev2,        ""),        // Joystick 2 device
    OPT_N("DeadZone1",    deadzone1,      20),        // Joystick 1 deadzone is 20% around central position
    OPT_N("DeadZone2",    deadzone2,      20),        // Joystick 2 deadzone is 20% around central position

    OPT_N("Parallel1",    parallel1,      0),         // Nothing on parallel port 1
    OPT_N("Parallel2",    parallel2,      0),         // Nothing on parallel port 2
    OPT_S("PrinterDev",   printerdev,     ""),        // Printer device
    OPT_F("PrinterOnline",printeronline,  true),      // Printer online

    OPT_S("SerialDev1",   serialdev1,     ""),        // Serial port 1 device
    OPT_S("SerialDev2",   serialdev2,     ""),        // Serial port 2 device

    OPT_N("Midi",         midi,           0),         // Nothing on MIDI port
    OPT_S("MidiInDev",    midiindev,      ""),        // MIDI-In device
    OPT_S("MidiOutDev",   midioutdev,     ""),        // MIDI-Out device
//  OPT_N("NetworkId",    networkid,      1),         // Network station number, or something, eventually

    OPT_F("SambusClock",  sambusclock,    true),      // SAMBUS clock present
    OPT_F("DallasClock",  dallasclock,    false),     // DALLAS clock not present
    OPT_F("ClockSync",    clocksync,      true),      // Clocks advanced relative to real time

    OPT_F("Sound",        sound,          true),      // Sound enabled
    OPT_F("SAASound",     saasound,       true),      // SAA 1099 sound chip enabled
    OPT_F("Beeper",       beeper,         true),      // Spectrum-style beeper enabled

    OPT_N("Frequency",    freq,           44100),     // 44.1KHz
    OPT_N("Bits",         bits,           16),        // 16-bit
    OPT_F("Stereo",       stereo,         true),      // Stereo
    OPT_N("Latency",      latency,        5),         // Sound latency of five frames

    OPT_N("DriveLights",  drivelights,    1),         // Show drive activity lights
    OPT_N("Profile",      profile,        1),         // Show only speed and framerate
    OPT_F("Status",       status,         true),      // Show status line for changed options, etc.

    OPT_F("PauseInactive",pauseinactive,  false),     // Continue to run when inactive

    OPT_S("LogFile",      logfile,        ""),        // No logfile
    OPT_S("FnKeys",       fnkeys,
     "F1=12,SF1=13,AF1=35,CF1=14,F2=15,SF2=16,AF2=36,CF2=17,F3=28,SF3=11,CF3=10,F4=22,AF4=25,SF4=23,F5=5,SF5=7,F6=8,F7=6,F8=4,F9=9,SF9=19,F10=24,F11=0,F12=1,SF12=1,CF12=25"),

    { NULL, 0 }
};

inline bool IsTrue (const char* pcsz_)
{
    return pcsz_ && (!strcasecmp(pcsz_, "true") || !strcasecmp(pcsz_, "on") || !strcasecmp(pcsz_, "enabled") || 
                    !strcasecmp(pcsz_, "yes") || !strcasecmp(pcsz_, "1"));
}

// Find a named option in the options list above
OPTION* FindOption (const char* pcszName_)
{
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
//          case OT_STRING:     return &p->pcszDefault;     // Don't use - points to string table!
        }
    }

    // This should never happen, thanks to a compile-time check in the header
    static void* pv = NULL;
    return &pv;
}

bool Options::Load (int argc_, char* argv_[])
{
    FILE* hfOptions = fopen(OSD::GetFilePath(OPTIONS_FILE), "rb");
    if (hfOptions)
    {
        char szLine[256];
        while (fgets(szLine, sizeof szLine, hfOptions))
        {
            char *pszValue = strchr(szLine, '='), *pszName = strtok(szLine, " \t=");

            if (!pszValue)
                continue;

            // Skip delimiters up to the value, and take the value up to a <CR> or <LF>
            strtok(pszValue += strspn(++pszValue, " \t=\r\n"), "\r\n");

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
        if (*pcszOption == '-' || *pcszOption == '/')
        {
            // Find the option in the list of known options
            OPTION* p = FindOption(pcszOption+1);

            if (p)
            {
                switch (p->nType)
                {
                    case OT_BOOL:
                    {
                        *p->pf = (argv_[1] && *argv_[1] == '-') || (argc_-- && IsTrue(*++argv_));

                        // For backwards compatability we must force the autoboot option if it's enabled
                        if (!strcasecmp(p->pcszName, "AutoBoot"))
                            g_fAutoBoot |= *p->pf;

                        continue;
                    }

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
                case 1:  SetOption(disk1, pcszOption);  g_fAutoBoot = true;             break;
                case 2:  SetOption(disk2, pcszOption);                                  break;
                default: TRACE("Unexpected command-line parameter: %s\n", pcszOption);  break;
            }
        }
    }

    return true;
}


bool Options::Save ()
{
    // Open the options file for writing, fail if we can't
    FILE* hfOptions = fopen(OSD::GetFilePath(OPTIONS_FILE), "wb");
    if (!hfOptions)
        return false;

    // Loop through each option to write out
    for (OPTION* p = aOptions ; p->pcszName ; p++)
    {
        switch (p->nType)
        {
            case OT_BOOL:       fprintf(hfOptions, "%s=%s\n", p->pcszName, *p->pf ? "Yes" : "No");  break;
            case OT_INT:        fprintf(hfOptions, "%s=%d\n", p->pcszName, *p->pn);                 break;
            case OT_STRING:     fprintf(hfOptions, "%s=%s\n", p->pcszName, p->ppsz);                break;
        }
    }

    fclose(hfOptions);
    return true;
}
