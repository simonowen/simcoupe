// Part of SimCoupe - A SAM Coupe emulator
//
// UI.cpp: SDL user interface
//
//  Copyright (c) 1999-2006  Simon Owen
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

#include "Action.h"
#include "CPU.h"
#include "Display.h"
#include "Frame.h"
#include "GUI.h"
#include "Input.h"
#include "Options.h"

#ifdef _DEBUG
#define WINDOW_CAPTION      "SimCoupe/SDL [DEBUG]"
#else
#define WINDOW_CAPTION      "SimCoupe/SDL"
#endif

bool g_fActive = true;


bool UI::Init (bool fFirstInit_/*=false*/)
{
    bool fRet = true;

    Exit(true);
    TRACE("-> UI::Init()\n");

    // Set the window caption and disable the cursor until needed
    SDL_WM_SetCaption(WINDOW_CAPTION, WINDOW_CAPTION);
    SDL_ShowCursor(SDL_DISABLE);

    // To help on platforms without a native GUI, we'll display a one-time welcome message
#if !defined(__APPLE__) && !defined(_WINDOWS)
    if (GetOption(firstrun))
    {
        // Clear the option so we don't show it again
        SetOption(firstrun, 0);

        // Simple message box showing some keys
        GUI::Start(new CMessageBox(NULL,
                    "Some useful keys to get you started:\n\n"
                    "  F1 - Insert disk image\n"
                    "  F10 - Options\n"
                    "  F12 - Reset\n"
                    "  Ctrl-F12 - Exit emulator\n"
                    "  Numpad-9 - Boot drive 1\n\n"
                    "Consult the README for further details.",
                    "Welcome to SimCoupe!",
                    mbInformation));
    }
#endif

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


// Check and process any incoming messages
bool UI::CheckEvents ()
{
    SDL_Event event;

    // If the GUI is active the input won't be polled by CPU.cpp, so do it here
    if (GUI::IsActive())
        Input::Update();

    // Re-pause after a single frame-step
    if (g_fFrameStep)
        Action::Do(actFrameStep);

    while (1)
    {
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_QUIT:
                    return false;

                case SDL_JOYAXISMOTION:
                case SDL_JOYHATMOTION:
                case SDL_JOYBUTTONDOWN:
                case SDL_JOYBUTTONUP:
                case SDL_MOUSEMOTION:
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    Input::ProcessEvent(&event);
                    break;

                case SDL_ACTIVEEVENT:
                    g_fActive = (event.active.gain != 0);
                    Input::ProcessEvent(&event);
                    break;

                case SDL_VIDEOEXPOSE:
                    Display::SetDirty();
                    break;

                case SDL_USEREVENT:
                {
                    switch (event.user.code)
                    {
                        case UE_OPENFILE:
                        {
                            char *psz = reinterpret_cast<char*>(event.user.data1);
                            TRACE("UE_OPENFILE: %s\n", psz);

                            if (GetOption(drive1) != dskImage)
                                Message(msgWarning, "Floppy drive %d is not present", 1);
                            else if (pDrive1->Insert(psz))
                                Frame::SetStatus("%s  inserted into drive 1", pDrive1->GetFile());

                            free(psz);
                            break;
                        }

                        case UE_RESETBUTTON:
                            Action::Do(actResetButton, true);
                            Action::Do(actResetButton, false);
                            break;

                        case UE_TEMPTURBOON:
                        case UE_TEMPTURBOOFF:
                            Action::Do(actToggleTurbo,event.user.code == UE_TEMPTURBOON);
                            break;

                        case UE_TOGGLEFULLSCREEN:   Action::Do(actToggleFullscreen,false);break;
                        case UE_TOGGLESYNC:         Action::Do(actToggleSync);      break;
                        case UE_TOGGLEGREYSCALE:    Action::Do(actToggleGreyscale); break;
                        case UE_NMIBUTTON:          Action::Do(actNmiButton);       break;
                        case UE_TOGGLESCANLINES:    Action::Do(actToggleScanlines); break;
                        case UE_TOGGLE54:           Action::Do(actToggle5_4);       break;
                        case UE_DEBUGGER:           Action::Do(actDebugger);        break;
                        case UE_SAVESCREENSHOT:     Action::Do(actSaveScreenshot);  break;
                        case UE_CHANGEPROFILER:     Action::Do(actChangeProfiler);  break;
                        case UE_PAUSE:              Action::Do(actPause);           break;
                        case UE_TOGGLETURBO:        Action::Do(actToggleTurbo);     break;
                        case UE_CHANGEFRAMESKIP:    Action::Do(actChangeFrameSkip); break;
                        case UE_TOGGLEMUTE:         Action::Do(actToggleMute);      break;
                        case UE_RELEASEMOUSE:       Action::Do(actReleaseMouse);    break;
                        case UE_CHANGEWINDOWSIZE:   Action::Do(actChangeWindowSize);break;
                        case UE_CHANGEBORDERS:      Action::Do(actChangeBorders);   break;
                        case UE_OPTIONS:            Action::Do(actOptions);         break;

                        default:
                            TRACE("Unhandled SDL_event (%d)\n", event.type);
                            break;
                    }
                }

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
    if (eType_ == msgInfo)
        GUI::Start(new CMessageBox(NULL, pcszMessage_, WINDOW_CAPTION, mbInformation));
    else if (eType_ == msgWarning)
        GUI::Start(new CMessageBox(NULL, pcszMessage_, WINDOW_CAPTION, mbWarning));
    else
        GUI::Start(new CMessageBox(NULL, pcszMessage_, WINDOW_CAPTION, mbError));
}

////////////////////////////////////////////////////////////////////////////////

bool UI::DoAction (int nAction_, bool fPressed_)
{
    // Key pressed?
    if (fPressed_)
    {
        switch (nAction_)
        {
            case actChangeWindowSize:
                GUI::Start(new CMessageBox(NULL, "Window sizing is not supported under SDL", "Sorry!", mbInformation));
                break;

            case actExitApplication:
            {
                SDL_Event event = { SDL_QUIT };
                SDL_PushEvent(&event);
                break;
            }

            case actPause:
            {
                // Reverse logic because we've the default processing hasn't occurred yet
                if (g_fPaused)
                    SDL_WM_SetCaption(WINDOW_CAPTION, WINDOW_CAPTION);
                else
                    SDL_WM_SetCaption(WINDOW_CAPTION " - Paused", WINDOW_CAPTION " - Paused");

                // Perform default processing
                return false;
            }

            // Perform the switch on key-up, to avoid an SDL 1.2.x bug
            case actToggleFullscreen:
                break;

            case actToggle5_4:
#ifndef USE_OPENGL
                GUI::Start(new CMessageBox(NULL, "5:4 mode is not available under SDL", "Sorry!", mbInformation));
#endif
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
            // To avoid an SDL bug (in 1.2.0 anyway), we'll do the following on key up instead of down
            case actToggleFullscreen:
                SetOption(fullscreen, !GetOption(fullscreen));
                Sound::Silence();
                Frame::Init();

                // Grab the mouse automatically in full-screen, or release in windowed mode
                Input::Acquire(!!GetOption(fullscreen), !GUI::IsActive());
                break;

#ifdef USE_OPENGL
            case actToggle5_4:
                SetOption(ratio5_4, !GetOption(ratio5_4));
                Frame::Init();
                Frame::SetStatus("%s aspect ratio", GetOption(ratio5_4) ? "5:4" : "1:1");
                break;
#endif
            // Not processed
            default:
                return false;
        }
    }

    // Action processed
    return true;
}
