// Part of SimCoupe - A SAM Coupé emulator
//
// Options.cpp: Option saving, loading and command-line processing
//
//  Copyright (c) 1999-2001  Simon Owen
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

#include "OSD.h"
#include "Util.h"

const char* const OPTIONS_FILE = "SimCoupe.cfg";

enum { OT_BOOL, OT_INT, OT_STRING };

typedef struct
{
    const char* pcszName;                                       // Option name used in config file
    int         nType;                                          // Option type

    union { void* pv;  char* ppsz;  bool* pf;  int* pn; };      // Address of config variable
    union { ULONGLONG pu; DWORD dw;  bool f;  int n;  const char* pcsz; };    // Default value of option

    bool        fSpecified;
}
OPTION;

// Helper macro for structure definition below
#define OPT(n,t,v,d)        { n, t, {(void*)&Options::s_Options.v}, {(DWORD)(d)} }


OPTIONS Options::s_Options;

OPTION aOptions[] = 
{
    OPT("LogFile",      OT_STRING,  logfile,        ""),        // No logfile

    OPT("FastReset",    OT_BOOL,    fastreset,      true),      // Allow fast Z80 resets

    OPT("Sync",         OT_INT,     sync,           1),         // Sync to 50Hz
    OPT("FrameSkip",    OT_INT,     frameskip,      0),         // Auto frame-skipping
    OPT("Scale",        OT_INT,     scale,          2),         // Windowed display is 2x2
    OPT("Ratio5_4",     OT_BOOL,    ratio5_4,       false),     // Don't use 5:4 screen ratio
    OPT("Scanlines",    OT_BOOL,    scanlines,      false),     // Don't use scanlines
    OPT("Fullscreen",   OT_BOOL,    fullscreen,     false),     // Not full screen
    OPT("Depth",        OT_INT,     depth,          16),        // Full screen mode uses 16-bit colour
    OPT("Borders",      OT_INT,     borders,        2),         // Same amount of borders as previous version
    OPT("StretchToFit", OT_BOOL,    stretchtofit,   true),      // Stretch image to fit
    OPT("Surface",      OT_INT,     surface,        999),       // Try for the best possible by default

    OPT("ROM0",         OT_STRING,  rom0,           "sam_rom0.rom"),
    OPT("ROM1",         OT_STRING,  rom1,           "sam_rom1.rom"),
    OPT("MainMemory",   OT_INT,     mainmem,        512),       // 512K main memory
    OPT("ExternalMem",  OT_INT,     externalmem,    0),         // No external memory

    OPT("Disk1",        OT_STRING,  disk1,          "disk1"),   // Disk for floppy drive 1
    OPT("Disk2",        OT_STRING,  disk2,          "disk2"),   // Disk for floppy drive 2
    OPT("Drive1",       OT_INT,     drive1,         1),         // Floppy drive 1 present
    OPT("Drive2",       OT_INT,     drive2,         1),         // Floppy drive 2 present

    OPT("KeyMapping",   OT_INT,     keymapping,     1),         // SAM keyboard mapping
    OPT("AltForCntrl",  OT_BOOL,    altforcntrl,    false),     // Left-Alt not used for SAM Cntrl
    OPT("AltGrForEdit", OT_BOOL,    altgrforedit,   true),      // Right-Alt used for SAM Edit
    OPT("Mouse",        OT_INT,     mouse,          1),         // Mouse connected

    OPT("JoyDev1",      OT_STRING,  joydev1,        ""),        // Joystick 1 device
    OPT("JoyDev2",      OT_STRING,  joydev2,        ""),        // Joystick 2 device
    OPT("DeadZone1",    OT_INT,     deadzone1,      20),        // Joystick 1 deadzone is 20% around central position
    OPT("DeadZone2",    OT_INT,     deadzone2,      20),        // Joystick 2 deadzone is 20% around central position

    OPT("Parallel1",    OT_INT,     parallel1,      0),         // Nothing on parallel port 1
    OPT("Parallel2",    OT_INT,     parallel2,      0),         // Nothing on parallel port 2
    OPT("PrinterDev",   OT_STRING,  printerdev,     ""),        // Printer device

    OPT("SerialDev1",   OT_STRING,  serialdev1,     ""),        // Serial port 1 device
    OPT("SerialDev2",   OT_STRING,  serialdev2,     ""),        // Serial port 2 device

    OPT("Midi",         OT_INT,     midi,           1),         // Used for MIDI music, if available
    OPT("MidiIn",       OT_INT,     midiin,         -1),        // MIDI-In device number
    OPT("MidiOut",      OT_INT,     midiout,        -1),        // MIDI-Out device number
//  OPT("NetworkId",    OT_INT,     networkid,      1),         // Network station number, or something, eventually

    OPT("SambusClock",  OT_BOOL,    sambusclock,    true),      // SAMBUS clock present
    OPT("DallasClock",  OT_BOOL,    dallasclock,    false),     // DALLAS clock not present
    OPT("ClockSync",    OT_BOOL,    clocksync,      true),      // Clocks advanced relative to real time

    OPT("Sound",        OT_BOOL,    sound,          true),      // Sound enabled
    OPT("Beeper",       OT_BOOL,    beeper,         true),      // Spectrum-style beeper enabled

    OPT("SAASound",     OT_BOOL,    saasound,       true),      // SAA 1099 sound chip enabled
    OPT("Frequency",    OT_INT,     freq,		    22050),     // 22KHz
    OPT("Bits",         OT_INT,     bits,           16),        // 16-bit
    OPT("Stereo",       OT_BOOL,    stereo,         true),      // Stereo
    OPT("Filter",       OT_BOOL,    filter,         false),     // Sound filter disabled (not implemented by SAASOUND yet)
    OPT("Latency",      OT_INT,     latency,        5),         // Sound latency of five frames

    OPT("DriveLights",  OT_INT,     drivelights,    1),         // Show drive activity lights
    OPT("Profile",      OT_INT,     profile,        1),         // Show only speed and framerate
    OPT("Status",       OT_BOOL,    status,         true),      // Show status line for changed options, etc.

    OPT("FnKeys",       OT_STRING,  fnkeys,
     "F1=12,SF1=13,CF1=14,F2=15,SF2=16,CF2=17,F3=28,SF3=11,CF3=10,F4=22,SF4=23,F5=5,SF5=7,F6=8,F7=6,F8=4,F9=9,SF9=19,F10=24,F11=0,F12=1,CF12=25"),

    OPT("PauseInactive",OT_BOOL,    pauseinactive,  false),     // Continue to run when inactive

    OPT("AutoBoot",     OT_BOOL,    autoboot,       false),     // Don't auto-boot inserted disk

    { 0, 0, {0}, {0} }
};

inline bool IsTrue (const char* pcsz_)
{
    return pcsz_ && (!strcasecmp(pcsz_, "true") || !strcasecmp(pcsz_, "on") || !strcasecmp(pcsz_, "enabled") || 
                    !strcasecmp(pcsz_, "yes") || !strcasecmp(pcsz_, "1"));
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
                        case OT_BOOL:       *p->pf = *pszValue ? IsTrue(pszValue) : p->f;       break;
                        case OT_INT:        *p->pn = *pszValue ? atoi(pszValue) : p->n;         break;
                        case OT_STRING:     strcpy(p->ppsz, *pszValue ? pszValue : p->pcsz);    break;
                    }
                }
            }
        }

        // We're done with the file
        fclose(hfOptions);
    }

    // Look through the option list again
    for (OPTION* p = aOptions ; p->pcszName ; p++)
    {
        // If a value for this option wasn't found in the cfg file, use the default
        if (!p->fSpecified)
        {
            switch (p->nType)
            {
                case OT_BOOL:       *p->pf = p->f;              break;
                case OT_INT:        *p->pn = p->n;              break;
                case OT_STRING:     strcpy(p->ppsz, p->pcsz);   break;
            }
        }
    }


    // Process any commmand-line arguments to look for options
    while (argc_ && --argc_)
    {
        const char* pcszOption = *++argv_;
        if (*pcszOption++ == '-')
        {
            // Find the option in the list of known options
            OPTION* p;
            for (p = aOptions ; p->pcszName && strcasecmp(p->pcszName, pcszOption); p++);

            if (p->pcszName)
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
            TRACE("Invalid command-line parameter: %s\n", pcszOption);
    }

    return true;
}


bool Options::Save ()
{
    // Some options are not persistant
    SetOption(autoboot,false);

	// Open the options file for writing, fail if we can't
    FILE* hfOptions = fopen(OSD::GetFilePath(OPTIONS_FILE), "wb");
    if (!hfOptions)
        return false;

    // Try to keep the file as backwards compatible with older Win32 files
    fputs("[Options]\n", hfOptions);

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
