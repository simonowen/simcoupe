// Part of SimCoupe - A SAM Coupe emulator
//
// Keyboard.cpp: Common keyboard handling
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
#include "Keyboard.h"

#include "Input.h"
#include "Joystick.h"
#include "Memory.h"
#include "Options.h"


typedef struct
{
    int nChar;					// Symbol or HK_ virtual keycode
    eSamKey nSamMods, nSamKey;  // Up to 2 SAM keys needed to generate the above symbol
    int nKey, nMods;			// Host scancode and modifiers

} MAPPED_KEY;


int anNativeKey[HK_MAX-HK_MIN+1];

int nComboKey, nComboMods;
DWORD dwComboTime;

BYTE abKeys[512>>3];
inline bool IsPressed(int k)    { return !!(abKeys[k>>3] & (1<<(k&7))); }
inline void PressKey(int k)     { abKeys[k>>3] |= (1 << (k&7)); }
inline void ReleaseKey(int k)   { abKeys[k>>3] &= ~(1 << (k&7)); }
inline void ToggleKey(int k)    { abKeys[k>>3] ^= (1 << (k&7)); }

inline bool IsPressed(eHostKey k) { return IsPressed(anNativeKey[k-HK_MIN]); }
inline void PressKey(eHostKey k)  { PressKey(anNativeKey[k-HK_MIN]); }
inline void ReleaseKey(eHostKey k){ ReleaseKey(anNativeKey[k-HK_MIN]); }
inline void ToggleKey(eHostKey k) { ToggleKey(anNativeKey[k-HK_MIN]); }

static void PrepareKeyTable (MAPPED_KEY* asKeys_);
static void ProcessShiftedKeys (MAPPED_KEY* asKeys_);
static void ProcessUnshiftedKeys (MAPPED_KEY* asKeys_);


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
    { '-',  SK_SYMBOL, SK_J },		{ '^',  SK_SYMBOL, SK_H },      { '+',  SK_SYMBOL, SK_K },
    { '=',  SK_SYMBOL, SK_L },		{ ':',  SK_SYMBOL, SK_Z },      { 163,  SK_SYMBOL, SK_X },
    { '?',  SK_SYMBOL, SK_C },		{ '/',  SK_SYMBOL, SK_V },      { '*',  SK_SYMBOL, SK_B },
    { ',',  SK_SYMBOL, SK_N },		{ '.',  SK_SYMBOL, SK_M },

    // Useful mappings
    { HK_BACKSPACE, SK_SHIFT, SK_0 },
    { HK_APPS,	    SK_SHIFT, SK_1 },
    { HK_CAPSLOCK,  SK_SHIFT, SK_2 },
    { HK_LEFT,      SK_SHIFT, SK_5 },
    { HK_DOWN,      SK_SHIFT, SK_6 },
    { HK_UP,        SK_SHIFT, SK_7 },
    { HK_RIGHT,     SK_SHIFT, SK_8 },
    { HK_RCTRL,     SK_SHIFT, SK_NONE },

    { 0 }
};


bool Keyboard::Init (bool fFirstInit_/*=false*/)
{
    int i;

    // Fill the SAM keys for the main keyboard matrix
    for (i = SK_MIN ; i < SK_MAX ; i++)
    {
        asKeyMatrix[i].nSamMods = SK_NONE;
        asKeyMatrix[i].nSamKey = eSamKey(i);
    }

    // HK_ to scancode mapping
    for (i = HK_MIN ; i < HK_MAX ; i++)
        anNativeKey[i-HK_MIN] = Input::MapChar(i);

    // Prepare key tables in advance if possible (Win32)
    PrepareKeyTable(asKeyMatrix);
    PrepareKeyTable(asSamKeys);
    PrepareKeyTable(asSpectrumKeys);

    Purge();
    return true;
}

void Keyboard::Exit (bool fReInit_/*=false*/)
{
}


void Keyboard::Purge ()
{
    memset(abKeys, 0, sizeof(abKeys));
    memset(keybuffer, 0xff, sizeof(keybuffer));
}


// Build the SAM keyboard matrix from the current PC state
void Keyboard::Update ()
{
    // Save a copy of the current key state, so we can modify it during matching below
    BYTE abKeysCopy[_countof(abKeys)];
    memcpy(abKeysCopy, abKeys, sizeof(abKeys));

    // No SAM keys are pressed initially
    memset(keybuffer, 0xff, sizeof(keybuffer));


    // Left and right shift keys are equivalent, and also complementary!
    bool fShiftToggle = IsPressed(HK_LSHIFT) && IsPressed(HK_RSHIFT);
    if (IsPressed(HK_RSHIFT)) PressKey(HK_LSHIFT);

    // Left-Alt can optionally be used as the SAM Cntrl key
    if (GetOption(altforcntrl) && IsPressed(HK_LALT))
    {
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
        static const int anS[] = { SK_F1, SK_F2, SK_F3, SK_F4, SK_F5, SK_F6, SK_F7, SK_F8, SK_F9, SK_F0 };

        for (UINT u = 0 ; u < _countof(anS) ; u++)
        {
            if (IsPressed(static_cast<eHostKey>(HK_F1+u)))
            {
                // Press the SAM function key
                PressSamKey(anS[u]);
                break;
            }
        }
    }

    int nMapping = GetOption(keymapping);

    // In Auto mode, use Spectrum mappings if a 48K ROM appears to be present (beeper routine check)
    if (nMapping == 1 && !memcmp(phys_read_addr(0x03b5), "\xF3\x7D\xCB\x3D\xCB\x3D\x2F", 7))
        nMapping = 3;

    // Process the key combinations required for the mode we're in
    switch (nMapping)
    {
        case 0:	// Raw (no mapping)
            break;

        default:
        case 2:	// SAM
            ProcessShiftedKeys(asSamKeys);
            ProcessShiftedKeys(asKeyMatrix);
            ProcessUnshiftedKeys(asSamKeys);
            break;

        case 3:	// Spectrum
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

    // Apply Sinclair joystick movements
    keybuffer[4] &= ~Joystick::ReadSinclair2(0);
    keybuffer[3] &= ~Joystick::ReadSinclair1(1);

    // Restore the key states
    memcpy(abKeys, abKeysCopy, sizeof(abKeys));
}


// Prepare the more complicated combination keys that are fairly keyboard specific
void PrepareKeyTable (MAPPED_KEY* asKeys_)
{
    for (int i = 0 ; asKeys_[i].nChar ; i++)
    {
        asKeys_[i].nKey = Input::MapChar(asKeys_[i].nChar, &asKeys_[i].nMods);
//		TRACE("%d maps to %d with mods of %02x\n", asKeys_[i].nChar, asKeys_[i].nKey, asKeys_[i].nMods);
    }
}


// Process shifted key combinations
void ProcessShiftedKeys (MAPPED_KEY* asKeys_)
{
    int nMods = 0;
    if (IsPressed(HK_LSHIFT)) nMods |= HM_SHIFT;
    if (IsPressed(HK_LCTRL))  nMods |= HM_CTRL;
    if (IsPressed(HK_LALT))   nMods |= HM_ALT;
    if (IsPressed(HK_RALT))   nMods |= (HM_CTRL|HM_ALT);


    // Have the shift states changed while a combo is in progress?
    if (nComboMods && nComboMods != nMods)
    {
        // If the combo key is still pressed, start the timer running to re-press it as we're about to release it
        if (IsPressed(nComboKey))
            dwComboTime = OSD::GetTime();

        // We're done with the shift state now, so clear it to prevent the time getting reset
        nComboMods = 0;
    }

    // Combo unpress timer active?
    if (dwComboTime)
    {
        // If we're within the threshold, ensure the key remains released
        if ((OSD::GetTime() - dwComboTime) < 250)
            ReleaseKey(nComboKey);

        // Otherwise clear the expired timer
        else
            dwComboTime = 0;
    }


    for (int i = 0 ; asKeys_[i].nChar ; i++)
    {
        // Key and necessary modifiers pressed?
        if (asKeys_[i].nMods && IsPressed(asKeys_[i].nKey) && (asKeys_[i].nMods & nMods) == asKeys_[i].nMods)
        {
            TRACE("%d (%d) pressed with mods %02x (of %02x)\n", asKeys_[i].nKey, asKeys_[i].nChar, asKeys_[i].nMods, nMods);

            // Press the keys required to generate the symbol
            PressSamKey(asKeys_[i].nSamMods);
            PressSamKey(asKeys_[i].nSamKey);

            // Release the main key
            ReleaseKey(asKeys_[i].nKey);

            // Release the modifiers keys and clear the processed modifier bit(s)
            if (asKeys_[i].nMods & HM_SHIFT) { ReleaseKey(HK_LSHIFT); nMods &= ~HM_SHIFT; }
            if (asKeys_[i].nMods & HM_CTRL)  { ReleaseKey(HK_LCTRL); nMods &= ~HM_CTRL; }
            if (asKeys_[i].nMods & HM_ALT)   { ReleaseKey(HK_LALT); ReleaseKey(HK_LCTRL); nMods &= ~(HM_CTRL|HM_ALT); }

            // Remember the combo key details for later release
            nComboKey = asKeys_[i].nKey;
            nComboMods = asKeys_[i].nMods;
        }
    }
}

// Process simple unshifted keys
void ProcessUnshiftedKeys (MAPPED_KEY* asKeys_)
{
    for (int i = 0 ; asKeys_[i].nChar ; i++)
    {
        if (!asKeys_[i].nMods && IsPressed(asKeys_[i].nKey))
        {
            PressSamKey(asKeys_[i].nSamMods);
            PressSamKey(asKeys_[i].nSamKey);
        }
    }
}


// Update a combination key table with a symbol
static bool UpdateKeyTable (MAPPED_KEY* asKeys_, int nKey_, int nMods_, int nChar_)
{
    // Convert upper-case symbols to lower-case without shift
    if (nChar_ >= 'A' && nChar_ <= 'Z')
    {
        nChar_ += 'a'-'A';
        nMods_ &= ~HM_SHIFT;
    }

    // Convert ctrl-letter and ctrl-digit to the base key
    if (nMods_ & HM_CTRL)
    {
        if ((nChar_ >= 'a' && nChar_ <= 'z') || (nChar_ >= '0' && nChar_ <= '9'))
            nMods_ &= ~HM_CTRL;
    }


    for (int i = 0 ; asKeys_[i].nChar ; i++)
    {
        // Is there a mapping entry for the symbol?
        if (asKeys_[i].nChar == nChar_)
        {
            // Log if the mapping is new
            if (!asKeys_[i].nKey)
                TRACE("%d maps to %d with mods of %02x\n", nChar_, nKey_, nMods_);

            // Update the key mapping
            asKeys_[i].nKey = nKey_;
            asKeys_[i].nMods = nMods_;

            return true;
        }
    }

    return false;
}


// Set the state of a native key, remember any character generated from it
void Keyboard::SetKey (int nCode_, bool fPressed_, int nMods_/*=0*/, int nChar_/*=0*/)
{
    // Key released?
    if (!fPressed_)
        ReleaseKey(nCode_);

    // Only perform press processing if the key isn't already down (a repeat)
    else if (!IsPressed(nCode_))
    {
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
