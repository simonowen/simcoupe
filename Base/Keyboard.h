// Part of SimCoupe - A SAM Coupe emulator
//
// Keyboard.h: Common keyboard handling
//
//  Copyright (c) 1999-2014  Simon Owen
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

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "IO.h"

namespace Keyboard
{
bool Init(bool fFirstInit_ = false);
void Exit(bool fReInit_ = false);

void Update();
void Purge();

void SetKey(int nCode_, bool fPressed_, int nMods_ = 0, int nChar_ = 0);
}


// Helper macros for SAM keyboard matrix manipulation
inline bool IsSamKeyPressed(int k) { return !(keybuffer[(k) >> 3] & (1 << ((k) & 7))); }
inline void PressSamKey(int k) { keybuffer[k >> 3] &= ~(1 << (k & 7)); }
inline void ReleaseSamKey(int k) { keybuffer[k >> 3] |= (1 << (k & 7)); }
inline void ToggleSamKey(int k) { keybuffer[k >> 3] ^= (1 << (k & 7)); }
inline void ReleaseAllSamKeys() { memset(keybuffer, 0xff, sizeof(keybuffer)); }

// Key constants used with the key macros above
enum eSamKey
{
    SK_MIN = 0, SK_MINMINUS1 = SK_MIN - 1,
    SK_SHIFT, SK_Z, SK_X, SK_C, SK_V, SK_F1, SK_F2, SK_F3,
    SK_A, SK_S, SK_D, SK_F, SK_G, SK_F4, SK_F5, SK_F6,
    SK_Q, SK_W, SK_E, SK_R, SK_T, SK_F7, SK_F8, SK_F9,
    SK_1, SK_2, SK_3, SK_4, SK_5, SK_ESCAPE, SK_TAB, SK_CAPS,
    SK_0, SK_9, SK_8, SK_7, SK_6, SK_MINUS, SK_PLUS, SK_DELETE,
    SK_P, SK_O, SK_I, SK_U, SK_Y, SK_EQUALS, SK_QUOTES, SK_F0,
    SK_RETURN, SK_L, SK_K, SK_J, SK_H, SK_SEMICOLON, SK_COLON, SK_EDIT,
    SK_SPACE, SK_SYMBOL, SK_M, SK_N, SK_B, SK_COMMA, SK_PERIOD, SK_INV,
    SK_CONTROL, SK_UP, SK_DOWN, SK_LEFT, SK_RIGHT, SK_NONE, SK_MAX = SK_NONE
};

enum eHostKey
{
    HK_BACKSPACE = '\b', HK_TAB = '\t', HK_RETURN = '\r', HK_ESC = '\x1b', HK_SPACE = ' ',

    HK_LSHIFT = 256, HK_RSHIFT, HK_LCTRL, HK_RCTRL, HK_LALT, HK_RALT, HK_LWIN, HK_RWIN,
    HK_LEFT, HK_RIGHT, HK_UP, HK_DOWN,
    HK_KP0, HK_KP1, HK_KP2, HK_KP3, HK_KP4, HK_KP5, HK_KP6, HK_KP7, HK_KP8, HK_KP9,
    HK_F1, HK_F2, HK_F3, HK_F4, HK_F5, HK_F6, HK_F7, HK_F8, HK_F9, HK_F10, HK_F11, HK_F12,
    HK_CAPSLOCK, HK_NUMLOCK, HK_KPPLUS, HK_KPMINUS, HK_KPMULT, HK_KPDIVIDE, HK_KPENTER, HK_KPDECIMAL,
    HK_PRINT, HK_SCROLL, HK_PAUSE, HK_INSERT, HK_DELETE, HK_HOME, HK_END, HK_PGUP, HK_PGDN,
    HK_APPS, HK_SECTION,
    HK_NONE, HK_MAX = HK_NONE, HK_MIN = HK_LSHIFT
};

enum eHostMods
{
    HM_NONE = 0x00, HM_LSHIFT = 0x01, HM_RSHIFT = 0x02, HM_LCTRL = 0x04, HM_RCTRL = 0x08, HM_LALT = 0x10, HM_RALT = 0x20,
    HM_SHIFT = (HM_LSHIFT | HM_RSHIFT), HM_CTRL = (HM_LCTRL | HM_RCTRL), HM_ALT = (HM_LALT | HM_RALT)
};

#endif
