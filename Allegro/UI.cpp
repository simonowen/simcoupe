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

    set_window_close_hook(Quit);
    set_window_title(WINDOW_CAPTION);

    return fRet;
}

void UI::Exit (bool fReInit_/*=false*/)
{
}


// Check and process any incoming messages
bool UI::CheckEvents ()
{
    Input::Update();

    // Re-pause after a single frame-step
    if (g_fFrameStep)
        Action::Do(actFrameStep);

    if (g_fPaused || (!g_fActive && GetOption(pauseinactive)))
        yield_timeslice();

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
