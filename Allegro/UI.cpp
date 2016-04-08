// Part of SimCoupe - A SAM Coupe emulator
//
// UI.cpp: Allegro user interface
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "SimCoupe.h"
#include "UI.h"

#include "Action.h"
#include "CPU.h"
#include "Display.h"
#include "GUIDlg.h"
#include "Input.h"
#include "Options.h"

#ifdef _DEBUG
#define WINDOW_CAPTION      "SimCoupe/Allegro [DEBUG]"
#else
#define WINDOW_CAPTION      "SimCoupe/Allegro"
#endif

bool g_fActive = true;

bool g_fQuit;
void Quit () { g_fQuit = true; }


bool UI::Init (bool fFirstInit_/*=false*/)
{
    bool fRet = true;
    Exit(true);

    set_close_button_callback(Quit);
    set_window_title(WINDOW_CAPTION);

    return fRet;
}

void UI::Exit (bool fReInit_/*=false*/)
{
}


// Check and process any incoming messages
bool UI::CheckEvents ()
{
    static bool fFirstCall = true;

    // Welcome the user on the first run, but not the first call to this function
    if (!fFirstCall && GetOption(firstrun))
    {
        // Clear the option so we don't show it again
        SetOption(firstrun, 0);

        // Simple message box showing some keys
        GUI::Start(new CMessageBox(NULL,
                    "Some useful keys to get you started:\n\n"
                    "  F1 - Insert disk image\n"
                    "  F10 - Options\n"
                    "  F12 - Reset\n"
                    "  Ctrl-F12 - Exit emulator\n\n"
                    "Consult the ReadMe.txt for further details.",
                    "Welcome to SimCoupe!",
                    mbInformation));

        // Disable scanlines by default in Allegro
        SetOption(scanlines, false);
    }

    Input::Update();

    // Re-pause after a single frame-step
    if (g_fFrameStep)
        Action::Do(actFrameStep);

    if (g_fPaused || (!g_fActive && GetOption(pauseinactive)))
        rest(0);

    fFirstCall = false;
    return !g_fQuit;
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

void UI::ResizeWindow (bool fUseOption_/*=false*/)
{
    Display::SetDirty();
}

////////////////////////////////////////////////////////////////////////////////

bool UI::DoAction (int nAction_, bool fPressed_)
{
    // Key being pressed?
    if (fPressed_)
    {
        switch (nAction_)
        {
            case actChangeWindowSize:
#ifdef ALLEGRO_DOS
                GUI::Start(new CMessageBox(NULL, "Window scaling not supported under DOS", "Sorry!", mbInformation));
                break;
#endif
                return false;

              case actToggleFullscreen:
#ifdef ALLEGRO_DOS
                GUI::Start(new CMessageBox(NULL, "Toggle fullscreen not available under DOS", "Sorry!", mbInformation));
                break;
#endif
                return false;

            case actToggle5_4:
#ifdef ALLEGRO_DOS
                GUI::Start(new CMessageBox(NULL, "5:4 mode not yet available", "Sorry!", mbInformation));
                break;
#endif
                return false;

            case actExitApplication:
                Quit();
                break;

            case actPause:
            {
                set_window_title(g_fPaused ? WINDOW_CAPTION : WINDOW_CAPTION " - Paused");
                return false;
            }

            // Not processed
            default:
                return false;
        }
    }

    // Key release not processed
    else
        return false;

    // Action processed
    return true;
}
