// Part of SimCoupe - A SAM Coupe emulator
//
// Actions.cpp: Actions bound to functions keys, etc.
//
//  Copyright (c) 2005-2015 Simon Owen
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

#include "SimCoupe.h"
#include "Actions.h"

#include "AVI.h"
#include "CPU.h"
#include "Debug.h"
#include "Frame.h"
#include "GIF.h"
#include "GUI.h"
#include "GUIDlg.h"
#include "Input.h"
#include "Options.h"
#include "Parallel.h"
#include "Sound.h"
#include "Tape.h"
#include "UI.h"
#include "Video.h"
#include "WAV.h"

namespace Actions
{

struct ActionKey
{
    Action action{ Action::None };
    int fn_key{};
    bool ctrl;
    bool alt;
    bool shift;
};

static std::vector<ActionKey> s_mappings;

struct ActionEntry
{
    Action action;
    std::string name;
    std::string desc;
};

static const std::vector<ActionEntry> actions =
{
    { Action::NewDisk1, "NewDisk1", "New disk 1", },
    { Action::InsertDisk1, "InsertDisk1", "Insert disk 1" },
    { Action::EjectDisk1, "EjectDisk1", "Close disk 1" },
    { Action::SaveDisk1, "SaveDisk1", "Save disk 1" },
    { Action::NewDisk2, "NewDisk2", "New disk 2" },
    { Action::InsertDisk2, "InsertDisk2", "Insert disk 2" },
    { Action::EjectDisk2, "EjectDisk2", "Close disk 2" },
    { Action::SaveDisk2, "SaveDisk2", "Save disk 2" },
    { Action::InsertTape, "InsertTape", "Insert Tape" },
    { Action::EjectTape, "EjectTape", "Eject Tape" },
    { Action::TapeBrowser, "TapeBrowser", "Tape Browser" },
    { Action::Paste, "Paste", "Paste Clipboard" },
    { Action::ImportData, "ImportData", "Import data" },
    { Action::ExportData, "ExportData", "Export data" },
    { Action::SavePNG, "SavePNG", "Save screenshot (PNG)" },
    { Action::SaveSSX, "SaveSSX", "Save screenshot (SSX)" },
    { Action::TogglePrinter, "TogglePrinter", "Toggle printer online" },
    { Action::FlushPrinter, "FlushPrinter", "Flush printer" },
    { Action::ToggleFullscreen, "ToggleFullscreen", "Toggle fullscreen" },
    { Action::Toggle54, "Toggle54", "Toggle 5:4 display" },
    { Action::ToggleSmoothing, "ToggleSmoothing", "Toggle graphics smoothing" },
    { Action::ToggleMotionBlur, "ToggleMotionBlur", "Toggle motion blur" },
    { Action::RecordAvi, "RecordAvi", "Record AVI video" },
    { Action::RecordAviHalf, "RecordAviHalf", "Record AVI half-size" },
    { Action::RecordAviStop, "RecordAviStop", "Stop AVI Recording" },
    { Action::RecordGif, "RecordGif", "Record GIF animation" },
    { Action::RecordGifLoop, "RecordGifLoop", "Record GIF loop" },
    { Action::RecordGifStop, "RecordGifStop", "Stop GIF Recording" },
    { Action::RecordWav, "RecordWav", "Record WAV audio" },
    { Action::RecordWavSegment, "RecordWavSegment", "Record WAV segment" },
    { Action::RecordWavStop, "RecordWavStop", "Stop WAV Recording" },
    { Action::SpeedNormal, "SpeedNormal", "Speed Normal" },
    { Action::SpeedSlower, "SpeedSlower", "Speed Slower" },
    { Action::SpeedFaster, "SpeedFaster", "Speed Faster" },
    { Action::SpeedTurbo, "SpeedTurbo", "Turbo speed (when held)" },
    { Action::ToggleTurbo, "ToggleTurbo", "Toggle turbo speed" },
    { Action::Reset, "Reset", "Reset button" },
    { Action::Nmi, "Nmi", "NMI button" },
    { Action::Pause, "Pause", "Pause" },
    { Action::FrameStep, "FrameStep", "Frame step" },
    { Action::ReleaseMouse, "ReleaseMouse", "Release mouse capture" },
    { Action::Options, "Options", "Options" },
    { Action::Debugger, "Debugger", "Debugger" },
    { Action::About, "About", "About SimCoupe" },
    { Action::Minimise, "Minimise", "Minimise window" },
    { Action::ExitApp, "ExitApp", "Exit application" },
};

bool Do(Action action, bool pressed/*=true*/)
{
    // OS-specific functionality takes precedence
    if (UI::DoAction(action, pressed))
        return true;

    // Key pressed?
    if (pressed)
    {
        switch (action)
        {
        case Action::Reset:
            // Ensure we're not paused, to avoid confusion
            g_fPaused = false;

            CPU::Reset(true);
            break;

        case Action::Nmi:
            CPU::NMI();
            break;

        case Action::Toggle54:
            SetOption(ratio5_4, !GetOption(ratio5_4));
            Video::OptionsChanged();
            Frame::SetStatus("{} aspect ratio", GetOption(ratio5_4) ? "5:4" : "1:1");
            break;

        case Action::ToggleSmoothing:
            SetOption(smooth, !GetOption(smooth));
            Video::OptionsChanged();
            Frame::SetStatus("Smoothing {}", GetOption(smooth) ? "enabled" : "disabled");
            break;

        case Action::ToggleMotionBlur:
            SetOption(motionblur, !GetOption(motionblur));
            Video::OptionsChanged();
            Frame::SetStatus("Motion blur {}", GetOption(motionblur) ? "enabled" : "disabled");
            break;

        case Action::InsertDisk1:
            if (GetOption(drive1) != drvFloppy)
                Message(MsgType::Info, "Floppy drive 1 is not present");
            else
                GUI::Start(new BrowseFloppy(1));
            break;

        case Action::EjectDisk1:
            if (pFloppy1->HasDisk())
            {
                Frame::SetStatus("{}  ejected from drive 1", pFloppy1->DiskFile());
                pFloppy1->Eject();
            }
            break;

        case Action::SaveDisk1:
            if (pFloppy1->HasDisk() && pFloppy1->DiskModified() && pFloppy1->Save())
                Frame::SetStatus("{}  changes saved", pFloppy1->DiskFile());
            break;

        case Action::InsertDisk2:
            if (GetOption(drive2) != drvFloppy)
                Message(MsgType::Info, "Floppy drive 2 is not present");
            else
                GUI::Start(new BrowseFloppy(2));
            break;

        case Action::EjectDisk2:
            if (pFloppy2->HasDisk())
            {
                Frame::SetStatus("{}  ejected from drive 2", pFloppy2->DiskFile());
                pFloppy2->Eject();
            }
            break;

        case Action::SaveDisk2:
            if (pFloppy2->HasDisk() && pFloppy2->DiskModified() && pFloppy2->Save())
                Frame::SetStatus("{}  changes saved", pFloppy2->DiskFile());
            break;

        case Action::NewDisk1:
            GUI::Start(new NewDiskDialog(1));
            break;

        case Action::NewDisk2:
            GUI::Start(new NewDiskDialog(2));
            break;

        case Action::InsertTape:
        case Action::TapeBrowser:
            GUI::Start(new BrowseTape());
            break;

        case Action::EjectTape:
            if (Tape::IsInserted())
            {
                Frame::SetStatus("{}  ejected", Tape::GetFile());
                Tape::Eject();
            }
            break;

        case Action::SavePNG:
            Frame::SavePNG();
            break;

        case Action::SaveSSX:
            Frame::SaveSSX();
            break;

        case Action::Debugger:
            if (!GUI::IsActive())
                Debug::Start();
            else
                GUI::Stop();
            break;

        case Action::ImportData:
            GUI::Start(new ImportDialog);
            break;

        case Action::ExportData:
            GUI::Start(new ExportDialog);
            break;

        case Action::Options:
            GUI::Start(new OptionsDialog);
            break;

        case Action::About:
            GUI::Start(new AboutDialog);
            break;

        case Action::ToggleTurbo:
            g_nTurbo ^= TURBO_KEY;
            Frame::SetStatus("Turbo mode {}", (g_nTurbo & TURBO_KEY) ? "enabled" : "disabled");
            break;

        case Action::SpeedTurbo:
            g_nTurbo |= TURBO_KEY;
            break;

        case Action::ReleaseMouse:
            if (Input::IsMouseAcquired())
            {
                Input::AcquireMouse(false);
                Frame::SetStatus("Mouse capture released");
            }
            break;

        case Action::FrameStep:
            // Dummy for now, to be restored with future CPU core changes
            break;

        case Action::Pause:
        {
            // Prevent pausing when the GUI is active
            if (GUI::IsActive())
                break;

            g_fPaused = !g_fPaused;

            Input::Purge();
            break;
        }

        case Action::ToggleFullscreen:
            SetOption(fullscreen, !GetOption(fullscreen));
            Video::OptionsChanged();
            break;

        case Action::TogglePrinter:
            SetOption(printeronline, !GetOption(printeronline));
            Frame::SetStatus("Printer {}", GetOption(printeronline) ? "online" : "offline");
            break;

        case Action::FlushPrinter:
            pPrinterFile->Flush();
            break;

        case Action::RecordGif:
            GIF::Toggle(false);
            break;

        case Action::RecordGifLoop:
            GIF::Toggle(true);
            break;

        case Action::RecordGifStop:
            GIF::Stop();
            break;

        case Action::RecordWav:
            WAV::Toggle(false);
            break;

        case Action::RecordWavSegment:
            WAV::Toggle(true);
            break;

        case Action::RecordWavStop:
            WAV::Stop();
            break;

        case Action::RecordAvi:
            AVI::Toggle(false);
            break;

        case Action::RecordAviHalf:
            AVI::Toggle(true);
            break;

        case Action::RecordAviStop:
            AVI::Stop();
            break;

        case Action::SpeedFaster:
            switch (GetOption(speed))
            {
            case 50:   SetOption(speed, 100); break;
            case 100:  SetOption(speed, 200); break;
            case 200:  SetOption(speed, 300); break;
            case 300:  SetOption(speed, 500); break;
            default:   SetOption(speed, 1000); break;
            }

            Frame::SetStatus("{}% Speed", GetOption(speed));
            break;

        case Action::SpeedSlower:
            switch (GetOption(speed))
            {
            case 200:  SetOption(speed, 100); break;
            case 300:  SetOption(speed, 200); break;
            case 500:  SetOption(speed, 300); break;
            case 1000: SetOption(speed, 500); break;
            default:   SetOption(speed, 50); break;
            }

            Frame::SetStatus("{}% Speed", GetOption(speed));
            break;

        case Action::SpeedNormal:
            SetOption(speed, 100);
            Frame::SetStatus("100% Speed");
            break;

            // Not processed
        default:
            return false;
        }
    }
    else    // Key released
    {
        switch (action)
        {
        case Action::Reset:
            CPU::Reset(false);
            break;

        case Action::SpeedTurbo:
        case Action::SpeedFaster:
            CPU::Reset(false);
            g_nTurbo &= ~TURBO_KEY;
            break;

            // Not processed
        default:
            return false;
        }
    }

    // Action processed
    return true;
}

static void UpdateMappings()
{
    for (auto entry : split(GetOption(fkeys), ','))
    {
        auto fields = split(entry, '=');
        if (fields.size() != 2)
            continue;

        auto lower_action = tolower(fields[1]);
        auto it_action = std::find_if(actions.begin(), actions.end(),
            [&](ActionEntry action) { return tolower(action.name) == lower_action; });
        if (it_action == actions.end())
        {
            TRACE("Unknown action: {}\n", fields[1]);
            continue;
        }

        try
        {
            ActionKey key{};
            key.action = it_action->action;

            auto mapping = tolower(fields[0]);
            for (auto it = mapping.begin(); it != mapping.end(); ++it)
            {
                switch (*it)
                {
                case 'c': key.ctrl = true; continue;
                case 'a': key.alt = true; continue;
                case 's': key.shift = true; continue;
                case 'f':
                if ((it + 1) != mapping.end())
                {
                    key.fn_key = std::stoi(std::string(it + 1, mapping.end()));
                }
                break;
                }
            }

            if (key.fn_key)
            {
                s_mappings.emplace_back(std::move(key));
            }
        }
        catch (...)
        {
            // invalid mapping
        }
    }
}

void Key(int fn_key, bool pressed, bool ctrl, bool alt, bool shift)
{
    if (s_mappings.empty() && !GetOption(fkeys).empty())
    {
        UpdateMappings();

        // Use the default mappings if the configuration was incompatible.
        if (s_mappings.empty())
        {
            Config config{};
            SetOption(fkeys, config.fkeys);
            UpdateMappings();
        }
    }

    auto it = std::find_if(s_mappings.begin(), s_mappings.end(),
        [&](ActionKey ak)
        {
            return ak.fn_key == fn_key &&
                ak.ctrl == ctrl &&
                ak.alt == alt &&
                ak.shift == shift;
        });

    if (it != s_mappings.end())
    {
        Do(it->action, pressed);
    }
}

} // namespace Actions
