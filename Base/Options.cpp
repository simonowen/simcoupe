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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

// Notes:
//  Options specified on the command-line override options in the file.
//  The settings are only and written back when it's closed.

#include "SimCoupe.h"
#include "Options.h"

#include "SAMIO.h"

namespace Options
{
Config g_config;


static void SetValue(int& value, const std::string& str)
{
    try {
        value = std::stoi(str);
    } catch (...) {
        // keep default value
    }
}

static void SetValue(bool& value, const std::string& str)
{
    value = str == "1" || tolower(str) == "yes";
}

static void SetValue(std::string& value, const std::string_view& sv)
{
    value = sv;
}

static bool SetNamedValue(const std::string& option_name, const std::string& str)
{
    auto name = trim(tolower(option_name));

    if (name == "cfgversion") SetValue(g_config.cfgversion, str);
    else if (name == "firstrun") SetValue(g_config.firstrun, str);
    else if (name == "windowpos") SetValue(g_config.windowpos, str);
    else if (name == "tvaspect") SetValue(g_config.tvaspect, str);
    else if (name == "fullscreen") SetValue(g_config.fullscreen, str);
    else if (name == "visiblearea") SetValue(g_config.visiblearea, str);
    else if (name == "smooth") SetValue(g_config.smooth, str);
    else if (name == "motionblur") SetValue(g_config.motionblur, str);
    else if (name == "allowmotionblur") SetValue(g_config.allowmotionblur, str);
    else if (name == "blurpercent") SetValue(g_config.blurpercent, str);
    else if (name == "maxintensity") SetValue(g_config.maxintensity, str);
    else if (name == "blackborder") SetValue(g_config.blackborder, str);
    else if (name == "tryvrr") SetValue(g_config.tryvrr, str);
    else if (name == "gifframeskip") SetValue(g_config.gifframeskip, str);
    else if (name == "rom") SetValue(g_config.rom, str);
    else if (name == "romwrite") SetValue(g_config.romwrite, str);
    else if (name == "atombootrom") SetValue(g_config.atombootrom, str);
    else if (name == "fastreset") SetValue(g_config.fastreset, str);
    else if (name == "asicdelay") SetValue(g_config.asicdelay, str);
    else if (name == "mainmem") SetValue(g_config.mainmem, str);
    else if (name == "externalmem") SetValue(g_config.externalmem, str);
    else if (name == "cmosz80") SetValue(g_config.cmosz80, str);
    else if (name == "speed") SetValue(g_config.speed, str);
    else if (name == "drive1") SetValue(g_config.drive1, str);
    else if (name == "drive2") SetValue(g_config.drive2, str);
    else if (name == "turbodisk") SetValue(g_config.turbodisk, str);
    else if (name == "dosboot") SetValue(g_config.dosboot, str);
    else if (name == "dosdisk") SetValue(g_config.dosdisk, str);
    else if (name == "stdfloppy") SetValue(g_config.stdfloppy, str);
    else if (name == "nextfile") SetValue(g_config.nextfile, str);
    else if (name == "turbotape") SetValue(g_config.turbotape, str);
    else if (name == "tapetraps") SetValue(g_config.tapetraps, str);
    else if (name == "disk1") SetValue(g_config.disk1, str);
    else if (name == "disk2") SetValue(g_config.disk2, str);
    else if (name == "atomdisk0") SetValue(g_config.atomdisk0, str);
    else if (name == "atomdisk1") SetValue(g_config.atomdisk1, str);
    else if (name == "sdidedisk") SetValue(g_config.sdidedisk, str);
    else if (name == "tape") SetValue(g_config.tape, str);
    else if (name == "autoload") SetValue(g_config.autoload, str);
    else if (name == "autoboot") SetValue(g_config.autoboot, str);
    else if (name == "diskerrorfreq") SetValue(g_config.diskerrorfreq, str);
    else if (name == "samdiskhelper") SetValue(g_config.samdiskhelper, str);
    else if (name == "inpath") SetValue(g_config.inpath, str);
    else if (name == "outpath") SetValue(g_config.outpath, str);
    else if (name == "mru0") SetValue(g_config.mru0, str);
    else if (name == "mru1") SetValue(g_config.mru1, str);
    else if (name == "mru2") SetValue(g_config.mru2, str);
    else if (name == "mru3") SetValue(g_config.mru3, str);
    else if (name == "mru4") SetValue(g_config.mru4, str);
    else if (name == "mru5") SetValue(g_config.mru5, str);
    else if (name == "mru6") SetValue(g_config.mru6, str);
    else if (name == "mru7") SetValue(g_config.mru7, str);
    else if (name == "mru8") SetValue(g_config.mru8, str);
    else if (name == "keymapping") SetValue(g_config.keymapping, str);
    else if (name == "altforcntrl") SetValue(g_config.altforcntrl, str);
    else if (name == "altgrforedit") SetValue(g_config.altgrforedit, str);
    else if (name == "mouse") SetValue(g_config.mouse, str);
    else if (name == "mouseesc") SetValue(g_config.mouseesc, str);
    else if (name == "joydev1") SetValue(g_config.joydev1, str);
    else if (name == "joydev2") SetValue(g_config.joydev2, str);
    else if (name == "joytype1") SetValue(g_config.joytype1, str);
    else if (name == "joytype2") SetValue(g_config.joytype2, str);
    else if (name == "deadzone1") SetValue(g_config.deadzone1, str);
    else if (name == "deadzone2") SetValue(g_config.deadzone2, str);
    else if (name == "parallel1") SetValue(g_config.parallel1, str);
    else if (name == "parallel2") SetValue(g_config.parallel2, str);
    else if (name == "printeronline") SetValue(g_config.printeronline, str);
    else if (name == "flushdelay") SetValue(g_config.flushdelay, str);
    else if (name == "midi") SetValue(g_config.midi, str);
    else if (name == "midiindev") SetValue(g_config.midiindev, str);
    else if (name == "midioutdev") SetValue(g_config.midioutdev, str);
    else if (name == "sambusclock") SetValue(g_config.sambusclock, str);
    else if (name == "dallasclock") SetValue(g_config.dallasclock, str);
    else if (name == "audiosync") SetValue(g_config.audiosync, str);
    else if (name == "latency") SetValue(g_config.latency, str);
    else if (name == "dac7c") SetValue(g_config.dac7c, str);
    else if (name == "samplerfreq") SetValue(g_config.samplerfreq, str);
    else if (name == "voicebox") SetValue(g_config.voicebox, str);
    else if (name == "sid") SetValue(g_config.sid, str);
    else if (name == "drivelights") SetValue(g_config.drivelights, str);
    else if (name == "profile") SetValue(g_config.profile, str);
    else if (name == "status") SetValue(g_config.status, str);
    else if (name == "breakonexec") SetValue(g_config.breakonexec, str);
    else if (name == "fkeys") SetValue(g_config.fkeys, str);
    else if (name == "rasterdebug") SetValue(g_config.rasterdebug, str);
    else
    {
        return false;
    }

    return true;
}

bool Load(int argc_, char* argv_[])
{
    // Set defaults.
    g_config = {};

    auto path = OSD::MakeFilePath(PathType::Settings, OPTIONS_FILE);
    std::ifstream file(path);
    for (std::string line; std::getline(file, line); )
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::stringstream ss(line);
        std::string key, value;
        if (std::getline(ss, key, '=') && std::getline(ss, value))
        {
            if (!SetNamedValue(key, value))
            {
                TRACE("Unknown setting: {}={}", key, value);
            }
        }
    }

    // If the loaded configuration is incompatible, reset to defaults.
    if (g_config.cfgversion != ConfigVersion)
        g_config = {};

    auto drive_arg = 1;
    g_config.autoboot = true;

    while (argc_ && --argc_)
    {
        auto pcszOption = *++argv_;
        if (*pcszOption == '-')
        {
            pcszOption++;
            argc_--;

            if (argc_ <= 0 || !SetNamedValue(pcszOption, *++argv_))
            {
                TRACE("Unknown command-line option: {}\n", pcszOption);
            }
        }
        else
        {
            // Bare filenames will be inserted into drive 1 then 2
            switch (drive_arg++)
            {
            case 1:
                SetOption(disk1, pcszOption);
                SetOption(drive1, drvFloppy);
                break;

            case 2:
                SetOption(disk2, pcszOption);
                SetOption(drive2, drvFloppy);
                break;

            default:
                TRACE("Unexpected command-line parameter: {}\n", pcszOption);
                break;
            }
        }
    }

    if (drive_arg > 1)
        IO::QueueAutoBoot(AutoLoadType::Disk);

    return true;
}

bool Save()
{
    auto path = OSD::MakeFilePath(PathType::Settings, OPTIONS_FILE);
    try
    {
        std::ofstream ofs(path, std::ofstream::out);
        ofs << std::noboolalpha;

        using namespace std;
        ofs << "cfgversion=" << to_string(g_config.cfgversion) << std::endl;
        ofs << "firstrun=" << to_string(g_config.firstrun) << std::endl;
        ofs << "windowpos=" << to_string(g_config.windowpos) << std::endl;
        ofs << "tvaspect=" << to_string(g_config.tvaspect) << std::endl;
        ofs << "fullscreen=" << to_string(g_config.fullscreen) << std::endl;
        ofs << "visiblearea=" << to_string(g_config.visiblearea) << std::endl;
        ofs << "smooth=" << to_string(g_config.smooth) << std::endl;
        ofs << "motionblur=" << to_string(g_config.motionblur) << std::endl;
        ofs << "allowmotionblur=" << to_string(g_config.allowmotionblur) << std::endl;
        ofs << "blurpercent=" << to_string(g_config.blurpercent) << std::endl;
        ofs << "maxintensity=" << to_string(g_config.maxintensity) << std::endl;
        ofs << "blackborder=" << to_string(g_config.blackborder) << std::endl;
        ofs << "tryvrr=" << to_string(g_config.tryvrr) << std::endl;
        ofs << "gifframeskip=" << to_string(g_config.gifframeskip) << std::endl;
        ofs << "rom=" << to_string(g_config.rom) << std::endl;
        ofs << "romwrite=" << to_string(g_config.romwrite) << std::endl;
        ofs << "atombootrom=" << to_string(g_config.atombootrom) << std::endl;
        ofs << "fastreset=" << to_string(g_config.fastreset) << std::endl;
        ofs << "asicdelay=" << to_string(g_config.asicdelay) << std::endl;
        ofs << "mainmem=" << to_string(g_config.mainmem) << std::endl;
        ofs << "externalmem=" << to_string(g_config.externalmem) << std::endl;
        ofs << "cmosz80=" << to_string(g_config.cmosz80) << std::endl;
        ofs << "speed=" << to_string(g_config.speed) << std::endl;
        ofs << "drive1=" << to_string(g_config.drive1) << std::endl;
        ofs << "drive2=" << to_string(g_config.drive2) << std::endl;
        ofs << "turbodisk=" << to_string(g_config.turbodisk) << std::endl;
        ofs << "dosboot=" << to_string(g_config.dosboot) << std::endl;
        ofs << "dosdisk=" << to_string(g_config.dosdisk) << std::endl;
        ofs << "stdfloppy=" << to_string(g_config.stdfloppy) << std::endl;
        ofs << "nextfile=" << to_string(g_config.nextfile) << std::endl;
        ofs << "turbotape=" << to_string(g_config.turbotape) << std::endl;
        ofs << "tapetraps=" << to_string(g_config.tapetraps) << std::endl;
        ofs << "disk1=" << to_string(g_config.disk1) << std::endl;
        ofs << "disk2=" << to_string(g_config.disk2) << std::endl;
        ofs << "atomdisk0=" << to_string(g_config.atomdisk0) << std::endl;
        ofs << "atomdisk1=" << to_string(g_config.atomdisk1) << std::endl;
        ofs << "sdidedisk=" << to_string(g_config.sdidedisk) << std::endl;
        ofs << "tape=" << to_string(g_config.tape) << std::endl;
        ofs << "autoload=" << to_string(g_config.autoload) << std::endl;
        ofs << "diskerrorfreq=" << to_string(g_config.diskerrorfreq) << std::endl;
        ofs << "samdiskhelper=" << to_string(g_config.samdiskhelper) << std::endl;
        ofs << "inpath=" << to_string(g_config.inpath) << std::endl;
        ofs << "outpath=" << to_string(g_config.outpath) << std::endl;
        ofs << "mru0=" << to_string(g_config.mru0) << std::endl;
        ofs << "mru1=" << to_string(g_config.mru1) << std::endl;
        ofs << "mru2=" << to_string(g_config.mru2) << std::endl;
        ofs << "mru3=" << to_string(g_config.mru3) << std::endl;
        ofs << "mru4=" << to_string(g_config.mru4) << std::endl;
        ofs << "mru5=" << to_string(g_config.mru5) << std::endl;
        ofs << "mru6=" << to_string(g_config.mru6) << std::endl;
        ofs << "mru7=" << to_string(g_config.mru7) << std::endl;
        ofs << "mru8=" << to_string(g_config.mru8) << std::endl;
        ofs << "keymapping=" << to_string(g_config.keymapping) << std::endl;
        ofs << "altforcntrl=" << to_string(g_config.altforcntrl) << std::endl;
        ofs << "altgrforedit=" << to_string(g_config.altgrforedit) << std::endl;
        ofs << "mouse=" << to_string(g_config.mouse) << std::endl;
        ofs << "mouseesc=" << to_string(g_config.mouseesc) << std::endl;
        ofs << "joydev1=" << to_string(g_config.joydev1) << std::endl;
        ofs << "joydev2=" << to_string(g_config.joydev2) << std::endl;
        ofs << "joytype1=" << to_string(g_config.joytype1) << std::endl;
        ofs << "joytype2=" << to_string(g_config.joytype2) << std::endl;
        ofs << "deadzone1=" << to_string(g_config.deadzone1) << std::endl;
        ofs << "deadzone2=" << to_string(g_config.deadzone2) << std::endl;
        ofs << "parallel1=" << to_string(g_config.parallel1) << std::endl;
        ofs << "parallel2=" << to_string(g_config.parallel2) << std::endl;
        ofs << "printeronline=" << to_string(g_config.printeronline) << std::endl;
        ofs << "flushdelay=" << to_string(g_config.flushdelay) << std::endl;
        ofs << "midi=" << to_string(g_config.midi) << std::endl;
        ofs << "midiindev=" << to_string(g_config.midiindev) << std::endl;
        ofs << "midioutdev=" << to_string(g_config.midioutdev) << std::endl;
        ofs << "sambusclock=" << to_string(g_config.sambusclock) << std::endl;
        ofs << "dallasclock=" << to_string(g_config.dallasclock) << std::endl;
        ofs << "audiosync=" << to_string(g_config.audiosync) << std::endl;
        ofs << "latency=" << to_string(g_config.latency) << std::endl;
        ofs << "dac7c=" << to_string(g_config.dac7c) << std::endl;
        ofs << "samplerfreq=" << to_string(g_config.samplerfreq) << std::endl;
        ofs << "voicebox=" << to_string(g_config.voicebox) << std::endl;
        ofs << "sid=" << to_string(g_config.sid) << std::endl;
        ofs << "drivelights=" << to_string(g_config.drivelights) << std::endl;
        ofs << "profile=" << to_string(g_config.profile) << std::endl;
        ofs << "status=" << to_string(g_config.status) << std::endl;
        ofs << "breakonexec=" << to_string(g_config.breakonexec) << std::endl;
        ofs << "fkeys=" << to_string(g_config.fkeys) << std::endl;
        ofs << "rasterdebug=" << to_string(g_config.rasterdebug) << std::endl;
    }
    catch (...)
    {
        TRACE("Failed to save options\n");
        return false;
    }

    return true;
}

} // namespace Options
