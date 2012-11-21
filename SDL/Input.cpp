// Part of SimCoupe - A SAM Coupe emulator
//
// Input.cpp: SDL keyboard, mouse and joystick input
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

#include "SimCoupe.h"

#include "Action.h"
#include "Frame.h"
#include "GUI.h"
#include "Input.h"
#include "IO.h"
#include "Joystick.h"
#include "Keyboard.h"
#include "Options.h"
#include "Mouse.h"
#include "Util.h"
#include "UI.h"

//#define USE_JOYPOLLING

SDL_Joystick *pJoystick1, *pJoystick2;

bool fMouseActive, fKeyboardActive;
int nCentreX, nCentreY;

////////////////////////////////////////////////////////////////////////////////

bool Input::Init (bool fFirstInit_/*=false*/)
{
    Exit(true);

    // Initialise the joystick subsystem
    if (!SDL_InitSubSystem(SDL_INIT_JOYSTICK))
    {
        // Loop through the available devices for the ones to use (if any)
        for (int i = 0 ; i < SDL_NumJoysticks() ; i++)
        {
            // Match against the required joystick names, or auto-select the first available
            if (!pJoystick1 && (!strcasecmp(SDL_JoystickName(i), GetOption(joydev1)) || !*GetOption(joydev1)))
                pJoystick1 = SDL_JoystickOpen(i);
            else if (!pJoystick2 && (!strcasecmp(SDL_JoystickName(i), GetOption(joydev2)) || !*GetOption(joydev2)))
                pJoystick2 = SDL_JoystickOpen(i);
        }

#ifdef USE_JOYPOLLING
        // Disable joystick events as we'll poll ourselves when necessary
        SDL_JoystickEventState(SDL_DISABLE);
#endif
    }

    Keyboard::Init();
    SDL_EnableUNICODE(1);

    fMouseActive = false;

    return true;
}

void Input::Exit (bool fReInit_/*=false*/)
{
    if (!fReInit_)
    {
        if (SDL_WasInit(SDL_INIT_JOYSTICK))
        {
            if (pJoystick1) {SDL_JoystickClose(pJoystick1); pJoystick1 = NULL; }
            if (pJoystick2) {SDL_JoystickClose(pJoystick2); pJoystick2 = NULL; }

            SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
        }
    }
}


void Input::AcquireMouse (bool fAcquire_)
{
    fMouseActive = fAcquire_;

    // Mouse active?
    if (fMouseActive && GetOption(mouse))
    {
        // Move the mouse to the centre of the window
        nCentreX = Frame::GetWidth() >> 1;
        nCentreY = Frame::GetHeight() >> 1;
        SDL_WarpMouse(nCentreX, nCentreY);
    }
}


// Purge pending keyboard and/or mouse events
void Input::Purge ()
{
    // Remove queued input messages
    SDL_Event event;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_MOUSEEVENTMASK|SDL_KEYEVENTMASK) > 0) ;

    // Discard relative motion and clear the key modifiers
    int n;
    SDL_GetRelativeMouseState(&n, &n);
    SDL_SetModState(KMOD_NONE);

    Keyboard::Purge();
}


#ifdef USE_JOYPOLLING
// Read the specified joystick
static void ReadJoystick (int nJoystick_, SDL_Joystick *pJoystick_, int nTolerance_)
{
    int nPosition = HJ_CENTRE;
    DWORD dwButtons = 0;
    Uint8 bHat = 0;
    int i;

    int nDeadZone = 32768*nTolerance_/100;
    int nButtons = SDL_JoystickNumButtons(pJoystick_);
    int nHats = SDL_JoystickNumHats(pJoystick_);
    int nX = SDL_JoystickGetAxis(pJoystick_, 0);
    int nY = SDL_JoystickGetAxis(pJoystick_, 1);

    for (i = 0 ; i < nButtons ; i++)
        dwButtons |= SDL_JoystickGetButton(pJoystick_, i) << i;

    for (i = 0 ; i < nHats ; i++)
        bHat |= SDL_JoystickGetHat(pJoystick_, i);


    if ((nX < -nDeadZone) || (bHat & SDL_HAT_LEFT))  nPosition |= HJ_LEFT;
    if ((nX >  nDeadZone) || (bHat & SDL_HAT_RIGHT)) nPosition |= HJ_RIGHT;
    if ((nY < -nDeadZone) || (bHat & SDL_HAT_UP))    nPosition |= HJ_UP;
    if ((nY >  nDeadZone) || (bHat & SDL_HAT_DOWN))  nPosition |= HJ_DOWN;

    Joystick::SetPosition(nJoystick_, nPosition);
    Joystick::SetButtons(nJoystick_, dwButtons);
}
#endif // USE_JOYPOLLING


// Process SDL event messages
bool Input::FilterEvent (SDL_Event* pEvent_)
{
    switch (pEvent_->type)
    {
        case SDL_ACTIVEEVENT:
            // Release the mouse when we lose input focus
            if (!pEvent_->active.gain && pEvent_->active.state & SDL_APPINPUTFOCUS)
                AcquireMouse(false);

            // Refresh the keyboard mappings when we become active as they might have changed
            if (pEvent_->active.gain && pEvent_->active.state & SDL_APPACTIVE)
                Keyboard::Init();

            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            SDL_keysym* pKey = &pEvent_->key.keysym;

            bool fPress = pEvent_->type == SDL_KEYDOWN;
            bool fCtrl  = !!(pKey->mod & KMOD_CTRL);
            bool fAlt   = !!(pKey->mod & KMOD_ALT);
            bool fShift = !!(pKey->mod & KMOD_SHIFT);

            // Unpause on key press if paused, so the user doesn't think we've hung
            if (fPress && g_fPaused)
                Action::Do(actPause);

            // Use key repeats for GUI mode only
            if (fKeyboardActive == GUI::IsActive())
            {
                fKeyboardActive = !fKeyboardActive;
                SDL_EnableKeyRepeat(fKeyboardActive ? 0 : 250, fKeyboardActive ? 0 : 30);
            }

            // Check for the Windows key, for use as a modifier
            int numkeys = 0;
            Uint8 *pKeyStates = SDL_GetKeyState(&numkeys);
            bool fWin = pKeyStates[SDLK_LSUPER] || pKeyStates[SDLK_RSUPER];

            // Check for function keys (unless the Windows key is pressed)
            if (!fWin && pKey->sym >= SDLK_F1 && pKey->sym <= SDLK_F12)
            {
                Action::Key(pKey->sym-SDLK_F1+1, fPress, fCtrl, fAlt, fShift);
                break;
            }

            // Some additional function keys
            bool fAction = true;
            switch (pKey->sym)
            {
                case SDLK_RETURN:       fAction = fAlt; if (fAction) Action::Do(actToggleFullscreen, fPress); break;
                case SDLK_KP_DIVIDE:    Action::Do(actDebugger, fPress); break;
                case SDLK_KP_MULTIPLY:  Action::Do(fCtrl ? actResetButton : actTempTurbo, fPress); break;
                case SDLK_KP_PLUS:      Action::Do(fCtrl ? actTempTurbo : actSpeedFaster, fPress); break;
                case SDLK_KP_MINUS:     Action::Do(fCtrl ? actSpeedNormal : actSpeedSlower, fPress); break;

                case SDLK_SYSREQ:
                case SDLK_PRINT:        Action::Do(actSaveScreenshot, fPress);    break;
                case SDLK_SCROLLOCK:
                case SDLK_PAUSE:        Action::Do(fCtrl ? actResetButton : fShift ? actFrameStep : actPause, fPress);   break;
                default:                fAction = false; break;
            }

            // Have we processed the key?
            if (fAction)
                break;


            // Keep only the normal ctrl/shift/alt modifiers
            pKey->mod = static_cast<SDLMod>(pKey->mod & (KMOD_CTRL|KMOD_SHIFT|KMOD_ALT));

            int nMods = HM_NONE;
            if (pKey->mod & KMOD_SHIFT) nMods |= HM_SHIFT;
            if (pKey->mod & KMOD_LCTRL) nMods |= HM_CTRL;
            if (pKey->mod & KMOD_LALT)  nMods |= HM_ALT;


            // A number of adjustments must be made to keydown characters
            if (fPress)
            {
                // Force some simple keys that might be wrong
                switch (pKey->sym)
                {
                    // Force the character some some basic keys
                    case SDLK_BACKSPACE:
                    case SDLK_TAB:
                    case SDLK_RETURN:
                    case SDLK_ESCAPE:
                        pKey->unicode = pKey->sym;
                        break;
/*
// Are these still needed?
                    // Alt-Gr comes through as SDLK_MODE on some platforms and SDLK_RALT on others, so accept both
                    // Right-Alt on the Mac somehow appears as SDLK_KP_ENTER, so we'll map that for now too!
                    case SDLK_MODE:
                    case SDLK_KP_ENTER:
                        pKey->sym = SDLK_RALT;
                        break;
*/
/*
// Are these still needed?
                    // Keys not recognised by SDL
                    case SDLK_UNKNOWN:
                        
                        switch (pKey->scancode)
                        {
                            case 0x56:  pKey->sym = SDLK_WORLD_95;  break;
                            case 0xc5:  pKey->sym = SDLK_PAUSE;     break;

                            // Fill some missing keypad symbols on Solaris
                            case 0x77:  pKey->sym = SDLK_KP1;       break;
                            case 0x79:  pKey->sym = SDLK_KP3;       break;
                            case 0x63:  pKey->sym = SDLK_KP5;       break;
                            case 0x4b:  pKey->sym = SDLK_KP7;       break;
                            case 0x4d:  pKey->sym = SDLK_KP9;       break;
                            default:                                break;
                        }
                        break;
*/
                    default:
                        // Keypad numbers
                        if (pKey->sym >= SDLK_KP0 && pKey->sym <= SDLK_KP9)
                            pKey->unicode = HK_KP0 + pKey->sym - SDLK_KP0;

                        // Keypad symbols (mask symbol)
                        else if (pKey->sym >= SDLK_KP_PERIOD && pKey->sym <= SDLK_KP_EQUALS)
                            pKey->unicode = 0;

                        // Cursor keys
                        else if (pKey->sym >= SDLK_UP && pKey->sym <= SDLK_LEFT)
                        {
                            int anCursors[] = { HK_UP, HK_DOWN, HK_RIGHT, HK_LEFT };
                            pKey->unicode = anCursors[pKey->sym - SDLK_UP];
                        }

                        // Navigation block
                        else if (pKey->sym >= SDLK_HOME && pKey->sym <= SDLK_PAGEDOWN)
                        {
                            int anMovement[] = { HK_HOME, HK_END, HK_PGUP, HK_PGDN };
                            pKey->unicode = anMovement[pKey->sym - SDLK_HOME];
                        }

                        // Delete
                        else if (pKey->sym == SDLK_DELETE)
                            pKey->unicode = HK_DELETE;

                        // Convert ctrl-letter/digit to the base key (locale mapping not possible)
                        if (pKey->mod & KMOD_CTRL)
                        {
                            if (pKey->sym >= SDLK_a && pKey->sym <= SDLK_z)
                                pKey->unicode = 'a' + pKey->sym - SDLK_a;

                            else if (pKey->sym >= SDLK_0 && pKey->sym <= SDLK_9)
                                pKey->unicode = '0' + pKey->sym - SDLK_0;
                        }

                        break;
                }
            }
/*
// Are these still needed?
#if defined(sun) || defined(SOLARIS)
            // Fix some keypad mappings known to be wrong on Solaris
            if (pKey->sym == 0x111 && pKey->scancode == 0x4c)
                pKey->sym = SDLK_KP8;
            else if (pKey->sym == 0x112 && pKey->scancode == 0x78)
                pKey->sym = SDLK_KP2;
            else if (pKey->sym == 0x113 && pKey->scancode == 0x64)
                pKey->sym = SDLK_KP6;
            else if (pKey->sym == 0x114 && pKey->scancode == 0x62)
                pKey->sym = SDLK_KP4;
#endif
*/
            TRACE("Key %s: %d (mods=%03x u=%d)\n", (pEvent_->key.state == SDL_PRESSED) ? "down" : "up", pKey->sym, pKey->mod, pKey->unicode);

            if (GUI::IsActive())
            {
                // Pass any printable characters to the GUI
                if (fPress && pKey->unicode)
                    GUI::SendMessage(GM_CHAR, pKey->unicode, nMods);
            }
            else
            {
                // Optionally release the mouse capture if Esc is pressed
                if (fPress && pKey->sym == SDLK_ESCAPE && GetOption(mouseesc))
                    AcquireMouse(false);

                // Key press (CapsLock/NumLock are toggle keys in SDL, so we much treat any event as a press)
                if (fPress || pKey->sym == SDLK_CAPSLOCK || pKey->sym == SDLK_NUMLOCK)
                    Keyboard::SetKey(pKey->sym, true, nMods, pKey->unicode);

                // Key release
                else if (!fPress)
                    Keyboard::SetKey(pKey->sym, false);
            }

            break;
        }

        case SDL_MOUSEMOTION:
        {
            int nX = pEvent_->motion.x, nY = pEvent_->motion.y;

            // Show the cursor in windowed mode unless the mouse is acquired or the GUI is active
            bool fShowCursor = !fMouseActive && !GUI::IsActive() && !GetOption(fullscreen);
            SDL_ShowCursor(fShowCursor ? SDL_ENABLE : SDL_DISABLE);

            // Mouse in use by the GUI?
            if (GUI::IsActive())
            {
                Video::DisplayToSamPoint(&nX, &nY);
                GUI::SendMessage(GM_MOUSEMOVE, nX, nY);
            }

            // Mouse captured for emulation?
            else if (fMouseActive)
            {
                // Work out the relative movement since last time
                nX -= nCentreX;
                nY -= nCentreY;

                // Any native movement?
                if (nX || nY)
                {
                    Video::DisplayToSamSize(&nX, &nY);

                    // Any SAM movement?
                    if (nX || nY)
                    {
                        // Update the SAM mouse and re-centre the cursor
                        pMouse->Move(nX, -nY);
                        SDL_WarpMouse(nCentreX, nCentreY);
                    }
                }
            }
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
        {
            int nX = pEvent_->button.x, nY = pEvent_->button.y;

            // Button presses go to the GUI if it's active
            if (GUI::IsActive())
            {
                Video::DisplayToSamPoint(&nX, &nY);

                switch (pEvent_->button.button)
                {
                    // Mouse wheel up and down
                    case 4:  GUI::SendMessage(GM_MOUSEWHEEL, -1); break;
                    case 5:  GUI::SendMessage(GM_MOUSEWHEEL,  1); break;

                    // Any other mouse button
                    default: GUI::SendMessage(GM_BUTTONDOWN, nX, nY); break;
                }
            }

            // Pass the button click through if the mouse is active
            else if (fMouseActive)
            {
                pMouse->SetButton(pEvent_->button.button, true);
                TRACE("Mouse button %d pressed\n", pEvent_->button.button);
            }

            // If the mouse interface is enabled, a left-click acquires it
            else if (GetOption(mouse) && pEvent_->button.button == 1)
                AcquireMouse();

            break;
        }

        case SDL_MOUSEBUTTONUP:
            // Button presses go to the GUI if it's active
            if (GUI::IsActive())
            {
                int nX = pEvent_->button.x, nY = pEvent_->button.y;
                Video::DisplayToSamPoint(&nX, &nY);
                GUI::SendMessage(GM_BUTTONUP, nX, nY);
            }
            else if (fMouseActive)
            {
                TRACE("Mouse button %d released\n", pEvent_->button.button);
                pMouse->SetButton(pEvent_->button.button, false);
            }
            break;


#ifndef USE_JOYPOLLING

        case SDL_JOYAXISMOTION:
        {
            SDL_JoyAxisEvent* p = &pEvent_->jaxis;
            int nJoystick = (SDL_JoystickIndex(pJoystick1) == p->which) ? 0 : 1;
            int nDeadZone = 32768 * (!nJoystick ? GetOption(deadzone1) : GetOption(deadzone2)) / 100;

            // We'll use even axes as X and odd as Y
            if (!(p->axis & 1))
                Joystick::SetX(nJoystick, (p->value < -nDeadZone) ? HJ_LEFT : (p->value > nDeadZone) ? HJ_RIGHT : HJ_CENTRE);
            else
                Joystick::SetY(nJoystick, (p->value < -nDeadZone) ? HJ_UP : (p->value > nDeadZone) ? HJ_DOWN : HJ_CENTRE);

            break;
        }

        case SDL_JOYHATMOTION:
        {
            SDL_JoyHatEvent *p = &pEvent_->jhat;
            int nJoystick = (SDL_JoystickIndex(pJoystick1) == p->which) ? 0 : 1;
            Uint8 bHat = p->value;

            int nPosition = HJ_CENTRE;
            if (bHat & SDL_HAT_LEFT)  nPosition |= HJ_LEFT;
            if (bHat & SDL_HAT_RIGHT) nPosition |= HJ_RIGHT;
            if (bHat & SDL_HAT_DOWN)  nPosition |= HJ_DOWN;
            if (bHat & SDL_HAT_UP)    nPosition |= HJ_UP;

            Joystick::SetPosition(nJoystick, nPosition);
            break;
        }

        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
        {
            SDL_JoyButtonEvent *p = &pEvent_->jbutton;
            int nJoystick = (SDL_JoystickIndex(pJoystick1) == p->which) ? 0 : 1;

            Joystick::SetButton(nJoystick, p->button, (p->state == SDL_PRESSED));
            break;
        }
#endif
    }

    // Allow additional event processing
    return false;
}


void Input::Update ()
{
#ifdef USE_JOYPOLLING
    // Either joystick active?
    if (pJoystick1 || pJoystick2)
    {
        // Update and read the current joystick states
        SDL_JoystickUpdate();
        if (pJoystick1) ReadJoystick(0, pJoystick1, GetOption(deadzone1));
        if (pJoystick2) ReadJoystick(1, pJoystick2, GetOption(deadzone2));
    }
#endif

    Keyboard::Update();

    // CapsLock/NumLock are toggle keys in SDL and must be released manually
    Keyboard::SetKey(SDLK_CAPSLOCK, false);
    Keyboard::SetKey(SDLK_NUMLOCK, false);
}


int Input::MapChar (int nChar_, int *pnMods_)
{
    // Regular characters details aren't known until the key press
    if (nChar_ < HK_MIN)
        return 0;

    // Host keycode
    switch (nChar_)
    {
        case HK_LSHIFT:     return SDLK_LSHIFT;
        case HK_RSHIFT:     return SDLK_RSHIFT;
        case HK_LCTRL:      return SDLK_LCTRL;
        case HK_RCTRL:      return SDLK_RCTRL;
        case HK_LALT:       return SDLK_LALT;
        case HK_RALT:       return SDLK_RALT;
        case HK_LWIN:       return SDLK_LSUPER;
        case HK_RWIN:       return SDLK_RSUPER;

        case HK_LEFT:       return SDLK_LEFT;
        case HK_RIGHT:      return SDLK_RIGHT;
        case HK_UP:         return SDLK_UP;
        case HK_DOWN:       return SDLK_DOWN;

        case HK_KP0:        return SDLK_KP0;
        case HK_KP1:        return SDLK_KP1;
        case HK_KP2:        return SDLK_KP2;
        case HK_KP3:        return SDLK_KP3;
        case HK_KP4:        return SDLK_KP4;
        case HK_KP5:        return SDLK_KP5;
        case HK_KP6:        return SDLK_KP6;
        case HK_KP7:        return SDLK_KP7;
        case HK_KP8:        return SDLK_KP8;
        case HK_KP9:        return SDLK_KP9;

        case HK_F1:         return SDLK_F1;
        case HK_F2:         return SDLK_F2;
        case HK_F3:         return SDLK_F3;
        case HK_F4:         return SDLK_F4;
        case HK_F5:         return SDLK_F5;
        case HK_F6:         return SDLK_F6;
        case HK_F7:         return SDLK_F7;
        case HK_F8:         return SDLK_F8;
        case HK_F9:         return SDLK_F9;
        case HK_F10:        return SDLK_F10;
        case HK_F11:        return SDLK_F11;
        case HK_F12:        return SDLK_F12;

        case HK_CAPSLOCK:   return SDLK_CAPSLOCK;
        case HK_NUMLOCK:    return SDLK_NUMLOCK;
        case HK_KPPLUS:     return SDLK_KP_PLUS;
        case HK_KPMINUS:    return SDLK_KP_MINUS;
        case HK_KPMULT:     return SDLK_KP_MULTIPLY;
        case HK_KPDIVIDE:   return SDLK_KP_DIVIDE;
        case HK_KPENTER:    return SDLK_KP_ENTER;
        case HK_KPDECIMAL:  return SDLK_KP_PERIOD;

        case HK_INSERT:     return SDLK_INSERT;
        case HK_DELETE:     return SDLK_DELETE;
        case HK_HOME:       return SDLK_HOME;
        case HK_END:        return SDLK_END;
        case HK_PGUP:       return SDLK_PAGEUP;
        case HK_PGDN:       return SDLK_PAGEDOWN;

        case HK_ESC:        return SDLK_ESCAPE;
        case HK_TAB:        return SDLK_TAB;
        case HK_BACKSPACE:  return SDLK_BACKSPACE;
        case HK_RETURN:     return SDLK_RETURN;

        case HK_APPS:       return SDLK_MENU;
        case HK_NONE:       return 0;
    }

    return 0;
}
