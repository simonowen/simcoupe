// Part of SimCoupe - A SAM Coupe emulator
//
// Keyboard.cpp: Common keyboard handling
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

#include "SimCoupe.h"
#include "Keyboard.h"

#include "Input.h"
#include "Joystick.h"
#include "Keyin.h"
#include "Memory.h"
#include "Options.h"

namespace Keyboard
{
std::array<uint8_t, 9> key_matrix;
inline void PressSamKey(int k) { key_matrix[k >> 3] &= ~(1 << (k & 7)); }

struct MAPPED_KEY
{
    int nChar;                  // Symbol or HK_ virtual keycode
    eSamKey nSamMods, nSamKey;  // Up to 2 SAM keys needed to generate the above symbol
    int nKey, nMods;            // Host scancode and modifiers

};


int nComboKey, nComboMods;
std::optional<std::chrono::steady_clock::time_point> combo_time;

std::array<uint8_t, 512 / 8> key_states;
inline bool IsPressed(int k) { return !!(key_states[k >> 3] & (1 << (k & 7))); }
inline void PressKey(int k) { key_states[k >> 3] |= (1 << (k & 7)); }
inline void ReleaseKey(int k) { key_states[k >> 3] &= ~(1 << (k & 7)); }
inline void ToggleKey(int k) { key_states[k >> 3] ^= (1 << (k & 7)); }

std::array<int, HK_MAX - HK_MIN + 1> hk_mappings;
inline bool IsPressed(eHostKey k) { return IsPressed(hk_mappings[k - HK_MIN]); }
inline void PressKey(eHostKey k) { PressKey(hk_mappings[k - HK_MIN]); }
inline void ReleaseKey(eHostKey k) { ReleaseKey(hk_mappings[k - HK_MIN]); }
inline void ToggleKey(eHostKey k) { ToggleKey(hk_mappings[k - HK_MIN]); }

static void PrepareKeyTable(MAPPED_KEY* asKeys_);
static void ProcessShiftedKeys(MAPPED_KEY* asKeys_);
static void ProcessUnshiftedKeys(MAPPED_KEY* asKeys_);


// Main keyboard matrix (minus modifiers)
MAPPED_KEY asKeyMatrix[] =
{
    { HK_LSHIFT }, { 'z' },      { 'x' },     { 'c' },     { 'v' },      { HK_KP1 },  { HK_KP2 },  { HK_KP3 },
    { 'a' },       { 's' },      { 'd' },     { 'f' },     { 'g' },      { HK_KP4 },  { HK_KP5 },  { HK_KP6 },
    { 'q' },       { 'w' },      { 'e' },     { 'r' },     { 't' },      { HK_KP7 },  { HK_KP8 },  { HK_KP9 },
    { '1' },       { '2' },      { '3' },     { '4' },     { '5' },      { HK_ESC },  { HK_TAB },  { HK_CAPSLOCK },
    { '0' },       { '9' },      { '8' },     { '7' },     { '6' },      { HK_NONE }, { HK_NONE }, { HK_BACKSPACE },
    { 'p' },       { 'o' },      { 'i' },     { 'u' },     { 'y' },      { HK_NONE }, { HK_NONE }, { HK_KP0 },
    { HK_RETURN }, { 'l' },      { 'k' },     { 'j' },     { 'h' },      { HK_NONE }, { HK_NONE }, { HK_NONE },
    { ' ' },       { HK_LCTRL }, { 'm' },     { 'n' },     { 'b' },      { HK_NONE }, { HK_NONE }, { HK_INSERT },
    { HK_RCTRL },  { HK_UP },    { HK_DOWN }, { HK_LEFT }, { HK_RIGHT },

    { 0 }
};

// SAM-specific keys
MAPPED_KEY asSamKeys[] =
{
    { '!',  SK_SHIFT,  SK_1 },      { '@',  SK_SHIFT,  SK_2 },      { '#',  SK_SHIFT,  SK_3 },
    { '$',  SK_SHIFT,  SK_4 },      { '%',  SK_SHIFT,  SK_5 },      { '&',  SK_SHIFT,  SK_6 },
    { '\'', SK_SHIFT,  SK_7 },      { '(',  SK_SHIFT,  SK_8 },      { ')',  SK_SHIFT,  SK_9 },
    { '~',  SK_SHIFT,  SK_0 },      { '-',  SK_NONE,   SK_MINUS },  { '/',  SK_SHIFT,  SK_MINUS },
    { '+',  SK_NONE,   SK_PLUS },   { '*',  SK_SHIFT,  SK_PLUS },   { '<',  SK_SYMBOL, SK_Q },
    { '>',  SK_SYMBOL, SK_W },      { '[',  SK_SYMBOL, SK_R },      { ']',  SK_SYMBOL, SK_T },
    { '=',  SK_NONE,   SK_EQUALS }, { '_',  SK_SHIFT,  SK_EQUALS }, { '"',  SK_NONE,   SK_QUOTES },
    { '`',  SK_SHIFT,  SK_QUOTES }, { '{',  SK_SYMBOL, SK_F },      { '}',  SK_SYMBOL, SK_G },
    { '^',  SK_SYMBOL, SK_H },      { 163,  SK_SYMBOL, SK_L },      { ';',  SK_NONE,   SK_SEMICOLON },
    { ':',  SK_NONE,   SK_COLON },  { '?',  SK_SYMBOL, SK_X },      { '.',  SK_NONE,   SK_PERIOD },
    { ',',  SK_NONE,   SK_COMMA },  { '\\', SK_SHIFT,  SK_INV },    { '|',  SK_SYMBOL, SK_9 },

    // Mac keyboard symbols to access both pound symbols, to help UK and US users
    { 167,  SK_SHIFT,  SK_3 },  // section symbol to #
    { 177,  SK_SYMBOL, SK_L },  // +/- to GBP symbol

    // Useful mappings
    { HK_DELETE,    SK_SHIFT,   SK_DELETE },
    { HK_HOME,      SK_CONTROL, SK_LEFT },
    { HK_END,       SK_CONTROL, SK_RIGHT },
    { HK_PGUP,      SK_NONE,    SK_F4 },
    { HK_PGDN,      SK_NONE,    SK_F1 },
    { HK_NUMLOCK,   SK_SYMBOL,  SK_EDIT },
    { HK_APPS,      SK_NONE,    SK_EDIT },
    { HK_KPDECIMAL, SK_SHIFT,   SK_QUOTES },

    { 0 }
};

// Spectrum-specific keys
MAPPED_KEY asSpectrumKeys[] =
{
    { '!',  SK_SYMBOL, SK_1 },      { '@',  SK_SYMBOL, SK_2 },      { '#',  SK_SYMBOL, SK_3 },
    { '$',  SK_SYMBOL, SK_4 },      { '%',  SK_SYMBOL, SK_5 },      { '&',  SK_SYMBOL, SK_6 },
    { '\'', SK_SYMBOL, SK_7 },      { '(',  SK_SYMBOL, SK_8 },      { ')',  SK_SYMBOL, SK_9 },
    { '_',  SK_SYMBOL, SK_0 },      { '<',  SK_SYMBOL, SK_R },      { '>',  SK_SYMBOL, SK_T },
    { '`',  SK_SYMBOL, SK_I },      { ';',  SK_SYMBOL, SK_O },      { '"',  SK_SYMBOL, SK_P },
    { '-',  SK_SYMBOL, SK_J },      { '^',  SK_SYMBOL, SK_H },      { '+',  SK_SYMBOL, SK_K },
    { '=',  SK_SYMBOL, SK_L },      { ':',  SK_SYMBOL, SK_Z },      { 163,  SK_SYMBOL, SK_X },
    { '?',  SK_SYMBOL, SK_C },      { '/',  SK_SYMBOL, SK_V },      { '*',  SK_SYMBOL, SK_B },
    { ',',  SK_SYMBOL, SK_N },      { '.',  SK_SYMBOL, SK_M },

    // Useful mappings
    { HK_BACKSPACE, SK_SHIFT, SK_0 },
    { HK_APPS,      SK_SHIFT, SK_1 },
    { HK_CAPSLOCK,  SK_SHIFT, SK_2 },
    { HK_LEFT,      SK_SHIFT, SK_5 },
    { HK_DOWN,      SK_SHIFT, SK_6 },
    { HK_UP,        SK_SHIFT, SK_7 },
    { HK_RIGHT,     SK_SHIFT, SK_8 },
    { HK_RCTRL,     SK_SHIFT, SK_NONE },

    { 0 }
};


bool Init()
{
    // Fill the SAM keys for the main keyboard matrix
    for (int i = SK_MIN; i < SK_MAX; i++)
    {
        asKeyMatrix[i].nSamMods = SK_NONE;
        asKeyMatrix[i].nSamKey = static_cast<eSamKey>(i);
    }

    // HK_ to scancode mapping
    for (int j = HK_MIN; j < HK_MAX; j++)
        hk_mappings[j - HK_MIN] = Input::MapChar(j);

    // Prepare key tables in advance if possible (Win32)
    PrepareKeyTable(asKeyMatrix);
    PrepareKeyTable(asSamKeys);
    PrepareKeyTable(asSpectrumKeys);

    Purge();
    return true;
}

void Purge()
{
    key_states.fill(0);
    key_matrix.fill(0xff);
}


// Build the SAM keyboard matrix from the current PC state
void Update()
{
    // Save a copy of the current key state, so we can modify it during matching below
    static decltype(key_states) key_states_copy;
    key_states_copy = key_states;

    // No SAM keys are pressed initially
    key_matrix.fill(0xff);

    // Suppress normal key input if we're auto-typing
    if (Keyin::IsTyping())
        return;

    // Left and right shift keys are equivalent, and also complementary!
    bool fShiftToggle = IsPressed(HK_LSHIFT) && IsPressed(HK_RSHIFT);
    if (IsPressed(HK_RSHIFT)) PressKey(HK_LSHIFT);

    // Left-Alt?
    if (IsPressed(HK_LALT))
    {
        if (!GetOption(altforcntrl) || IsPressed(HK_TAB))
            return;

        ReleaseKey(HK_LALT);
        PressSamKey(SK_CONTROL);
    }

    // AltGr can optionally be used for SAM Edit
    if (IsPressed(HK_RALT))
    {
        if (GetOption(altgrforedit))
        {
            // Release AltGr and press the context menu key (also used for SAM Edit)
            ReleaseKey(HK_RALT);
            PressSamKey(SK_EDIT);
        }

        // Also release Ctrl and Alt, which is how AltGr often behaves
        ReleaseKey(HK_LCTRL);
        ReleaseKey(HK_LALT);
    }


    // The Windows keys can be used with the regular function keys for the SAM keypad
    if (IsPressed(HK_LWIN) || IsPressed(HK_RWIN))
    {
        // Note: the SK_ range isn't contiguous
        static const int anS[] = { SK_F1, SK_F2, SK_F3, SK_F4, SK_F5, SK_F6, SK_F7, SK_F8, SK_F9, SK_F0, SK_F1 };
        bool fkey_detected{ false };

        for (unsigned int u = 0; u < std::size(anS); u++)
        {
            if (IsPressed(static_cast<eHostKey>(HK_F1 + u)))
            {
                PressSamKey(anS[u]);
                ReleaseKey(HK_F1 + u);
                ReleaseKey(HK_APPS);
                ReleaseKey(HK_RCTRL);
                fkey_detected = true;
                break;
            }
        }

        if (!fkey_detected)
            return;
    }

    int nMapping = GetOption(keymapping);

    // In Auto mode, use Spectrum mappings if a 48K ROM appears to be present (beeper routine check)
    if (nMapping == 1 && !memcmp(AddrReadPtr(0x03b5), "\xF3\x7D\xCB\x3D\xCB\x3D\x2F", 7))
        nMapping = 3;

    // Process the key combinations required for the mode we're in
    switch (nMapping)
    {
    case 0: // Raw (no mapping)
        break;

    default:
    case 2: // SAM
        ProcessShiftedKeys(asSamKeys);
        ProcessShiftedKeys(asKeyMatrix);
        ProcessUnshiftedKeys(asSamKeys);
        break;

    case 3: // Spectrum
        ProcessShiftedKeys(asSpectrumKeys);
        ProcessShiftedKeys(asKeyMatrix);
        ProcessUnshiftedKeys(asSpectrumKeys);
        break;
    }


    // Toggle shift if both shift keys are down to allow shifted versions of keys that are
    // shifted on the PC but unshifted on SAM
    if (fShiftToggle)
        ToggleKey(HK_LSHIFT);

    // Process the base key mappings
    ProcessUnshiftedKeys(asKeyMatrix);

    // Apply joystick 1 input if either device is mapped to it
    if (GetOption(joytype1) == jtJoystick1) key_matrix[4] &= ~Joystick::ReadSinclair2(0);
    if (GetOption(joytype2) == jtJoystick1) key_matrix[4] &= ~Joystick::ReadSinclair2(1);

    // Apply joystick 2 input if either device is mapped to it
    if (GetOption(joytype1) == jtJoystick2) key_matrix[3] &= ~Joystick::ReadSinclair1(0);
    if (GetOption(joytype2) == jtJoystick2) key_matrix[3] &= ~Joystick::ReadSinclair1(1);

    // Restore the key states
    key_states = key_states_copy;
}


// Prepare the more complicated combination keys that are fairly keyboard specific
void PrepareKeyTable(MAPPED_KEY* asKeys_)
{
    for (int i = 0; asKeys_[i].nChar; i++)
    {
        asKeys_[i].nKey = Input::MapChar(asKeys_[i].nChar, &asKeys_[i].nMods);
        // TRACE("{} maps to {} with mods of {:02x}\n", asKeys_[i].nChar, asKeys_[i].nKey, asKeys_[i].nMods);
    }
}


// Process shifted key combinations
void ProcessShiftedKeys(MAPPED_KEY* asKeys_)
{
    int nMods = 0;
    if (IsPressed(HK_LSHIFT)) nMods |= HM_SHIFT;
    if (IsPressed(HK_LCTRL))  nMods |= HM_CTRL;
    if (IsPressed(HK_LALT))   nMods |= HM_ALT;
    if (IsPressed(HK_RALT))   nMods |= (HM_CTRL | HM_ALT);

    // Have the mods changed while the combo was active?
    if (combo_time && nComboMods != nMods)
    {
        // If we're within the threshold, ensure the key remains released
        auto now = std::chrono::steady_clock::now();
        if ((now - *combo_time) < std::chrono::milliseconds(250))
        {
            ReleaseKey(nComboKey);
        }
        else
        {
            combo_time = std::nullopt;
        }
    }

    for (int i = 0; asKeys_[i].nChar; i++)
    {
        // Key and necessary modifiers pressed?
        if (asKeys_[i].nMods && IsPressed(asKeys_[i].nKey) && (asKeys_[i].nMods & nMods) == asKeys_[i].nMods)
        {
            // TRACE("{} ({}) pressed with mods {:02x} (of {:02x})\n", asKeys_[i].nKey, asKeys_[i].nChar, asKeys_[i].nMods, nMods);

            // Press the keys required to generate the symbol
            PressSamKey(asKeys_[i].nSamMods);
            PressSamKey(asKeys_[i].nSamKey);

            // Release the main key
            ReleaseKey(asKeys_[i].nKey);

            // Release the modifiers keys and clear the processed modifier bit(s)
            if (asKeys_[i].nMods & HM_SHIFT) { ReleaseKey(HK_LSHIFT); nMods &= ~HM_SHIFT; }
            if (asKeys_[i].nMods & HM_CTRL) { ReleaseKey(HK_LCTRL); nMods &= ~HM_CTRL; }
            if (asKeys_[i].nMods & HM_ALT) { ReleaseKey(HK_LALT); ReleaseKey(HK_LCTRL); nMods &= ~(HM_CTRL | HM_ALT); }

            // Remember the combo key details and current time
            nComboKey = asKeys_[i].nKey;
            nComboMods = asKeys_[i].nMods;
            combo_time = std::chrono::steady_clock::now();
        }
    }
}

// Process simple unshifted keys
void ProcessUnshiftedKeys(MAPPED_KEY* asKeys_)
{
    for (int i = 0; asKeys_[i].nChar; i++)
    {
        if (!asKeys_[i].nMods && IsPressed(asKeys_[i].nKey))
        {
            PressSamKey(asKeys_[i].nSamMods);
            PressSamKey(asKeys_[i].nSamKey);
        }
    }
}


// Update a combination key table with a symbol
static bool UpdateKeyTable(MAPPED_KEY* asKeys_, int nKey_, int nMods_, int nChar_)
{
    // Treat numeric keypad as base keys.
    if (nKey_ >= HK_KP0 && nKey_ <= HK_KP9)
    {
        nMods_ = 0;
    }

    // Convert upper-case symbols to lower-case without shift
    if (nChar_ >= 'A' && nChar_ <= 'Z')
    {
        nChar_ += 'a' - 'A';
        nMods_ &= ~HM_SHIFT;
    }

    // Convert ctrl-letter and ctrl-digit to the base key
    if (nMods_ & HM_CTRL)
    {
        if ((nChar_ >= 'a' && nChar_ <= 'z') || (nChar_ >= '0' && nChar_ <= '9'))
            nMods_ &= ~HM_CTRL;
    }

    for (int i = 0; asKeys_[i].nChar; i++)
    {
        // Is there a mapping entry for the symbol?
        if (asKeys_[i].nChar == nChar_)
        {
            // Log if the mapping is new
            if (!asKeys_[i].nKey)
                TRACE("{} maps to {} with mods of {:02x}\n", nChar_, nKey_, nMods_);

            // Update the key mapping
            asKeys_[i].nKey = nKey_;
            asKeys_[i].nMods = nMods_;

            return true;
        }
    }

    return false;
}


// Set the state of a native key, remember any character generated from it
void SetKey(int nCode_, bool fPressed_, int nMods_/*=0*/, int nChar_/*=0*/)
{
    // Key released?
    if (!fPressed_)
        ReleaseKey(nCode_);
    else
    {
        // Press the key code
        PressKey(nCode_);

        // If a character was supplied (non-Win32), update the mapping entries
        if (nChar_)
        {
            UpdateKeyTable(asKeyMatrix, nCode_, nMods_, nChar_);
            UpdateKeyTable(asSamKeys, nCode_, nMods_, nChar_);
            UpdateKeyTable(asSpectrumKeys, nCode_, nMods_, nChar_);
        }
    }
}

} // namespace Keyboard
