// Part of SimCoupe - A SAM Coupe emulator
//
// Input.cpp: WinCE keyboard, pad and SIP input
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

#include "SimCoupe.h"
#include "resource.h"

#include "Action.h"
#include "CPU.h"
#include "Display.h"
#include "Input.h"
#include "IO.h"
#include "Mouse.h"
#include "Options.h"
#include "UI.h"
#include "Video.h"

//GXKeyList g_gxkl;
HWND g_hwndSIP;
int nSipKey = SK_NONE, nSipMods;
bool fJoySip, fSipDirty;
int actToggleJoySip = -2;

// These are the common virtual key codes, but do some devices differ?
enum { VK_BUTTON1 = 0xc1, VK_BUTTON2, VK_BUTTON3, VK_BUTTON4 };

int anHwKeys[] = { VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_RETURN, VK_BUTTON1, VK_BUTTON2, VK_BUTTON3, VK_BUTTON4 };
int anKeyMap[sizeof(anHwKeys)/sizeof(anHwKeys[0])];

int nComboKey;
short nComboModifiers;
DWORD dwComboTime;

bool afKeyStates[256], afKeys[256];
inline bool IsPressed(int nKey_)    { return afKeyStates[nKey_]; }
inline void PressKey(int nKey_)     { afKeyStates[nKey_] = true; }
inline void ReleaseKey(int nKey_)   { afKeyStates[nKey_] = false; }
inline void ToggleKey(int nKey_)    { afKeyStates[nKey_] = !afKeyStates[nKey_]; }
inline void SetMasterKey(int nKey_, bool fHeld_=true) { afKeys[nKey_] = fHeld_; }


#undef MOD_SHIFT
enum { MOD_NONE=0, MOD_SHIFT=1, MOD_LCTRL=2, MOD_LALT=4, MOD_RALT=8 };
enum { SMOD_NONE=0, SMOD_SHIFT=1, SMOD_SYMBOL=2, SMOD_CONTROL=4, SMOD_EDIT=8 };


typedef struct
{
    int nChar;                  // Symbol character (if any)
    eSamKey nSamKey, nSamMods;  // Up to 2 SAM keys needed to generate the above symbol
    int nKey, nMods;            // virtual key and modifiers
}
COMBINATION_KEY;

typedef struct
{
    int nChar;
    int nKey;
}
SIMPLE_KEY;

typedef struct
{
    int nKey;
    eSamKey nSamKey, nSamMods;
}
MAPPED_KEY;


SIMPLE_KEY asSamKeys [SK_MAX] =
{
    {0,VK_SHIFT},   {'z'},          {'x'},        {'c'},        {'v'},        {0,VK_NUMPAD1}, {0,VK_NUMPAD2}, {0,VK_NUMPAD3},
    {'a'},          {'s'},          {'d'},        {'f'},        {'g'},        {0,VK_NUMPAD4}, {0,VK_NUMPAD5}, {0,VK_NUMPAD6},
    {'q'},          {'w'},          {'e'},        {'r'},        {'t'},        {0,VK_NUMPAD7}, {0,VK_NUMPAD8}, {0,VK_NUMPAD9},
    {'1'},          {'2'},          {'3'},        {'4'},        {'5'},        {0,VK_ESCAPE},  {0,VK_TAB},     {0,VK_CAPITAL},
    {'0'},          {'9'},          {'8'},        {'7'},        {'6'},        {0},            {0},            {0,VK_BACK},
    {'p'},          {'o'},          {'i'},        {'u'},        {'y'},        {0},            {0},            {0,VK_NUMPAD0},
    {0,VK_RETURN},  {'l'},          {'k'},        {'j'},        {'h'},        {0},            {0},            {0},
    {' '},          {0,VK_LCONTROL},{'m'},        {'n'},        {'b'},        {0},            {0},            {0,VK_INSERT},
    {0,VK_RCONTROL},{0,VK_UP},      {0,VK_DOWN},  {0,VK_LEFT},  {0,VK_RIGHT}
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
    { '^',  SK_SYMBOL, SK_H },      { 163/*£*/,  SK_SYMBOL, SK_L }, { ';',  SK_SEMICOLON, SK_NONE },
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
    { ':',  SK_SYMBOL, SK_Z },  { 163/*£*/,  SK_SYMBOL, SK_X },  { '?',  SK_SYMBOL, SK_C },
    { '/',  SK_SYMBOL, SK_V },  { '*',  SK_SYMBOL, SK_B },       { ',',  SK_SYMBOL, SK_N },
    { '.',  SK_SYMBOL, SK_M },  { '\b', SK_SHIFT,  SK_0 },

    { '\0', SK_NONE,    SK_NONE,    0 }
};


// Handy mappings from unused PC keys to a SAM combination
MAPPED_KEY asSamMappings[] =
{
    // Some useful combinations
    { VK_DELETE,    SK_DELETE, SK_SHIFT },
    { VK_HOME,      SK_LEFT,   SK_CONTROL },
    { VK_END,       SK_RIGHT,  SK_CONTROL },
    { VK_PRIOR,     SK_F4,     SK_NONE },
    { VK_NEXT,      SK_F1,     SK_NONE },
    { VK_NUMLOCK,   SK_EDIT,   SK_SYMBOL },
    { VK_APPS,      SK_EDIT,   SK_NONE },
    { VK_DECIMAL,   SK_QUOTES, SK_SHIFT },
    { 0 }
};

// Handy mappings from unused PC keys to a Spectrum combination
MAPPED_KEY asSpectrumMappings[] =
{
    // Some useful combinations
    { VK_DELETE,   SK_0,      SK_SHIFT },
    { VK_HOME,     SK_LEFT,   SK_NONE },
    { VK_END,      SK_RIGHT,  SK_NONE },
    { VK_PRIOR,    SK_UP,     SK_NONE },
    { VK_NEXT,     SK_DOWN,   SK_NONE },
    { VK_CAPITAL,  SK_2,      SK_SHIFT },
    { 0 }
};


typedef struct
{
    int     nKey;
    int     x,y;
    int     w,h;
}
KEYAREA;

// SAM key code and position on the SIP
KEYAREA asKeyAreas[] =
{
    // Normal square keys, each 11x11
    { SK_ESCAPE, 1,1 }, { SK_1, 13,1 }, { SK_2, 25,1 }, { SK_3, 37,1 },
    { SK_4, 49,1 }, { SK_5, 61,1 }, { SK_6, 73,1 }, { SK_7, 85,1 },
    { SK_8, 97,1 }, { SK_9, 109,1 }, { SK_0, 121,1 }, { SK_MINUS, 133,1 },
    { SK_PLUS, 145,1 }, { SK_F7, 173,1 }, { SK_F8, 185,1 }, { SK_F9, 197,1 },
    { SK_Q, 17,13 }, { SK_W, 29,13 }, { SK_E, 41,13 }, { SK_R, 53,13 },
    { SK_T, 65,13 }, { SK_Y, 77,13 }, { SK_U, 89,13 }, { SK_I, 101,13 },
    { SK_O, 113,13 }, { SK_P, 125,13 }, { SK_EQUALS, 137,13 }, { SK_QUOTES, 149,13 },
    { SK_F4, 173,13 }, { SK_F5, 185,13 }, { SK_F6, 197,13 }, { SK_A, 21,25 },
    { SK_S, 33,25 }, { SK_D, 45,25 }, { SK_F, 57,25 }, { SK_G, 69,25 },
    { SK_H, 81,25 }, { SK_J, 93,25 }, { SK_K, 105,25 }, { SK_L, 117,25 },
    { SK_SEMICOLON, 129,25 }, { SK_COLON, 141,25 }, { SK_F1, 173,25 },
    { SK_F2, 185,25 }, { SK_F3, 197,25 }, { SK_Z, 27,37 }, { SK_X, 39,37 },
    { SK_C, 51,37 }, { SK_V, 63,37 }, { SK_B, 75,37 }, { SK_N, 87,37 },
    { SK_M, 99,37 }, { SK_COMMA, 111,37 }, { SK_PERIOD, 123,37 }, { SK_INV, 135,37 },
    { SK_F0, 173,37 }, { SK_UP, 185,37 }, { SK_PERIOD, 197,37 }, { SK_LEFT, 173,49 },
    { SK_DOWN, 185,49 }, { SK_RIGHT, 197,49 },

    // Areas requiring custom width and height (mostly the grey SAM keys)
    { SK_DELETE, 157,1, 15,11 }, { SK_TAB, 1,13, 15,11 }, { SK_RETURN, 161,13, 11,12 },
    { SK_RETURN, 153,25, 19,11 }, { SK_CAPS, 1,25, 19,11 }, { SK_SHIFT, 1,37, 25,11 },
    { SK_SHIFT, 147,37, 25,11 }, { SK_SYMBOL, 1,49, 20,11 }, { SK_CONTROL, 22,49, 16,11 },
    { SK_SPACE, 39,49, 95,11 }, { SK_EDIT, 135,49, 16,11 }, { SK_SYMBOL, 152,49, 20,11 },

    // Function keys
    { -1, 213,1 }, { -2, 225,1 }, { -3, 213,13 }, { -4, 225,13 }, { -5, 213,25 },
    { -6, 225,25 },{ -7, 213,37 }, { -8, 225,37 }, { -9, 213,49 }, { -10, 225,49 },

    // End marker
    { SK_NONE }
};

// On-screen joystick
KEYAREA asJoyAreas[] =
{
    { SK_6, 1,1, 33,29 },  { SK_7, 35,1, 33,29 },  { SK_9, 69,1, 33,29 },  { SK_8, 103,1, 33,29 },  { SK_0, 137,1,  71,29 },
    { SK_1, 1,31, 33,29 }, { SK_2, 35,31, 33,29 }, { SK_4, 69,31, 33,29 }, { SK_3, 103,31, 33,29 }, { SK_5, 137,31, 71,29 },

    // Function keys
    { -1, 213,1 }, { -2, 225,1 }, { -3, 213,13 }, { -4, 225,13 }, { -5, 213,25 },
    { -6, 225,25 },{ -7, 213,37 }, { -8, 225,37 }, { -9, 213,49 }, { -10, 225,49 },

    { SK_NONE }
};

// Actions for each of the 10 function buttons, in the 3 shifted states: normal, shift and symbol
static int anActions[][10] =
{
    { actInsertFloppy1, actInsertFloppy2, actOptions, actToggleJoySip, actPause, actTempTurbo, -1, actAbout, actResetButton, actExitApplication },
    { actEjectFloppy1, actEjectFloppy2, -1, -1, -1, -1, -1, -1, actNmiButton, actMinimise },
    { actSaveFloppy1, actSaveFloppy2, -1, -1, -1, -1, -1, -1, -1, -1 }
};


// Prototypes of functions below
HWND CreateSIP();
long CALLBACK SIPWndProc (HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_);

////////////////////////////////////////////////////////////////////////////////

bool Input::Init (bool fFirstInit_/*=false*/)
{
    if (fFirstInit_)
    {
        // Create the SIP
        g_hwndSIP = CreateSIP();

        // Grab control of all the buttons
        GXOpenInput();
    }

    // Keymap defined?
    if (*GetOption(keymap))
    {
        char szKeys[256];
        strncpy(szKeys, GetOption(keymap), sizeof(szKeys));
        char *psz = strtok(szKeys, ",");

        // Assign the SAM key code for each pad/button mapping
        for (int i = 0 ; i < sizeof(anKeyMap)/sizeof(anKeyMap[0]) ; i++)
        {
            int nKey = (psz && psz[0]) ? strtoul(psz, NULL, 0) : SK_NONE;
            anKeyMap[i] = (nKey < SK_MAX) ? nKey : SK_NONE;
            psz = strtok(NULL, ",");
        }
    }

    // Initialise SAM mouse
    Mouse::Init(fFirstInit_);

    return true;
}

void Input::Exit (bool fReInit_/*=false*/)
{
    if (!fReInit_)
    {
        // Release button control back to Windows
        GXCloseInput();

        // Destroy the SIP window
        DestroyWindow(g_hwndSIP);
    }

    Mouse::Exit(fReInit_);
}


// Find the key positioned under a given point, if any
KEYAREA* FindKey (int nX_, int nY_, KEYAREA* pKeys_)
{
    // The SIP is actually offset by 2 pixels
    nX_ -= 2;

    for (KEYAREA* pKeys = pKeys_ ; pKeys->nKey != SK_NONE ; pKeys++)
    {
        // Test the tap point against the key area
        if (nX_ >= pKeys->x && nX_ < pKeys->x+pKeys->w &&
            nY_ >= pKeys->y && nY_ < pKeys->y+pKeys->h)
            return pKeys;
    }

    // No match
    return NULL;
}


HWND CreateSIP ()
{
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = SIPWndProc;
    wc.hInstance = __hinstance;
    wc.lpszClassName = _T("SimCoupeSIPClass");

    // Create a window for the display (initially invisible)
    if (!RegisterClass(&wc))
        return NULL;

    HWND hwnd = CreateWindowEx(0, wc.lpszClassName, _T(""), WS_CHILD,
                            (g_gxdp.cxWidth-SIP_WIDTH)>>1, g_gxdp.cyHeight-SIP_HEIGHT,
                            SIP_WIDTH, SIP_HEIGHT, g_hwnd, NULL, __hinstance, NULL);

    // Show the SIP in portrait mode
    if (!GetOption(fullscreen))
        ShowWindow(hwnd, SW_SHOW);

    return hwnd;
}

// SIP window procedure (duh!)
long CALLBACK SIPWndProc (HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static HBITMAP hbmpNormal, hbmpShift, hbmpSymbol, hbmpJoystick;

    switch (uMsg_)
    {
        case WM_CREATE:
            // Load the keyboard layouts
            hbmpNormal = LoadBitmap(__hinstance, MAKEINTRESOURCE(IDB_NORMAL));
            hbmpShift = LoadBitmap(__hinstance, MAKEINTRESOURCE(IDB_SHIFT));
            hbmpSymbol = LoadBitmap(__hinstance, MAKEINTRESOURCE(IDB_SYMBOL));
            hbmpJoystick = LoadBitmap(__hinstance, MAKEINTRESOURCE(IDB_JOYSTICK));
            return 0;

        case WM_DESTROY:
            // Cleanup the bitmap objects
            DeleteObject(hbmpNormal);
            DeleteObject(hbmpShift);
            DeleteObject(hbmpSymbol);
            DeleteObject(hbmpJoystick);
            break;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd_, &ps);

            // Decide on the key layout to show, which depends on the shift keys
            HBITMAP hbmpLayout = IsSamKeyPressed(SK_SHIFT) ? hbmpShift : IsSamKeyPressed(SK_SYMBOL) ?
                                 hbmpSymbol : fJoySip ? hbmpJoystick : hbmpNormal;

            // Create a memory DC and off-screen bitmap, to avoid flicker
            HDC hdc1 = CreateCompatibleDC(hdc);
            HBITMAP hbmpNew = CreateCompatibleBitmap(hdc, SIP_WIDTH, SIP_HEIGHT);
            HGDIOBJ hbmpOld1 = SelectObject(hdc1, hbmpNew);

            // Copy the current SIP image to the working bitmap
            HDC hdc2 = CreateCompatibleDC(hdc);
            HGDIOBJ hbmpOld2 = SelectObject(hdc2, hbmpLayout);
            BitBlt(hdc1, 0,0, SIP_WIDTH,SIP_HEIGHT, hdc2, 0,0, SRCCOPY);
            SelectObject(hdc2, hbmpOld2);
            DeleteDC(hdc2);

            KEYAREA *pSip = fJoySip ? asJoyAreas : asKeyAreas;

            // Look through the known keys, including the function buttons
            for (KEYAREA* pKeys = pSip ; pKeys->nKey != SK_NONE ; pKeys++)
            {
                // Fill in the default size if needed
                if (!pKeys->w)
                    pKeys->w = pKeys->h = 11;

                // Invert the key area if the associated SAM key is pressed, or the pause button if paused
                if (((pKeys->nKey < 0) ? (pKeys->nKey == nSipKey) : IsSamKeyPressed(pKeys->nKey)) || (g_fPaused && pKeys->nKey == -5))
                    BitBlt(hdc1, pKeys->x+2,pKeys->y, pKeys->w,pKeys->h, NULL, 0,0, DSTINVERT);
            }

            // Copy the final image to the display, and clean up the temporary objects
            BitBlt(hdc, 0,0, SIP_WIDTH,SIP_HEIGHT, hdc1, 0,0, SRCCOPY);
            SelectObject(hdc1, hbmpOld1);
            DeleteObject(hbmpNew);
            DeleteDC(hdc1);

            EndPaint(hwnd_, &ps);
            break;
        }

        case WM_LBUTTONDOWN:
        {
            KEYAREA *pKey;
            int nX = LOWORD(lParam_), nY = HIWORD(lParam_);

            fSipDirty = true;

            // Check if the stylus is over a SIP key
            if (!(pKey = FindKey(nX, nY, fJoySip ? asJoyAreas : asKeyAreas)))
                break;

            // Capture the stylus so we see when it's released
            SetCapture(hwnd_);

            // Function key?
            if (pKey->nKey < 0)
            {
                int nKey = -(pKey->nKey+1);
                int nShift = IsSamKeyPressed(SK_SHIFT) ? SMOD_SHIFT : IsSamKeyPressed(SK_SYMBOL) ? SMOD_SYMBOL : SMOD_NONE;
                int nAction = anActions[nShift][nKey];

                // Special-case action
                if (nAction == actToggleJoySip)
                {
                    fJoySip = !fJoySip;
                    fSipDirty = true;
                }
                else if (nAction != -1)	// Standard action
                {
                    nSipKey = pKey->nKey;
                    Action::Do(nAction);
                }
            }
            else
            {
                nSipKey = SK_NONE;

                if (pKey->nKey == SK_SHIFT && !(nSipMods & SMOD_SHIFT))
                    nSipMods |= SMOD_SHIFT;
                else if (pKey->nKey == SK_SYMBOL && !(nSipMods & SMOD_SYMBOL))
                    nSipMods |= SMOD_SYMBOL;
                else if (pKey->nKey == SK_CONTROL && !(nSipMods & SMOD_CONTROL))
                    nSipMods |= SMOD_CONTROL;
                else
                    nSipKey = pKey->nKey;
            }

            break;
        }

        case WM_CAPTURECHANGED:
        case WM_LBUTTONUP:
            fSipDirty = true;

            // Release the capture if we had it
            if (GetCapture() == hwnd_)
                ReleaseCapture();

            // Function key?
            if (nSipKey < 0)
            {
                int nKey = -(nSipKey+1);
                int nShift = IsSamKeyPressed(SK_SHIFT) ? SMOD_SHIFT : IsSamKeyPressed(SK_SYMBOL) ? SMOD_SYMBOL : SMOD_NONE;

                nSipKey = SK_NONE;
                nSipMods = 0;

                // Perform the assigned release action, if any
                if (anActions[nShift][nKey] != -1)
                    Action::Do(anActions[nShift][nKey], false);
            }
            else if (nSipKey != SK_NONE)
            {
                if (nSipKey == SK_SHIFT && (nSipMods & SMOD_SHIFT))
                    nSipMods &= ~SMOD_SHIFT;
                else if (nSipKey == SK_SYMBOL && (nSipMods & SMOD_SYMBOL))
                    nSipMods &= ~SMOD_SYMBOL;
                else if (nSipKey == SK_CONTROL && (nSipMods & SMOD_CONTROL))
                    nSipMods &= ~SMOD_CONTROL;
                else
                    nSipMods = 0;

                nSipKey = SK_NONE;
            }

            break;
    }

    return DefWindowProc(hwnd_, uMsg_, wParam_, lParam_);
}


void Input::Acquire (bool fKeyboard_/*=true*/, bool fMouse_/*=true*/)
{
    Purge();
}

void Input::Purge (bool fKeyboard_/*=true*/, bool fMouse_/*=true*/)
{
    ZeroMemory(afKeyStates, sizeof(afKeyStates));
    ZeroMemory(afKeys, sizeof(afKeys));
    ReleaseAllSamKeys();

    nSipKey = SK_NONE;
    nSipMods = 0;

    Update();
}

void ReadKeyboard ()
{
    // Make a copy of the master key table with the real keyboard states
    memcpy(afKeyStates, afKeys, sizeof(afKeyStates));

    // Left-Alt can optionally be used as the SAM Cntrl key
    if (GetOption(altforcntrl) && IsPressed(VK_LMENU))
    {
        ReleaseKey(VK_LMENU);
        PressKey(VK_RCONTROL);
    }

    // AltGr can optionally be used for SAM Edit
    if (GetOption(altgrforedit) && IsPressed(VK_RMENU))
    {
        // Release AltGr and press the context menu key (also used for SAM Edit)
        ReleaseKey(VK_RMENU);
        PressKey(VK_APPS);

        // Also release Ctrl and Alt, which is how AltGr often behaves
        ReleaseKey(VK_LCONTROL);
        ReleaseKey(VK_LMENU);
    }
}


// Update a combination key table with a symbol
bool UpdateKeyTable (SIMPLE_KEY* asKeys_, int nSym_, int nKey_, int nMods_)
{
    // Convert upper-case symbols to lower-case without shift
    if (nSym_ >= 'A' && nSym_ <= 'Z')
    {
        nSym_ += 'a'-'A';
        nMods_ &= ~MOD_SHIFT;
    }

    // Convert control characters to the base key, as it will be needed for SAM Symbol combinations
    else if ((nMods_ & MOD_LCTRL) && nSym_ < ' ')
        nSym_ += 'a'-1;


    for (int i = 0 ; i < SK_MAX ; i++)
    {
        // Is there a mapping entry for the symbol?
        if (asKeys_[i].nChar == nSym_)
        {
            // Log if the mapping is new
            if (!asKeys_[i].nKey)
                TRACE("%c maps to %d\n", nSym_, nKey_);

            // Update the key mapping
            asKeys_[i].nKey = nKey_;
            return true;
        }
    }

    return false;
}

// Update a combination key table with a symbol
bool UpdateKeyTable (COMBINATION_KEY* asKeys_, int nSym_, int nKey_, int nMods_)
{
    for (int i = 0 ; asKeys_[i].nSamKey != SK_NONE ; i++)
    {
        // Is there a mapping entry for the symbol?
        if (asKeys_[i].nChar == nSym_)
        {
            // Log if the mapping is new
            if (!asKeys_[i].nKey)
                TRACE("%c maps to %d with mods of %#02x\n", nSym_, nKey_, nMods_);

            // Update the key mapping
            asKeys_[i].nKey = nKey_;
            asKeys_[i].nMods = nMods_;
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
        if (asKeys_[i].nKey && IsPressed(asKeys_[i].nKey))
        {
            PressSamKey(i);

            if (i != SK_CONTROL)
                PressSamKey(i);
        }
    }
}

// Process the additional keys mapped from PC to SAM, ignoring shift state
void ProcessKeyTable (MAPPED_KEY* asKeys_)
{
    // Build the rest of the SAM matrix from the simple non-symbol PC keys
    for (int i = 0 ; asKeys_[i].nKey ; i++)
    {
        if (IsPressed(asKeys_[i].nKey))
        {
            PressSamKey(asKeys_[i].nSamKey);
            PressSamKey(asKeys_[i].nSamMods);
        }
    }
}

// Process more complicated key combinations
void ProcessKeyTable (COMBINATION_KEY* asKeys_)
{
    short nShifts = 0;
    if (IsPressed(VK_SHIFT))    nShifts |= MOD_SHIFT;
    if (IsPressed(VK_LCONTROL)) nShifts |= MOD_LCTRL;
    if (IsPressed(VK_LMENU))    nShifts |= MOD_LALT;
    if (IsPressed(VK_RMENU))    nShifts |= MOD_RALT;

    // Have the shift states changed while a combo is in progress?
    if (nComboModifiers != MOD_NONE && nComboModifiers != nShifts)
    {
        // If the combo key is still pressed, start the timer running to re-press it as we're about to release it
        if (IsPressed(nComboKey))
        {
            TRACE("Starting combo timer\n");
            dwComboTime = OSD::GetTime();
        }

        // We're done with the shift state now, so clear it to prevent the time getting reset
        nComboModifiers = MOD_NONE;
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
        if (IsPressed(asKeys_[i].nKey) && asKeys_[i].nMods == nShifts)
        {
            // Release the PC keys used for the key combination
            ReleaseKey(asKeys_[i].nKey);
            if (nShifts & MOD_SHIFT)  ToggleKey(VK_SHIFT);
            if (nShifts & MOD_LCTRL)  ToggleKey(VK_LCONTROL);
            if (nShifts & MOD_LALT) { ToggleKey(VK_LMENU); ReleaseKey(VK_RCONTROL); }

            // Press the SAM key(s) required to generate the symbol
            PressSamKey(asKeys_[i].nSamKey);
            PressSamKey(asKeys_[i].nSamMods);

            // Remember the key involved with the shifted state for a combo
            nComboKey = asKeys_[i].nKey;
            nComboModifiers = nShifts;
        }
    }
}

// Build the SAM keyboard matrix from the current PC state
void SetSamKeyState ()
{
    // No SAM keys are pressed initially
    ReleaseAllSamKeys();

    // Set any SIP selection on the SAM keyboard
    if (nSipKey >= 0 && nSipKey != SK_NONE) PressSamKey(nSipKey);
    if (nSipMods & SMOD_SHIFT) PressSamKey(SK_SHIFT);
    if (nSipMods & SMOD_SYMBOL) PressSamKey(SK_SYMBOL);
    if (nSipMods & SMOD_CONTROL) PressSamKey(SK_CONTROL);

    // If the control pad or hardware keys are pressed, activate the appropriate SAM key
    for (int i = 0 ; i < sizeof(anHwKeys)/sizeof(anHwKeys[0]) ; i++)
    {
        if (IsPressed(anHwKeys[i]))
            PressSamKey(anKeyMap[i]);
    }

    // Left and right shift keys are equivalent, and also complementary!
    bool fShiftToggle = IsPressed(VK_LSHIFT) && IsPressed(VK_RSHIFT);

    // Process the key combinations required for the mode we're in
    switch (GetOption(keymapping))
    {
        // SAM keys
        case 1:
            ProcessKeyTable(asSamSymbols);
            ProcessKeyTable(asSamMappings);
            break;

        // Spectrum mappings
        case 2:
            ProcessKeyTable(asSpectrumSymbols);
            ProcessKeyTable(asSpectrumMappings);
            break;
    }

    // Toggle shift if both shift keys are down to allow shifted versions of keys that are
    // shifted on the PC but unshifted on the SAM
    if (fShiftToggle)
        ReleaseKey(VK_SHIFT);

    // Process the simple key mappings
    ProcessKeyTable(asSamKeys);

    // Update the SIP if it may have changed
    if (fSipDirty)
    {
        InvalidateRect(g_hwndSIP, NULL, FALSE);
        fSipDirty = false;
    }
}

void Input::Update ()
{
    ReadKeyboard();
    SetSamKeyState();
}


bool Input::FilterMessage (HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static int nLastX, nLastY;
    static WPARAM wLastKey;

    switch (uMsg_)
    {
        case WM_MOUSEMOVE:
        {
            int nNewX = GET_X_LPARAM(lParam_), nNewY = GET_Y_LPARAM(lParam_);

            if (nNewX < 0) nNewX++;
            if (nNewY < 0) nNewY++;

            int nX = nNewX - nLastX, nY = nNewY - nLastY;
            nLastX = nNewX, nLastY = nNewY;

            // Has it moved at all?
            if (nX || nY)
            {
                nY = -nY;

                if (GetOption(fullscreen))
                {
                    // Landscape left, the WinCE inverted Y coords require is to invert X
                    if (1)
                        nX = -nX;

                    swap(nX, nY);
                }

                Mouse::Move(nX, nY);
            }
            break;
        }

        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        {
            nLastX = GET_X_LPARAM(lParam_);
            nLastY = GET_Y_LPARAM(lParam_);
            POINT pt = { nLastX, nLastY };

            if (uMsg_ == WM_LBUTTONDBLCLK)
            {
                RECT rect;
                if (GetOption(fullscreen))
                    SetRect(&rect, (g_gxdp.cxWidth-SCREEN_LINES)/2, (g_gxdp.cyHeight-SCREEN_PIXELS)/2, SCREEN_LINES, SCREEN_PIXELS);
                else
                    SetRect(&rect, 0, (g_gxdp.cyHeight-SIP_HEIGHT-SCREEN_LINES)/2, 240, SCREEN_LINES);

                // Offset the width+height to give a screen rectangle
                rect.right += rect.left-1;
                rect.bottom += rect.top-1;

                // If the mouse is enabled and the double-tap was on the main screen, treat it as a SAM mouse click
                // Otherwise toggle between portrait and landscape modes
                if (GetOption(mouse) && PtInRect(&rect, pt))
                    Mouse::SetButton(1);
                else
                    Action::Do(actToggleFullscreen);
            }

            SetCapture(hwnd_);
            break;
        }

        case WM_LBUTTONUP:
            // If we captured to catch the release, release it now
            if (GetCapture() == hwnd_)
                ReleaseCapture();

            Mouse::SetButton(1, false);
            break;

        case WM_CHAR:
        {
            int nSym = wParam_&0xff, nKey = wLastKey&0xff, nShifts = 0;
            if (GetKeyState(VK_SHIFT)    < 0) nShifts |= MOD_SHIFT;
            if (GetKeyState(VK_LCONTROL) < 0) nShifts |= MOD_LCTRL;
            if (GetKeyState(VK_LMENU)    < 0) nShifts |= MOD_LALT;
            if (GetKeyState(VK_RMENU)    < 0) nShifts |= MOD_RALT;

            // Ignore symbols on the keypad
            if (nKey >= VK_NUMPAD0 && nKey <= VK_DIVIDE)
                nSym = 0;

            if (nSym && !UpdateKeyTable(asSamKeys, nSym, nKey, nShifts))
            {
                // The table we update depends on the key mapping being used
                switch (GetOption(keymapping))
                {
                    case 1: UpdateKeyTable(asSamSymbols, nSym, nKey, nShifts);      break;
                    case 2: UpdateKeyTable(asSpectrumSymbols, nSym, nKey, nShifts); break;
                }
            }

            fSipDirty = true;
            break;
        }

        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
            // Eat key repeats
            if (lParam_ & 0x40000000)
                return true;

            // Fall through...
            wLastKey = wParam_;

        case WM_SYSKEYUP:
        case WM_KEYUP:
        {
            bool fPressed = !(uMsg_ & 1);
            WORD wKey = static_cast<WORD>(wParam_);

            // In fullscreen more we rotate the pad to match the screen orientation
            if (GetOption(fullscreen))
            {
                switch (wKey)
                {
                    case VK_LEFT:  wKey = VK_UP;	break;
                    case VK_RIGHT: wKey = VK_DOWN;	break;
                    case VK_UP:	   wKey = VK_RIGHT;	break;
                    case VK_DOWN:  wKey = VK_LEFT;	break;
                }
            }

            TRACE("%#02X %s\n", wKey, fPressed?"pressed":"released");
            afKeys[wKey & 0xff] = fPressed;

            switch (wKey)
            {
                case VK_SHIFT:
                    afKeys[VK_LSHIFT] = GetKeyState(VK_LSHIFT) < 0;
                    afKeys[VK_RSHIFT] = GetKeyState(VK_RSHIFT) < 0;
                    break;

                case VK_CONTROL:
                    afKeys[VK_LCONTROL] = GetKeyState(VK_LCONTROL) < 0;
                    afKeys[VK_RCONTROL] = GetKeyState(VK_RCONTROL) < 0;
                    break;

                case VK_MENU:
                    afKeys[VK_LMENU] = GetKeyState(VK_LMENU) < 0;
                    afKeys[VK_RMENU] = GetKeyState(VK_RMENU) < 0;
                    break;

                case VK_CAPITAL:
                case VK_LWIN:
                case VK_APPS:
                    afKeys[VK_SHIFT] = 0;
                    break;

                default:
                    break;
            }

            fSipDirty = true;
            break;
		}
    }

    // Message not processed
    return false;
}
