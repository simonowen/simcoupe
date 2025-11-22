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

namespace
{

using Options::g_config;

template<typename T>
void write_option(std::ofstream& ofs, const std::string& name, const T& value, const T& default_value)
{
    if (value != default_value)
    {
        ofs << name << '=';

        if constexpr (std::is_same_v<T, bool>)
        {
            ofs << (value ? "1" : "0");
        }
        else
        {
            ofs << value;
        }

        ofs << '\n';
    }
}

template<typename T>
void set_value(T& value, const std::string& str)
{
    if constexpr (std::is_same_v<T, bool>)
    {
        value = (str == "1");
    }
    else if constexpr (std::is_same_v<T, int>)
    {
        try {
            value = std::stoi(str);
        }
        catch (...) {
            // keep default
        }
    }
    else
    {
        value = str;
    }
}

auto set_named_value(const std::string& option_name, const std::string& str) -> bool
{
    const auto name{ trim(tolower(option_name)) };

    if (name == "cfgversion") { set_value(g_config.cfgversion, str); }
    else if (name == "firstrun") { set_value(g_config.firstrun, str); }
    else if (name == "windowpos") { set_value(g_config.windowpos, str); }
    else if (name == "tvaspect") { set_value(g_config.tvaspect, str); }
    else if (name == "fullscreen") { set_value(g_config.fullscreen, str); }
    else if (name == "visiblearea") { set_value(g_config.visiblearea, str); }
    else if (name == "smooth") { set_value(g_config.smooth, str); }
    else if (name == "motionblur") { set_value(g_config.motionblur, str); }
    else if (name == "allowmotionblur") { set_value(g_config.allowmotionblur, str); }
    else if (name == "blurpercent") { set_value(g_config.blurpercent, str); }
    else if (name == "maxintensity") { set_value(g_config.maxintensity, str); }
    else if (name == "blackborder") { set_value(g_config.blackborder, str); }
    else if (name == "tryvrr") { set_value(g_config.tryvrr, str); }
    else if (name == "gifframeskip") { set_value(g_config.gifframeskip, str); }
    else if (name == "rom") { set_value(g_config.rom, str); }
    else if (name == "romwrite") { set_value(g_config.romwrite, str); }
    else if (name == "atombootrom") { set_value(g_config.atombootrom, str); }
    else if (name == "fastreset") { set_value(g_config.fastreset, str); }
    else if (name == "asicdelay") { set_value(g_config.asicdelay, str); }
    else if (name == "mainmem") { set_value(g_config.mainmem, str); }
    else if (name == "externalmem") { set_value(g_config.externalmem, str); }
    else if (name == "cmosz80") { set_value(g_config.cmosz80, str); }
    else if (name == "im2random") { set_value(g_config.im2random, str); }
    else if (name == "speed") { set_value(g_config.speed, str); }
    else if (name == "drive1") { set_value(g_config.drive1, str); }
    else if (name == "drive2") { set_value(g_config.drive2, str); }
    else if (name == "turbodisk") { set_value(g_config.turbodisk, str); }
    else if (name == "dosboot") { set_value(g_config.dosboot, str); }
    else if (name == "dosdisk") { set_value(g_config.dosdisk, str); }
    else if (name == "stdfloppy") { set_value(g_config.stdfloppy, str); }
    else if (name == "nextfile") { set_value(g_config.nextfile, str); }
    else if (name == "turbotape") { set_value(g_config.turbotape, str); }
    else if (name == "tapetraps") { set_value(g_config.tapetraps, str); }
    else if (name == "disk1") { set_value(g_config.disk1, str); }
    else if (name == "disk2") { set_value(g_config.disk2, str); }
    else if (name == "atomdiskleft0") { set_value(g_config.atomdiskleft0, str); }
    else if (name == "atomdiskleft1") { set_value(g_config.atomdiskleft1, str); }
    else if (name == "atomdisk0") { set_value(g_config.atomdisk0, str); }
    else if (name == "atomdisk1") { set_value(g_config.atomdisk1, str); }
    else if (name == "sdidedisk") { set_value(g_config.sdidedisk, str); }
    else if (name == "tape") { set_value(g_config.tape, str); }
    else if (name == "autoload") { set_value(g_config.autoload, str); }
    else if (name == "autoboot") { set_value(g_config.autoboot, str); }
    else if (name == "diskerrorfreq") { set_value(g_config.diskerrorfreq, str); }
    else if (name == "samdiskhelper") { set_value(g_config.samdiskhelper, str); }
    else if (name == "inpath") { set_value(g_config.inpath, str); }
    else if (name == "outpath") { set_value(g_config.outpath, str); }
    else if (name == "mru0") { set_value(g_config.mru0, str); }
    else if (name == "mru1") { set_value(g_config.mru1, str); }
    else if (name == "mru2") { set_value(g_config.mru2, str); }
    else if (name == "mru3") { set_value(g_config.mru3, str); }
    else if (name == "mru4") { set_value(g_config.mru4, str); }
    else if (name == "mru5") { set_value(g_config.mru5, str); }
    else if (name == "mru6") { set_value(g_config.mru6, str); }
    else if (name == "mru7") { set_value(g_config.mru7, str); }
    else if (name == "mru8") { set_value(g_config.mru8, str); }
    else if (name == "keymapping") { set_value(g_config.keymapping, str); }
    else if (name == "altforcntrl") { set_value(g_config.altforcntrl, str); }
    else if (name == "altgrforedit") { set_value(g_config.altgrforedit, str); }
    else if (name == "mouse") { set_value(g_config.mouse, str); }
    else if (name == "mouseesc") { set_value(g_config.mouseesc, str); }
    else if (name == "keyin") { set_value(g_config.keyin, str); }
    else if (name == "joydev1") { set_value(g_config.joydev1, str); }
    else if (name == "joydev2") { set_value(g_config.joydev2, str); }
    else if (name == "joytype1") { set_value(g_config.joytype1, str); }
    else if (name == "joytype2") { set_value(g_config.joytype2, str); }
    else if (name == "deadzone1") { set_value(g_config.deadzone1, str); }
    else if (name == "deadzone2") { set_value(g_config.deadzone2, str); }
    else if (name == "parallel1") { set_value(g_config.parallel1, str); }
    else if (name == "parallel2") { set_value(g_config.parallel2, str); }
    else if (name == "printeronline") { set_value(g_config.printeronline, str); }
    else if (name == "flushdelay") { set_value(g_config.flushdelay, str); }
    else if (name == "midi") { set_value(g_config.midi, str); }
    else if (name == "midiindev") { set_value(g_config.midiindev, str); }
    else if (name == "midioutdev") { set_value(g_config.midioutdev, str); }
    else if (name == "sambusclock") { set_value(g_config.sambusclock, str); }
    else if (name == "dallasclock") { set_value(g_config.dallasclock, str); }
    else if (name == "audiosync") { set_value(g_config.audiosync, str); }
    else if (name == "saahighpass") { set_value(g_config.saahighpass, str); }
    else if (name == "latency") { set_value(g_config.latency, str); }
    else if (name == "dac7c") { set_value(g_config.dac7c, str); }
    else if (name == "samplerfreq") { set_value(g_config.samplerfreq, str); }
    else if (name == "voicebox") { set_value(g_config.voicebox, str); }
    else if (name == "sid") { set_value(g_config.sid, str); }
    else if (name == "drivelights") { set_value(g_config.drivelights, str); }
    else if (name == "profile") { set_value(g_config.profile, str); }
    else if (name == "status") { set_value(g_config.status, str); }
    else if (name == "breakonexec") { set_value(g_config.breakonexec, str); }
    else if (name == "fkeys") { set_value(g_config.fkeys, str); }
    else if (name == "rasterdebug") { set_value(g_config.rasterdebug, str); }
    else
    {
        return false;
    }

    return true;
}

} // namespace

namespace Options
{

Config g_config;

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
            if (!set_named_value(key, value))
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

            if (argc_ <= 0 || !set_named_value(pcszOption, *++argv_))
            {
                TRACE("Unknown command-line option: {}\n", pcszOption);
            }
        }
        else
        {
            auto full_path{ fs::absolute(pcszOption) };

            // Bare filenames will be inserted into drive 1 then 2
            switch (drive_arg++)
            {
            case 1:
                SetOption(disk1, full_path.string());
                SetOption(drive1, drvFloppy);
                break;

            case 2:
                SetOption(disk2, full_path.string());
                SetOption(drive2, drvFloppy);
                break;

            default:
                TRACE("Unexpected command-line parameter: {}\n", pcszOption);
                break;
            }
        }
    }

    if (!g_config.keyin.empty())
        IO::QueueAutoBoot(AutoLoadType::Keyin);
    else if (drive_arg > 1)
        IO::QueueAutoBoot(AutoLoadType::Disk);

    return true;
}

auto Save() -> bool
{
    auto path{ OSD::MakeFilePath(PathType::Settings, OPTIONS_FILE) };
    Config defaults{};

    try
    {
        std::ofstream ofs(path, std::ofstream::out);
        write_option(ofs, "cfgversion", g_config.cfgversion, defaults.cfgversion);
        write_option(ofs, "firstrun", g_config.firstrun, defaults.firstrun);
        write_option(ofs, "windowpos", g_config.windowpos, defaults.windowpos);
        write_option(ofs, "tvaspect", g_config.tvaspect, defaults.tvaspect);
        write_option(ofs, "fullscreen", g_config.fullscreen, defaults.fullscreen);
        write_option(ofs, "visiblearea", g_config.visiblearea, defaults.visiblearea);
        write_option(ofs, "smooth", g_config.smooth, defaults.smooth);
        write_option(ofs, "motionblur", g_config.motionblur, defaults.motionblur);
        write_option(ofs, "allowmotionblur", g_config.allowmotionblur, defaults.allowmotionblur);
        write_option(ofs, "blurpercent", g_config.blurpercent, defaults.blurpercent);
        write_option(ofs, "maxintensity", g_config.maxintensity, defaults.maxintensity);
        write_option(ofs, "blackborder", g_config.blackborder, defaults.blackborder);
        write_option(ofs, "tryvrr", g_config.tryvrr, defaults.tryvrr);
        write_option(ofs, "gifframeskip", g_config.gifframeskip, defaults.gifframeskip);
        write_option(ofs, "rom", g_config.rom, defaults.rom);
        write_option(ofs, "romwrite", g_config.romwrite, defaults.romwrite);
        write_option(ofs, "atombootrom", g_config.atombootrom, defaults.atombootrom);
        write_option(ofs, "fastreset", g_config.fastreset, defaults.fastreset);
        write_option(ofs, "asicdelay", g_config.asicdelay, defaults.asicdelay);
        write_option(ofs, "mainmem", g_config.mainmem, defaults.mainmem);
        write_option(ofs, "externalmem", g_config.externalmem, defaults.externalmem);
        write_option(ofs, "cmosz80", g_config.cmosz80, defaults.cmosz80);
        write_option(ofs, "im2random", g_config.im2random, defaults.im2random);
        write_option(ofs, "speed", g_config.speed, defaults.speed);
        write_option(ofs, "drive1", g_config.drive1, defaults.drive1);
        write_option(ofs, "drive2", g_config.drive2, defaults.drive2);
        write_option(ofs, "turbodisk", g_config.turbodisk, defaults.turbodisk);
        write_option(ofs, "dosboot", g_config.dosboot, defaults.dosboot);
        write_option(ofs, "dosdisk", g_config.dosdisk, defaults.dosdisk);
        write_option(ofs, "stdfloppy", g_config.stdfloppy, defaults.stdfloppy);
        write_option(ofs, "nextfile", g_config.nextfile, defaults.nextfile);
        write_option(ofs, "turbotape", g_config.turbotape, defaults.turbotape);
        write_option(ofs, "tapetraps", g_config.tapetraps, defaults.tapetraps);
        write_option(ofs, "disk1", g_config.disk1, defaults.disk1);
        write_option(ofs, "disk2", g_config.disk2, defaults.disk2);
        write_option(ofs, "atomdiskleft0", g_config.atomdiskleft0, defaults.atomdiskleft0);
        write_option(ofs, "atomdiskleft1", g_config.atomdiskleft1, defaults.atomdiskleft1);
        write_option(ofs, "atomdisk0", g_config.atomdisk0, defaults.atomdisk0);
        write_option(ofs, "atomdisk1", g_config.atomdisk1, defaults.atomdisk1);
        write_option(ofs, "sdidedisk", g_config.sdidedisk, defaults.sdidedisk);
        write_option(ofs, "tape", g_config.tape, defaults.tape);
        write_option(ofs, "autoload", g_config.autoload, defaults.autoload);
        write_option(ofs, "diskerrorfreq", g_config.diskerrorfreq, defaults.diskerrorfreq);
        write_option(ofs, "samdiskhelper", g_config.samdiskhelper, defaults.samdiskhelper);
        write_option(ofs, "inpath", g_config.inpath, defaults.inpath);
        write_option(ofs, "outpath", g_config.outpath, defaults.outpath);
        write_option(ofs, "mru0", g_config.mru0, defaults.mru0);
        write_option(ofs, "mru1", g_config.mru1, defaults.mru1);
        write_option(ofs, "mru2", g_config.mru2, defaults.mru2);
        write_option(ofs, "mru3", g_config.mru3, defaults.mru3);
        write_option(ofs, "mru4", g_config.mru4, defaults.mru4);
        write_option(ofs, "mru5", g_config.mru5, defaults.mru5);
        write_option(ofs, "mru6", g_config.mru6, defaults.mru6);
        write_option(ofs, "mru7", g_config.mru7, defaults.mru7);
        write_option(ofs, "mru8", g_config.mru8, defaults.mru8);
        write_option(ofs, "keymapping", g_config.keymapping, defaults.keymapping);
        write_option(ofs, "altforcntrl", g_config.altforcntrl, defaults.altforcntrl);
        write_option(ofs, "altgrforedit", g_config.altgrforedit, defaults.altgrforedit);
        write_option(ofs, "mouse", g_config.mouse, defaults.mouse);
        write_option(ofs, "mouseesc", g_config.mouseesc, defaults.mouseesc);
        write_option(ofs, "joydev1", g_config.joydev1, defaults.joydev1);
        write_option(ofs, "joydev2", g_config.joydev2, defaults.joydev2);
        write_option(ofs, "joytype1", g_config.joytype1, defaults.joytype1);
        write_option(ofs, "joytype2", g_config.joytype2, defaults.joytype2);
        write_option(ofs, "deadzone1", g_config.deadzone1, defaults.deadzone1);
        write_option(ofs, "deadzone2", g_config.deadzone2, defaults.deadzone2);
        write_option(ofs, "parallel1", g_config.parallel1, defaults.parallel1);
        write_option(ofs, "parallel2", g_config.parallel2, defaults.parallel2);
        write_option(ofs, "printeronline", g_config.printeronline, defaults.printeronline);
        write_option(ofs, "flushdelay", g_config.flushdelay, defaults.flushdelay);
        write_option(ofs, "midi", g_config.midi, defaults.midi);
        write_option(ofs, "midiindev", g_config.midiindev, defaults.midiindev);
        write_option(ofs, "midioutdev", g_config.midioutdev, defaults.midioutdev);
        write_option(ofs, "sambusclock", g_config.sambusclock, defaults.sambusclock);
        write_option(ofs, "dallasclock", g_config.dallasclock, defaults.dallasclock);
        write_option(ofs, "audiosync", g_config.audiosync, defaults.audiosync);
        write_option(ofs, "saahighpass", g_config.saahighpass, defaults.saahighpass);
        write_option(ofs, "latency", g_config.latency, defaults.latency);
        write_option(ofs, "dac7c", g_config.dac7c, defaults.dac7c);
        write_option(ofs, "samplerfreq", g_config.samplerfreq, defaults.samplerfreq);
        write_option(ofs, "voicebox", g_config.voicebox, defaults.voicebox);
        write_option(ofs, "sid", g_config.sid, defaults.sid);
        write_option(ofs, "drivelights", g_config.drivelights, defaults.drivelights);
        write_option(ofs, "profile", g_config.profile, defaults.profile);
        write_option(ofs, "status", g_config.status, defaults.status);
        write_option(ofs, "breakonexec", g_config.breakonexec, defaults.breakonexec);
        write_option(ofs, "fkeys", g_config.fkeys, defaults.fkeys);
        write_option(ofs, "rasterdebug", g_config.rasterdebug, defaults.rasterdebug);
    }
    catch (...)
    {
        TRACE("Failed to save options\n");
        return false;
    }

    return true;
}

} // namespace Options
