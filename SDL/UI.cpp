// Part of SimCoupe - A SAM Coupe emulator
//
// UI.cpp: SDL user interface
//
//  Copyright (c) 1999-2002  Simon Owen
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
#include "SDL.h"

#include "UI.h"

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

const char* const WINDOW_CAPTION =
#if defined(__BEOS__) || defined(__APPLE__) || defined(__QNX__)
    "SimCoup\xc3\xa9/SDL"
#else
    "SimCoup\xe9/SDL"
#endif
#ifdef _DEBUG
    " [DEBUG]"
#endif
;

void DoAction (int nAction_, bool fPressed_=true);

bool g_fActive = true, g_fFrameStep = false;


enum eActions
{
    actNmiButton, actResetButton, actToggleSaaSound, actToggleBeeper, actToggleFullscreen,
    actToggle5_4, actToggle50HzSync, actChangeWindowScale, actChangeFrameSkip, actChangeProfiler, actChangeMouse,
    actChangeKeyMode, actInsertFloppy1, actEjectFloppy1, actSaveFloppy1, actInsertFloppy2, actEjectFloppy2,
    actSaveFloppy2, actNewDisk, actSaveScreenshot, actFlushPrintJob, actDebugger, actImportData, actExportData,
    actDisplayOptions, actExitApplication, actToggleTurbo, actTempTurbo, actReleaseMouse, actPause, actFrameStep,
    actPrinterOnline, MAX_ACTION
};

const char* aszActions[MAX_ACTION] =
{
    "NMI button", "Reset button", "Toggle SAA 1099 sound", "Toggle beeper sound", "Toggle fullscreen",
    "Toggle 5:4 aspect ratio", "Toggle 50Hz frame sync", "Change window scale", "Change frame-skip mode",
    "Change profiler mode", "Change mouse mode", "Change keyboard mode", "Insert floppy 1", "Eject floppy 1",
    "Save changes to floppy 1", "Insert floppy 2", "Eject floppy 2", "Save changes to floppy 2", "New Disk",
    "Save screenshot", "Flush print job", "Debugger", "Import data", "Export data", "Display options",
    "Exit application", "Toggle turbo speed", "Turbo speed (when held)", "Release mouse capture", "Pause",
    "Step single frame", "Toggle printer online"
};


bool UI::Init (bool fFirstInit_/*=false*/)
{
    bool fRet = true;

    Exit(true);
    TRACE("-> UI::Init()\n");

    // Set the window caption and disable the cursor until needed
    SDL_WM_SetCaption(WINDOW_CAPTION, WINDOW_CAPTION);
    SDL_ShowCursor(SDL_DISABLE);

    TRACE("<- UI::Init() returning %s\n", fRet ? "true" : "false");
    return fRet;
}


void UI::Exit (bool fReInit_/*=false*/)
{
    TRACE("-> UI::Exit(%s)\n", fReInit_ ? "reinit" : "");

    if (!fReInit_)
        SDL_QuitSubSystem(SDL_INIT_TIMER);

    TRACE("<- UI::Exit()\n");
}


void ProcessKey (SDL_Event* pEvent_)
{
    SDL_keysym* pKey = &pEvent_->key.keysym;
    bool fPress = pEvent_->type == SDL_KEYDOWN;

    // A function key?
    if (pKey->sym >= SDLK_F1 && pKey->sym <= SDLK_F12)
    {
        // Read the current state of the Ctrl/Alt/Shift keys
        bool fCtrl  = (pKey->mod & KMOD_CTRL)  != 0;
        bool fAlt   = (pKey->mod & KMOD_ALT)   != 0;
        bool fShift = (pKey->mod & KMOD_SHIFT) != 0;

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
                if (pKey->sym != (SDLK_F1 + (int)strtoul(psz, &psz, 0) - 1))
                    continue;

                // If the Ctrl/Shift states match, perform the action
                if (fCtrl == fCtrled && fAlt == fAlted && fShift == fShifted)
                    DoAction(strtoul(++psz, NULL, 0), fPress);
            }
        }
    }

    // Some additional function keys
    switch (pKey->sym)
    {
        case SDLK_RETURN:       if (pKey->mod & KMOD_ALT) DoAction(actToggleFullscreen, fPress);    break;
        case SDLK_KP_MINUS:     DoAction(actResetButton, fPress);       break;
        case SDLK_KP_DIVIDE:    DoAction(actDebugger, fPress);          break;
        case SDLK_KP_MULTIPLY:  DoAction(actNmiButton, fPress);         break;
        case SDLK_KP_PLUS:      DoAction(actTempTurbo, fPress);         break;
        case SDLK_SYSREQ:       DoAction(actSaveScreenshot, fPress);    break;
        case SDLK_SCROLLOCK:    // Pause key on some platforms comes through as scoll lock?
        case SDLK_PAUSE:        DoAction((pKey->mod & KMOD_CTRL) ? actResetButton :
                                         (pKey->mod & KMOD_SHIFT) ? actFrameStep : actPause, fPress);   break;
        default:                break;
    }
}

// Check and process any incoming messages
bool UI::CheckEvents ()
{
    SDL_Event event;

    // If the GUI is active the input won't be polled by CPU.cpp, so do it here
    if (GUI::IsActive())
        Input::Update();

    // Re-pause after a single frame-step
    if (g_fFrameStep)
        DoAction(actFrameStep);

    while (1)
    {
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT:
                    return false;

                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    Input::ProcessEvent(&event);
                    ProcessKey(&event);
                    break;

                case SDL_JOYAXISMOTION:
                case SDL_JOYHATMOTION:
                case SDL_JOYBUTTONDOWN:
                case SDL_JOYBUTTONUP:
                case SDL_MOUSEMOTION:
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                    Input::ProcessEvent(&event);
                    break;

                case SDL_ACTIVEEVENT:
                    g_fActive = (event.active.gain != 0);
                    Input::ProcessEvent(&event);
                    break;

                case SDL_VIDEOEXPOSE:
                    Display::SetDirty();
                    break;

                default:
                    TRACE("Unhandled SDL_event (%d)\n", event.type);
                    break;
            }
        }

        // Continue running if we're active or allowed to run in the background
        if (!g_fPaused && (g_fActive || !GetOption(pauseinactive)))
            break;

        SDL_WaitEvent(NULL);
    }

    return true;
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

////////////////////////////////////////////////////////////////////////////////

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

            case actToggleSaaSound:
                SetOption(saasound, !GetOption(saasound));
                Sound::Init();
                Frame::SetStatus("SAA 1099 sound chip %s", GetOption(saasound) ? "enabled" : "disabled");
                break;

            case actToggleBeeper:
                SetOption(beeper, !GetOption(beeper));
                Sound::Init();
                Frame::SetStatus("Beeper %s", GetOption(beeper) ? "enabled" : "disabled");
                break;

            case actToggle50HzSync:
                SetOption(sync, !GetOption(sync));
                Frame::SetStatus("Frame sync %s", GetOption(sync) ? "enabled" : "disabled");
                break;

            case actChangeWindowScale:
                GUI::Start(new CMessageBox(NULL, "Window scaling not supported under SDL", "Sorry!", mbInformation));
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
                SetOption(profile, (GetOption(profile)+1) % 4);
                break;

            case actChangeMouse:
                SetOption(mouse, !GetOption(mouse));
                Input::Acquire(GetOption(mouse) != 0);
                Frame::SetStatus("Mouse %s", !GetOption(mouse) ? "disabled" : "enabled");
                break;

            case actChangeKeyMode:
                SetOption(keymapping, (GetOption(keymapping)+1) % 3);
                Frame::SetStatus(!GetOption(keymapping) ? "Raw keyboard mode" :
                                GetOption(keymapping)==1 ? "SAM Coupe keyboard mode" : "Spectrum keyboard mode");
                break;

            case actInsertFloppy1:
                if (GetOption(drive1) == dskImage)
                    GUI::Start(new CInsertFloppy(1));
                break;

            case actEjectFloppy1:
                if (GetOption(drive1) == dskImage && pDrive1->IsInserted())
                {
                    SetOption(disk1, pDrive1->GetImage());
                    pDrive1->Eject();
                    Frame::SetStatus("Ejected disk from drive 1");
                }
                break;

            case actSaveFloppy1:
                if (GetOption(drive1) == dskImage && pDrive1->IsModified() && pDrive1->Flush())
                    Frame::SetStatus("Saved changes to disk in drive 1");
                break;

            case actInsertFloppy2:
                if (GetOption(drive2) == dskImage)
                    GUI::Start(new CInsertFloppy(2));
                break;

            case actEjectFloppy2:
                if (GetOption(drive2) == dskImage && pDrive2->IsInserted())
                {
                    SetOption(disk2, pDrive2->GetImage());
                    pDrive2->Eject();
                    Frame::SetStatus("Ejected disk from drive 2");
                }
                break;

            case actSaveFloppy2:
                if (GetOption(drive2) == dskImage && pDrive2->IsModified() && pDrive2->Flush())
                    Frame::SetStatus("Saved changes to disk in drive 2");
                break;

            case actNewDisk:
                GUI::Start(new CMessageBox(NULL, "New Disk not yet implemented", "Sorry!", mbInformation));
                break;

            case actSaveScreenshot:
                Frame::SaveFrame();
                break;

            case actDebugger:
				GUI::Start(new CMessageBox(NULL, "Debugger not yet implemented", "Sorry!", mbInformation));
                break;

            case actImportData:
                GUI::Start(new CMessageBox(NULL, "Import Data not yet implemented", "Sorry!", mbInformation));
                break;

            case actExportData:
                GUI::Start(new CMessageBox(NULL, "Export Data not yet implemented", "Sorry!", mbInformation));
                break;

            case actDisplayOptions:
                GUI::Start(new COptionsDialog);
                break;

            case actExitApplication:
            {
                SDL_Event event = { SDL_QUIT };
                SDL_PushEvent(&event);
                break;
            }

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
                    Sound::Silence();
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
            }   // Fall through to actPause...

            case actPause:
            {
                g_fPaused = !g_fPaused;

                if (g_fPaused)
                {
                    Sound::Stop();

                    char szPaused[64];
                    sprintf(szPaused, "%s - Paused", WINDOW_CAPTION);
                    SDL_WM_SetCaption(szPaused, szPaused);
                }
                else
                {
                    Sound::Play();
                    SDL_WM_SetCaption(WINDOW_CAPTION, WINDOW_CAPTION);
                    g_fFrameStep = (nAction_ == actFrameStep);
                }

                Video::CreatePalettes();

                Display::SetDirty();
                Frame::Redraw();
                break;
            }

            case actFlushPrintJob:
                IO::InitParallel();
                Frame::SetStatus("Flushed active print job");
                break;

            case actPrinterOnline:
                SetOption(printeronline, !GetOption(printeronline));
                Frame::SetStatus("Printer %s", GetOption(printeronline) ? "ONLINE" : "OFFLINE");
                break;
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
                    Sound::Silence();
                    g_fTurbo = false;
                }
                break;

            // To avoid an SDL bug (in 1.2.0 anyway), we'll do the following on key up instead of down
            case actToggleFullscreen:
                SetOption(fullscreen, !GetOption(fullscreen));
                Sound::Silence();

                // Reinitialise the video
                Frame::Init();

                // Grab the mouse automatically in full-screen, or release in windowed mode
                Input::Acquire(GetOption(fullscreen), !GUI::IsActive());
                break;

            case actToggle5_4:
#ifdef USE_OPENGL
                SetOption(ratio5_4, !GetOption(ratio5_4));
                Frame::Init();
                Frame::SetStatus("%s pixel size", GetOption(ratio5_4) ? "5:4" : "1:1");
#else
                GUI::Start(new CMessageBox(NULL, "5:4 mode not available under SDL", "Sorry!", mbInformation));
#endif
                break;
        }
    }
}
