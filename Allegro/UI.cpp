// Part of SimCoupe - A SAM Coupe emulator
//
// UI.cpp: Allegro user interface
//
//  Copyright (c) 1999-2005  Simon Owen
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
//  At present this module only really contains the event processing
//  code, for forwarding to other modules and processing fn keys

#include "SimCoupe.h"

#include "UI.h"

#include "Debug.h"
#include "CDrive.h"
#include "Clock.h"
#include "CPU.h"
#include "Display.h"
#include "Frame.h"
#include "GUIDlg.h"
#include "Input.h"
#include "Options.h"
#include "OSD.h"
#include "Memory.h"
#include "Sound.h"
#include "Video.h"

char* WINDOW_CAPTION = "SimCoup\xc3\xa9/Allegro"
#ifdef _DEBUG
    " [DEBUG]"
#endif
;

void DoAction (int nAction_, bool fPressed_=true);

bool g_fActive = true, g_fFrameStep, g_fQuit;


enum eActions
{
    actNewDisk1, actInsertFloppy1, actEjectFloppy1, actSaveFloppy1, actNewDisk2, actInsertFloppy2, actEjectFloppy2, actSaveFloppy2,
    actExitApplication, actOptions, actDebugger, actImportData, actExportData, actSaveScreenshot, actChangeProfiler,
    actResetButton, actNmiButton, actPause, actFrameStep, actToggleTurbo, actTempTurbo,
    actToggleSync, actToggleFullscreen, actChangeWindowSize, actChangeBorders, actToggle5_4,
    actChangeFrameSkip, actToggleScanlines, actToggleGreyscale, actToggleMute, actReleaseMouse,
    MAX_ACTION
};

const char* aszActions[MAX_ACTION] =
{
    "New disk 1", "Open disk 1", "Close disk 1", "Save disk 1", "New disk 2", "Open disk 2", "Close disk 2", "Save disk 2",
    "Exit application", "Options", "Debugger", "Import data", "Export data", "Save screenshot", "Change profiler mode",
    "Reset button", "NMI button", "Pause", "Step single frame", "Toggle turbo speed", "Turbo speed (when held)",
    "Toggle frame sync", "Toggle fullscreen", "Change window size", "Change border size", "Toggle 5:4 display",
    "Change frame-skip mode", "Toggle scanlines", "Toggle greyscale", "Mute sound", "Release mouse capture"
};


bool UI::Init (bool fFirstInit_/*=false*/)
{
    bool fRet = true;

    Exit(true);
    TRACE("-> UI::Init()\n");

    set_window_close_hook(Quit);
    set_window_title(WINDOW_CAPTION);

    TRACE("<- UI::Init() returning %s\n", fRet ? "true" : "false");
    return fRet;
}

void UI::Exit (bool fReInit_/*=false*/)
{
    TRACE("-> UI::Exit(%s)\n", fReInit_ ? "reinit" : "");
    TRACE("<- UI::Exit()\n");
}


void UI::Quit ()
{
    g_fQuit = true;
}


void UI::ProcessKey (BYTE bKey_, BYTE bMods_)
{
    bool fPress = !(bKey_ & 0x80);
    bKey_ &= 0x7f;

    // Read the current states of the Ctrl/Alt/Shift keys
    bool fCtrl  = (bMods_ & KB_CTRL_FLAG)  != 0;
    bool fShift = (bMods_ & KB_SHIFT_FLAG) != 0;
    bool fAlt   = (bMods_ & KB_ALT_FLAG)   != 0;

    // A function key?
    if (bKey_ >= KEY_F1 && bKey_ <= KEY_F12)
    {
        // Grab a copy of the function key definition string (could do with being converted to upper-case)
        char szKeys[256];
        strcpy(szKeys, GetOption(fnkeys));

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
                if (bKey_ != (KEY_F1 + (int)strtoul(psz, &psz, 0) - 1))
                    continue;

                // If the Ctrl/Alt/Shift states match, perform the action
                if (fCtrl == fCtrled && fAlt == fAlted && fShift == fShifted)
                    DoAction(strtoul(++psz, NULL, 0), fPress);
            }
        }
    }

    // Some additional function keys
    switch (bKey_)
    {
        case KEY_ESC:         if (GetOption(mouseesc)) Input::Acquire(false); break;
        case KEY_ENTER:       if (fAlt) DoAction(actToggleFullscreen, fPress); break;
        case KEY_MINUS_PAD:   if (GetOption(keypadreset)) DoAction(actResetButton, fPress);   break;
        case KEY_SLASH_PAD:   DoAction(actDebugger, fPress);          break;
        case KEY_ASTERISK:    DoAction(actNmiButton, fPress);         break;
        case KEY_PLUS_PAD:    DoAction(actTempTurbo, fPress);         break;
        case KEY_PRTSCR:      DoAction(actSaveScreenshot, fPress);    break;
        case KEY_SCRLOCK:     // Pause key on some platforms comes through as scoll lock?
        case KEY_PAUSE:
            DoAction((fCtrl ? actResetButton : (fShift ? actFrameStep : actPause)), fPress);   break;
        default:                break;
    }
}


// Check and process any incoming messages
bool UI::CheckEvents ()
{
    Input::Update();

    // Re-pause after a single frame-step
    if (g_fFrameStep)
        DoAction(actFrameStep);

    if (g_fPaused || (!g_fActive && GetOption(pauseinactive)))
        yield_timeslice();

    return !g_fQuit;
}

void UI::ShowMessage (eMsgType eType_, const char* pcszMessage_)
{
    switch (eType_)
    {
        case msgWarning:
            GUI::Start(new CMessageBox(NULL, pcszMessage_, "Warning", mbWarning));
            break;

        case msgError:
            GUI::Start(new CMessageBox(NULL, pcszMessage_, "Error", mbError));
            break;

        // Something went seriously wrong!
        case msgFatal:
            break;

        default:
            break;
    }
}


void UI::ResizeWindow (bool fUseOption_/*=false*/)
{
    Display::SetDirty();
}

////////////////////////////////////////////////////////////////////////////////

bool InsertDisk (CDiskDevice* pDrive_, const char* pName_)
{
    // Remember the read-only state of the previous disk
    bool fReadOnly = !pDrive_->IsWriteable();

    // Eject any previous disk, saving if necessary
    pDrive_->Eject();

    // Open the new disk (using the requested read-only mode), and insert it into the drive if successful
    return pDrive_->Insert(pName_, fReadOnly);
}

void DoAction (int nAction_, bool fPressed_)
{
    // Key being pressed?
    if (fPressed_)
    {
        switch (nAction_)
        {
            case actNmiButton:
                CPU::NMI();
                break;

            case actResetButton:
                // Simulate the reset button being held by part-resetting the CPU and I/O, and holding the sound chip
                CPU::Reset(true);
                Sound::Stop();
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

            case actChangeWindowSize:
#ifdef ALLEGRO_DOS
                GUI::Start(new CMessageBox(NULL, "Window scaling not supported under DOS", "Sorry!", mbInformation));
#else
                SetOption(scale, (GetOption(scale) % 3) + 1);
                Frame::Init();
                Frame::SetStatus("%u%% size", GetOption(scale)*50);
                break;
#endif
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
                SetOption(profile, OSD::GetTime() ? (GetOption(profile)+1) % 4 : !GetOption(profile));
                break;

            case actInsertFloppy1:
                if (GetOption(drive1) != dskImage)
                    Message(msgWarning, "Floppy drive %d is not present", 1);
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
                if (GetOption(drive1) == dskImage && pDrive1->IsModified() && pDrive1->Flush())
                    Frame::SetStatus("%s  changes saved", pDrive1->GetFile());
                break;

            case actInsertFloppy2:
                if (GetOption(drive2) != dskImage)
                    Message(msgWarning, "Floppy drive %d is not present", 2);
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
                if (GetOption(drive2) == dskImage && pDrive2->IsModified() && pDrive2->Flush())
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

            case actExitApplication:
                UI::Quit();
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
                g_fPaused = !g_fPaused;

                if (g_fPaused)
                {
                    Sound::Stop();

                    char szPaused[64];
                    sprintf(szPaused, "%s - Paused", WINDOW_CAPTION);
                    set_window_title(szPaused);
                }
                else
                {
                    Sound::Play();
                    set_window_title(WINDOW_CAPTION);
                    g_fFrameStep = (nAction_ == actFrameStep);
                }

                Video::CreatePalettes();

                Display::SetDirty();
                Frame::Redraw();
                break;
            }
        }
    }

    // Key released
    else
    {
        switch (nAction_)
        {
            case actResetButton:
                // Reset the CPU, and prepare fast reset if necessary
                CPU::Reset(false);

                // Start the fast-boot timer
                if (GetOption(fastreset))
                    g_nFastBooting = EMULATED_FRAMES_PER_SECOND * 5;

                // Start the sound playing, so the sound chip continues to play the current settings
                Sound::Play();
                break;

            case actTempTurbo:
                if (g_fTurbo)
                {
                    Sound::Play();
                    g_fTurbo = false;
                }
                break;

            case actToggleFullscreen:
#ifdef ALLEGRO_DOS
                GUI::Start(new CMessageBox(NULL, "Toggle fullscreen not available under DOS", "Sorry!", mbInformation));
#else
                SetOption(fullscreen, !GetOption(fullscreen));
                Sound::Silence();

                if (GetOption(fullscreen))
                {
                    // ToDo: remember the the window position before then re-initialising the video system
                    Frame::Init();
                }
                else
                {
                    // Re-initialise the video system then set the window back how it was before
                    Frame::Init();
                    // ToDo: restore the window position and size
                }
#endif
                break;

            case actToggle5_4:
#ifdef ALLEGRO_DOS
                GUI::Start(new CMessageBox(NULL, "5:4 mode not yet available", "Sorry!", mbInformation));
#else
                SetOption(ratio5_4, !GetOption(ratio5_4));
                Frame::Init();

                Frame::SetStatus("%s pixel size", GetOption(ratio5_4) ? "5:4" : "1:1");
#endif
                break;
        }
    }
}
