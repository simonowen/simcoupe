// Part of SimCoupe - A SAM Coupe emulator
//
// UI.cpp: SDL user interface
//
//  Copyright (c) 1999-2012 Simon Owen
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
#include "Frame.h"
#include "GUI.h"
#include "Input.h"
#include "Options.h"
#include "Sound.h"
#include "SDL12.h"
#include "SDL_GL.h"

#ifdef _DEBUG
#define WINDOW_CAPTION      "SimCoupe/SDL [DEBUG]"
#else
#define WINDOW_CAPTION      "SimCoupe/SDL"
#endif


bool UI::Init (bool fFirstInit_/*=false*/)
{
    bool fRet = true;

    Exit(true);
    TRACE("UI::Init(%d)\n", fFirstInit_);

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

    return fRet;
}

void UI::Exit (bool fReInit_/*=false*/)
{
    TRACE("UI::Exit(%d)\n", fReInit_);
}


// Create a video object to render the display
VideoBase *UI::GetVideo (bool fFirstInit_)
{
    VideoBase *pVideo = NULL;

#ifdef USE_OPENGL
    // Try for OpenGL first
    pVideo = new OpenGLVideo;
    if (pVideo && !pVideo->Init(fFirstInit_))
        delete pVideo, pVideo = NULL;
#endif

    if (!pVideo)
    {
        // Fall back on a regular SDL surface
        pVideo = new SDLVideo;
        if (!pVideo->Init(fFirstInit_))
        {
            delete pVideo, pVideo = NULL;
            Message(msgError, "Video initialisation failed!");
        }
    }

    return pVideo ;
}


// Check and process any incoming messages
bool UI::CheckEvents ()
{
    SDL_Event event;

    // If the GUI is active the input won't be polled by CPU.cpp, so do it here
    if (GUI::IsActive())
        Input::Update();

    while (1)
    {
        while (SDL_PollEvent(&event))
        {
            // Input has first go at processing any messages
            if (Input::FilterEvent(&event))
                continue;

            switch (event.type)
            {
                case SDL_QUIT:
                    return false;

                case SDL_VIDEOEXPOSE:
                    Video::SetDirty();
                    break;

                case SDL_USEREVENT:
                {
                    switch (event.user.code)
                    {
                        case UE_OPENFILE:
                        {
                            char *psz = reinterpret_cast<char*>(event.user.data1);
                            TRACE("UE_OPENFILE: %s\n", psz);

                            if (GetOption(drive1) != drvFloppy)
                                Message(msgWarning, "Floppy drive %d is not present", 1);
                            else if (pFloppy1->Insert(psz))
                                Frame::SetStatus("%s  inserted into drive 1", pFloppy1->DiskFile());

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
                        case UE_TOGGLEGREYSCALE:    Action::Do(actToggleGreyscale); break;
                        case UE_NMIBUTTON:          Action::Do(actNmiButton);       break;
                        case UE_TOGGLESCANLINES:    Action::Do(actToggleScanlines); break;
                        case UE_TOGGLE54:           Action::Do(actToggle5_4);       break;
                        case UE_DEBUGGER:           Action::Do(actDebugger);        break;
                        case UE_SAVESCREENSHOT:     Action::Do(actSaveScreenshot);  break;
                        case UE_PAUSE:              Action::Do(actPause);           break;
                        case UE_TOGGLETURBO:        Action::Do(actToggleTurbo);     break;
                        case UE_TOGGLEMUTE:         Action::Do(actToggleMute);      break;
                        case UE_RELEASEMOUSE:       Action::Do(actReleaseMouse);    break;
                        case UE_OPTIONS:            Action::Do(actOptions);         break;
                        case UE_IMPORTDATA:         Action::Do(actImportData);      break;
                        case UE_EXPORTDATA:         Action::Do(actExportData);      break;
                        case UE_RECORDAVI:          Action::Do(actRecordAvi);       break;
                        case UE_RECORDAVIHALF:      Action::Do(actRecordAviHalf);   break;
                        case UE_RECORDGIF:          Action::Do(actRecordGif);       break;
                        case UE_RECORDGIFLOOP:      Action::Do(actRecordGifLoop);   break;
                        case UE_RECORDWAV:          Action::Do(actRecordWav);       break;
                        case UE_RECORDWAVSEGMENT:   Action::Do(actRecordWavSegment);break;

                        default:
                            TRACE("Unhandled user event (%d)\n", event.type);
                            break;
                    }
                }
            }
        }

        // If we're not paused, break out to run the next frame
        if (!g_fPaused)
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
            case actExitApplication:
            {
                SDL_Event event = { SDL_QUIT };
                SDL_PushEvent(&event);
                break;
            }

            case actPause:
            {
                // Reverse logic because the default processing hasn't occurred yet
                if (g_fPaused)
                    SDL_WM_SetCaption(WINDOW_CAPTION, WINDOW_CAPTION);
                else
                    SDL_WM_SetCaption(WINDOW_CAPTION " - Paused", WINDOW_CAPTION " - Paused");

                // Perform default processing
                return false;
            }

            // Not processed
            default:
                return false;
        }
    }
    else    // Key released
    {
        // Not processed
        return false;
    }

    // Action processed
    return true;
}
