// Part of SimCoupe - A SAM Coupé emulator
//
// Input.cpp: SDL keyboard, mouse and joystick input
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

// ToDo:
//  - smarter automatic key mapping, to replace the current UK version
//  - add the Spectrum mode mapping back in?

#include "SimCoupe.h"

#include "Display.h"
#include "Frame.h"
#include "GUI.h"
#include "Input.h"
#include "IO.h"
#include "Util.h"
#include "Options.h"
#include "Mouse.h"
#include "UI.h"


typedef struct
{
    char    nChar;                  // Symbol character (if any)

    SDLKey  nKey;                   // Key to press
    int     nModifiers;             // Modifier(s) to use with above key

    eSamKey nSamKey, nSamModifiers; // Up to 2 SAM keys needed to generate the above symbol
}
COMBINATION_KEY;


SDL_Joystick *pJoystick1, *pJoystick2;

SDLKey nComboKey;
SDLMod nComboModifiers;
DWORD dwComboTime;

bool afKeyStates[SDLK_LAST], afKeys[SDLK_LAST];
inline bool IsPressed(int nKey_)    { return afKeyStates[nKey_]; }
inline void PressKey(int nKey_)     { afKeyStates[nKey_] = true; }
inline void ReleaseKey(int nKey_)   { afKeyStates[nKey_] = false; }
inline void ToggleKey(int nKey_)    { afKeyStates[nKey_] = !afKeyStates[nKey_]; }
inline void SetKeyHeld(int nKey_, bool fHeld_=true) { afKeys[nKey_] = fHeld_; }


// Mapping of SAM keyboard matrix positions to single keys
SDLKey aeSamKeys [SK_MAX] =
{
    SDLK_LSHIFT,    SDLK_z,     SDLK_x,     SDLK_c,     SDLK_v,     SDLK_KP1,       SDLK_KP2,       SDLK_KP3,
    SDLK_a,         SDLK_s,     SDLK_d,     SDLK_f,     SDLK_g,     SDLK_KP4,       SDLK_KP5,       SDLK_KP6,
    SDLK_q,         SDLK_w,     SDLK_e,     SDLK_r,     SDLK_t,     SDLK_KP7,       SDLK_KP8,       SDLK_KP9,
    SDLK_1,         SDLK_2,     SDLK_3,     SDLK_4,     SDLK_5,     SDLK_ESCAPE,    SDLK_TAB,       SDLK_CAPSLOCK,
    SDLK_0,         SDLK_9,     SDLK_8,     SDLK_7,     SDLK_6,     SDLK_UNKNOWN,   SDLK_UNKNOWN,   SDLK_BACKSPACE,
    SDLK_p,         SDLK_o,     SDLK_i,     SDLK_u,     SDLK_y,     SDLK_UNKNOWN,   SDLK_UNKNOWN,   SDLK_KP0,
    SDLK_RETURN,    SDLK_l,     SDLK_k,     SDLK_j,     SDLK_h,     SDLK_UNKNOWN,   SDLK_UNKNOWN,   SDLK_UNKNOWN,
    SDLK_SPACE,     SDLK_LCTRL, SDLK_m,     SDLK_n,     SDLK_b,     SDLK_UNKNOWN,   SDLK_UNKNOWN,   SDLK_INSERT,
    SDLK_RCTRL,     SDLK_UP,    SDLK_DOWN,  SDLK_LEFT,  SDLK_RIGHT
};

// Symbols with SAM keyboard details
COMBINATION_KEY asSamSymbols[] =
{
    { '!',  SDLK_1,             KMOD_LSHIFT,    SK_1,       SK_SHIFT },
    { '@',  SDLK_QUOTE,         KMOD_LSHIFT,    SK_2,       SK_SHIFT },
    { '#',  SDLK_BACKSLASH,     KMOD_NONE,      SK_3,       SK_SHIFT },
    { '$',  SDLK_4,             KMOD_LSHIFT,    SK_4,       SK_SHIFT },
    { '%',  SDLK_5,             KMOD_LSHIFT,    SK_5,       SK_SHIFT },
    { '&',  SDLK_7,             KMOD_LSHIFT,    SK_6,       SK_SHIFT },
    { '\'', SDLK_QUOTE,         KMOD_NONE,      SK_SHIFT,   SK_7 },
    { '(',  SDLK_9,             KMOD_LSHIFT,    SK_8,       SK_SHIFT },
    { ')',  SDLK_0,             KMOD_LSHIFT,    SK_9,       SK_SHIFT },
    { '~',  SDLK_BACKSLASH,     KMOD_LSHIFT,    SK_0,       SK_SHIFT },
    { '-',  SDLK_MINUS,         KMOD_NONE,      SK_MINUS,   SK_NONE },
    { '/',  SDLK_SLASH,         KMOD_NONE,      SK_MINUS,   SK_SHIFT },
    { '+',  SDLK_EQUALS,        KMOD_LSHIFT,    SK_PLUS,    SK_NONE },
    { '4',  SDLK_UNDERSCORE,    KMOD_NONE,      SK_4,       SK_NONE },
    { '5',  SDLK_UNDERSCORE,    KMOD_LSHIFT,    SK_5,       SK_NONE },
    { '*',  SDLK_8,             KMOD_LSHIFT,    SK_PLUS,    SK_SHIFT },
    { '<',  SDLK_COMMA,         KMOD_LSHIFT,    SK_Q,       SK_SYMBOL },
    { '>',  SDLK_PERIOD,        KMOD_LSHIFT,    SK_W,       SK_SYMBOL },
    { '[',  SDLK_LEFTBRACKET,   KMOD_NONE,      SK_R,       SK_SYMBOL },
    { ']',  SDLK_RIGHTBRACKET,  KMOD_NONE,      SK_T,       SK_SYMBOL },
    { '=',  SDLK_EQUALS,        KMOD_NONE,      SK_EQUALS,  SK_NONE },
    { '_',  SDLK_MINUS,         KMOD_LSHIFT,    SK_EQUALS,  SK_SHIFT },
    { '"',  SDLK_2,             KMOD_LSHIFT,    SK_QUOTES,  SK_NONE },
    { '`',  SDLK_KP_PERIOD,     KMOD_NONE,      SK_QUOTES,  SK_SHIFT },     // Used for (c)
    { '{',  SDLK_LEFTBRACKET,   KMOD_LSHIFT,    SK_F,       SK_SYMBOL },
    { '}',  SDLK_RIGHTBRACKET,  KMOD_LSHIFT,    SK_G,       SK_SYMBOL },
    { '^',  SDLK_6,             KMOD_LSHIFT,    SK_H,       SK_SYMBOL },
    { (BYTE)'£', SDLK_3,        KMOD_LSHIFT,    SK_L,       SK_SYMBOL },
    { ';',  SDLK_SEMICOLON,     KMOD_NONE,      SK_SEMICOLON, SK_NONE },
    { ':',  SDLK_SEMICOLON,     KMOD_LSHIFT,    SK_COLON,   SK_NONE },
    { '?',  SDLK_SLASH,         KMOD_LSHIFT,    SK_X,       SK_SYMBOL },
    { '.',  SDLK_PERIOD,        KMOD_NONE,      SK_PERIOD,  SK_NONE },
    { ',',  SDLK_COMMA,         KMOD_NONE,      SK_COMMA,   SK_NONE },
    { '\\', SDLK_BACKQUOTE,     KMOD_NONE,      SK_SHIFT,   SK_INV },
    { '|',  SDLK_BACKQUOTE,     KMOD_LSHIFT,    SK_L,       SK_SYMBOL },

    // Some useful combinations (would be nice to have them truly configurable tho)
    { 0,    SDLK_DELETE,        KMOD_NONE,      SK_DELETE,  SK_SHIFT },
    { 0,    SDLK_HOME,          KMOD_NONE,      SK_LEFT,    SK_CONTROL },
    { 0,    SDLK_END,           KMOD_NONE,      SK_RIGHT,   SK_CONTROL },
    { 0,    SDLK_PAGEUP,        KMOD_NONE,      SK_F4,      SK_NONE },
    { 0,    SDLK_PAGEDOWN,      KMOD_NONE,      SK_F1,      SK_NONE },
    { 0,    SDLK_NUMLOCK,       KMOD_NONE,      SK_EDIT,    SK_SYMBOL },
    { 0,    SDLK_MENU,          KMOD_NONE,      SK_EDIT,    SK_NONE },
    { 0,    SDLK_KP_PERIOD,     KMOD_NONE,      SK_QUOTES,  SK_SHIFT },

    { '\0', SDLK_UNKNOWN,       KMOD_NONE,      SK_NONE,    SK_NONE }
};
/*
// Symbols with Spectrum keyboard details
COMBINATION_KEY asSpectrumSymbols[] =
{
    { '!',  SK_SYMBOL, SK_1 },      { '@',  SK_SYMBOL, SK_2 },      { '#',  SK_SYMBOL, SK_3 },
    { '$',  SK_SYMBOL, SK_4 },      { '%',  SK_SYMBOL, SK_5 },      { '&',  SK_SYMBOL, SK_6 },
    { '\'', SK_SYMBOL, SK_7 },      { '(',  SK_SYMBOL, SK_8 },      { ')',  SK_SYMBOL, SK_9 },
    { '_',  SK_SYMBOL, SK_0 },      { '<',  SK_SYMBOL, SK_R },      { '>',  SK_SYMBOL, SK_T },
    { ';',  SK_SYMBOL, SK_O },      { '"',  SK_SYMBOL, SK_P },      { '-',  SK_SYMBOL, SK_J },
    { '^',  SK_SYMBOL, SK_H },      { '+',  SK_SYMBOL, SK_K },      { '=',  SK_SYMBOL, SK_L },
    { ':',  SK_SYMBOL, SK_Z },      { '£',  SK_SYMBOL, SK_X },      { '?',  SK_SYMBOL, SK_C },
    { '/',  SK_SYMBOL, SK_V },      { '*',  SK_SYMBOL, SK_B },      { ',',  SK_SYMBOL, SK_N },
    { '.',  SK_SYMBOL, SK_M },      { '\b', SK_SHIFT,  SK_0 },
    { '\0' }
};
*/

////////////////////////////////////////////////////////////////////////////////

bool ReadKeyboard ();
void SetSamKeyState ();


bool Input::Init (bool fFirstInit_/*=false*/)
{
    Exit(true);

    // Initialise the joysticks if any are
    if ((*GetOption(joydev1) || *GetOption(joydev2)) && !SDL_InitSubSystem(SDL_INIT_JOYSTICK))
    {
        // Loop through the available devices for the ones to use (if any)
        for (int i = 0 ; i < SDL_NumJoysticks() ; i++)
        {
            if (!strcasecmp(SDL_JoystickName(i), GetOption(joydev1)))
                pJoystick1 = SDL_JoystickOpen(i);
            else if (!strcasecmp(SDL_JoystickName(i), GetOption(joydev2)))
                pJoystick2 = SDL_JoystickOpen(i);
        }
    }

    Mouse::Init(fFirstInit_);
    Purge();
    // Force all modifier keys off (avoids Ctrl getting stuck from Ctrl-F5 in Visual Studio)
    SDL_SetModState(KMOD_NONE);

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

    Mouse::Exit(fReInit_);
}


void Input::Acquire (bool fMouse_/*=true*/, bool fKeyboard_/*=true*/)
{
    // Flush out any buffered data if we're changing the acquisition state
    Purge();

    // Emulation mode doesn't need translations or key repeat, but the GUI needs both
    if (fKeyboard_)
    {
        SDL_EnableUNICODE(0);
        SDL_EnableKeyRepeat(0, 0);
    }
    else
    {
        SDL_EnableUNICODE(1);
        SDL_EnableKeyRepeat(250, 30);
    }

    SDL_ShowCursor(!fMouse_);
}


// Purge pending keyboard and/or mouse events
void Input::Purge (bool fMouse_/*=true*/, bool fKeyboard_/*=true*/)
{
    SDL_Event event;

    if (fKeyboard_)
    {
        // Remove any queued key events
        while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_KEYDOWNMASK|SDL_KEYUPMASK) > 0)
            ;

        // Release all keys
        memset(afKeyStates, 0, sizeof afKeyStates);
        memset(afKeys, 0, sizeof afKeys);
        ReleaseAllSamKeys();
    }

    if (fMouse_)
    {
        // Remove any queued mouse events
        while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_MOUSEEVENTMASK) > 0)
            ;

        int nX, nY;
        SDL_GetRelativeMouseState(&nX, &nY);

        // No buttons pressed
        Mouse::SetButton(1, false);
        Mouse::SetButton(2, false);
        Mouse::SetButton(3, false);
    }
}


// Process and SDL event message
void Input::ProcessEvent (SDL_Event* pEvent_)
{
    switch (pEvent_->type)
    {
        case SDL_ACTIVEEVENT:
        {
            SDL_ShowCursor(pEvent_->active.gain ? SDL_ENABLE : SDL_DISABLE);
            Purge();
            break;
        }

        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            SDL_keysym* pKey = &pEvent_->key.keysym;
            TRACE("Key %s: %d\n", (pEvent_->key.state == SDL_PRESSED) ? "down" : "up", pKey->sym);
//          Frame::SetStatus("Key %s: %d\n", (pEvent_->key.state == SDL_PRESSED) ? "down" : "up", pKey->sym);

            // Pass any printable characters to the GUI
            if (GUI::IsActive())
            {
                // Convert the cursor keys to GUI symbols
                if (pKey->sym >= SDLK_UP && pKey->sym <= SDLK_LEFT)
                {
                    int anCursors[] = { GK_UP, GK_DOWN, GK_RIGHT, GK_LEFT };
                    pKey->unicode = anCursors[pKey->sym - SDLK_UP];
                }

                // Pass any printable key-down messages to the GUI
                if (pEvent_->type == SDL_KEYDOWN && pKey->unicode < 0x80)
                    GUI::SendMessage(GM_CHAR, pKey->unicode, (pKey->mod & KMOD_LSHIFT) != 0);

                break;
            }

            // Update the master key table with the change
            SetKeyHeld(pKey->sym, pEvent_->type == SDL_KEYDOWN);

            break;
        }

        case SDL_MOUSEMOTION:
        {
//          Frame::SetStatus("Mouse:  %d %d", pEvent_->motion.xrel, pEvent_->motion.yrel);

            // Mouse in use by the GUI?
            if (GUI::IsActive())
            {
                int nX = pEvent_->motion.x, nY = pEvent_->motion.y;
                Display::DisplayToSam(&nX, &nY);
                GUI::SendMessage(GM_MOUSEMOVE, nX, nY);
            }

            // Is the mouse captured?
            else if (SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE)
            {
                // Work out the relative movement since last time
                int nX = pEvent_->motion.x - (Frame::GetWidth() >> 1), nY = pEvent_->motion.y - (Frame::GetHeight() >> 1);

                // Has it moved at all?             
                if (nX || nY)
                {
                    // We need to track partial units, as we're higher resolution than SAM
                    static int nXX, nYY;

                    // Add on the new movement
                    nXX += nX;
                    nYY += nY;

                    // How far has the mouse moved in SAM units?
                    nX = nXX;
                    nY = nYY;
                    int nX2 = 0, nY2 = 0;
                    Display::DisplayToSam(&nX, &nY);
                    Display::DisplayToSam(&nX2, &nY2);
                    nX -= nX2;
                    nY -= nY2;

                    // Update the SAM mouse position
                    Mouse::Move(nX, -nY);
                    TRACE("Mouse move: X:%-03d Y:%-03d\n", nX, nY);

                    // How far is the SAM mouse movement in native units?
                    nX2 = nY2 = 0;
                    Display::SamToDisplay(&nX, &nY);
                    Display::SamToDisplay(&nX2, &nY2);
                    nX -= nX2;
                    nY -= nY2;

                    // Subtract the used portion of the movement, and leave the remainder for next time
                    nXX -= nX;
                    nYY -= nY;

                    // Move the mouse back to the centre to stop it escaping
                    SDL_WarpMouse(Frame::GetWidth() >> 1, Frame::GetHeight() >> 1);
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
                Display::DisplayToSam(&nX, &nY);
                GUI::SendMessage(GM_BUTTONDOWN, nX, nY);
            }

            // If the mouse isn't grabbed, grab it now and hide the cursor (and ignore the button down event)
            else if (SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE)
            {
                SDL_ShowCursor(SDL_DISABLE);
                SDL_WarpMouse(Frame::GetWidth() >> 1, Frame::GetHeight() >> 1);
            }
            else
            {
                Mouse::SetButton(pEvent_->button.button, true);
                TRACE("Mouse button %d pressed\n", pEvent_->button.button);
            }

            break;
        }

        case SDL_MOUSEBUTTONUP:
            // Button presses go to the GUI if it's active
            if (GUI::IsActive())
            {
                int nX = pEvent_->button.x, nY = pEvent_->button.y;
                Display::DisplayToSam(&nX, &nY);
                GUI::SendMessage(GM_BUTTONUP, nX, nY);
            }
            else
            {
                TRACE("Mouse button %d released\n", pEvent_->button.button);
                Mouse::SetButton(pEvent_->button.button, false);
            }
            break;

        case SDL_JOYAXISMOTION:
        {
            SDL_JoyAxisEvent* pJoy = &pEvent_->jaxis;
            int nDeadZone = 0x7fffL * GetOption(deadzone1) / 100;

            SetKeyHeld(SDLK_6, !pJoy->axis && pJoy->value <= -nDeadZone);
            SetKeyHeld(SDLK_7, !pJoy->axis && pJoy->value >=  nDeadZone);
            SetKeyHeld(SDLK_8,  pJoy->axis && pJoy->value >=  nDeadZone);
            SetKeyHeld(SDLK_9,  pJoy->axis && pJoy->value <= -nDeadZone);

            break;
        }

        case SDL_JOYHATMOTION:
        {
            int nHat = pEvent_->jhat.value;

            SetKeyHeld(SDLK_6, (nHat & SDL_HAT_LEFT) != 0);
            SetKeyHeld(SDLK_7, (nHat & SDL_HAT_RIGHT) != 0);
            SetKeyHeld(SDLK_8, (nHat & SDL_HAT_DOWN) != 0);
            SetKeyHeld(SDLK_9, (nHat & SDL_HAT_UP) != 0);

            break;
        }

        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
            afKeys[SDLK_0] = pEvent_->type == SDL_JOYBUTTONDOWN;
            break;
    }
}


void Input::Update ()
{
    // Read keyboard and mouse
    ReadKeyboard();

    // Update the SAM keyboard matrix from the current key state (including joystick movement)
    SetSamKeyState();
}


// Read the current keyboard state, and make any special adjustments needed before it's processed
bool ReadKeyboard ()
{
    bool fRet = true;

    // Make a copy of the master key table with the real keyboard states
    memcpy(afKeyStates, afKeys, sizeof afKeyStates);

    // If the option is set, Left-ALT does the same as Right-Control: to generate SAM Cntrl
    if (GetOption(altforcntrl) && IsPressed(SDLK_LALT))
        PressKey(SDLK_RCTRL);

    // AltGr can optionally be used for SAM Edit
    if (GetOption(altgrforedit) && IsPressed(SDLK_RALT))
    {
        // AltGr is usually seen with left-control down (NT/W2K), so release it
        ReleaseKey(SDLK_LCTRL);

        // Release AltGr (needed for Win9x it seems) and press the context menu key (also used for SAM Edit)
        ReleaseKey(SDLK_RALT);
        PressKey(SDLK_MENU);
    }

    // A couple of Windows niceties
    if (IsPressed(SDLK_LALT))
    {
        // Alt-Tab for switching apps should not be seen
        if (IsPressed(SDLK_TAB))
            ReleaseKey(SDLK_TAB);

        // Alt-F4 for Close will close us gracefully
        if (IsPressed(SDLK_F4))
        {
            SDL_Event event = { SDL_QUIT };
            SDL_PushEvent(&event);
        }
    }

    return fRet;
}


// Process more complicated key combinations
void ProcessKeyTable (COMBINATION_KEY* asKeys_)
{
    short nShifts = 0;
    if (IsPressed(SDLK_LSHIFT)) nShifts |= KMOD_LSHIFT;
    if (IsPressed(SDLK_LCTRL)) nShifts |= KMOD_LCTRL;
    if (IsPressed(SDLK_LALT)) nShifts |= KMOD_LALT;
    if (IsPressed(SDLK_RALT)) nShifts |= (KMOD_LCTRL|KMOD_LALT);

    // Have the shift states changed while a combo is in progress?
    if (nComboModifiers != KMOD_NONE && nComboModifiers != nShifts)
    {
        // If the combo key is still pressed, start the timer running to re-press it as we're about to release it
        if (IsPressed(nComboKey))
        {
            TRACE("Starting combo timer\n");
            dwComboTime = OSD::GetTime();
        }

        // We're done with the shift state now, so clear it to prevent the time getting reset
        nComboModifiers = KMOD_NONE;
    }

    // Combo unpress timer active?
    if (dwComboTime)
    {
        TRACE("Combo timer active\n");

        // If we're within the threshold, ensure the key remains released
        if ((OSD::GetTime() - dwComboTime) < 250)
        {
            TRACE("Releasing combo key\n");
            ReleaseKey(nComboKey);
        }

        // Otherwise clear the expired timer
        else
        {
            TRACE("Combo timer expired\n");
            dwComboTime = 0;
        }
    }

    for (int i = 0 ; asKeys_[i].nSamKey != SK_NONE; i++)
    {
        if (IsPressed(asKeys_[i].nKey) && asKeys_[i].nModifiers == nShifts)
        {
            // Release the PC keys used for the key combination
            ReleaseKey(asKeys_[i].nKey);
            if (nShifts & KMOD_LSHIFT) ToggleKey(SDLK_LSHIFT);
            if (nShifts & KMOD_LCTRL) ToggleKey(SDLK_LCTRL);
            if (nShifts & KMOD_LALT) { ToggleKey(SDLK_LALT); ReleaseKey(SDLK_RCTRL); }

            // Press the SAM key(s) required to generate the symbol
            PressSamKey(asKeys_[i].nSamKey);
            PressSamKey(asKeys_[i].nSamModifiers);

            // Remember the key involved with the shifted state for a combo
            nComboKey = asKeys_[i].nKey;
            nComboModifiers = static_cast<SDLMod>(nShifts);
        }
    }
}


// Build the SAM keyboard matrix from the current PC state
void SetSamKeyState ()
{
    // No SAM keys are pressed initially
    ReleaseAllSamKeys();

    // Return to ignore common Windows ALT- combinations so the SAM doesn't see them
    if (!GetOption(altforcntrl) && (IsPressed(SDLK_LALT) && (IsPressed(SDLK_TAB) || IsPressed(SDLK_ESCAPE) || IsPressed(SDLK_SPACE))))
        return;

    // Left and right shift keys are equivalent, and also complementary!
    bool fShiftToggle = IsPressed(SDLK_LSHIFT) && IsPressed(SDLK_RSHIFT);
    afKeyStates[SDLK_LSHIFT] |= afKeyStates[SDLK_RSHIFT];

    // Process the key combinations required for the mode we're in
    switch (GetOption(keymapping))
    {
        case 0:                                         break;  // Raw keyboard
        case 1:     ProcessKeyTable(asSamSymbols);      break;  // SAM symbol keys
//      case 2:     ProcessKeyTable(asSpectrumSymbols); break;  // Spectrum symbol keys
    }

    // Toggle shift if both shift keys are down to allow shifted versions of keys that are
    // shifted on the PC but unshifted on the SAM
    if (fShiftToggle)
        ToggleKey(SDLK_LSHIFT);

    // Build the rest of the SAM matrix from the simple non-symbol PC keys
    for (int i = 0 ; i < SK_MAX ; i++)
    {
        if (aeSamKeys[i] && IsPressed(aeSamKeys[i]))    // should be '.bScanCode' after OS mapping
            PressSamKey(i);
    }
}
