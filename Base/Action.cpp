// Part of SimCoupe - A SAM Coupe emulator
//
// Action.cpp: Actions bound to functions keys, etc.
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
#include "Action.h"

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

namespace Action
{

const char* aszActions[MAX_ACTION] =
{
    "New disk 1", "Open disk 1", "Close disk 1", "Save disk 1", "New disk 2", "Open disk 2", "Close disk 2", "Save disk 2",
    "Exit application", "Options", "Debugger", "Import data", "Export data", "Save screenshot", "",
    "Reset button", "NMI button", "Pause", "", "Toggle turbo speed", "Turbo speed (when held)",
    "Toggle Hi-res Scanlines", "Toggle fullscreen", "", "", "Toggle 5:4 display",
    "Toggle Smoothing", "Toggle scanlines", "Toggle greyscale", "Mute sound", "Release mouse capture",
    "Toggle printer online", "Flush printer", "About SimCoupe", "Minimise window", "Record GIF animation", "Record GIF loop",
    "Stop GIF Recording", "Record WAV audio", "Record WAV segment", "Stop WAV Recording", "Record AVI video", "Record AVI half-size", "Stop AVI Recording",
    "Speed Faster", "Speed Slower", "Speed Normal", "Paste Clipboard", "Insert Tape", "Eject Tape", "Tape Browser"
};


bool Do (int nAction_, bool fPressed_/*=true*/)
{
    // OS-specific functionality takes precedence
    if (UI::DoAction(nAction_, fPressed_))
        return true;

    // Key pressed?
    if (fPressed_)
    {
        switch (nAction_)
        {
            case actResetButton:
                // Ensure we're not paused, to avoid confusion
                g_fPaused = false;

                CPU::Reset(true);
                break;

            case actNmiButton:
                CPU::NMI();
                break;

            case actToggleMute:
                SetOption(sound, !GetOption(sound));
                Sound::Init();
                Frame::SetStatus("Sound %s", GetOption(sound) ? "enabled" : "muted");
                break;

            case actToggleGreyscale:
                SetOption(greyscale, !GetOption(greyscale));
                Video::UpdatePalette();
                Frame::SetStatus("%s", GetOption(greyscale) ? "Greyscale" : "Colour");
                break;

            case actToggle5_4:
                if (Video::CheckCaps(VCAP_STRETCH))
                {
                    SetOption(ratio5_4, !GetOption(ratio5_4));
                    Video::UpdateSize();
                    Frame::SetStatus("%s aspect ratio", GetOption(ratio5_4) ? "5:4" : "1:1");
                }
                break;

            case actToggleScanlines:
                SetOption(scanlines, !GetOption(scanlines));
                Video::UpdatePalette();
                Frame::SetStatus("Scanlines %s", GetOption(scanlines) ? "enabled" : "disabled");
                break;

            case actToggleFilter:
                if (Video::CheckCaps(VCAP_FILTER))
                {
                    SetOption(filter, !GetOption(filter));
                    Video::UpdateSize();
                    Frame::SetStatus("Smoothing %s", GetOption(filter) ? "enabled" : "disabled");
                }
                break;

            case actToggleScanHiRes:
                if (GetOption(scanlines) && Video::CheckCaps(VCAP_SCANHIRES))
                {
                    SetOption(scanhires, !GetOption(scanhires));
                    Frame::SetStatus("Hi-res scanlines %s", GetOption(scanhires) ? "enabled" : "disabled");
                }
                break;

            case actInsertFloppy1:
                if (GetOption(drive1) != drvFloppy)
                    Message(msgInfo, "Floppy drive %d is not present", 1);
                else
                    GUI::Start(new CInsertFloppy(1));
                break;

            case actEjectFloppy1:
                if (pFloppy1->HasDisk())
                {
                    Frame::SetStatus("%s  ejected from drive %d", pFloppy1->DiskFile(), 1);
                    pFloppy1->Eject();
                }
                break;

            case actSaveFloppy1:
                if (pFloppy1->HasDisk() && pFloppy1->DiskModified() && pFloppy1->Save())
                    Frame::SetStatus("%s  changes saved", pFloppy1->DiskFile());
                break;

            case actInsertFloppy2:
                if (GetOption(drive2) != drvFloppy)
                    Message(msgInfo, "Floppy drive %d is not present", 2);
                else
                    GUI::Start(new CInsertFloppy(2));
                break;

            case actEjectFloppy2:
                if (pFloppy2->HasDisk())
                {
                    Frame::SetStatus("%s  ejected from drive %d", pFloppy2->DiskFile(), 2);
                    pFloppy2->Eject();
                }
                break;

            case actSaveFloppy2:
                if (pFloppy2->HasDisk() && pFloppy2->DiskModified() && pFloppy2->Save())
                    Frame::SetStatus("%s  changes saved", pFloppy2->DiskFile());
                break;

            case actNewDisk1:
                GUI::Start(new CNewDiskDialog(1));
                break;

            case actNewDisk2:
                GUI::Start(new CNewDiskDialog(2));
                break;

            case actTapeInsert:
            case actTapeBrowser:
                GUI::Start(new CInsertTape());
                break;

            case actTapeEject:
                if (Tape::IsInserted())
                {
                    Frame::SetStatus("%s  ejected", Tape::GetFile());
                    Tape::Eject();
                }
                break;

            case actSaveScreenshot:
                Frame::SaveScreenshot();
                break;

            case actDebugger:
                if (!GUI::IsActive())
                    Debug::Start();
                break;

            case actImportData:
                GUI::Start(new CImportDialog);
                break;

            case actExportData:
                GUI::Start(new CExportDialog);
                break;

            case actOptions:
                GUI::Start(new COptionsDialog);
                break;

            case actAbout:
                GUI::Start(new CAboutDialog);
                break;

            case actToggleTurbo:
            {
                g_nTurbo ^= TURBO_KEY;
                Sound::Silence();
                Frame::SetStatus("Turbo mode %s", (g_nTurbo&TURBO_KEY) ? "enabled" : "disabled");
                break;
            }

            case actTempTurbo:
                if (!(g_nTurbo&TURBO_KEY))
                {
                    g_nTurbo |= TURBO_KEY;
                    Sound::Silence();
                }
                break;

            case actReleaseMouse:
                if (Input::IsMouseAcquired())
                {
                    Input::AcquireMouse(false);
                    Frame::SetStatus("Mouse capture released");
                }
                break;

            case actFrameStep:
                // Dummy for now, to be restored with future CPU core changes
                break;

            case actPause:
            {
                // Prevent pausing when the GUI is active
                if (GUI::IsActive())
                    break;

                g_fPaused = !g_fPaused;

                Input::Purge();
                break;
            }

            case actToggleFullscreen:
                SetOption(fullscreen, !GetOption(fullscreen));
                Sound::Silence();
                Video::UpdateSize();
                break;

            case actPrinterOnline:
                SetOption(printeronline, !GetOption(printeronline));
                Frame::SetStatus("Printer %s", GetOption(printeronline) ? "online" : "offline");
                break;

            case actFlushPrinter:
                pPrinterFile->Flush();
                break;

            case actRecordGif:
                GIF::Toggle(false);
                break;

            case actRecordGifLoop:
                GIF::Toggle(true);
                break;

            case actRecordGifStop:
                GIF::Stop();
                break;

            case actRecordWav:
                WAV::Toggle(false);
                break;

            case actRecordWavSegment:
                WAV::Toggle(true);
                break;

            case actRecordWavStop:
                WAV::Stop();
                break;

            case actRecordAvi:
                AVI::Toggle(false);
                break;

            case actRecordAviHalf:
                AVI::Toggle(true);
                break;

            case actRecordAviStop:
                AVI::Stop();
                break;

            case actSpeedFaster:
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

            case actSpeedSlower:
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

            case actSpeedNormal:
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
        switch (nAction_)
        {
            case actResetButton:
                CPU::Reset(false);
                break;

            case actTempTurbo:
            case actSpeedFaster:
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


void Key (int nFnKey_, bool fPressed_, bool fCtrl_, bool fAlt_, bool fShift_)
{
    // Grab a copy of the function key definition string (could do with being converted to upper-case)
    char szKeys[256];
    strncpy(szKeys, GetOption(fnkeys), sizeof(szKeys)-1);
    szKeys[sizeof(szKeys)-1] = '\0';

    // Process each of the 'key=action' pairs in the string
    for (char* psz = strtok(szKeys, ", \t") ; psz ; psz = strtok(nullptr, ", \t"))
    {
        // Leading C/A/S characters indicate that Ctrl/Alt/Shift modifiers are required with the key
        bool fCtrled  = (*psz == 'C');  if (fCtrled)  psz++;
        bool fAlted   = (*psz == 'A');  if (fAlted)   psz++;
        bool fShifted = (*psz == 'S');  if (fShifted) psz++;

        // Currently we only support function keys F1-F12
        if (*psz++ == 'F')
        {
            // If we've not found a matching key, keep looking...
            if (nFnKey_ != static_cast<int>(strtoul(psz, &psz, 10)))
                continue;

            // If the Ctrl/Shift states match, perform the action
            if (fCtrl_ == fCtrled && fAlt_ == fAlted && fShift_ == fShifted)
            {
                int nAction = static_cast<int>(strtoul(++psz, nullptr, 10));
                Do(nAction, fPressed_);
                break;
            }
        }
    }
}

} // namespace Action
