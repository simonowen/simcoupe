// Part of SimCoupe - A SAM Coupe emulator
//
// ODMenu.h: Owner-draw Win32 menus with images
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

#pragma once

#include <commctrl.h>

#ifndef RT_TOOLBAR
#define RT_TOOLBAR  MAKEINTRESOURCE(241)
#endif

const int SIGNATURE = 0x31415926;

struct MENUICON
{
    UINT uID;       // Menu command ID
    int nOffset;    // Offset into image map to use
};

struct MenuItem
{
    MenuItem() { dwSig = SIGNATURE; szText[0] = '\0'; nImage = -1; }

    bool IsOurs() const { return this && dwSig == SIGNATURE; }
    static MenuItem* GetItem(ULONG_PTR ulp_) { MenuItem* p = reinterpret_cast<MenuItem*>(ulp_); return p->IsOurs() ? p : nullptr; }

    DWORD   dwSig;
    char    szText[64];
    UINT    fType;
    bool    fDefault;
    int     nImage;
};

class OwnerDrawnMenu
{
public:
    OwnerDrawnMenu(HINSTANCE hinst_ = nullptr, int nId_ = 0, MENUICON* pIconMap_ = nullptr);
    virtual ~OwnerDrawnMenu();

    LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LRESULT* plResult_);

protected:
    void Cleanup();

    bool OnMeasureItem(LPMEASUREITEMSTRUCT lpms);
    bool OnDrawItem(LPDRAWITEMSTRUCT lpds);
    void OnInitMenuPopup(HMENU hmenu_, UINT nIndex_, BOOL bSysMenu_);
    void OnMenuSelect(UINT nItemID_, UINT nFlags_, HMENU hmenuSys_);
    LRESULT OnMenuChar(UINT nChar_, UINT nFlags_, HMENU hmenu_);

    void DrawMenuText(HDC hdc, LPRECT rc, LPCSTR text, bool fDisabled_);
    bool DrawCheck(HDC hdc, RECT rc, UINT uType, UINT uState_);
    void DrawGreyedImage(HDC hdc_, HIMAGELIST hil_, int i, int x, int y);
    void ConvertMenu(HMENU hmenu_, UINT nIndex_, BOOL fSysMenu_, bool fConvert_);

protected:
    HIMAGELIST m_hil = nullptr;
    MENUICON* m_pIconMap = nullptr;
    SIZE m_zButton{};

    SIZE m_zBorder{};
    HFONT m_hfont = nullptr;
    HFONT m_hfontBold = nullptr;

    int m_nConverted = 0;
    HMENU m_aConverted[64];
};
