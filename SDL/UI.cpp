// Part of SimCoupe - A SAM Coupe emulator
//
// UI.cpp: SDL user interface
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
//  At present this module only really contains the event processing
//  code, for forwarding to other modules and processing fn keys

#include "SimCoupe.h"
#include "UI.h"

#include "Actions.h"
#include "CPU.h"
#include "Frame.h"
#include "GUI.h"
#include "Input.h"
#include "Options.h"
#include "Sound.h"
#include "SDL20.h"

bool UI::Init(bool fFirstInit_/*=false*/)
{
    bool fRet = true;

    Exit(true);

    // Set the window caption and disable the cursor until needed
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    SDL_ShowCursor(SDL_DISABLE);

    // To help on platforms without a native GUI, we'll display a one-time welcome message
#if !defined(__APPLE__) && !defined(_WINDOWS)
    if (GetOption(firstrun))
    {
        // Clear the option so we don't show it again
        SetOption(firstrun, 0);

        // Simple message box showing some keys
        GUI::Start(new MsgBox(nullptr,
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

void UI::Exit(bool fReInit_/*=false*/)
{
}


// Create a video object to render the display
std::unique_ptr<IVideoRenderer> UI::CreateVideo()
{
    return std::make_unique<SDLTexture>();
}


// Check and process any incoming messages
bool UI::CheckEvents()
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

            case SDL_DROPFILE:
                if (pFloppy1->Insert(event.drop.file, true))
                    Frame::SetStatus("%s  inserted into drive 1", pFloppy1->DiskFile());

                SDL_free(event.drop.file);
                break;

            case SDL_USEREVENT:
            {
                switch (event.user.code)
                {
                case UE_OPENFILE:
                {
                    char* psz = reinterpret_cast<char*>(event.user.data1);
                    TRACE("UE_OPENFILE: %s\n", psz);

                    if (GetOption(drive1) != drvFloppy)
                        Message(msgWarning, "Floppy drive %d is not present", 1);
                    else if (pFloppy1->Insert(psz, true))
                        Frame::SetStatus("%s  inserted into drive 1", pFloppy1->DiskFile());

                    free(psz);
                    break;
                }

                case UE_RESETBUTTON:
                    Actions::Do(Action::ResetButton, true);
                    Actions::Do(Action::ResetButton, false);
                    break;

                case UE_TEMPTURBOON:
                case UE_TEMPTURBOOFF:
                    Actions::Do(Action::ToggleTurbo, event.user.code == UE_TEMPTURBOON);
                    break;

                case UE_TOGGLEFULLSCREEN:   Actions::Do(Action::ToggleFullscreen); break;
                case UE_TOGGLEGREYSCALE:    Actions::Do(Action::ToggleGreyscale); break;
                case UE_NMIBUTTON:          Actions::Do(Action::NmiButton);       break;
                case UE_TOGGLE54:           Actions::Do(Action::Toggle5_4);       break;
                case UE_DEBUGGER:           Actions::Do(Action::Debugger);        break;
                case UE_SAVESCREENSHOT:     Actions::Do(Action::SavePNG);         break;
                case UE_PAUSE:              Actions::Do(Action::Pause);           break;
                case UE_TOGGLETURBO:        Actions::Do(Action::ToggleTurbo);     break;
                case UE_TOGGLEMUTE:         Actions::Do(Action::ToggleMute);      break;
                case UE_RELEASEMOUSE:       Actions::Do(Action::ReleaseMouse);    break;
                case UE_OPTIONS:            Actions::Do(Action::Options);         break;
                case UE_IMPORTDATA:         Actions::Do(Action::ImportData);      break;
                case UE_EXPORTDATA:         Actions::Do(Action::ExportData);      break;
                case UE_RECORDAVI:          Actions::Do(Action::RecordAvi);       break;
                case UE_RECORDAVIHALF:      Actions::Do(Action::RecordAviHalf);   break;
                case UE_RECORDGIF:          Actions::Do(Action::RecordGif);       break;
                case UE_RECORDGIFLOOP:      Actions::Do(Action::RecordGifLoop);   break;
                case UE_RECORDWAV:          Actions::Do(Action::RecordWav);       break;
                case UE_RECORDWAVSEGMENT:   Actions::Do(Action::RecordWavSegment); break;

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

        Sound::Silence();
        SDL_WaitEvent(nullptr);
    }

    return true;
}

void UI::ShowMessage(eMsgType eType_, const char* pcszMessage_)
{
    if (eType_ == msgInfo)
        GUI::Start(new MsgBox(nullptr, pcszMessage_, WINDOW_CAPTION, mbInformation));
    else if (eType_ == msgWarning)
        GUI::Start(new MsgBox(nullptr, pcszMessage_, WINDOW_CAPTION, mbWarning));
    else
    {
        fprintf(stderr, "error: %s\n", pcszMessage_);
        GUI::Start(new MsgBox(nullptr, pcszMessage_, WINDOW_CAPTION, mbError));
    }
}

////////////////////////////////////////////////////////////////////////////////

bool UI::DoAction(Action action, bool pressed)
{
    // Key pressed?
    if (pressed)
    {
        switch (action)
        {
        case Action::ExitApplication:
        {
            SDL_Event event = { SDL_QUIT };
            SDL_PushEvent(&event);
            break;
        }

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
