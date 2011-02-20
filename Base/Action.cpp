// Part of SimCoupe - A SAM Coupe emulator
//
// Action.cpp: Actions bound to functions keys, etc.
//
//  Copyright (c) 2005-2011  Simon Owen
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

#include "SimCoupe.h"
#include "Action.h"

#include "CPU.h"
#include "Debug.h"
#include "Display.h"
#include "Frame.h"
#include "GUI.h"
#include "GUIDlg.h"
#include "Input.h"
#include "Options.h"
#include "Parallel.h"
#include "UI.h"
#include "Video.h"

const char* Action::aszActions[MAX_ACTION] =
{
    "New disk 1", "Open disk 1", "Close disk 1", "Save disk 1", "New disk 2", "Open disk 2", "Close disk 2", "Save disk 2",
    "Exit application", "Options", "Debugger", "Import data", "Export data", "Save screenshot", "Change profiler mode",
    "Reset button", "NMI button", "Pause", "Step single frame", "Toggle turbo speed", "Turbo speed (when held)",
    "Toggle frame sync", "Toggle fullscreen", "Change window size", "Change border size", "Toggle 5:4 display",
    "Change frame-skip mode", "Toggle scanlines", "Toggle greyscale", "Mute sound", "Release mouse capture",
    "Toggle printer online", "Flush printer", "About SimCoupe", "Minimise window"
};

bool g_fFrameStep;

bool Action::Do (int nAction_, bool fPressed_/*=true*/)
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
                if (g_fPaused)
                {
                    g_fPaused = false;
                    Video::CreatePalettes();
                }

                CPU::Reset(true);
                Sound::Stop();
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
                Video::CreatePalettes();
                break;

            case actToggleSync:
                SetOption(sync, !GetOption(sync));
                Frame::SetStatus("Frame sync %s", GetOption(sync) ? "enabled" : "disabled");
                break;

            case actToggle5_4:
                SetOption(ratio5_4, !GetOption(ratio5_4));
                Frame::Init();
                Frame::SetStatus("%s aspect ratio", GetOption(ratio5_4) ? "5:4" : "1:1");
                break;

            case actToggleScanlines:
                SetOption(scanlines, !GetOption(scanlines));
                Video::CreatePalettes();
                Frame::SetStatus("Scanlines %s", GetOption(scanlines) ? "enabled" : "disabled");
                break;

            case actChangeFrameSkip:
            {
                SetOption(frameskip, (GetOption(frameskip)+1) % 11);

                int n = GetOption(frameskip);
                switch (n)
                {
                    case 0:     Frame::SetStatus("Automatic frame-skip");   break;
                    case 1:     Frame::SetStatus("No frames skipped");      break;
                    default:    Frame::SetStatus("Displaying every %d%s frame", n, (n==2) ? "nd" : (n==3) ? "rd" : "th");   break;
                }
                break;
            }

            case actChangeProfiler:
                SetOption(profile, !GetOption(profile));
                break;

            case actInsertFloppy1:
                if (GetOption(drive1) != dskImage)
                    Message(msgInfo, "Floppy drive %d is not present", 1);
                else
                    GUI::Start(new CInsertFloppy(1));
                break;

            case actEjectFloppy1:
                if (GetOption(drive1) == dskImage && pDrive1->IsInserted())
                {
                    Frame::SetStatus("%s  ejected from drive %d", pDrive1->GetFile(), 1);
                    pDrive1->Eject();
                }
                break;

            case actSaveFloppy1:
                if (GetOption(drive1) == dskImage && pDrive1->IsModified() && pDrive1->Save())
                    Frame::SetStatus("%s  changes saved", pDrive1->GetFile());
                break;

            case actInsertFloppy2:
                if (GetOption(drive2) != dskImage)
                    Message(msgInfo, "Floppy drive %d is not present", 2);
                else
                    GUI::Start(new CInsertFloppy(2));
                break;

            case actEjectFloppy2:
                if (GetOption(drive2) == dskImage && pDrive2->IsInserted())
                {
                    Frame::SetStatus("%s  ejected from drive %d", pDrive2->GetFile(), 2);
                    pDrive2->Eject();
                }
                break;

            case actSaveFloppy2:
                if (GetOption(drive2) == dskImage && pDrive2->IsModified() && pDrive2->Save())
                    Frame::SetStatus("%s  changes saved", pDrive2->GetFile());
                break;

            case actNewDisk1:
                GUI::Start(new CNewDiskDialog(1));
                break;

            case actNewDisk2:
                GUI::Start(new CNewDiskDialog(2));
                break;

            case actSaveScreenshot:
                Frame::SaveFrame();
                break;

            case actDebugger:
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
                g_fTurbo = !g_fTurbo;
                Sound::Silence();
                Frame::SetStatus("Turbo mode %s", g_fTurbo ? "enabled" : "disabled");
                break;
            }

            case actTempTurbo:
                if (!g_fTurbo)
                {
                    g_fTurbo = true;
                    Sound::Stop();
                }
                break;

            case actReleaseMouse:
                Input::Acquire(false);
                Frame::SetStatus("Mouse capture released");
                break;

            case actFrameStep:
            {
                // Run for one frame then pause
                static int nFrameSkip = 0;

                // On first entry, save the current frameskip setting
                if (!g_fFrameStep)
                {
                    nFrameSkip = GetOption(frameskip);
                    g_fFrameStep = true;
                }

                SetOption(frameskip, g_fPaused ? 1 : nFrameSkip);
            }
            // Fall through to actPause...

            case actPause:
            {
                // Prevent pausing when the GUI is active
                if (GUI::IsActive())
                    break;

                g_fPaused = !g_fPaused;

                if (g_fPaused)
                    Sound::Stop();
                else
                {
                    Sound::Play();
                    g_fFrameStep = (nAction_ == actFrameStep);
                }

                Video::CreatePalettes();
                Display::SetDirty();
                Frame::Redraw();
                Input::Purge();
                break;
            }

            case actToggleFullscreen:
                SetOption(fullscreen, !GetOption(fullscreen));
                Sound::Silence();
                Frame::Init();

                // Grab the mouse automatically in full-screen, or release in windowed mode
                Input::Acquire(!!GetOption(fullscreen), !GUI::IsActive());
                break;

            case actPrinterOnline:
                SetOption(printeronline, !GetOption(printeronline));
                Frame::SetStatus("Printer %s", GetOption(printeronline) ? "online" : "offline");
                break;

            case actFlushPrinter:
                // If port 1 is a printer, flush it
                if (GetOption(parallel1) == 1)
                    reinterpret_cast<CPrintBuffer*>(pParallel1)->Flush();

                // If port 2 is a printer, flush it
                if (GetOption(parallel2) == 1)
                    reinterpret_cast<CPrintBuffer*>(pParallel2)->Flush();

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
                // Reset the CPU and restart the sound
                CPU::Reset(false);
                Sound::Play();
                break;

            case actTempTurbo:
                if (g_fTurbo)
                {
                    Sound::Play();
                    g_fTurbo = false;
                }
                break;

            // Not processed
            default:
                return false;
        }
    }

    // Action processed
    return true;
}


void Action::Key (int nFnKey_, bool fPressed_, bool fCtrl_, bool fAlt_, bool fShift_)
{
    // Grab a copy of the function key definition string (could do with being converted to upper-case)
    char szKeys[256];
    strncpy(szKeys, GetOption(fnkeys), sizeof(szKeys));

    // Process each of the 'key=action' pairs in the string
    for (char* psz = strtok(szKeys, ", \t") ; psz ; psz = strtok(NULL, ", \t"))
    {
        // Leading C/A/S characters indicate that Ctrl/Alt/Shift modifiers are required with the key
        bool fCtrled  = (*psz == 'C');  if (fCtrled)  psz++;
        bool fAlted   = (*psz == 'A');  if (fAlted)   psz++;
        bool fShifted = (*psz == 'S');  if (fShifted) psz++;

        // Currently we only support function keys F1-F12
        if (*psz++ == 'F')
        {
            // If we've not found a matching key, keep looking...
            if (nFnKey_ != (int)strtoul(psz, &psz, 0))
                continue;

            // If the Ctrl/Shift states match, perform the action
            if (fCtrl_ == fCtrled && fAlt_ == fAlted && fShift_ == fShifted)
            {
                Do(strtoul(++psz, NULL, 0), fPressed_);
                break;
            }
        }
    }
}
