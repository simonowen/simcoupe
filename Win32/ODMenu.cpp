// Part of SimCoupe - A SAM Coupe emulator
//
// ODMenu.cpp: Owner-draw Win32 menus with images
//
//  Copyright (c) 1999-2015 Simon Owen
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

// Derived from CoolMenu, written by Paul DiLascia
// See: http://www.microsoft.com/msj/0198/coolmenu.htm

#include "SimCoupe.h"
#include "ODMenu.h"

const int CXGAP = 2;            // Pixels between button and text
const int CXTEXTMARGIN = 2;     // Pixels after hilite to start text
const int CYTEXTMARGIN = 2;     // Pixels below hilite to start text


// Structure of RT_TOOLBAR resource
struct TOOLBARDATA
{
    WORD wVersion;      // version # should be 1
    WORD wWidth;        // width of one bitmap
    WORD wHeight;       // height of one bitmap
    WORD wItemCount;    // number of items
    WORD items[1];      // array of command IDs, actual size is wItemCount
};


OwnerDrawnMenu::OwnerDrawnMenu(HINSTANCE hinst_, int nId_, MENUICON* pIconMap_)
    : m_pIconMap(pIconMap_)
{
#ifndef NO_IMAGES
    HRSRC hrsrc;
    HGLOBAL hgres;
    TOOLBARDATA* ptbd;

    if (!hinst_) hinst_ = GetModuleHandle(nullptr);

    if ((hrsrc = FindResource(hinst_, MAKEINTRESOURCE(nId_), RT_TOOLBAR)) && (hgres = LoadResource(hinst_, hrsrc)) &&
        (ptbd = reinterpret_cast<TOOLBARDATA*>(LockResource(hgres))) && ptbd->wVersion == 1)
    {
        m_zButton.cx = ptbd->wWidth;
        m_zButton.cy = ptbd->wHeight;

        m_hil = nId_ ? ImageList_LoadBitmap(hinst_, MAKEINTRESOURCE(nId_), m_zButton.cx, 10, RGB(255, 0, 255)) : nullptr;
    }
#endif
}

OwnerDrawnMenu::~OwnerDrawnMenu()
{
    Cleanup();
}

void OwnerDrawnMenu::Cleanup()
{
    if (m_hfont) DeleteObject(m_hfont);
    if (m_hfontBold) DeleteObject(m_hfontBold);
    m_hfont = m_hfontBold = nullptr;
}


LRESULT OwnerDrawnMenu::WindowProc(HWND hwnd_, UINT uMsg_, WPARAM wParam_, LPARAM lParam_, LRESULT* plResult_)
{
    switch (uMsg_)
    {
    case WM_MEASUREITEM:
        *plResult_ = TRUE;
        return OnMeasureItem(reinterpret_cast<LPMEASUREITEMSTRUCT>(lParam_));

    case WM_DRAWITEM:
        *plResult_ = TRUE;
        return OnDrawItem(reinterpret_cast<LPDRAWITEMSTRUCT>(lParam_));

    case WM_INITMENUPOPUP:
        OnInitMenuPopup(reinterpret_cast<HMENU>(wParam_), LOWORD(lParam_), HIWORD(lParam_));
        break;

    case WM_MENUSELECT:
        OnMenuSelect(LOWORD(wParam_), HIWORD(wParam_), reinterpret_cast<HMENU>(lParam_));
        break;

    case WM_MENUCHAR:
        *plResult_ = OnMenuChar(LOWORD(wParam_), HIWORD(wParam_), reinterpret_cast<HMENU>(lParam_));
        return !!*plResult_;

    case WM_SYSCOLORCHANGE:
    case WM_SETTINGCHANGE:
        Cleanup();
        break;
    }

    return false;
}


bool OwnerDrawnMenu::OnMeasureItem(LPMEASUREITEMSTRUCT lpms)
{
    MenuItem* pmi = MenuItem::GetItem(lpms->itemData);
    if (lpms->CtlType != ODT_MENU || !pmi)
        return false;

    if (pmi->fType & MFT_SEPARATOR)
    {
        lpms->itemHeight = GetSystemMetrics(SM_CYMENU) >> 1;
        lpms->itemWidth = 0;
    }
    else
    {
        if (!m_hfont)
        {
            NONCLIENTMETRICS info = { sizeof(info) };
            SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(info), &info, 0);

            m_hfont = CreateFontIndirect(&info.lfMenuFont);
            info.lfMenuFont.lfWeight = FW_BOLD;
            m_hfontBold = CreateFontIndirect(&info.lfMenuFont);
        }

        RECT r = { 0,0,0,0 };
        HDC hdc = GetDC(nullptr);
        HGDIOBJ hfontOld = SelectObject(hdc, pmi->fDefault ? m_hfontBold : m_hfont);
        DrawText(hdc, pmi->szText, -1, &r, DT_SINGLELINE | DT_EXPANDTABS | DT_VCENTER | DT_CALCRECT);
        SelectObject(hdc, hfontOld);
        ReleaseDC(nullptr, hdc);

        // Standard menu height, or text height if larger
        m_zBorder.cx = m_zBorder.cy = lpms->itemHeight = std::max(static_cast<LONG>(GetSystemMetrics(SM_CYMENU)), m_zButton.cy);

#ifdef NO_IMAGES
        m_zBorder.cx = GetSystemMetrics(SM_CXMENUCHECK) + 1;
#endif

        // Text width, margins, button-text gap, button/margin gaps, minus check mark size fiddle
        lpms->itemWidth = (m_zBorder.cx << 1) + CXGAP + (CXTEXTMARGIN << 1) + r.right - r.left - (GetSystemMetrics(SM_CXMENUCHECK) - 1);
    }

    return true;
}

bool OwnerDrawnMenu::OnDrawItem(LPDRAWITEMSTRUCT lpds)
{
    MenuItem* pmi = MenuItem::GetItem(lpds->itemData);
    if (lpds->CtlType != ODT_MENU || !pmi)
        return false;

    HDC hdc = lpds->hDC;
    RECT r = lpds->rcItem;

    if (pmi->fType & MFT_SEPARATOR)
    {
        r.top += (r.bottom - r.top) >> 1;
        DrawEdge(hdc, &r, EDGE_ETCHED, BF_TOP);
    }
    else
    {
        bool fDisabled = (lpds->itemState & ODS_GRAYED) != 0;
        bool fSelected = (lpds->itemState & ODS_SELECTED) != 0;
        bool fChecked = (lpds->itemState & ODS_CHECKED) != 0;

        RECT rBorder = { r.left, r.top, r.left + m_zBorder.cx, r.top + m_zBorder.cy };

        int nBgCol = (fSelected && !fDisabled) ? COLOR_HIGHLIGHT : COLOR_MENU;
        if ((fSelected || lpds->itemAction == ODA_SELECT))
            FillRect(hdc, &r, GetSysColorBrush(nBgCol));

        if (fChecked && !fDisabled)
        {
#ifndef NO_IMAGES
            if (pmi->nImage != -1)
            {
                FillRect(hdc, &rBorder, GetSysColorBrush(COLOR_BTNHIGHLIGHT));
                FrameRect(hdc, &rBorder, GetSysColorBrush(COLOR_BTNSHADOW));
            }
            else
#endif
                DrawCheck(hdc, rBorder, pmi->fType, lpds->itemState);
        }

#ifndef NO_IMAGES
        if (pmi->nImage != -1)
        {
            int nX = rBorder.left + ((m_zBorder.cx - m_zButton.cx) >> 1);
            int nY = rBorder.top + ((m_zBorder.cy - m_zButton.cy) >> 1);

            if (fDisabled)
                DrawGreyedImage(hdc, m_hil, pmi->nImage, nX, nY);
            else
                ImageList_Draw(m_hil, pmi->nImage, hdc, nX, nY, ILD_TRANSPARENT);
        }
#endif

        RECT rText = r;
        rText.left += m_zBorder.cx + CXGAP + CXTEXTMARGIN;
        rText.right -= m_zBorder.cx;
        rText.top += CYTEXTMARGIN;

        SetBkMode(hdc, TRANSPARENT);
        DrawMenuText(hdc, &rText, pmi->szText, fDisabled);
    }

    return true;
}


void OwnerDrawnMenu::DrawMenuText(HDC hdc_, LPRECT lprc, LPCSTR pcsz_, bool fDisabled_)
{
    char sz[256];
    lstrcpyn(sz, pcsz_, sizeof(sz));

    char* psz = strchr(sz, '\t');
    if (psz)
    {
        *psz++ = '\0';

        // DSS_RIGHT doesn't seem to work, so calculate the shortcut position
        RECT r = { };
        DrawText(hdc_, psz, -1, &r, DT_SINGLELINE | DT_CALCRECT);

        DrawState(hdc_, nullptr, nullptr, (LPARAM)psz, lstrlen(psz),
            lprc->right - r.right, lprc->top, lprc->right, lprc->bottom,
            DST_PREFIXTEXT | (fDisabled_ ? DSS_DISABLED : 0));
    }

    DrawState(hdc_, nullptr, nullptr, (LPARAM)sz, lstrlen(sz),
        lprc->left, lprc->top, lprc->right, lprc->bottom,
        DST_PREFIXTEXT | (fDisabled_ ? DSS_DISABLED : 0));
}

bool OwnerDrawnMenu::DrawCheck(HDC hdc, RECT r, UINT uType, UINT uState_)
{
    RECT rBox = r;
    InflateRect(&rBox, 1, 1);
    FillRect(hdc, &rBox, GetSysColorBrush(COLOR_MENU));
    InflateRect(&rBox, -2, -2);
    FrameRect(hdc, &rBox, GetSysColorBrush(COLOR_HIGHLIGHT));

    int cx = GetSystemMetrics(SM_CXMENUCHECK), cy = GetSystemMetrics(SM_CYMENUCHECK);
    int x = r.left + ((r.right - r.left - cx + 1) / 2), y = r.top + ((r.bottom - r.top - cy + 1) / 2);
    RECT rCheck = { 0, 0, cx, cy };

    HDC hdcM = CreateCompatibleDC(hdc), hdc2 = CreateCompatibleDC(hdc);
    HBITMAP hbmpM = CreateCompatibleBitmap(hdc, cx, cy), hbmp2 = CreateCompatibleBitmap(hdc, cx, cy);
    HGDIOBJ hbmpOldM = SelectObject(hdcM, hbmpM), hbmpOld2 = SelectObject(hdc2, hbmp2);

    DrawFrameControl(hdcM, &rCheck, DFC_MENU, (uType & MFT_RADIOCHECK) ? DFCS_MENUBULLET : DFCS_MENUCHECK);
    FillRect(hdc2, &rCheck, GetSysColorBrush(COLOR_MENUTEXT));

    BitBlt(hdc, x, y, cx, cy, hdc2, 0, 0, SRCINVERT);
    BitBlt(hdc, x, y, cx, cy, hdcM, 0, 0, SRCAND);
    BitBlt(hdc, x, y, cx, cy, hdc2, 0, 0, SRCINVERT);

    SelectObject(hdc2, hbmpOld2); DeleteObject(hbmp2); DeleteDC(hdc2);
    SelectObject(hdcM, hbmpOldM); DeleteObject(hbmpM); DeleteDC(hdcM);

    return true;
}


#ifndef NO_IMAGES

void OwnerDrawnMenu::DrawGreyedImage(HDC hdc_, HIMAGELIST hil_, int i, int x, int y)
{
    IMAGEINFO info;
    ImageList_GetImageInfo(hil_, i, &info);
    int cx = info.rcImage.right - info.rcImage.left, cy = info.rcImage.bottom - info.rcImage.top;

    HDC hdcMem = CreateCompatibleDC(hdc_);
    HBITMAP hbmp = CreateCompatibleBitmap(hdc_, cx, cy);
    HGDIOBJ hbmpOld = SelectObject(hdcMem, hbmp);

    PatBlt(hdcMem, 0, 0, cx, cy, WHITENESS);
    ImageList_Draw(hil_, i, hdcMem, 0, 0, ILD_TRANSPARENT);

    HGDIOBJ hbrOld = SelectObject(hdc_, GetSysColorBrush(COLOR_3DSHADOW));
    BitBlt(hdc_, x + 1, y + 1, cx, cy, hdcMem, 0, 0, 0xb8074a);
    SelectObject(hdc_, hbrOld);

    SelectObject(hdcMem, hbmpOld);
}

#endif


void OwnerDrawnMenu::OnInitMenuPopup(HMENU hmenu_, UINT nIndex_, BOOL bSysMenu_)
{
    ConvertMenu(hmenu_, nIndex_, bSysMenu_, true);
}

LRESULT OwnerDrawnMenu::OnMenuChar(UINT nChar_, UINT nFlags_, HMENU hmenu_)
{
    int anMatches[64], nFound = 0, nCurrent = -1;

    int nItems = GetMenuItemCount(hmenu_), i;

    for (i = 0; i < nItems && nFound < 64; i++)
    {
        MENUITEMINFO info = { sizeof(info) };
        info.fMask = MIIM_TYPE | MIIM_STATE | MIIM_DATA;
        GetMenuItemInfo(hmenu_, i, TRUE, &info);

        MenuItem* pmi = MenuItem::GetItem(info.dwItemData);
        if ((info.fType & MFT_OWNERDRAW) && pmi->IsOurs())
        {
            char* pszAmp = nullptr;
            for (char* psz = pmi->szText; *psz; psz = CharNext(psz))
                if (*psz == '&')
                    pszAmp = psz;

            if (pszAmp && toupper(nChar_) == toupper(pszAmp[1]))
                anMatches[nFound++] = i;
        }

        if (info.fState & MFS_HILITE)
            nCurrent = i;
    }

    if (nFound == 1)
        return MAKELONG(anMatches[0], MNC_EXECUTE);
    else if (nFound)
    {
        for (i = 0; i < nFound; i++)
            if (anMatches[i] > nCurrent)
                return MAKELONG(anMatches[i], MNC_SELECT);
    }

    return 0;
}

void OwnerDrawnMenu::OnMenuSelect(UINT nItemID_, UINT nFlags_, HMENU hmenuSys_)
{
    if (!hmenuSys_ && nFlags_ == 0xFFFF)
    {
        while (m_nConverted)
            ConvertMenu(m_aConverted[--m_nConverted], 0, FALSE, false);
    }
}


void OwnerDrawnMenu::ConvertMenu(HMENU hmenu_, UINT nIndex_, BOOL fSysMenu_, bool fConvert_)
{
    UINT uDefault = GetMenuDefaultItem(hmenu_, FALSE, GMDI_USEDISABLED);

    UINT nItems = GetMenuItemCount(hmenu_);

    for (UINT i = 0; i < nItems; i++)
    {
        char szItem[256] = "";

        MENUITEMINFO info = { sizeof(info) };
        info.fMask = MIIM_SUBMENU | MIIM_DATA | MIIM_ID | MIIM_TYPE;
        info.dwTypeData = szItem;
        info.cch = sizeof(szItem);
        GetMenuItemInfo(hmenu_, i, TRUE, &info);
        MenuItem* pmi = MenuItem::GetItem(info.dwItemData);

        // Reject foreign owner-drawn items
        if (info.dwItemData && !pmi)
            continue;

        // Ignore system menu items
        if (fSysMenu_ && (!info.wID || info.wID >= 0xf000))
            continue;

        // Nothing to change, yet
        info.fMask = 0;

        if (fConvert_)
        {
            if (!(info.fType & MFT_OWNERDRAW))
            {
                info.fType |= MFT_OWNERDRAW;
                info.fMask |= MIIM_TYPE;

                if (!pmi)
                {
                    info.dwItemData = reinterpret_cast<ULONG_PTR>(pmi = new MenuItem);
                    info.fMask |= MIIM_DATA;
                    pmi->fType = info.fType;

#ifndef NO_IMAGES
                    for (int i = 0; m_hil && m_pIconMap[i].uID; i++)
                    {
                        if (m_pIconMap[i].uID == info.wID)
                            pmi->nImage = m_pIconMap[i].nOffset;
                    }
#endif
                }

                lstrcpyn(pmi->szText, (info.fType & MFT_SEPARATOR) ? "" : info.dwTypeData, sizeof(pmi->szText));
                pmi->fDefault = (info.wID == uDefault);
            }

            for (int i = 0; i <= m_nConverted; i++)
            {
                if (i == m_nConverted)
                    m_aConverted[m_nConverted++] = hmenu_;
                else if (m_aConverted[i] != hmenu_)
                    continue;

                break;
            }
        }
        else
        {
            if (info.fType & MFT_OWNERDRAW)
            {
                info.fType &= ~MFT_OWNERDRAW;
                info.fMask |= MIIM_TYPE;
                lstrcpyn(szItem, pmi->szText, sizeof(szItem));
            }

            if (pmi)
            {
                info.dwItemData = 0;
                info.fMask |= MIIM_DATA;
                delete pmi;
            }

            if (info.fMask & MIIM_TYPE)
            {
                info.dwTypeData = szItem;
                info.cch = lstrlen(szItem);
            }
        }

        if (info.fMask)
            SetMenuItemInfo(hmenu_, i, TRUE, &info);
    }
}
