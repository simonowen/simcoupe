// Part of SimCoupe - A SAM Coupe emulator
//
// Input.cpp: Allegro keyboard, mouse and joystick input
//
//  Copyright (c) 1999-2003  Simon Owen
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
    int     nChar;                  // Symbol character (if any)

    eSamKey nSamKey, nSamModifiers; // Up to 2 SAM keys needed to generate the above symbol

    BYTE    bKey, bMods;            // Scancode and shift modifiers
}
COMBINATION_KEY;

typedef struct
{
    int     nChar;
    BYTE    bKey;
}
SIMPLE_KEY;

typedef struct
{
    BYTE    bKey;
    eSamKey nSamKey, nSamModifiers;
}
MAPPED_KEY;


BYTE bComboKey, bComboModifiers;
DWORD dwComboTime;

bool fMouseActive;

short anKeyStates[KEY_MAX], anKeys[KEY_MAX];
inline bool IsPressed(int nKey_)    { return anKeyStates[nKey_] > 0; }
inline void PressKey(int nKey_)     { anKeyStates[nKey_] = 1; }
inline void ReleaseKey(int nKey_)   { anKeyStates[nKey_] = 0; }
inline void ToggleKey(int nKey_)    { anKeyStates[nKey_] = (anKeyStates[nKey_] <= 0); }
inline void SetMasterKey(int nKey_, bool fHeld_=true) { anKeys[nKey_] = fHeld_; }

void KeyCallback (int nScanCode_);
void MouseCallback (int nFlags_);


SIMPLE_KEY asSamKeys [SK_MAX] =
{
    {0,KEY_LSHIFT},  {'z'},           {'x'},       {'c'},       {'v'},       {0,KEY_1_PAD}, {0,KEY_2_PAD}, {0,KEY_3_PAD},
    {'a'},           {'s'},           {'d'},       {'f'},       {'g'},       {0,KEY_4_PAD}, {0,KEY_5_PAD}, {0,KEY_6_PAD},
    {'q'},           {'w'},           {'e'},       {'r'},       {'t'},       {0,KEY_7_PAD}, {0,KEY_8_PAD}, {0,KEY_9_PAD},
    {'1'},           {'2'},           {'3'},       {'4'},       {'5'},       {0,KEY_ESC},   {0,KEY_TAB},   {0,KEY_CAPSLOCK},
    {'0'},           {'9'},           {'8'},       {'7'},       {'6'},       {0},           {0},           {0,KEY_BACKSPACE},
    {'p'},           {'o'},           {'i'},       {'u'},       {'y'},       {0},           {0},           {0,KEY_0_PAD},
    {0,KEY_ENTER},   {'l'},           {'k'},       {'j'},       {'h'},       {0},           {0},           {0},
    {' '},           {0,KEY_LCONTROL},{'m'},       {'n'},       {'b'},       {0},           {0},           {0,KEY_INSERT},
    {0,KEY_RCONTROL},{0,KEY_UP},      {0,KEY_DOWN},{0,KEY_LEFT},{0,KEY_RIGHT}
};

// Symbols with SAM keyboard details
COMBINATION_KEY asSamSymbols[] =
{
    { '!',  SK_SHIFT, SK_1 },       { '@',  SK_SHIFT, SK_2 },       { '#',  SK_SHIFT, SK_3 },
    { '$',  SK_SHIFT, SK_4 },       { '%',  SK_SHIFT, SK_5 },       { '&',  SK_SHIFT, SK_6 },
    { '\'', SK_SHIFT, SK_7 },       { '(',  SK_SHIFT, SK_8 },       { ')',  SK_SHIFT, SK_9 },
    { '~',  SK_SHIFT, SK_0 },       { '-',  SK_MINUS, SK_NONE },    { '/',  SK_SHIFT, SK_MINUS },
    { '+',  SK_PLUS, SK_NONE },     { '*',  SK_SHIFT, SK_PLUS },    { '<',  SK_SYMBOL, SK_Q },
    { '>',  SK_SYMBOL, SK_W },      { '[',  SK_SYMBOL, SK_R },      { ']',  SK_SYMBOL, SK_T },
    { '=',  SK_EQUALS, SK_NONE },   { '_',  SK_SHIFT, SK_EQUALS },  { '"',  SK_QUOTES, SK_NONE },
    { '`',  SK_SHIFT, SK_QUOTES },  { '{',  SK_SYMBOL, SK_F },      { '}',  SK_SYMBOL, SK_G },
    { '^',  SK_SYMBOL, SK_H },      { 163/*£*/, SK_SYMBOL, SK_L },  { ';',  SK_SEMICOLON, SK_NONE },
    { ':',  SK_COLON, SK_NONE },    { '?',  SK_SYMBOL, SK_X },      { '.',  SK_PERIOD, SK_NONE },
    { ',',  SK_COMMA, SK_NONE },    { '\\', SK_SHIFT, SK_INV },     { '|',  SK_SYMBOL, SK_9 },

    { '\0', SK_NONE,    SK_NONE,    0 }
};

// Symbols with Spectrum keyboard details
COMBINATION_KEY asSpectrumSymbols[] =
{
    { '!',  SK_SYMBOL, SK_1 },  { '@',  SK_SYMBOL, SK_2 },       { '#',  SK_SYMBOL, SK_3 },
    { '$',  SK_SYMBOL, SK_4 },  { '%',  SK_SYMBOL, SK_5 },       { '&',  SK_SYMBOL, SK_6 },
    { '\'', SK_SYMBOL, SK_7 },  { '(',  SK_SYMBOL, SK_8 },       { ')',  SK_SYMBOL, SK_9 },
    { '_',  SK_SYMBOL, SK_0 },  { '<',  SK_SYMBOL, SK_R },       { '>',  SK_SYMBOL, SK_T },
    { ';',  SK_SYMBOL, SK_O },  { '"',  SK_SYMBOL, SK_P },       { '-',  SK_SYMBOL, SK_J },
    { '^',  SK_SYMBOL, SK_H },  { '+',  SK_SYMBOL, SK_K },       { '=',  SK_SYMBOL, SK_L },
    { ':',  SK_SYMBOL, SK_Z },  { 163/*£*/, SK_SYMBOL, SK_X },   { '?',  SK_SYMBOL, SK_C },
    { '/',  SK_SYMBOL, SK_V },  { '*',  SK_SYMBOL, SK_B },       { ',',  SK_SYMBOL, SK_N },
    { '.',  SK_SYMBOL, SK_M },  { '\b', SK_SHIFT,  SK_0 },
    { '\0' }
};

// Handy mappings from unused PC keys to a SAM combination
MAPPED_KEY asPCMappings[] =
{
    // Some useful combinations
    { KEY_DEL,      SK_DELETE, SK_SHIFT },
    { KEY_HOME,     SK_LEFT,   SK_CONTROL },
    { KEY_END,      SK_RIGHT,  SK_CONTROL },
    { KEY_PGUP,     SK_F4,     SK_NONE },
    { KEY_PGDN,     SK_F1,     SK_NONE },
    { KEY_NUMLOCK,  SK_EDIT,   SK_SYMBOL },
    { KEY_MENU,     SK_EDIT,   SK_NONE },
    { KEY_DEL_PAD,  SK_QUOTES, SK_SHIFT },
    { 0 }
};

////////////////////////////////////////////////////////////////////////////////

// Keyboard interrupt handler
void KeyCallback (int nScanCode_)
{
    // Update the master table with the press or release
    anKeys[nScanCode_ & 0x7f] = (nScanCode_ & 0x80) ? -1 : 2;
}
END_OF_FUNCTION(KeyCallback);


bool Input::Init (bool fFirstInit_/*=false*/)
{
    Exit(true);

    install_keyboard();
    install_mouse();
    install_joystick(JOY_TYPE_AUTODETECT);

    key_led_flag = 0;

    LOCK_VARIABLE(anKeys);
    LOCK_FUNCTION((void*)KeyCallback);
    keyboard_lowlevel_callback = KeyCallback;

    Mouse::Init(fFirstInit_);
    fMouseActive = false;

    Purge();
    return true;
}

void Input::Exit (bool fReInit_/*=false*/)
{
    Mouse::Exit(fReInit_);

    remove_joystick();
    remove_mouse();
    remove_keyboard();
}


void Input::Acquire (bool fMouse_/*=true*/, bool fKeyboard_/*=true*/)
{
    // Flush out any buffered data if we're changing the acquisition state
    Purge();

    // We don't want key repeats when the emulation is using the keyboard
    set_keyboard_rate(fKeyboard_ ? 0 : 250, fKeyboard_ ? 0 : 30);

    // Set the active state of the mouse
    fMouseActive = fMouse_;
}


// Purge pending keyboard and/or mouse events
void Input::Purge (bool fMouse_/*=true*/, bool fKeyboard_/*=true*/)
{
    if (fKeyboard_)
    {
        clear_keybuf();

        // Release all keys
        memset(anKeyStates, 0, sizeof anKeyStates);
        memset(anKeys, 0, sizeof anKeys);
        ReleaseAllSamKeys();
    }

    if (fMouse_)
    {
        // No buttons pressed
        Mouse::SetButton(1, false);
        Mouse::SetButton(2, false);
        Mouse::SetButton(3, false);
    }
}


// Update a combination key table with a symbol
bool UpdateKeyTable (SIMPLE_KEY* asKeys_, int nKey_, int nMods_)
{
    BYTE bCode = nKey_ >> 8, bChar = nKey_ & 0xff;

    // Convert upper-case symbols to lower-case without shift
    if (bChar >= 'A' && bChar <= 'Z')
        bChar += 'a'-'A';

    for (int i = 0 ; i < SK_MAX ; i++)
    {
        // Is there a mapping entry for the symbol?
        if (asKeys_[i].nChar == bChar)
        {
            // Log if the mapping is new
            if (!asKeys_[i].bKey)
                TRACE("%c maps to %d\n", bChar, bCode);

            // Update the key mapping
            asKeys_[i].bKey = bCode;
            return true;
        }
    }

    return false;
}

// Update a combination key table with a symbol
bool UpdateKeyTable (COMBINATION_KEY* asKeys_, int nKey_, int nMods_)
{
    BYTE bCode = nKey_ >> 8, bChar = nKey_ & 0xff, bMods = nMods_ & (KB_SHIFT_FLAG|KB_CTRL_FLAG|KB_ALT_FLAG);

    for (int i = 0 ; asKeys_[i].nSamKey != SK_NONE ; i++)
    {
        // Is there a mapping entry for the symbol?
        if (asKeys_[i].nChar == bChar)
        {
            // Log if the mapping is new
            if (!asKeys_[i].bKey)
                TRACE("%c maps to %d with mods of %#04x\n", bChar, bCode, bMods);

            // Update the key mapping
            asKeys_[i].bKey = bCode;
            asKeys_[i].bMods = bMods;
            return true;
        }
    }

    return false;
}


// Process simple key presses
void ProcessKeyTable (SIMPLE_KEY* asKeys_)
{
    // Build the rest of the SAM matrix from the simple non-symbol PC keys
    for (int i = 0 ; i < SK_MAX ; i++)
    {
        if (asKeys_[i].bKey && IsPressed(asKeys_[i].bKey))
            PressSamKey(i);
    }
}

// Process the additional keys mapped from PC to SAM, ignoring shift state
void ProcessKeyTable (MAPPED_KEY* asKeys_)
{
    // Build the rest of the SAM matrix from the simple non-symbol PC keys
    for (int i = 0 ; asKeys_[i].bKey ; i++)
    {
        if (IsPressed(asKeys_[i].bKey))
        {
            PressSamKey(asKeys_[i].nSamKey);
            PressSamKey(asKeys_[i].nSamModifiers);
        }
    }
}

// Process more complicated key combinations
void ProcessKeyTable (COMBINATION_KEY* asKeys_)
{
    BYTE bShifts = 0;
    if (IsPressed(KEY_LSHIFT))   bShifts |= KB_SHIFT_FLAG;
    if (IsPressed(KEY_LCONTROL)) bShifts |= KB_CTRL_FLAG;
    if (IsPressed(KEY_ALT))      bShifts |= KB_ALT_FLAG;
//  if (IsPressed(KEY_ALTGR))    bShifts |= KB_ALT_FLAG;

    // Have the shift states changed while a combo is in progress?
    if (bComboModifiers && bComboModifiers != bShifts)
    {
        // If the combo key is still pressed, start the timer running to re-press it as we're about to release it
        if (IsPressed(bComboKey))
        {
            TRACE("Starting combo timer\n");
            dwComboTime = OSD::GetTime();
        }

        // We're done with the shift state now, so clear it to prevent the time getting reset
        bComboModifiers = 0;
    }

    // Combo unpress timer active?
    if (dwComboTime)
    {
        TRACE("Combo timer active\n");

        // If we're within the threshold, ensure the key remains released
        if ((OSD::GetTime() - dwComboTime) < 250)
        {
            TRACE("Releasing combo key\n");
            ReleaseKey(bComboKey);
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
        if (IsPressed(asKeys_[i].bKey) && asKeys_[i].bMods == bShifts)
        {
            // Release the PC keys used for the key combination
            ReleaseKey(asKeys_[i].bKey);
            if (bShifts & KB_SHIFT_FLAG) ToggleKey(KEY_LSHIFT);
            if (bShifts & KB_CTRL_FLAG) ToggleKey(KEY_LCONTROL);
            if (bShifts & KB_ALT_FLAG) { ToggleKey(KEY_ALT); ReleaseKey(KEY_RCONTROL); }

            // Press the SAM key(s) required to generate the symbol
            PressSamKey(asKeys_[i].nSamKey);
            PressSamKey(asKeys_[i].nSamModifiers);

            // Remember the key involved with the shifted state for a combo
            bComboKey = asKeys_[i].bKey;
            bComboModifiers = bShifts;
        }
    }
}


// Read the current keyboard state, and make any special adjustments needed before it's processed
bool ReadKeyboard ()
{
    bool fRet = true;

    // Process any buffered key presses
    for (poll_keyboard() ; keypressed() ; )
    {
        // Fetch the key code/char and shift states
        int nKey = readkey(), nMods = key_shifts;
        BYTE bKey = nKey >> 8, bChar = nKey & 0xff;
//      Frame::SetStatus("Key: %02x  Char:%02x  Mods:%04x", bKey, bChar, nMods & 0xff);

        // Ignore symbols on the keypad
        if (bKey >= KEY_SLASH_PAD && bKey <= KEY_ENTER_PAD || bKey >= KEY_0_PAD && bKey <= KEY_9_PAD)
            bChar = 0;

        // Pass any printable characters to the GUI
        if (GUI::IsActive())
        {
            // Convert the cursor keys to GUI symbols
            if (bKey >= KEY_0_PAD && bKey <= KEY_9_PAD)
                bChar = GK_KP0 + bKey - KEY_0_PAD;
            else if (bKey >= KEY_LEFT && bKey <= KEY_DOWN)
                bChar = GK_LEFT + (bKey - KEY_LEFT);
            else if (bKey >= KEY_HOME && bKey <= KEY_PGDN)
                bChar = GK_HOME + (bKey - KEY_HOME);

            // Pass any printable key-down messages to the GUI
            if (bChar)
                GUI::SendMessage(GM_CHAR, bChar, (nMods & KB_SHIFT_FLAG) != 0);

            break;
        }

        // If there is a character to process, check for a simple SAM key mapping
        else if (bChar)
        {
            if (!UpdateKeyTable(asSamKeys, nKey, nMods))
            {
                // Otherwise try for a symbol, depending on the key mapping selected
                switch (GetOption(keymapping))
                {
                    case 1: UpdateKeyTable(asSamSymbols, nKey, nMods);      break;
                    case 2: UpdateKeyTable(asSpectrumSymbols, nKey, nMods); break;
                }
            }
        }
    }

    for (int i = 0 ; i < KEY_MAX ; i++)
    {
        if (anKeys[i] == 2)
        {
            UI::ProcessKey(i, key_shifts);
            anKeys[i] = 1;
        }
        else if (anKeys[i] == -1)
        {
            UI::ProcessKey(i|0x80, key_shifts);
            anKeys[i] = 0;
        }
    }

    // Make a copy of the master key table with the real keyboard states
    memcpy(anKeyStates, anKeys, sizeof anKeyStates);

    // If the option is set, Left-ALT does the same as Right-Control: to generate SAM Cntrl
    if (GetOption(altforcntrl) && IsPressed(KEY_ALT))
        PressKey(KEY_RCONTROL);

    // AltGr can optionally be used for SAM Edit
    if (GetOption(altgrforedit) && IsPressed(KEY_ALTGR))
    {
        // AltGr is usually seen with left-control down (NT/W2K), so release it
        ReleaseKey(KEY_LCONTROL);

        // Release AltGr (needed for Win9x it seems) and press the context menu key (also used for SAM Edit)
        ReleaseKey(KEY_ALTGR);
        PressKey(KEY_MENU);
    }

    return fRet;
}


// Build the SAM keyboard matrix from the current PC state
void SetSamKeyState ()
{
    // No SAM keys are pressed initially
    ReleaseAllSamKeys();

    // Left and right shift keys are equivalent, and also complementary!
    bool fShiftToggle = IsPressed(KEY_LSHIFT) && IsPressed(KEY_RSHIFT);
    if (IsPressed(KEY_RSHIFT)) PressKey(KEY_LSHIFT);

    // Process the key combinations required for the mode we're in
    switch (GetOption(keymapping))
    {
        case 0:                                         break;  // Raw keyboard
        case 1:     ProcessKeyTable(asSamSymbols);      break;  // SAM symbol keys
        case 2:     ProcessKeyTable(asSpectrumSymbols); break;  // Spectrum symbol keys
    }

    // Toggle shift if both shift keys are down to allow shifted versions of keys that are
    // shifted on the PC but unshifted on the SAM
    if (fShiftToggle)
        ToggleKey(KEY_LSHIFT);

    // Process the simple key and additional PC key mappings
    ProcessKeyTable(asSamKeys);
    ProcessKeyTable(asPCMappings);
}


void ReadMouse ()
{
    static int nLastPos, nLastButtons;

    // Return if there's no mouse
    if (poll_mouse() < 0)
        return;

    int nX = mouse_x, nY = mouse_y;

    // Has mouse moved?
    if (mouse_pos != nLastPos)
    {
        // Mouse in use by the GUI?
        if (GUI::IsActive())
        {
            Display::DisplayToSamPoint(&nX, &nY);
            GUI::SendMessage(GM_MOUSEMOVE, nX, nY);
        }
        else if (fMouseActive)
        {
            // Work out the relative movement since last time
            nX -= (Frame::GetWidth() >> 1);
            nY -= (Frame::GetHeight() >> 1);

#ifdef ALLEGRO_DOS
//          nX *= 5, nY *= 5;
#endif

            // Has it moved at all?
            if (nX || nY)
            {
                // We need to track partial units, as we're higher resolution than SAM
                static int nXX, nYY;

                // Add on the new movement
                nXX += nX, nYY += nY;

                // How far has the mouse moved in SAM units?
                nX = nXX, nY = nYY;
                Display::DisplayToSamSize(&nX, &nY);

                // Update the SAM mouse position
                Mouse::Move(nX, -nY);
//              TRACE("Mouse move: X:%-03d Y:%-03d\n", nX, nY);

                // How far is the SAM mouse movement in native units?
                Display::SamToDisplaySize(&nX, &nY);

                // Subtract the used portion of the movement, and leave the remainder for next time
                nXX -= nX, nYY -= nY;

                // Move the mouse back to the centre to stop it escaping
                position_mouse(Frame::GetWidth() >> 1, Frame::GetHeight() >> 1);
            }
        }

        // Remember the mouse position
        nLastPos = mouse_pos;
    }

    // Has a button state changed?
    if (mouse_b != nLastButtons)
    {
        if (GUI::IsActive())
        {
            bool fPress = (mouse_b & (mouse_b ^ nLastButtons)) != 0;

            Display::DisplayToSamPoint(&nX, &nY);
            GUI::SendMessage(fPress ? GM_BUTTONDOWN : GM_BUTTONUP, nX, nY);
        }

        // Grab the mouse on a left-click, if not already active (don't let the emulation see the click either)
        else if (!fMouseActive && (mouse_b ^ nLastButtons))
        {
            Input::Acquire();
            position_mouse(Frame::GetWidth() >> 1, Frame::GetHeight() >> 1);
        }
        else
        {
            // Set the current button states
            Mouse::SetButton(1, (mouse_b & 1) != 0);
            Mouse::SetButton(2, (mouse_b & 2) != 0);
            Mouse::SetButton(3, (mouse_b & 4) != 0);
        }

        // Remember the button states
        nLastButtons = mouse_b;
    }
}


void ReadJoystick ()
{
    // Return if there's no joystick
    if (poll_joystick() < 0)
        return;

    if (num_joysticks >= 1)
    {
        int nDeadZone = 128 * GetOption(deadzone1) / 100;
        JOYSTICK_INFO* pJoy = &joy[0];

        if (pJoy->stick[0].axis[0].pos <= -nDeadZone) PressSamKey(SK_6);    // Left
        if (pJoy->stick[0].axis[0].pos >=  nDeadZone) PressSamKey(SK_7);    // Right
        if (pJoy->stick[0].axis[1].pos >=  nDeadZone) PressSamKey(SK_8);    // Down
        if (pJoy->stick[0].axis[1].pos <= -nDeadZone) PressSamKey(SK_9);    // Up
        if (pJoy->button[0].b) PressSamKey(SK_0);                           // Fire
    }
}


void Input::Update ()
{
    // Read keyboard and mouse
    ReadKeyboard();
    ReadMouse();

    // Update the SAM keyboard matrix from the current key state (including joystick movement)
    SetSamKeyState();

    ReadJoystick();
}
