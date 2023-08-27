// Part of SimCoupe - A SAM Coupe emulator
//
// Input.cpp: SDL keyboard, mouse and joystick input
//
//  Copyright (c) 1999-2015 Simon Owen
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

#include "Actions.h"
#include "Frame.h"
#include "GUI.h"
#include "Input.h"
#include "Keyin.h"
#include "SAMIO.h"
#include "Joystick.h"
#include "Keyboard.h"
#include "Options.h"
#include "Mouse.h"
#include "UI.h"

//#define USE_JOYPOLLING

int nJoystick1 = -1, nJoystick2 = -1;
SDL_Joystick* pJoystick1, * pJoystick2;

bool fMouseActive, fKeyboardActive;
int nLastKey, nLastMods;
const Uint8* pKeyStates;

////////////////////////////////////////////////////////////////////////////////

bool Input::Init()
{
    Exit();

    // Loop through the available devices for the ones to use (if any)
    for (int i = 0; i < SDL_NumJoysticks(); i++)
    {
        // Ignore VirtualBox devices, as the default USB Tablet option
        // is seen as a joystick, which generates unwanted inputs
        if (!strncmp(SDL_JoystickNameForIndex(i), "VirtualBox", 10))
            continue;

        // Match against the required joystick names, or auto-select the first available
        if (!pJoystick1 && ((SDL_JoystickNameForIndex(i) == GetOption(joydev1)) || GetOption(joydev1).empty()))
            pJoystick1 = SDL_JoystickOpen(nJoystick1 = i);
        else if (!pJoystick2 && ((SDL_JoystickNameForIndex(i) == GetOption(joydev2)) || GetOption(joydev2).empty()))
            pJoystick2 = SDL_JoystickOpen(nJoystick2 = i);
    }

#ifdef USE_JOYPOLLING
    // Disable joystick events as we'll poll ourselves when necessary
    SDL_JoystickEventState(SDL_DISABLE);
#endif

    Keyboard::Init();

    SDL_StartTextInput();
    SDL_SetTextInputRect(nullptr);

    pKeyStates = SDL_GetKeyboardState(nullptr);

    fMouseActive = false;

    return true;
}

void Input::Exit()
{
    if (pJoystick1) { SDL_JoystickClose(pJoystick1); pJoystick1 = nullptr; nJoystick1 = -1; }
    if (pJoystick2) { SDL_JoystickClose(pJoystick2); pJoystick2 = nullptr; nJoystick2 = -1; }
}

// Return whether the emulation is using the mouse
bool Input::IsMouseAcquired()
{
    return fMouseActive;
}

void Input::AcquireMouse(bool active)
{
    if (fMouseActive == active)
        return;

    if (GetOption(mouse))
    {
        SDL_ShowCursor(active ? SDL_DISABLE : SDL_ENABLE);
        Video::MouseRelative();
        SDL_CaptureMouse(active ? SDL_TRUE : SDL_FALSE);
    }

    fMouseActive = active;
}


// Purge pending keyboard and/or mouse events
void Input::Purge()
{
    // Remove queued input messages
    SDL_Event event;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_KEYDOWN, SDL_JOYBUTTONUP));

    // Discard relative motion and clear the key modifiers
    int n;
    SDL_GetRelativeMouseState(&n, &n);
    SDL_SetModState(KMOD_NONE);

    Keyboard::Purge();
}


#ifdef USE_JOYPOLLING
// Read the specified joystick
static void ReadJoystick(int nJoystick_, SDL_Joystick* pJoystick_, int nTolerance_)
{
    int nPosition = HJ_CENTRE;
    uint32_t dwButtons = 0;
    Uint8 bHat = 0;
    int i;

    int nDeadZone = 32768 * nTolerance_ / 100;
    int nButtons = SDL_JoystickNumButtons(pJoystick_);
    int nHats = SDL_JoystickNumHats(pJoystick_);
    int nX = SDL_JoystickGetAxis(pJoystick_, 0);
    int nY = SDL_JoystickGetAxis(pJoystick_, 1);

    for (i = 0; i < nButtons; i++)
        dwButtons |= SDL_JoystickGetButton(pJoystick_, i) << i;

    for (i = 0; i < nHats; i++)
        bHat |= SDL_JoystickGetHat(pJoystick_, i);


    if ((nX < -nDeadZone) || (bHat & SDL_HAT_LEFT))  nPosition |= HJ_LEFT;
    if ((nX > nDeadZone) || (bHat & SDL_HAT_RIGHT)) nPosition |= HJ_RIGHT;
    if ((nY < -nDeadZone) || (bHat & SDL_HAT_UP))    nPosition |= HJ_UP;
    if ((nY > nDeadZone) || (bHat & SDL_HAT_DOWN))  nPosition |= HJ_DOWN;

    Joystick::SetPosition(nJoystick_, nPosition);
    Joystick::SetButtons(nJoystick_, dwButtons);
}
#endif // USE_JOYPOLLING


// Process SDL event messages
bool Input::FilterEvent(SDL_Event* pEvent_)
{
    switch (pEvent_->type)
    {
    case SDL_TEXTINPUT:
    {
        SDL_TextInputEvent* pEvent = &pEvent_->text;
        int nChr = pEvent->text[0];
        auto pbText = reinterpret_cast<uint8_t*>(pEvent->text);

        // Ignore symbols from the keypad
        if ((nLastKey >= HK_KP0 && nLastKey <= HK_KP9) || (nLastKey >= HK_KPPLUS && nLastKey <= HK_KPDECIMAL))
            break;
        else if (nLastKey == HK_SECTION)
            break;
        else if (pbText[1])
        {
            if (pbText[0] == 0xc2 && pbText[1] == 0xa3)
                nChr = 163; // GBP
            else if (pbText[0] == 0xc2 && pbText[1] == 0xa7)
                nChr = 167; // section symbol
            else if (pbText[0] == 0xc2 && pbText[1] == 0xb1)
                nChr = 177; // +/-
            else
                break;
        }

        TRACE("SDL_TEXTINPUT: {} (nLastKey={}, nLastMods={})\n", pEvent->text, nLastKey, nLastMods);
        Keyboard::SetKey(nLastKey, true, nLastMods, nChr);
        break;
    }

    case SDL_KEYDOWN:
    case SDL_KEYUP:
    {
        SDL_KeyboardEvent* pEvent = &pEvent_->key;
        SDL_Keysym* pKey = &pEvent->keysym;

        bool fPress = pEvent->type == SDL_KEYDOWN;
        if (fPress)
            SDL_ShowCursor(SDL_DISABLE);

        // Ignore key repeats unless the GUI is active
        if (pEvent->repeat && !GUI::IsActive())
            break;

        int nKey = MapKey(pKey->sym);
        int nMods = ((pKey->mod & KMOD_SHIFT) ? HM_SHIFT : 0) |
            ((pKey->mod & KMOD_LCTRL) ? HM_CTRL : 0) |
            ((pKey->mod & KMOD_LALT) ? HM_ALT : 0);

        int nChr = (nKey < HK_SPACE || (nKey < HK_MIN && (pKey->mod & KMOD_CTRL))) ? nKey : 0;

        TRACE("SDL_KEY{} ({:x} -> {:x})\n", fPress ? "DOWN" : "UP", pKey->sym, nKey);

        if (fPress)
        {
            nLastKey = nKey;
            nLastMods = nMods;
        }

        bool fCtrl = !!(pKey->mod & KMOD_CTRL);
        bool fAlt = !!(pKey->mod & KMOD_ALT);
        bool fShift = !!(pKey->mod & KMOD_SHIFT);

        // Unpause on key press if paused, so the user doesn't think we've hung
        if (fPress && g_fPaused && nKey != HK_PAUSE)
            Actions::Do(Action::Pause);

        // Use key repeats for GUI mode only
        if (fKeyboardActive == GUI::IsActive())
        {
            fKeyboardActive = !fKeyboardActive;
        }

        // Check for the Windows key, for use as a modifier
        bool fWin = pKeyStates[SDL_SCANCODE_LGUI] || pKeyStates[SDL_SCANCODE_RGUI];
        if (!fWin)
        {
            if (nKey >= HK_F1 && nKey <= HK_F12)
            {
                Actions::Key(nKey - HK_F1 + 1, fPress, fCtrl, fAlt, fShift);
                break;
            }
            else if (nKey >= '0' && nKey <= '9' && fAlt && !fCtrl && !fShift)
            {
                auto scale_2x = nKey - '0' + 1;
                auto height = Frame::Height() * scale_2x / 2;
                Video::ResizeWindow(height);
            }
        }

        if (GUI::IsActive())
        {
            // Pass any printable characters to the GUI
            if (fPress && nKey)
                GUI::SendMessage(GM_CHAR, nKey, nMods);

            break;
        }

        // Some additional function keys
        bool fAction = true;
        switch (nKey)
        {
        case HK_RETURN:     fAction = fAlt; if (fAction) Actions::Do(Action::ToggleFullscreen, fPress); break;
        case HK_KPDIVIDE:   Actions::Do(Action::Debugger, fPress); break;
        case HK_KPMULT:     Actions::Do(fCtrl ? Action::Reset : Action::SpeedTurbo, fPress); break;
        case HK_KPPLUS:     Actions::Do(fCtrl ? Action::SpeedTurbo : Action::SpeedFaster, fPress); break;
        case HK_KPMINUS:    Actions::Do(fCtrl ? Action::SpeedNormal : Action::SpeedSlower, fPress); break;

        case HK_PRINT:      Actions::Do(Action::SavePNG, fPress); break;
        case HK_SCROLL:
        case HK_PAUSE:      Actions::Do(fCtrl ? Action::Reset : fShift ? Action::FrameStep : Action::Pause, fPress); break;

        default:            fAction = false; break;
        }

        // Have we processed the key?
        if (fAction)
            break;

        // Optionally release the mouse capture if Esc is pressed
        if (fPress && nKey == HK_ESC && GetOption(mouseesc))
        {
            Actions::Do(Action::ReleaseMouse);
            Keyin::Stop();
        }

        // Key press (CapsLock/NumLock are toggle keys in SDL, so we must treat any event as a press)
        if (fPress || pKey->sym == SDLK_CAPSLOCK || pKey->sym == SDLK_NUMLOCKCLEAR)
        {
            Keyboard::SetKey(nKey, true, nMods, nChr);
        }

        // Key release
        else if (!fPress)
            Keyboard::SetKey(nKey, false);

        break;
    }

    case SDL_MOUSEMOTION:
    {
        int x = pEvent_->motion.x;
        int y = pEvent_->motion.y;

        bool hide_cursor = fMouseActive && !GUI::IsActive();
        SDL_ShowCursor(hide_cursor ? SDL_DISABLE : SDL_ENABLE);

        // Mouse in use by the GUI?
        if (GUI::IsActive())
        {
            Video::NativeToSam(x, y);
            GUI::SendMessage(GM_MOUSEMOVE, x, y);
        }
        else if (fMouseActive)
        {
            auto [dx, dy] = Video::MouseRelative();
            if (dx || dy)
            {
                TRACE("Mouse: {} {}\n", dx, -dy);
                pMouse->Move(dx, -dy);
            }
        }
        break;
    }

    case SDL_MOUSEBUTTONDOWN:
    {
        static std::optional<std::chrono::steady_clock::time_point> last_click_time;
        static int nLastButton = -1;

        int x = pEvent_->button.x;
        int y = pEvent_->button.y;
        auto now = std::chrono::steady_clock::now();
        bool double_click = (pEvent_->button.button == nLastButton) &&
            last_click_time && ((now - *last_click_time) < DOUBLE_CLICK_TIME);

        // Button presses go to the GUI if it's active
        if (GUI::IsActive())
        {
            switch (pEvent_->button.button)
            {
            case 4:
                GUI::SendMessage(GM_MOUSEWHEEL, -1);
                break;
            case 5:
                GUI::SendMessage(GM_MOUSEWHEEL, 1);
                break;
            default:
                Video::NativeToSam(x, y);
                GUI::SendMessage(GM_BUTTONDOWN, x, y);
                break;
            }
        }

        // Pass the button click through if the mouse is active
        else if (fMouseActive)
        {
            pMouse->SetButton(pEvent_->button.button, true);
            TRACE("Mouse button {} pressed\n", pEvent_->button.button);
        }

        // If the mouse interface is enabled and being read by something other than the ROM, a left-click acquires it
        // Otherwise a double-click is required to forcibly acquire it
        else if (GetOption(mouse) && pEvent_->button.button == 1 && (pMouse->IsActive() || double_click))
            AcquireMouse();

        // Remember the last click click time and button, for double-click tracking
        nLastButton = pEvent_->button.button;
        last_click_time = std::chrono::steady_clock::now();

        break;
    }

    case SDL_MOUSEBUTTONUP:
        // Button presses go to the GUI if it's active
        if (GUI::IsActive())
        {
            int x = pEvent_->button.x;
            int y = pEvent_->button.y;
            Video::NativeToSam(x, y);
            GUI::SendMessage(GM_BUTTONUP, x, y);
        }
        else if (fMouseActive)
        {
            TRACE("Mouse button {} released\n", pEvent_->button.button);
            pMouse->SetButton(pEvent_->button.button, false);
        }
        break;

    case SDL_MOUSEWHEEL:
        if (GUI::IsActive())
        {
            if (pEvent_->wheel.y > 0)
                GUI::SendMessage(GM_MOUSEWHEEL, -1);
            else if (pEvent_->wheel.y < 0)
                GUI::SendMessage(GM_MOUSEWHEEL, 1);
        }
        break;

#ifndef USE_JOYPOLLING

    case SDL_JOYAXISMOTION:
    {
        SDL_JoyAxisEvent* p = &pEvent_->jaxis;
        int nJoystick = (p->which == nJoystick1) ? 0 : 1;
        int nDeadZone = 32768 * (!nJoystick ? GetOption(deadzone1) : GetOption(deadzone2)) / 100;

        if (p->axis == 0)
            Joystick::SetX(nJoystick, (p->value < -nDeadZone) ? HJ_LEFT : (p->value > nDeadZone) ? HJ_RIGHT : HJ_CENTRE);
        else if (p->axis == 1)
            Joystick::SetY(nJoystick, (p->value < -nDeadZone) ? HJ_UP : (p->value > nDeadZone) ? HJ_DOWN : HJ_CENTRE);

        break;
    }

    case SDL_JOYHATMOTION:
    {
        SDL_JoyHatEvent* p = &pEvent_->jhat;
        int nJoystick = (p->which == nJoystick1) ? 0 : 1;
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
        SDL_JoyButtonEvent* p = &pEvent_->jbutton;
        int nJoystick = (p->which == nJoystick1) ? 0 : 1;

        Joystick::SetButton(nJoystick, p->button, (p->state == SDL_PRESSED));
        break;
    }
#endif

    case SDL_WINDOWEVENT:
    {
        if (pEvent_->window.event == SDL_WINDOWEVENT_FOCUS_LOST)
        {
            AcquireMouse(false);
        }
        break;
    }
    }

    // Allow additional event processing
    return false;
}


void Input::Update()
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
    Keyboard::SetKey(HK_CAPSLOCK, false);
    Keyboard::SetKey(HK_NUMLOCK, false);
}


int Input::MapChar(int nChar_, int* /*pnMods_*/)
{
    // Regular characters details aren't known until the key press
    if (nChar_ < HK_MIN)
        return 0;

    if (nChar_ >= HK_MIN && nChar_ < HK_MAX)
        return nChar_;

    return 0;
}

int Input::MapKey(int nKey_)
{
    // Host keycode
    switch (nKey_)
    {
    case SDLK_LSHIFT:   return HK_LSHIFT;
    case SDLK_RSHIFT:   return HK_RSHIFT;
    case SDLK_LCTRL:    return HK_LCTRL;
    case SDLK_RCTRL:    return HK_RCTRL;
    case SDLK_LALT:     return HK_LALT;
    case SDLK_RALT:     return HK_RALT;
    case SDLK_LGUI:     return HK_LWIN;
    case SDLK_RGUI:     return HK_RWIN;

    case SDLK_LEFT:     return HK_LEFT;
    case SDLK_RIGHT:    return HK_RIGHT;
    case SDLK_UP:       return HK_UP;
    case SDLK_DOWN:     return HK_DOWN;

    case SDLK_KP_0:     return HK_KP0;
    case SDLK_KP_1:     return HK_KP1;
    case SDLK_KP_2:     return HK_KP2;
    case SDLK_KP_3:     return HK_KP3;
    case SDLK_KP_4:     return HK_KP4;
    case SDLK_KP_5:     return HK_KP5;
    case SDLK_KP_6:     return HK_KP6;
    case SDLK_KP_7:     return HK_KP7;
    case SDLK_KP_8:     return HK_KP8;
    case SDLK_KP_9:     return HK_KP9;

    case SDLK_F1:       return HK_F1;
    case SDLK_F2:       return HK_F2;
    case SDLK_F3:       return HK_F3;
    case SDLK_F4:       return HK_F4;
    case SDLK_F5:       return HK_F5;
    case SDLK_F6:       return HK_F6;
    case SDLK_F7:       return HK_F7;
    case SDLK_F8:       return HK_F8;
    case SDLK_F9:       return HK_F9;
    case SDLK_F10:      return HK_F10;
    case SDLK_F11:      return HK_F11;
    case SDLK_F12:      return HK_F12;

    case SDLK_CAPSLOCK: return HK_CAPSLOCK;
    case SDLK_NUMLOCKCLEAR: return HK_NUMLOCK;
    case SDLK_KP_PLUS:  return HK_KPPLUS;
    case SDLK_KP_MINUS: return HK_KPMINUS;
    case SDLK_KP_MULTIPLY: return HK_KPMULT;
    case SDLK_KP_DIVIDE:return HK_KPDIVIDE;
    case SDLK_KP_ENTER: return HK_KPENTER;
    case SDLK_KP_PERIOD:return HK_KPDECIMAL;

    case SDLK_INSERT:   return HK_INSERT;
    case SDLK_DELETE:   return HK_DELETE;
    case SDLK_HOME:     return HK_HOME;
    case SDLK_END:      return HK_END;
    case SDLK_PAGEUP:   return HK_PGUP;
    case SDLK_PAGEDOWN: return HK_PGDN;

    case SDLK_ESCAPE:   return HK_ESC;
    case SDLK_TAB:      return HK_TAB;
    case SDLK_BACKSPACE:return HK_BACKSPACE;
    case SDLK_RETURN:   return HK_RETURN;

    case SDLK_PRINTSCREEN: return HK_PRINT;
    case SDLK_SCROLLLOCK:return HK_SCROLL;
    case SDLK_PAUSE:    return HK_PAUSE;

    case SDLK_APPLICATION: return HK_APPS;
    }

    return (nKey_ && nKey_ < HK_MIN) ? nKey_ : HK_NONE;
}
