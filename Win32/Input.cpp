// Part of SimCoupe - A SAM Coupe emulator
//
// Input.cpp: Win32 mouse and DirectInput keyboard input
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

// Notes:
//  On startup (or detected keyboard change) tables are built for the
//  keyboard mappings, to cope with different keyboard positions for both
//  symbols (e.g. US), letters (e.g. French), and digits (e.g. Czech).

#include "SimCoupe.h"
#include <windowsx.h>

#define DIRECTINPUT_VERSION     0x0500
#include <dinput.h>

#include "GUI.h"
#include "Input.h"
#include "SAMIO.h"
#include "Joystick.h"
#include "Keyboard.h"
#include "Keyin.h"
#include "Mouse.h"
#include "Options.h"
#include "UI.h"

typedef HRESULT(WINAPI* PFNDIRECTINPUTCREATE) (HINSTANCE, DWORD, LPDIRECTINPUTA*, LPUNKNOWN);

const unsigned int EVENT_BUFFER_SIZE = 16;
const unsigned int JOYSTICK_DEADZONE = 50; // 50% of analogue range

static PFNDIRECTINPUTCREATE pfnDirectInputCreate;
static LPDIRECTINPUT pdi;
static LPDIRECTINPUTDEVICE pdiKeyboard;
static LPDIRECTINPUTDEVICE2 pdidJoystick1, pdidJoystick2;
static HKL hkl;

static bool fMouseActive;

////////////////////////////////////////////////////////////////////////////////

static bool InitKeyboard();
static bool InitJoysticks();


bool Input::Init(bool fFirstInit_/*=false*/)
{
    bool fRet = false;

    Exit(true);

    auto hinstDInput = LoadLibrary("DINPUT.DLL");
    if (!hinstDInput)
    {
        Message(MsgType::Error, "DINPUT.DLL not found.");
        return false;
    }

    pfnDirectInputCreate =
        reinterpret_cast<PFNDIRECTINPUTCREATE>(
            GetProcAddress(hinstDInput, "DirectInputCreateA"));

    if (!pfnDirectInputCreate)
    {
        Message(MsgType::Error, "DirectInputCreate failed.");
        return false;
    }

    // If we can find DirectInput 5.0 we can have joystick support, otherwise fall back on 3.0 support for NT4
    if ((fRet = SUCCEEDED(pfnDirectInputCreate(GetModuleHandle(NULL), DIRECTINPUT_VERSION, &pdi, nullptr))))
        InitJoysticks();
    else
        fRet = SUCCEEDED(pfnDirectInputCreate(GetModuleHandle(NULL), 0x0300, &pdi, nullptr));

    if (fRet)
    {
        hkl = GetKeyboardLayout(0);
        InitKeyboard();

        Keyboard::Init();
        fMouseActive = false;
    }

    return fRet;
}


void Input::Exit(bool fReInit_/*=false*/)
{
    if (pdiKeyboard) { pdiKeyboard->Unacquire(); pdiKeyboard->Release(); pdiKeyboard = nullptr; }
    if (pdidJoystick1) { pdidJoystick1->Unacquire(); pdidJoystick1->Release(); pdidJoystick1 = nullptr; }
    if (pdidJoystick2) { pdidJoystick2->Unacquire(); pdidJoystick2->Release(); pdidJoystick2 = nullptr; }
    if (pdi) { pdi->Release(); pdi = nullptr; }

    pfnDirectInputCreate = nullptr;
}


bool InitKeyboard()
{
    HRESULT hr;
    if (FAILED(hr = pdi->CreateDevice(GUID_SysKeyboard, &pdiKeyboard, nullptr)))
        TRACE("!!! Failed to create keyboard device ({:08x})\n", hr);
    else if (FAILED(hr = pdiKeyboard->SetCooperativeLevel(g_hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND)))
        TRACE("!!! Failed to set cooperative level of keyboard device ({:08x})\n", hr);
    else if (FAILED(hr = pdiKeyboard->SetDataFormat(&c_dfDIKeyboard)))
        TRACE("!!! Failed to set data format of keyboard device ({:08x})\n", hr);
    else
    {
        DIPROPDWORD dipdw = { { sizeof(DIPROPDWORD), sizeof(DIPROPHEADER), 0, DIPH_DEVICE, }, EVENT_BUFFER_SIZE, };

        if (FAILED(hr = pdiKeyboard->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph)))
            TRACE("!!! Failed to set keyboard buffer size\n", hr);

        return true;
    }

    return false;
}


BOOL CALLBACK EnumJoystickProc(LPCDIDEVICEINSTANCE pdiDevice_, LPVOID lpv_)
{
    HRESULT hr;

    LPDIRECTINPUTDEVICE pdiJoystick;
    if (FAILED(hr = pdi->CreateDevice(pdiDevice_->guidInstance, &pdiJoystick, nullptr)))
        TRACE("!!! Failed to create joystick device ({:08x})\n", hr);
    else
    {
        DIDEVICEINSTANCE didi = { sizeof(didi) };
        strcpy(didi.tszInstanceName, "<unknown>");  // WINE fix for missing implementation

        if (FAILED(hr = pdiJoystick->GetDeviceInfo(&didi)))
            TRACE("!!! Failed to get joystick device info ({:08x})\n", hr);

        // Overloaded use - if custom data was supplied, it's a combo box ID to add the string to
        else if (lpv_)
            SendMessage(reinterpret_cast<HWND>(lpv_), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(didi.tszInstanceName));
        else
        {
            IDirectInputDevice2* pDevice;

            // We need an IDirectInputDevice2 interface for polling, so query for it
            if (FAILED(hr = pdiJoystick->QueryInterface(IID_IDirectInputDevice2, reinterpret_cast<void**>(&pDevice))))
                TRACE("!!! Failed to query joystick for IID_IDirectInputDevice2 ({:08x})\n", hr);

            // If the device name matches the joystick 1 device name, save a pointer to it
            else if (didi.tszInstanceName == GetOption(joydev1))
                pdidJoystick1 = pDevice;

            // If the device name matches the joystick 2 device name, save a pointer to it
            else if (didi.tszInstanceName == GetOption(joydev2))
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


bool InitJoystick(LPDIRECTINPUTDEVICE2& pJoystick_)
{
    HRESULT hr;

    // Set the joystick device to use the joystick format
    if (FAILED(hr = pJoystick_->SetDataFormat(&c_dfDIJoystick)))
        TRACE("!!! Failed to set data format of joystick device ({:08x})\n", hr);
    else if (FAILED(hr = pJoystick_->SetCooperativeLevel(g_hwnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND)))
        TRACE("!!! Failed to set cooperative level of joystick device ({:08x})\n", hr);
    else
    {
        // Deadzone tolerance percentage and range of each axis (-100 to +100)
        DIPROPDWORD diPdw = { { sizeof(diPdw), sizeof(diPdw.diph), 0, DIPH_BYOFFSET }, 10000UL * JOYSTICK_DEADZONE / 100UL };
        DIPROPRANGE diPrg = { { sizeof(diPrg), sizeof(diPrg.diph), 0, DIPH_BYOFFSET }, -100, +100 };

        int anAxes[] = { DIJOFS_X, DIJOFS_Y };

        for (size_t i = 0; i < std::size(anAxes); i++)
        {
            diPdw.diph.dwObj = diPrg.diph.dwObj = anAxes[i];

            if (FAILED(hr = pJoystick_->SetProperty(DIPROP_DEADZONE, &diPdw.diph)))
                TRACE("!!! Failed to set joystick deadzone ({:08x})\n", hr);
            else if (FAILED(hr = pJoystick_->SetProperty(DIPROP_RANGE, &diPrg.diph)))
                TRACE("!!! Failed to set joystick range ({:08x})\n", hr);
        }

        return true;
    }

    // Clean up
    pJoystick_->Release();
    pJoystick_ = nullptr;

    return false;
}


bool InitJoysticks()
{
    HRESULT hr;

    // Enumerate the joystick devices
    if (FAILED(hr = pdi->EnumDevices(DIDEVTYPE_JOYSTICK, EnumJoystickProc, nullptr, DIEDFL_ATTACHEDONLY)))
        TRACE("!!! Failed to enumerate joystick devices ({:08x})\n", hr);

    // Initialise matched joysticks
    if (pdidJoystick1) InitJoystick(pdidJoystick1);
    if (pdidJoystick2) InitJoystick(pdidJoystick2);

    return true;
}


// Return whether the emulation is using the mouse
bool Input::IsMouseAcquired()
{
    return fMouseActive;
}


void Input::AcquireMouse(bool fAcquire_)
{
    // Ignore if no state change
    if (fMouseActive == fAcquire_)
        return;

    fMouseActive = fAcquire_;

    // Mouse active?
    if (fMouseActive && GetOption(mouse))
    {
        RECT r;
        GetWindowRect(g_hwnd, &r);

        // Calculate the centre position and move the cursor there
        //ptCentre.x = r.left + (r.right - r.left) / 2;
        //ptCentre.y = r.top + (r.bottom - r.top) / 2;
        //SetCursorPos(ptCentre.x, ptCentre.y);

        // Confine the cursor to the client area so fast mouse movements don't escape
        ClipCursor(&r);
    }
    else
        ClipCursor(nullptr);
}


void Input::Purge()
{
    // Purge queued keyboard items
    if (pdiKeyboard && SUCCEEDED(pdiKeyboard->Acquire()))
    {
        DWORD dwItems = ULONG_MAX;
        if (SUCCEEDED(pdiKeyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), nullptr, &dwItems, 0)) && dwItems)
            TRACE("{} keyboard items purged\n", dwItems);
    }

    Keyboard::Purge();
}


void ReadKeyboard()
{
    if (pdiKeyboard && SUCCEEDED(pdiKeyboard->Acquire()))
    {
        DIDEVICEOBJECTDATA abEvents[EVENT_BUFFER_SIZE];
        DWORD dwItems = EVENT_BUFFER_SIZE;

        if (SUCCEEDED(pdiKeyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), abEvents, &dwItems, 0)))
        {
            for (DWORD i = 0; i < dwItems; i++)
            {
                int nScanCode = abEvents[i].dwOfs;
                bool fPressed = !!(abEvents[i].dwData & 0x80);

                Keyboard::SetKey(nScanCode, fPressed);
                TRACE("{} {}\n", nScanCode, fPressed ? "pressed" : "released");
            }
        }
    }
}

void ReadJoystick(int nJoystick_, LPDIRECTINPUTDEVICE2 pDevice_)
{
    HRESULT hr;

    DIJOYSTATE dijs = { 0 };
    if (pDevice_ && (FAILED(hr = pDevice_->Poll()) || FAILED(hr = pDevice_->GetDeviceState(sizeof(dijs), &dijs))))
    {
        if (FAILED(hr = pDevice_->Acquire()))
            return;

        if (FAILED(hr = pDevice_->Poll()) || FAILED(hr = pDevice_->GetDeviceState(sizeof(dijs), &dijs)))
            TRACE("!!! Failed to read joystick {} state ({:08x})\n", nJoystick_, hr);
    }


    DWORD dwButtons = 0;
    int nPosition = HJ_CENTRE;

    // Combine the states of all buttons so any button works as fire
    for (size_t i = 0; i < std::size(dijs.rgbButtons); i++)
    {
        if (dijs.rgbButtons[i] & 0x80)
            dwButtons |= (1U << i);
    }

    // Set the position from the main axes
    if (dijs.lX < 0) nPosition |= HJ_LEFT;
    if (dijs.lX > 0) nPosition |= HJ_RIGHT;
    if (dijs.lY < 0) nPosition |= HJ_UP;
    if (dijs.lY > 0) nPosition |= HJ_DOWN;

    // Consider all hat positions too
    for (size_t i = 0; i < std::size(dijs.rgdwPOV); i++)
    {
        static int an[] = { HJ_UP, HJ_UP | HJ_RIGHT, HJ_RIGHT, HJ_DOWN | HJ_RIGHT, HJ_DOWN, HJ_DOWN | HJ_LEFT, HJ_LEFT, HJ_UP | HJ_LEFT };

        // For best driver compatibility, only check the low WORD for the centre position
        if (LOWORD(dijs.rgdwPOV[i]) == 0xffff)
            continue;

        // Round the position and determine which of the 8 directions it's pointing
        int nPos = ((dijs.rgdwPOV[i] + 4500 / 2) / 4500) & 7;
        nPosition |= an[nPos];
    }

    Joystick::SetPosition(nJoystick_, nPosition);
    Joystick::SetButtons(nJoystick_, dwButtons);
}


// Update the keyboard and joystick inputs
void Input::Update()
{
    // Update native keyboard state
    ReadKeyboard();

    // Read the joysticks, if present
    if (pdidJoystick1) ReadJoystick(0, pdidJoystick1);
    if (pdidJoystick2) ReadJoystick(1, pdidJoystick2);

    // Update the SAM keyboard matrix from the current key state (including joystick movement)
    Keyboard::Update();
}


// Send a mouse message to the GUI, after mapping the mouse position to the SAM screen
bool SendGuiMouseMessage(int nMessage_, POINT* ppt_)
{
    // Map from display position to SAM screen position
    int nX = ppt_->x, nY = ppt_->y;
    Video::NativeToSam(nX, nY);

    // Finally, send the message now we have the appropriate position
    return GUI::SendMessage(nMessage_, nX, nY);
}

bool Input::FilterMessage(HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_)
{
    // Mouse button decoder table, from the lower 4 bits of the Windows message values
    static int anMouseButtons[16] = { 2, 1, 1, 1, 3, 3, 3, 2, 2 };

    int x = GET_X_LPARAM(lParam_);
    int y = GET_Y_LPARAM(lParam_);

    switch (uMsg_)
    {
        // Input language has changed - reinitialise input to pick up the new mappings
    case WM_INPUTLANGCHANGE:
        Init();
        break;

        // Release the mouse and purge keyboard input on activation changes
    case WM_ACTIVATE:
        AcquireMouse(false);
        Purge();
        break;

        // Release the mouse on entering the menu
    case WM_ENTERMENULOOP:
        AcquireMouse(false);
        Purge();    // needed to avoid Alt-X menu shortcuts from being seen
        break;

    case WM_MOUSEMOVE:
    {
        // If the GUI is active, pass the message to it
        if (GUI::IsActive())
        {
            Video::NativeToSam(x, y);
            GUI::SendMessage(GM_MOUSEMOVE, x, y);
            break;
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

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    {
        // The GUI gets first chance to process the message
        if (GUI::IsActive())
        {
            Video::NativeToSam(x, y);
            GUI::SendMessage(GM_BUTTONDOWN, x, y);
        }

        // If the mouse is already active, pass on button presses
        else if (fMouseActive)
        {
            pMouse->SetButton(anMouseButtons[uMsg_ & 0xf], true);
        }

        // If the mouse interface is enabled and being read by something other than the ROM, a left-click acquires it
        // Otherwise a double-click is required to forcibly acquire it
        else if (GetOption(mouse) && ((uMsg_ == WM_LBUTTONDOWN && pMouse->IsActive()) || uMsg_ == WM_LBUTTONDBLCLK))
        {
            AcquireMouse(true);
        }

        break;
    }

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    {
        // The GUI gets first chance to process the message
        if (GUI::IsActive())
        {
            Video::NativeToSam(x, y);
            GUI::SendMessage(GM_BUTTONUP, x, y);
        }

        // Pass the button release through to the mouse module
        else if (fMouseActive)
            pMouse->SetButton(anMouseButtons[uMsg_ & 0xf], false);

        break;
    }

    case WM_MOUSEWHEEL:
        if (GUI::IsActive())
        {
            GUI::SendMessage(GM_MOUSEWHEEL, (GET_WHEEL_DELTA_WPARAM(wParam_) < 0) ? 1 : -1);
            return true;
        }

        break;

    case WM_CHAR:
    case WM_KEYDOWN:
    {
        if (!GUI::IsActive())
        {
            // If escape is pressed, release mouse capture and stop any auto-typing
            if (wParam_ == VK_ESCAPE && GetOption(mouseesc))
            {
                Actions::Do(Action::ReleaseMouse);
                Keyin::Stop();
            }

            // Ignore key repeats for non-GUI keys
            return !!(lParam_ & 0x40000000);
        }

        // Determine the current shift states
        int nMods = HM_NONE;
        if (GetKeyState(VK_SHIFT) < 0)   nMods |= HM_SHIFT;
        if (GetKeyState(VK_CONTROL) < 0) nMods |= HM_CTRL;

        // Regular characters
        if (uMsg_ == WM_CHAR)
        {
            GUI::SendMessage(GM_CHAR, static_cast<int>(wParam_), nMods);
            break;
        }

        // Ctrl-letter (convert to lower-case)
        else if ((nMods & HM_CTRL) && wParam_ >= 'A' && wParam_ <= 'Z')
            GUI::SendMessage(GM_CHAR, int(wParam_ ^ ('a' ^ 'A')), nMods);

        // Ctrl-digit
        else if ((nMods & HM_CTRL) && wParam_ >= '0' && wParam_ <= '9')
            GUI::SendMessage(GM_CHAR, int(wParam_), nMods);

        // Keypad digits
        else if (wParam_ >= VK_NUMPAD0 && wParam_ <= VK_NUMPAD9)
            GUI::SendMessage(GM_CHAR, HK_KP0 + static_cast<int>(wParam_) - VK_NUMPAD0, nMods);

        // Keypad symbols
        else if (wParam_ >= VK_MULTIPLY && wParam_ <= VK_DIVIDE)
        {
            static int an[] = { HK_KPMULT, HK_KPPLUS, HK_RETURN, HK_KPMINUS, HK_KPDECIMAL, HK_KPDIVIDE };
            GUI::SendMessage(GM_CHAR, an[wParam_ - VK_MULTIPLY], nMods);
        }

        // Cursor keys + navigation cluster
        else if (wParam_ >= VK_PRIOR && wParam_ <= VK_DOWN)
        {
            static int an[] = { HK_PGUP, HK_PGDN, HK_END, HK_HOME, HK_LEFT, HK_UP, HK_RIGHT, HK_DOWN };
            GUI::SendMessage(GM_CHAR, an[wParam_ - VK_PRIOR], nMods);
        }

        // Delete
        else if (wParam_ == VK_DELETE)
            GUI::SendMessage(GM_CHAR, HK_DELETE, nMods);

        // Not processed here, but may come back through decoded as WM_CHAR
        else
            return true;

        return false;
    }
    }

    // Message not processed
    return false;
}


// Fill the supplied combo-box with the list of connected joysticks
void Input::FillJoystickCombo(HWND hwndCombo_)
{
    HRESULT hr;
    if (FAILED(hr = pdi->EnumDevices(DIDEVTYPE_JOYSTICK, EnumJoystickProc, reinterpret_cast<LPVOID>(hwndCombo_), DIEDFL_ATTACHEDONLY)))
        TRACE("!!! Failed to enumerate joystick devices ({:08x})\n", hr);
}


// Map a character to the native code and key modifiers needed to generate it
int Input::MapChar(int nChar_, int* pnMods_)
{
    if (!nChar_)
        return 0;

    // Regular character?
    if (nChar_ < HK_MIN)
    {
        // Convert from character to virtual keycode to raw scancode
        WORD wVirtual = VkKeyScanEx(nChar_, hkl);

        if (pnMods_)
        {
            int nMods = HM_NONE;
            if (wVirtual & 0x100) nMods |= HM_SHIFT;
            if (wVirtual & 0x200) nMods |= HM_CTRL;
            if (wVirtual & 0x400) nMods |= HM_ALT;
            *pnMods_ = nMods;
        }

        int nKey = MapVirtualKeyEx(wVirtual & 0xff, MAPVK_VK_TO_VSC, hkl);
        return nKey;
    }

    // Host keycode
    switch (nChar_)
    {
    case HK_LSHIFT:     return DIK_LSHIFT;
    case HK_RSHIFT:     return DIK_RSHIFT;
    case HK_LCTRL:      return DIK_LCONTROL;
    case HK_RCTRL:      return DIK_RCONTROL;
    case HK_LALT:       return DIK_LMENU;
    case HK_RALT:       return DIK_RMENU;
    case HK_LWIN:       return DIK_LWIN;
    case HK_RWIN:       return DIK_RWIN;

    case HK_LEFT:       return DIK_LEFT;
    case HK_RIGHT:      return DIK_RIGHT;
    case HK_UP:         return DIK_UP;
    case HK_DOWN:       return DIK_DOWN;

    case HK_KP0:        return DIK_NUMPAD0;
    case HK_KP1:        return DIK_NUMPAD1;
    case HK_KP2:        return DIK_NUMPAD2;
    case HK_KP3:        return DIK_NUMPAD3;
    case HK_KP4:        return DIK_NUMPAD4;
    case HK_KP5:        return DIK_NUMPAD5;
    case HK_KP6:        return DIK_NUMPAD6;
    case HK_KP7:        return DIK_NUMPAD7;
    case HK_KP8:        return DIK_NUMPAD8;
    case HK_KP9:        return DIK_NUMPAD9;

    case HK_F1:         return DIK_F1;
    case HK_F2:         return DIK_F2;
    case HK_F3:         return DIK_F3;
    case HK_F4:         return DIK_F4;
    case HK_F5:         return DIK_F5;
    case HK_F6:         return DIK_F6;
    case HK_F7:         return DIK_F7;
    case HK_F8:         return DIK_F8;
    case HK_F9:         return DIK_F9;
    case HK_F10:        return DIK_F10;
    case HK_F11:        return DIK_F11;
    case HK_F12:        return DIK_F12;

    case HK_CAPSLOCK:   return DIK_CAPITAL;
    case HK_NUMLOCK:    return DIK_NUMLOCK;
    case HK_KPPLUS:     return DIK_ADD;
    case HK_KPMINUS:    return DIK_SUBTRACT;
    case HK_KPMULT:     return DIK_MULTIPLY;
    case HK_KPDIVIDE:   return DIK_DIVIDE;
    case HK_KPENTER:    return DIK_NUMPADENTER;
    case HK_KPDECIMAL:  return DIK_DECIMAL;

    case HK_INSERT:     return DIK_INSERT;
    case HK_DELETE:     return DIK_DELETE;
    case HK_HOME:       return DIK_HOME;
    case HK_END:        return DIK_END;
    case HK_PGUP:       return DIK_PRIOR;
    case HK_PGDN:       return DIK_NEXT;

    case HK_ESC:        return DIK_ESCAPE;
    case HK_TAB:        return DIK_TAB;
    case HK_BACKSPACE:  return DIK_BACK;
    case HK_RETURN:     return DIK_RETURN;

    case HK_APPS:       return DIK_APPS;
    case HK_NONE:       return 0;
    }

    return 0;
}
