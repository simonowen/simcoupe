// Part of SimCoupe - A SAM Coupe emulator
//
// Input.cpp: WinCE input
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
#include "resource.h"

#include "Display.h"
#include "Input.h"
#include "IO.h"
#include "Mouse.h"
#include "Options.h"
#include "UI.h"
#include "Video.h"

GXKeyList g_gxkl;
HWND g_hwndSIP;


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

// Actions for each of the 10 function buttons, in the 3 shifted states
static int anActions[][10] =
{
    { actInsertFloppy1, actInsertFloppy2, actDisplayOptions, actAbout, actPause, actTempTurbo, -1, -1, actResetButton, actExitApplication },
    { actEjectFloppy1, actEjectFloppy2, -1, -1, -1, -1, -1, -1, actNmiButton, actMinimise },
    { actSaveFloppy1, actSaveFloppy2, -1, -1, -1, -1, -1, -1, -1, -1 }
};

// Array of states tracking the function buttons, so we draw them appropriately
bool afFunctionKeys[10];


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

        // Portrait keys, as we'll do our own landscape rotation
        g_gxkl = GXGetDefaultKeys(GX_NORMALKEYS);
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
KEYAREA* FindKey (int nX_, int nY_)
{
    // The SIP is actually offset by 2 pixels
    nX_ -= 2;

    for (KEYAREA* pKeys = &asKeyAreas[0] ; pKeys->nKey != SK_NONE ; pKeys++)
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
//  wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
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
    static HBITMAP hbmpNormal, hbmpShift, hbmpSymbol;
    static KEYAREA* pKey;
    static bool fSticky;

    switch (uMsg_)
    {
        case WM_CREATE:
            // Load the keyboard layouts
            hbmpNormal = LoadBitmap(__hinstance, MAKEINTRESOURCE(IDB_NORMAL));
            hbmpShift = LoadBitmap(__hinstance, MAKEINTRESOURCE(IDB_SHIFT));
            hbmpSymbol = LoadBitmap(__hinstance, MAKEINTRESOURCE(IDB_SYMBOL));
            return 0;

        case WM_DESTROY:
            // Cleanup the bitmap objects
            DeleteObject(hbmpNormal);
            DeleteObject(hbmpShift);
            DeleteObject(hbmpSymbol);
            break;

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd_, &ps);

            // Decide on the key layout to show, which depends on the shift keys
            HBITMAP hbmpLayout = hbmpNormal;
            if (IsSamKeyPressed(SK_SHIFT) && !IsSamKeyPressed(SK_SYMBOL) && !IsSamKeyPressed(SK_CONTROL))
                hbmpLayout = hbmpShift;
            else if (IsSamKeyPressed(SK_SYMBOL) && !IsSamKeyPressed(SK_SHIFT) && !IsSamKeyPressed(SK_CONTROL))
                hbmpLayout = hbmpSymbol;

            // Create a memory DC and off-screen bitmap, to avoid flicker
            HDC hdc1 = CreateCompatibleDC(hdc);
            HBITMAP hbmpNew = CreateCompatibleBitmap(hdc, SIP_WIDTH, SIP_HEIGHT);
            HBITMAP hbmpOld1 = reinterpret_cast<HBITMAP>(SelectObject(hdc1, hbmpNew));

            // Copy the current SIP image to the working bitmap
            HDC hdc2 = CreateCompatibleDC(hdc);
            HBITMAP hbmpOld2 = reinterpret_cast<HBITMAP>(SelectObject(hdc2, hbmpLayout));
            BitBlt(hdc1, 0,0, SIP_WIDTH,SIP_HEIGHT, hdc2, 0,0, SRCCOPY);
            SelectObject(hdc2, hbmpOld2);
            DeleteDC(hdc2);

            // Look through the known keys, including the function buttons
            for (KEYAREA* pKeys = &asKeyAreas[0] ; pKeys->nKey != SK_NONE ; pKeys++)
            {
                // Fill in the default size if needed
                if (!pKeys->w)
                    pKeys->w = pKeys->h = 11;

                // Invert the key area if the associated SAM key is pressed
                if ((pKeys->nKey < 0) ? afFunctionKeys[(-pKeys->nKey)-1] : IsSamKeyPressed(pKeys->nKey))
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
            // Find the key under the stylus position
            if (!(pKey = FindKey(LOWORD(lParam_), HIWORD(lParam_))))
                break;

            // Capture the stylus so we see when it's released
            SetCapture(hwnd_);

            // Function key?
            if (pKey->nKey < 0)
            {
                int nKey = (-pKey->nKey)-1, nShift = 0;
                afFunctionKeys[nKey] = true;

                // The shift states only applies if the one we're interested is pressed
                if (IsSamKeyPressed(SK_SHIFT) && !IsSamKeyPressed(SK_SYMBOL) && !IsSamKeyPressed(SK_CONTROL))
                    nShift = 1;
                else if (IsSamKeyPressed(SK_SYMBOL) && !IsSamKeyPressed(SK_SHIFT) && !IsSamKeyPressed(SK_CONTROL))
                    nShift = 2;

                // Perform the assigned action, if any
                if (anActions[nShift][nKey] != -1)
                    UI::DoAction(anActions[nShift][nKey]);

                // Prevent the pause button release being seen, so it remains drawn down when paused
                if (anActions[nShift][nKey] == actPause)
                    pKey = NULL;
            }

            // Shifted or sticky key?
            else if (!IsSamKeyPressed(pKey->nKey) && (fSticky || pKey->nKey == SK_SHIFT ||
                    pKey->nKey == SK_SYMBOL || pKey->nKey == SK_CONTROL))
            {
                // Press the key
                PressSamKey(pKey->nKey);

                // Forget the key so we ignore the release this time
                pKey = NULL;
                fSticky = false;
            }
            else
                PressSamKey(pKey->nKey);


            // Force a SIP update
            InvalidateRect(hwnd_, NULL, FALSE);
            break;

        case WM_LBUTTONUP:
            // Release the capture if we had it
            if (GetCapture() == hwnd_)
                ReleaseCapture();

            // Nothing to do if no key was seen pressed
            if (!pKey)
                break;

            // Function key?
            if (pKey->nKey < 0)
            {
                int nKey = (-pKey->nKey)-1, nShift = 0;
                afFunctionKeys[nKey] = false;

                // The shift states only applies if the one we're interested is pressed
                if (IsSamKeyPressed(SK_SHIFT) && !IsSamKeyPressed(SK_SYMBOL) && !IsSamKeyPressed(SK_CONTROL))
                    nShift = 1;
                else if (IsSamKeyPressed(SK_SYMBOL) && !IsSamKeyPressed(SK_SHIFT) && !IsSamKeyPressed(SK_CONTROL))
                    nShift = 2;

                // Perform the assigned release action, if any
                if (anActions[nShift][nKey] != -1)
                    UI::DoAction(anActions[nShift][nKey], false);
            }
            else
                ReleaseSamKey(pKey->nKey);

            // Once a non-shift key is pressed, release the shifts
            if (pKey->nKey != SK_SHIFT && pKey->nKey != SK_SYMBOL && pKey->nKey != SK_CONTROL)
            {
                ReleaseSamKey(SK_SHIFT);
                ReleaseSamKey(SK_SYMBOL);
                ReleaseSamKey(SK_CONTROL);
            }
            else
                pKey = NULL;

            // Force a SIP update
            InvalidateRect(hwnd_, NULL, FALSE);
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
    ZeroMemory(&afFunctionKeys, sizeof afFunctionKeys);
    ReleaseAllSamKeys();
}

void Input::Update ()
{
    // Nothing to do - we're driven by message events
}


bool Input::FilterMessage (HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    static int nLastX, nLastY;

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
            nLastX = GET_X_LPARAM(lParam_);
            nLastY = GET_Y_LPARAM(lParam_);
            SetCapture(hwnd_);
            break;

        case WM_LBUTTONDBLCLK:
            Mouse::SetButton(1);
            SetCapture(hwnd_);
            break;

        case WM_LBUTTONUP:
            // If we captured to catch the release, release it now
            if (GetCapture() == hwnd_)
                ReleaseCapture();

            Mouse::SetButton(1, false);
            break;

        case WM_KEYDOWN:
            // Eat key repeats
            if (lParam_ & 0x40000000)
                return true;

            // Fall through...

        case WM_KEYUP:
        {
            bool fPressed = (uMsg_ == WM_KEYDOWN);
            WORD wKey = static_cast<WORD>(wParam_);
            int nKey;

            int anKeys[] = { SK_6, SK_7, SK_8, SK_9, SK_SPACE, SK_SPACE, SK_ESCAPE, SK_RETURN };

            // Landscape modes need the key directions rotating
            if (GetOption(fullscreen))
            {
                // Force landscape left for now
                if (1)
                {
                    // Rotated left
                    swap(anKeys[3],anKeys[1]);
                    swap(anKeys[1],anKeys[2]);
                    swap(anKeys[2],anKeys[0]);
                }
                else
                {
                    // Rotated right
                    swap(anKeys[3],anKeys[1]);
                    swap(anKeys[1],anKeys[2]);
                    swap(anKeys[2],anKeys[0]);
                }
            }

            if (wKey == g_gxkl.vkLeft)
                nKey = anKeys[0];
            else if (wKey == g_gxkl.vkRight)
                nKey = anKeys[1];
            else if (wKey == g_gxkl.vkDown)
                nKey = anKeys[2];
            else if (wKey == g_gxkl.vkUp)
                nKey = anKeys[3];
            else if (wKey == g_gxkl.vkA)
                nKey = anKeys[4];
            else if (wKey == g_gxkl.vkB)
                nKey = anKeys[5];

            // The button assignments below are hard-coded for now
            else if (wKey == g_gxkl.vkC)
            {
                if (fPressed)
                    UI::DoAction(actToggleFullscreen);
                return true;
            }
            else if (wKey == 194)   // This is missing from the GAPI key list?
            {
                Mouse::SetButton(1, fPressed);
                return true;
            }
            else if (wKey == g_gxkl.vkStart)
            {
                // This duplicate prevents getting stuck in fullscreen during the beta testing
                // if the other button mapping is unavailable for some reason
                if (fPressed)
                    UI::DoAction(actToggleFullscreen);
                return true;
            }
            else
                break;

            // Press or release the key, depending on the message
            if (fPressed)
                PressSamKey(nKey);
            else
                ReleaseSamKey(nKey);

            // Force a SIP update
            InvalidateRect(g_hwndSIP, NULL, FALSE);
            return false;
        }
    }

    // Message not processed
    return false;
}
