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
        case Action::ResetButton:
            // Ensure we're not paused, to avoid confusion
            g_fPaused = false;

            CPU::Reset(true);
            break;

        case Action::NmiButton:
            CPU::NMI();
            break;

        case Action::ToggleMute:
            SetOption(sound, !GetOption(sound));
            Sound::Init();
            Frame::SetStatus("Sound %s", GetOption(sound) ? "enabled" : "muted");
            break;

        case Action::ToggleGreyscale:
            SetOption(greyscale, !GetOption(greyscale));
            Video::UpdatePalette();
            Frame::SetStatus("%s", GetOption(greyscale) ? "Greyscale" : "Colour");
            break;

        case Action::Toggle5_4:
            if (Video::CheckCaps(VCAP_STRETCH))
            {
                SetOption(ratio5_4, !GetOption(ratio5_4));
                Video::UpdateSize();
                Frame::SetStatus("%s aspect ratio", GetOption(ratio5_4) ? "5:4" : "1:1");
            }
            break;

        case Action::ToggleFilter:
            if (Video::CheckCaps(VCAP_FILTER))
            {
                SetOption(filter, !GetOption(filter));
                Video::UpdateSize();
                Frame::SetStatus("Smoothing %s", GetOption(filter) ? "enabled" : "disabled");
            }
            break;

        case Action::InsertFloppy1:
            if (GetOption(drive1) != drvFloppy)
                Message(msgInfo, "Floppy drive %d is not present", 1);
            else
                GUI::Start(new CInsertFloppy(1));
            break;

        case Action::EjectFloppy1:
            if (pFloppy1->HasDisk())
            {
                Frame::SetStatus("%s  ejected from drive %d", pFloppy1->DiskFile(), 1);
                pFloppy1->Eject();
            }
            break;

        case Action::SaveFloppy1:
            if (pFloppy1->HasDisk() && pFloppy1->DiskModified() && pFloppy1->Save())
                Frame::SetStatus("%s  changes saved", pFloppy1->DiskFile());
            break;

        case Action::InsertFloppy2:
            if (GetOption(drive2) != drvFloppy)
                Message(msgInfo, "Floppy drive %d is not present", 2);
            else
                GUI::Start(new CInsertFloppy(2));
            break;

        case Action::EjectFloppy2:
            if (pFloppy2->HasDisk())
            {
                Frame::SetStatus("%s  ejected from drive %d", pFloppy2->DiskFile(), 2);
                pFloppy2->Eject();
            }
            break;

        case Action::SaveFloppy2:
            if (pFloppy2->HasDisk() && pFloppy2->DiskModified() && pFloppy2->Save())
                Frame::SetStatus("%s  changes saved", pFloppy2->DiskFile());
            break;

        case Action::NewDisk1:
            GUI::Start(new CNewDiskDialog(1));
            break;

        case Action::NewDisk2:
            GUI::Start(new CNewDiskDialog(2));
            break;

        case Action::TapeInsert:
        case Action::TapeBrowser:
            GUI::Start(new CInsertTape());
            break;

        case Action::TapeEject:
            if (Tape::IsInserted())
            {
                Frame::SetStatus("%s  ejected", Tape::GetFile());
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
            break;

        case Action::ImportData:
            GUI::Start(new CImportDialog);
            break;

        case Action::ExportData:
            GUI::Start(new CExportDialog);
            break;

        case Action::Options:
            GUI::Start(new COptionsDialog);
            break;

        case Action::About:
            GUI::Start(new CAboutDialog);
            break;

        case Action::ToggleTurbo:
        {
            g_nTurbo ^= TURBO_KEY;
            Sound::Silence();
            Frame::SetStatus("Turbo mode %s", (g_nTurbo & TURBO_KEY) ? "enabled" : "disabled");
            break;
        }

        case Action::TempTurbo:
            if (!(g_nTurbo & TURBO_KEY))
            {
                g_nTurbo |= TURBO_KEY;
                Sound::Silence();
            }
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
            Sound::Silence();
            Video::UpdateSize();
            break;

        case Action::PrinterOnline:
            SetOption(printeronline, !GetOption(printeronline));
            Frame::SetStatus("Printer %s", GetOption(printeronline) ? "online" : "offline");
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

            Frame::SetStatus("%u%% Speed", GetOption(speed));
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

            Frame::SetStatus("%u%% Speed", GetOption(speed));
            break;

        case Action::SpeedNormal:
            SetOption(speed, 100);
            Frame::SetStatus("100%% Speed");
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
        case Action::ResetButton:
            CPU::Reset(false);
            break;

        case Action::TempTurbo:
        case Action::SpeedFaster:
            CPU::Reset(false);
            g_nTurbo = 0;
            break;

            // Not processed
        default:
            return false;
        }
    }

    // Action processed
    return true;
}


void Key(int fn_key, bool pressed, bool ctrl, bool alt, bool shift)
{
    // Grab a copy of the function key definition string (could do with being converted to upper-case)
    char szKeys[256];
    strncpy(szKeys, GetOption(fnkeys), sizeof(szKeys) - 1);
    szKeys[sizeof(szKeys) - 1] = '\0';

    // Process each of the 'key=action' pairs in the string
    for (char* psz = strtok(szKeys, ", \t"); psz; psz = strtok(nullptr, ", \t"))
    {
        // Leading C/A/S characters indicate that Ctrl/Alt/Shift modifiers are required with the key
        bool mapping_ctrl = (*psz == 'C');  if (mapping_ctrl)  psz++;
        bool mapping_alt = (*psz == 'A');  if (mapping_alt)   psz++;
        bool mapping_shift = (*psz == 'S');  if (mapping_shift) psz++;

        // Currently we only support function keys F1-F12
        if (*psz++ == 'F')
        {
            // If we've not found a matching key, keep looking...
            if (fn_key != static_cast<int>(strtoul(psz, &psz, 10)))
                continue;

            // If the Ctrl/Shift states match, perform the action
            if (ctrl == mapping_ctrl && alt == mapping_alt && shift == mapping_shift)
            {
                auto action = static_cast<Action>(strtoul(++psz, nullptr, 10));
                Do(action, pressed);
                break;
            }
        }
    }
}

std::string to_string(Action action)
{
    static const std::map<Action, std::string> action_descs =
    {
        { Action::NewDisk1, "New disk 1", },
        { Action::InsertFloppy1, "Open disk 1" },
        { Action::EjectFloppy1, "Close disk 1" },
        { Action::SaveFloppy1, "Save disk 1" },
        { Action::NewDisk2, "New disk 2" },
        { Action::InsertFloppy2, "Open disk 2" },
        { Action::EjectFloppy2, "Close disk 2" },
        { Action::SaveFloppy2, "Save disk 2" },
        { Action::ExitApplication, "Exit application" },
        { Action::Options, "Options" },
        { Action::Debugger, "Debugger" },
        { Action::ImportData, "Import data" },
        { Action::ExportData, "Export data" },
        { Action::SavePNG, "Save screenshot (PNG)" },
        { Action::ResetButton, "Reset button" },
        { Action::NmiButton, "NMI button" },
        { Action::Pause, "Pause" },
        { Action::FrameStep, "Frame step" },
        { Action::ToggleTurbo, "Toggle turbo speed" },
        { Action::TempTurbo, "Turbo speed (when held)" },
        { Action::ToggleFullscreen, "Toggle fullscreen" },
        { Action::Toggle5_4, "Toggle 5:4 display" },
        { Action::ToggleFilter, "Toggle graphics smoothing" },
        { Action::ToggleGreyscale, "Toggle greyscale" },
        { Action::ToggleMute, "Mute sound" },
        { Action::ReleaseMouse, "Release mouse capture" },
        { Action::PrinterOnline, "Toggle printer online" },
        { Action::FlushPrinter, "Flush printer" },
        { Action::About, "About SimCoupe" },
        { Action::Minimise, "Minimise window" },
        { Action::RecordGif, "Record GIF animation" },
        { Action::RecordGifLoop, "Record GIF loop" },
        { Action::RecordGifStop, "Stop GIF Recording" },
        { Action::RecordWav, "Record WAV audio" },
        { Action::RecordWavSegment, "Record WAV segment" },
        { Action::RecordWavStop, "Stop WAV Recording" },
        { Action::RecordAvi, "Record AVI video" },
        { Action::RecordAviHalf, "Record AVI half-size" },
        { Action::RecordAviStop, "Stop AVI Recording" },
        { Action::SpeedFaster, "Speed Faster" },
        { Action::SpeedSlower, "Speed Slower" },
        { Action::SpeedNormal, "Speed Normal" },
        { Action::Paste, "Paste Clipboard" },
        { Action::TapeInsert, "Insert Tape" },
        { Action::TapeEject, "Eject Tape" },
        { Action::TapeBrowser, "Tape Browser" },
        { Action::SaveSSX, "Save screenshot (SSX)" },
    };

    auto it = action_descs.find(action);
    if (it == action_descs.end())
        return "";

    return it->second;
}

} // namespace Actions
