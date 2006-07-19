// Part of SimCoupe - A SAM Coupe emulator
//
// Input.cpp: Win32 mouse and DirectInput keyboard input
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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

// Notes:
//  On startup (or detected keyboard change) two tables are built for the
//  keyboard mappings, to cope with different keyboard positions for both
//  symbols (e.g. US) and letters (e.g. French).

#include "SimCoupe.h"

#include "Display.h"
#include "GUI.h"
#include "Input.h"
#include "IO.h"
#include "Util.h"
#include "Options.h"
#include "Mouse.h"
#include "UI.h"


#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL               0x020a
#define GET_WHEEL_DELTA_WPARAM(w)   ((short)HIWORD(w))
#endif

const unsigned int KEYBOARD_BUFFER_SIZE = 20;

typedef struct
{
    int     nChar;              // Symbol on PC keyboard, or a keyboard scan-code if negative
    eSamKey nKey1, nKey2;       // Up to 2 SAM keys needed to generate the above symbol
    BYTE    bScanCode, bShifts; // Raw scan code and shift states needed (generated at run-time from current keyboard layout)
}
COMBINATION_KEY;

typedef struct
{
    int     nChar;
    BYTE    bScanCode;
}
SIMPLE_KEY;

typedef struct
{
    BYTE  bScanCode;
    eSamKey nSamKey, nSamModifiers;
}
MAPPED_KEY;


LPDIRECTINPUT pdi;
LPDIRECTINPUTDEVICE pdiKeyboard;
LPDIRECTINPUTDEVICE2 pdidJoystick1, pdidJoystick2;

BYTE bComboKey, bComboShifts;
DWORD dwComboTime;


bool fMouseActive, fPurgeKeyboard;
POINT ptMouse;

BYTE abKeyStates[256], abKeys[256];
inline bool IsPressed(BYTE bKey_)   { return (abKeyStates[bKey_] & 0x80) != 0; }
inline void PressKey(BYTE bKey_)    { abKeyStates[bKey_] |= 0x80; }
inline void ReleaseKey(BYTE bKey_)  { abKeyStates[bKey_] &= ~0x80; }
inline void ToggleKey(BYTE bKey_)   { abKeyStates[bKey_] ^= 0x80; }


// Mapping of SAM keyboard matrix positions to single PC keys.  Before use, these are converted to the corresponding
// DirectInput scan-codes for the current input locale, and are updated if the keyboard layout is changed.
SIMPLE_KEY asSamKeys [SK_MAX] =
{
    {-DIK_LSHIFT},  {'Z'},          {'X'},      {'C'},      {'V'},      {-DIK_NUMPAD1}, {-DIK_NUMPAD2}, {-DIK_NUMPAD3},
    {'A'},          {'S'},          {'D'},      {'F'},      {'G'},      {-DIK_NUMPAD4}, {-DIK_NUMPAD5}, {-DIK_NUMPAD6},
    {'Q'},          {'W'},          {'E'},      {'R'},      {'T'},      {-DIK_NUMPAD7}, {-DIK_NUMPAD8}, {-DIK_NUMPAD9},
    {-DIK_1},       {-DIK_2},       {-DIK_3},   {-DIK_4},   {-DIK_5},   {-DIK_ESCAPE},  {-DIK_TAB},     {-DIK_CAPITAL},
    {-DIK_0},       {-DIK_9},       {-DIK_8},   {-DIK_7},   {-DIK_6},   {0},            {0},            {-DIK_BACK},
    {'P'},          {'O'},          {'I'},      {'U'},      {'Y'},      {0},            {0},            {-DIK_NUMPAD0},
    {-DIK_RETURN},  {'L'},          {'K'},      {'J'},      {'H'},      {0},            {0},            {0},
    {-DIK_SPACE},   {-DIK_LCONTROL},{'M'},      {'N'},      {'B'},      {0},            {0},            {-DIK_INSERT},
    {-DIK_RCONTROL},{-DIK_UP},      {-DIK_DOWN},{-DIK_LEFT},{-DIK_RIGHT}
};

// Symbols with SAM keyboard details
COMBINATION_KEY asSamSymbols[] =
{
    { '0', SK_NONE, SK_0 }, { '1', SK_NONE, SK_1 }, { '2', SK_NONE, SK_2 }, { '3', SK_NONE, SK_3 }, { '4', SK_NONE, SK_4 },
    { '5', SK_NONE, SK_5 }, { '6', SK_NONE, SK_6 }, { '7', SK_NONE, SK_7 }, { '8', SK_NONE, SK_8 }, { '9', SK_NONE, SK_9 },

    { '!',  SK_SHIFT, SK_1 },       { '@',  SK_SHIFT, SK_2 },       { '#',  SK_SHIFT, SK_3 },
    { '$',  SK_SHIFT, SK_4 },       { '%',  SK_SHIFT, SK_5 },       { '&',  SK_SHIFT, SK_6 },
    { '\'', SK_SHIFT, SK_7 },       { '(',  SK_SHIFT, SK_8 },       { ')',  SK_SHIFT, SK_9 },
    { '~',  SK_SHIFT, SK_0 },       { '-',  SK_MINUS, SK_NONE },    { '/',  SK_SHIFT, SK_MINUS },
    { '+',  SK_PLUS, SK_NONE },     { '*',  SK_SHIFT, SK_PLUS },    { '<',  SK_SYMBOL, SK_Q },
    { '>',  SK_SYMBOL, SK_W },      { '[',  SK_SYMBOL, SK_R },      { ']',  SK_SYMBOL, SK_T },
    { '=',  SK_EQUALS, SK_NONE },   { '_',  SK_SHIFT, SK_EQUALS },  { '"',  SK_QUOTES, SK_NONE },
    { '`',  SK_SHIFT, SK_QUOTES },  { '{',  SK_SYMBOL, SK_F },      { '}',  SK_SYMBOL, SK_G },
    { '^',  SK_SYMBOL, SK_H },      { (BYTE)'£', SK_SYMBOL, SK_L }, { ';',  SK_SEMICOLON, SK_NONE },
    { ':',  SK_COLON, SK_NONE },    { '?',  SK_SYMBOL, SK_X },      { '.',  SK_PERIOD, SK_NONE },
    { ',',  SK_COMMA, SK_NONE },    { '\\', SK_SHIFT, SK_INV },     { '|',  SK_SYMBOL, SK_9 },

    { '\0' }
};

// Symbols with Spectrum keyboard details
COMBINATION_KEY asSpectrumSymbols[] =
{
    { '0', SK_NONE, SK_0 }, { '1', SK_NONE, SK_1 }, { '2', SK_NONE, SK_2 }, { '3', SK_NONE, SK_3 }, { '4', SK_NONE, SK_4 },
    { '5', SK_NONE, SK_5 }, { '6', SK_NONE, SK_6 }, { '7', SK_NONE, SK_7 }, { '8', SK_NONE, SK_8 }, { '9', SK_NONE, SK_9 },

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

// Handy mappings from unused PC keys to a SAM combination
MAPPED_KEY asSamMappings[] =
{
    // Some useful combinations
    { DIK_DELETE,    SK_DELETE, SK_SHIFT },
    { DIK_HOME,      SK_LEFT,   SK_CONTROL },
    { DIK_END,       SK_RIGHT,  SK_CONTROL },
    { DIK_PRIOR,     SK_F4,     SK_NONE },
    { DIK_NEXT,      SK_F1,     SK_NONE },
    { DIK_NUMLOCK,   SK_EDIT,   SK_SYMBOL },
    { DIK_APPS,      SK_EDIT,   SK_NONE },
    { DIK_DECIMAL,   SK_QUOTES, SK_SHIFT },
    { 0 }
};

// Handy mappings from unused PC keys to a Spectrum combination
MAPPED_KEY asSpectrumMappings[] =
{
    // Some useful combinations
    { DIK_DELETE,    SK_0,      SK_SHIFT },
    { DIK_HOME,      SK_LEFT,   SK_NONE },
    { DIK_END,       SK_RIGHT,  SK_NONE },
    { DIK_PRIOR,     SK_UP,     SK_NONE },
    { DIK_NEXT,      SK_DOWN,   SK_NONE },
    { DIK_CAPITAL,   SK_2,      SK_SHIFT },
    { 0 }
};

////////////////////////////////////////////////////////////////////////////////

static bool InitKeyboard ();
static bool InitJoysticks ();
static void PrepareKeyTable (COMBINATION_KEY* asKeys_);
static void PrepareKeyTable (SIMPLE_KEY* asKeys_);


bool Input::Init (bool fFirstInit_/*=false*/)
{
    bool fRet = false;

    Exit(true);
    TRACE("-> Input::Init(%s)\n", fFirstInit_ ? "first" : "");

    // If we can find DirectInput 5.0 we can have joystick support, otherwise fall back on 3.0 support for NT4
    if (fRet = SUCCEEDED(pfnDirectInputCreate(__hinstance, DIRECTINPUT_VERSION, &pdi, NULL)))
        InitJoysticks();
    else
        fRet = SUCCEEDED(pfnDirectInputCreate(__hinstance, 0x0300, &pdi, NULL));

    if (fRet)
    {
        InitKeyboard();
        fMouseActive = false;
        Mouse::Init(fFirstInit_);
        Purge();
    }

    TRACE("<- Input::Init() returning %s\n", fRet ? "true" : "false");
    return fRet;
}


void Input::Exit (bool fReInit_/*=false*/)
{
    TRACE("-> Input::Exit(%s)\n", fReInit_ ? "reinit" : "");

    if (pdiKeyboard) { pdiKeyboard->Unacquire(); pdiKeyboard->Release(); pdiKeyboard = NULL; }

    if (pdidJoystick1) { pdidJoystick1->Unacquire(); pdidJoystick1->Release(); pdidJoystick1 = NULL; }
    if (pdidJoystick2) { pdidJoystick2->Unacquire(); pdidJoystick2->Release(); pdidJoystick2 = NULL; }

    if (pdi) { pdi->Release(); pdi = NULL; }

    Mouse::Exit(fReInit_);

    TRACE("<- Input::Exit()\n");
}


bool InitKeyboard ()
{
    PrepareKeyTable(asSamSymbols);
    PrepareKeyTable(asSpectrumSymbols);
    PrepareKeyTable(asSamKeys);

    HRESULT hr;
    if (FAILED(hr = pdi->CreateDevice(GUID_SysKeyboard, &pdiKeyboard, NULL)))
        TRACE("!!! Failed to create keyboard device (%#08lx)\n", hr);
    else if (FAILED(hr = pdiKeyboard->SetCooperativeLevel(g_hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND)))
        TRACE("!!! Failed to set cooperative level of keyboard device (%#08lx)\n", hr);
    else if (FAILED(hr = pdiKeyboard->SetDataFormat(&c_dfDIKeyboard)))
        TRACE("!!! Failed to set data format of keyboard device (%#08lx)\n", hr);
    else
    {
        DIPROPDWORD dipdw =
        {
            {
                sizeof DIPROPDWORD,     // diph.dwSize
                sizeof DIPROPHEADER,    // diph.dwHeaderSize
                0,                      // diph.dwObj
                DIPH_DEVICE,            // diph.dwHow
            },
            KEYBOARD_BUFFER_SIZE,       // dwData
        };

        if (FAILED(hr = pdiKeyboard->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph)))
            TRACE("!!! Failed to set keyboard buffer size\n", hr);

        return true;
    }

    return false;
}


BOOL CALLBACK EnumJoystickProc (LPCDIDEVICEINSTANCE pdiDevice_, LPVOID lpv_)
{
    HRESULT hr;

    LPDIRECTINPUTDEVICE pdiJoystick;
    if (FAILED(hr = pdi->CreateDevice(pdiDevice_->guidInstance, &pdiJoystick, NULL)))
        TRACE("!!! Failed to create joystick device (%#08lx)\n", hr);
    else
    {
        DIDEVICEINSTANCE didi = { sizeof didi };
        strcpy(didi.tszInstanceName, "<unknown>");  // WINE fix for missing implementation

        if (FAILED(hr = pdiJoystick->GetDeviceInfo(&didi)))
            TRACE("!!! Failed to get joystick device info (%#08lx)\n", hr);

        // Overloaded use - if custom data was supplied, it's a combo box ID to add the string to
        else if (lpv_)
            SendMessage(reinterpret_cast<HWND>(lpv_), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(didi.tszInstanceName));
        else
        {
            IDirectInputDevice2* pDevice;

            // We need an IDirectInputDevice2 interface for polling, so query for it
            if (FAILED(hr = pdiJoystick->QueryInterface(IID_IDirectInputDevice2, reinterpret_cast<void **>(&pDevice))))
                TRACE("!!! Failed to query joystick for IID_IDirectInputDevice2 (%#08lx)\n", hr);

            // If the device name matches the joystick 1 device name, save a pointer to it
            else if (!lstrcmpi(didi.tszInstanceName, GetOption(joydev1)))
                pdidJoystick1 = pDevice;

            // If the device name matches the joystick 2 device name, save a pointer to it
            else if (!lstrcmpi(didi.tszInstanceName, GetOption(joydev2)))
                pdidJoystick2 = pDevice;

            // No match
            else
                pDevice->Release();

            pdiJoystick->Release();
        }
    }

    // Continue looking for other devices, even tho we failed with the current one
    return DIENUM_CONTINUE;
}


bool InitJoystick (LPDIRECTINPUTDEVICE2 pJoystick_, int nTolerance_)
{
    HRESULT hr;

    // Prepare the tolerance structure
    DIPROPDWORD diPdw;
    ZeroMemory(&diPdw, sizeof diPdw);
    diPdw.diph.dwSize       = sizeof diPdw;
    diPdw.diph.dwHeaderSize = sizeof diPdw.diph;
    diPdw.diph.dwHow        = DIPH_BYOFFSET;
    diPdw.dwData            = 10000UL * nTolerance_ / 100UL;

    // Prepare the axis range structure
    DIPROPRANGE diPrg;
    ZeroMemory(&diPrg, sizeof diPrg);
    diPrg.diph.dwSize       = sizeof diPrg;
    diPrg.diph.dwHeaderSize = sizeof diPrg.diph;
    diPrg.diph.dwHow        = DIPH_BYOFFSET;
    diPrg.lMin              = -(diPrg.lMax = 100);  // Range -100 to 100 - convenient for tolerance percentage


    // Set the joystick device to use the joystick format
    if (FAILED(hr = pJoystick_->SetDataFormat(&c_dfDIJoystick)))
        TRACE("!!! Failed to set data format of joystick device (%#08lx)\n", hr);
    else if (FAILED(hr = pJoystick_->SetCooperativeLevel(g_hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND)))
        TRACE("!!! Failed to set cooperative level of joystick device (%#08lx)\n", hr);
    else
    {
        // Set the actual device properties
        diPdw.diph.dwObj = diPrg.diph.dwObj = DIJOFS_X;
        if (FAILED(hr = pJoystick_->SetProperty(DIPROP_DEADZONE, &diPdw.diph)))
            TRACE("!!! Failed to set joystick 1 X-axis deadzone (%#08lx)\n", hr);
        else if (FAILED(hr = pJoystick_->SetProperty(DIPROP_RANGE, &diPrg.diph)))
            TRACE("!!! Failed to set X-axis range (%#08lx)\n", hr);
        else
        {
            diPdw.diph.dwObj = diPrg.diph.dwObj = DIJOFS_Y;
            if (FAILED(hr = pJoystick_->SetProperty(DIPROP_DEADZONE, &diPdw.diph)))
                TRACE("!!! Failed to set joystick 1 Y-axis deadzone (%#08lx)\n", hr);
            else if (FAILED(hr = pJoystick_->SetProperty(DIPROP_RANGE, &diPrg.diph)))
                TRACE("!!! Failed to set Y-axis range (%#08lx)\n", hr);
            else
                return true;
        }
    }

    return false;
}


bool InitJoysticks ()
{
    HRESULT hr;

    // Enumerate the joystick devices
    if (FAILED(hr = pdi->EnumDevices(DIDEVTYPE_JOYSTICK, EnumJoystickProc, NULL, DIEDFL_ATTACHEDONLY)))
        TRACE("!!! Failed to enumerate joystick devices (%#08lx)\n", hr);

    // Initialise joystick 1
    if (pdidJoystick1 && !InitJoystick(pdidJoystick1, GetOption(deadzone1)))
    {
        pdidJoystick1->Release();
        pdidJoystick1 = NULL;
    }

    // Initialise joystick 2
    if (pdidJoystick2 && !InitJoystick(pdidJoystick2, GetOption(deadzone2)))
    {
        pdidJoystick2->Release();
        pdidJoystick2 = NULL;
    }

    return true;
}


// Return whether the emulation is using the mouse
bool Input::IsMouseAcquired ()
{
    return fMouseActive;
}


void Input::Acquire (bool fMouse_/*=true*/, bool fKeyboard_/*=true*/)
{
    // Only acquire the mouse if the SAM mouse is enabled
    fMouse_ &= GetOption(mouse);

    // If the mouse is being acquired, move it to the centre of the screen
    if (fMouseActive != fMouse_ && (fMouseActive = fMouse_))
    {
        RECT r;

        // Calculate the central position in the client screen
        GetClientRect(g_hwnd, &r);
        ptMouse.x = r.right/2;
        ptMouse.y = r.bottom/2;

        // Move the cursor there, and store the position for later comparison
        ClientToScreen(g_hwnd, &ptMouse);
        SetCursorPos(ptMouse.x, ptMouse.y);

        // Restrict the cursor to the client area
        MapWindowPoints(g_hwnd, NULL, reinterpret_cast<POINT*>(&r.left), 2);
        ClipCursor(&r);
    }

    // If the mouse isn't active, ensure it's free to move anywhere
    if (!fMouse_)
        ClipCursor(NULL);

    // Flush out any buffered data
    Purge();
}

void Input::Purge (bool fMouse_/*=true*/, bool fKeyboard_/*=true*/)
{
    if (fKeyboard_)
    {
        ZeroMemory(abKeyStates, sizeof abKeyStates);
        ZeroMemory(abKeys, sizeof abKeys);
        ReleaseAllSamKeys();

        fPurgeKeyboard = true;
    }

    if (fMouse_)
    {
        MSG msg;
        while (PeekMessage(&msg, NULL, WM_MOUSEMOVE, WM_MOUSEMOVE, PM_REMOVE));
    }
}


bool ReadKeyboard ()
{
    bool fRet = false;

    if (!pdiKeyboard || GUI::IsActive())
    {
        Input::Purge(true, false);
        return false;
    }

    HRESULT hr;
    if (SUCCEEDED(hr = pdiKeyboard->Acquire()))
    {
        DWORD dwItems = 0U-1;

        // Now we've acquired the keyboard, we can purge it if flagged to do so
        if (fPurgeKeyboard)
        {
            pdiKeyboard->GetDeviceData(sizeof DIDEVICEOBJECTDATA, NULL, &dwItems, 0);
            fPurgeKeyboard = false;
        }

        // Remove and process key events one at a time for now - better for slow PCs
        dwItems = 1;

        DIDEVICEOBJECTDATA abEvents[KEYBOARD_BUFFER_SIZE];
        if (SUCCEEDED(hr = pdiKeyboard->GetDeviceData(sizeof DIDEVICEOBJECTDATA, abEvents, &dwItems, 0)))
        {
            // Process any item(s) we got back
            for (DWORD i = 0 ; i < dwItems ; i++)
            {
                BYTE bPressed = abKeys[abEvents[i].dwOfs] = static_cast<BYTE>(abEvents[i].dwData & 0x80);
                TRACE("%ld %s\n", abEvents[i].dwOfs, bPressed ? "pressed" : "released");
            }

            // Make a copy of the master key table with the real keyboard states
            memcpy(abKeyStates, abKeys, sizeof abKeyStates);
            fRet = true;
        }
    }

    // Ensure certain Windows Alt-key combinations don't make it through to the emulation
    if (IsPressed(DIK_LMENU))
    {
        int anIgnore[] = { DIK_TAB, DIK_ESCAPE, DIK_SPACE, DIK_RETURN };

        for (int i = 0 ; i < sizeof(anIgnore)/sizeof(anIgnore[0]) ; i++)
        {
            if (IsPressed(anIgnore[i]))
            {
                ReleaseKey(DIK_LMENU);
                ReleaseKey(anIgnore[i]);
            }
        }
    }

    // Left-Alt can optionally be used as the SAM Cntrl key
    if (GetOption(altforcntrl) && IsPressed(DIK_LMENU))
    {
        ReleaseKey(DIK_LMENU);
        PressKey(DIK_RCONTROL);
    }

    // The Windows keys can be used with the regular function keys for the SAM keypad
    if (GetOption(samfkeys) != (IsPressed(DIK_LWIN) || IsPressed(DIK_RWIN)))
    {
        for (int i = DIK_F1 ; i <= DIK_F10 ; i++)
        {
            if (IsPressed(i))
            {
                // Release the function key
                ReleaseKey(i);
                ReleaseKey(DIK_LWIN);
                ReleaseKey(DIK_RWIN);

                if (i == DIK_F10)
                    PressKey(DIK_NUMPAD0);
                else
                {
                    static const int anFn[] = { DIK_NUMPAD1, DIK_NUMPAD2, DIK_NUMPAD3, DIK_NUMPAD4, DIK_NUMPAD5,
                                                DIK_NUMPAD6, DIK_NUMPAD7, DIK_NUMPAD8, DIK_NUMPAD9 };
                    PressKey(anFn[i-DIK_F1]);
                }

                break;
            }
        }
    }

    // AltGr can optionally be used for SAM Edit
    if (GetOption(altgrforedit) && IsPressed(DIK_RMENU))
    {
        // Release AltGr and press the context menu key (also used for SAM Edit)
        ReleaseKey(DIK_RMENU);
        PressKey(DIK_APPS);

        // Also release Ctrl and Alt, which is how AltGr often behaves
        ReleaseKey(DIK_LCONTROL);
        ReleaseKey(DIK_LMENU);
    }

    return true;
}

void ReadJoystick (LPDIRECTINPUTDEVICE2 pDevice_, int nBaseKey_)
{
    HRESULT hr;

    DIJOYSTATE dijs;
    ZeroMemory(&dijs, sizeof dijs);

    if (pDevice_ && (FAILED(hr = pDevice_->Poll()) || FAILED(hr = pDevice_->GetDeviceState(sizeof dijs, &dijs))))
    {
        if (SUCCEEDED(hr = pDevice_->Acquire()) && (FAILED(hr = pDevice_->Poll()) ||
            FAILED(hr = pDevice_->GetDeviceState(sizeof dijs, &dijs))))
            TRACE("!!! Failed to read joystick state! (%#08lx)\n", hr);
    }

    // Combine the states of all buttons so any button works as fire
    BYTE bButtons = 0;
    for (int i = 0 ; i < (sizeof dijs.rgbButtons / sizeof dijs.rgbButtons[0]) ; i++)
        bButtons |= dijs.rgbButtons[i];

    // Simulate the PC key presses for the joystick movement and button(s?)
    if (dijs.lX < 0) PressKey(nBaseKey_ + 0);       // Left
    if (dijs.lX > 0) PressKey(nBaseKey_ + 1);       // Right
    if (dijs.lY > 0) PressKey(nBaseKey_ + 2);       // Down
    if (dijs.lY < 0) PressKey(nBaseKey_ + 3);       // Up
    if (bButtons & 0x80) PressKey(nBaseKey_ + 4);   // Fire
}


// Prepare the more complicated combination keys that are fairly keyboard specific
void PrepareKeyTable (SIMPLE_KEY* asKeys_)
{
    HKL hkl = GetKeyboardLayout(0);

    for (int i = 0 ; i < SK_MAX ; i++)
    {
        if (asKeys_[i].nChar)
            asKeys_[i].bScanCode = asKeys_[i].nChar > 0 ? MapVirtualKeyEx(asKeys_[i].nChar, 0, hkl) : -asKeys_[i].nChar;
    }
}

// Prepare the more complicated combination keys that are fairly keyboard specific
void PrepareKeyTable (COMBINATION_KEY* asKeys_)
{
    // Fetch the keyboard layout for the current thread
    HKL hkl = GetKeyboardLayout(0);

    for (int i = 0 ; asKeys_[i].nChar ; i++)
    {
        // Convert the symbol to a virtual key sequence, and then to a keyboard scan-code (with shifted keys' states)
        WORD wVirtual = VkKeyScanEx(asKeys_[i].nChar, hkl);
        asKeys_[i].bScanCode = MapVirtualKeyEx(wVirtual & 0xff, 0, hkl);
        asKeys_[i].bShifts = (wVirtual >> 8) & 7;
    }
}


// Process simple key presses
void ProcessKeyTable (SIMPLE_KEY* asKeys_)
{
    // Build the rest of the SAM matrix from the simple non-symbol PC keys
    for (int i = 0 ; i < SK_MAX ; i++)
    {
        if (asKeys_[i].bScanCode && IsPressed(asKeys_[i].bScanCode))
            PressSamKey(i);
    }
}

// Process the additional keys mapped from PC to SAM, ignoring shift state
void ProcessKeyTable (MAPPED_KEY* asKeys_)
{
    // Build the rest of the SAM matrix from the simple non-symbol PC keys
    for (int i = 0 ; asKeys_[i].bScanCode ; i++)
    {
        if (IsPressed(asKeys_[i].bScanCode))
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
    if (IsPressed(DIK_LSHIFT)) bShifts |= 1;
    if (IsPressed(DIK_LCONTROL)) bShifts |= 2;
    if (IsPressed(DIK_LMENU)) bShifts |= 4;
    if (IsPressed(DIK_RMENU)) bShifts |= (2|4);


    // Have the shift states changed while a combo is in progress?
    if (bComboShifts && bComboShifts != bShifts)
    {
        // If the combo key is still pressed, start the timer running to re-press it as we're about to release it
        if (IsPressed(bComboKey))
            dwComboTime = OSD::GetTime();

        // We're done with the shift state now, so clear it to prevent the time getting reset
        bComboShifts = 0;
    }

    // Combo unpress timer active?
    if (dwComboTime)
    {
        // If we're within the threshold, ensure the key remains released
        if ((OSD::GetTime() - dwComboTime) < 250)
            ReleaseKey(bComboKey);

        // Otherwise clear the expired timer
        else
            dwComboTime = 0;
    }


    for (int i = 0 ; asKeys_[i].nChar ; i++)
    {
        if (IsPressed(asKeys_[i].bScanCode) && asKeys_[i].bShifts == bShifts)
        {
            // Release the PC keys used for the key combination
            ReleaseKey(asKeys_[i].bScanCode);
            if (bShifts & 1) ToggleKey(DIK_LSHIFT);
            if (bShifts & 2) ToggleKey(DIK_LCONTROL);
            if (bShifts & 4) { ToggleKey(DIK_LMENU); ReleaseKey(DIK_RCONTROL); }

            // Press 1 or 2 SAM keys to generate the symbol
            PressSamKey(asKeys_[i].nKey1);
            PressSamKey(asKeys_[i].nKey2);

            // Remember the key involved with the shifted state for a combo
            bComboKey = asKeys_[i].bScanCode;
            bComboShifts = bShifts;
        }
    }
}


// Build the SAM keyboard matrix from the current PC state
void SetSamKeyState ()
{
    // No SAM keys are pressed initially
    ReleaseAllSamKeys();

    // Left and right shift keys are equivalent, and also complementary!
    bool fShiftToggle = IsPressed(DIK_LSHIFT) && IsPressed(DIK_RSHIFT);
    abKeyStates[DIK_LSHIFT] |= abKeyStates[DIK_RSHIFT];

    // Process the key combinations required for the mode we're in
    switch (GetOption(keymapping))
    {
        case 1: // SAM keys
            ProcessKeyTable(asSamSymbols);
            ProcessKeyTable(asSamMappings);
            break;

        case 2: // Spectrum mappings
            ProcessKeyTable(asSpectrumSymbols);
            ProcessKeyTable(asSpectrumMappings);
            break;
    }

    // Toggle shift if both shift keys are down to allow shifted versions of keys that are
    // shifted on the PC but unshifted on the SAM
    if (fShiftToggle)
        ToggleKey(DIK_LSHIFT);

    // Process the simple key mappings
    ProcessKeyTable(asSamKeys);
}


void Input::Update ()
{
    // Read keyboard and mouse
    ReadKeyboard();

    // Read the joysticks, if present
    if (pdidJoystick1) ReadJoystick(pdidJoystick1, DIK_6);
    if (pdidJoystick2) ReadJoystick(pdidJoystick2, DIK_1);

    // Update the SAM keyboard matrix from the current key state (including joystick movement)
    SetSamKeyState();
}


// Send a mouse message to the GUI, after mapping the mouse position to the SAM screen
bool SendGuiMouseMessage (int nMessage_, LPARAM lParam_)
{
    // Extract the cursor position from the mouse message parameter
    POINT pt = { GET_X_LPARAM(lParam_), GET_Y_LPARAM(lParam_) };

    // Map from display position to SAM screen position
    int nX = pt.x, nY = pt.y;
    Display::DisplayToSamPoint(&nX, &nY);

    // Finally, send the message now we have the appropriate position
    return GUI::SendMessage(nMessage_, nX, nY);
}

bool Input::FilterMessage (HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    // Mouse button decoder table, from the lower 3 bits of the Windows message values
    static int anMouseButtons[] = { 2, 1, 1, 0, 3, 3, 0, 2 };

    switch (uMsg_)
    {
        case WM_MOUSEMOVE:
        {
            int x = GET_X_LPARAM(lParam_), y = GET_Y_LPARAM(lParam_);

            RECT r;
            GetClientRect(hwnd_, &r);

            // Restrict the mouse position to the window edges
            x = (x < 0) ? 0 : (x >= r.right) ? r.right-1 : x;
            y = (y < 0) ? 0 : (y >= r.bottom) ? r.bottom-1 : y;

            // If the GUI is active, pass the message to it
            if (GUI::IsActive())
            {
                SendGuiMouseMessage(GM_MOUSEMOVE, (y << 16) | x);
                break;
            }

            // Otherwise the SAM mouse must be active for it to be of interest
            else if (!fMouseActive)
                break;


            // Work out the relative movement since last time
            POINT ptCursor;
            GetCursorPos(&ptCursor);
            int nX = ptCursor.x - ptMouse.x, nY = ptCursor.y - ptMouse.y;
            ptMouse = ptCursor;

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

                // How far is the SAM mouse movement in native units?
                Display::SamToDisplaySize(&nX, &nY);

                // Subtract the used portion of the movement, and leave the remainder for next time
                nXX -= nX, nYY -= nY;

                // Move the mouse back to the centre to stop it escaping
                ptMouse.x = r.right/2;
                ptMouse.y = r.bottom/2;
                ClientToScreen(hwnd_, &ptMouse);
                SetCursorPos(ptMouse.x, ptMouse.y);
            }
            break;
        }

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        {
            // The GUI always gets first chance to process the message
            if (GUI::IsActive())
            {
                SendGuiMouseMessage(GM_BUTTONDOWN, lParam_);
                SetCapture(hwnd_);
            }

            // If the mouse is already active, pass on button presses
            else if (fMouseActive)
                Mouse::SetButton(anMouseButtons[uMsg_ & 0x7], true);

            break;
        }

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        {
            // If we captured to catch the release, release it now
            if (GetCapture() == hwnd_)
                ReleaseCapture();

            // The GUI always gets first chance to process the message
            if (GUI::IsActive())
                SendGuiMouseMessage(GM_BUTTONUP, lParam_);

            // Pass the button release through to the mouse module
            else if (fMouseActive)
                Mouse::SetButton(anMouseButtons[uMsg_ & 0x7], false);

            break;
        }

        case WM_MOUSEWHEEL:
            if (GUI::IsActive())
                GUI::SendMessage(GM_MOUSEWHEEL, (GET_WHEEL_DELTA_WPARAM(wParam_) < 0) ? 1 : -1);
            else
                PressKey((GET_WHEEL_DELTA_WPARAM(wParam_) > 0) ? DIK_DOWN : DIK_UP);
            break;

        case WM_CHAR:
        {
            // Get the current shift states
            int nMods = ((GetKeyState(VK_SHIFT)   < 0) ? GKMOD_SHIFT : 0) |
                        ((GetKeyState(VK_CONTROL) < 0) ? GKMOD_CTRL : 0);

            // Pass the key-press to the GUI, and hide the cursor if it was accepted
            if (GUI::SendMessage(GM_CHAR, static_cast<int>(wParam_), nMods))
            {
                SetCursor(NULL);
                return true;
            }
            break;
        }

        case WM_KEYDOWN:
        {
            // Ignore key repeats for held non-GUI keys
            if (!GUI::IsActive())
                return (lParam_ & 0x40000000) != 0;

            // Get the current shift states
            int nMods = ((GetKeyState(VK_SHIFT)   < 0) ? GKMOD_SHIFT : 0) |
                        ((GetKeyState(VK_CONTROL) < 0) ? GKMOD_CTRL : 0);

            // Map any special keys to the GUI equivalents
            if (wParam_ >= VK_NUMPAD0 && wParam_ <= VK_NUMPAD9)
                GUI::SendMessage(GM_CHAR, GK_KP0+static_cast<int>(wParam_)-VK_NUMPAD0, nMods);
            else if (wParam_ >= '0' && wParam_ <= '9' && (nMods & GKMOD_CTRL))
                GUI::SendMessage(GM_CHAR, GK_CTRL_0+static_cast<int>(wParam_)-'0', nMods);
            else if (wParam_ >= VK_LEFT && wParam_ <= VK_DOWN)
            {
                int anCursors[] = { GK_LEFT, GK_UP, GK_RIGHT, GK_DOWN };
                GUI::SendMessage(GM_CHAR, anCursors[wParam_-VK_LEFT], nMods);
            }
            else if (wParam_ >= VK_PRIOR && wParam_ <= VK_HOME)
            {
                int anMovement[] = { GK_PAGEUP, GK_PAGEDOWN, GK_END, GK_HOME };
                GUI::SendMessage(GM_CHAR, anMovement[wParam_-VK_PRIOR], nMods);
            }

            return false;
        }
    }

    // Message not processed
    return false;
}


// Fill the supplied combo-box with the list of connected joysticks
void Input::FillJoystickCombo (HWND hwndCombo_)
{
    HRESULT hr;
    if (FAILED(hr = pdi->EnumDevices(DIDEVTYPE_JOYSTICK, EnumJoystickProc, reinterpret_cast<LPVOID>(hwndCombo_), DIEDFL_ATTACHEDONLY)))
        TRACE("!!! Failed to enumerate joystick devices (%#08lx)\n", hr);
}
