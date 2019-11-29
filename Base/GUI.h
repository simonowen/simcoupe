// Part of SimCoupe - A SAM Coupe emulator
//
// GUI.h: GUI and controls for on-screen interface
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

#pragma once

#include "GUIIcons.h"
#include "SAMIO.h"
#include "Screen.h"

const int GM_MOUSE_MESSAGE = 0x40000000;
const int GM_KEYBOARD_MESSAGE = 0x20000000;
const int GM_TYPE_MASK = 0x60000000;

const int GM_BUTTONUP = GM_MOUSE_MESSAGE | 1;
const int GM_BUTTONDOWN = GM_MOUSE_MESSAGE | 2;
const int GM_BUTTONDBLCLK = GM_MOUSE_MESSAGE | 3;
const int GM_MOUSEMOVE = GM_MOUSE_MESSAGE | 4;
const int GM_MOUSEWHEEL = GM_MOUSE_MESSAGE | 5;

const int GM_CHAR = GM_KEYBOARD_MESSAGE | 1;

const DWORD DOUBLE_CLICK_TIME = 400;    // Under 400ms between consecutive clicks counts as a double-click
const int DOUBLE_CLICK_THRESHOLD = 5;   // Distance between clicks for double-clicks to be recognised


class CWindow;
class CDialog;

class GUI
{
public:
    static bool IsActive() { return s_pGUI != nullptr; }
    static bool IsModal();

public:
    static bool Start(CWindow* pGUI_);
    static void Stop();

    static void Draw(CScreen* pScreen_);
    static bool SendMessage(int nMessage_, int nParam1_ = 0, int nParam2_ = 0);
    static void Delete(CWindow* pWindow_);

protected:
    static CWindow* s_pGUI;
    static std::queue<CWindow*> s_garbageQueue;
    static std::stack<CWindow*> s_dialogStack;
    static int s_nX, s_nY;

    friend class CWindow;
    friend class CDialog;     // only needed for test cross-hair to access cursor position
};

////////////////////////////////////////////////////////////////////////////////

// Control types, as returned by GetType() from any base CWindow pointer
enum {
    ctUnknown, ctText, ctButton, ctImageButton, ctCheckBox, ctComboBox, ctEdit, ctRadio,
    ctMenu, ctImage, ctFrame, ctListView, ctDialog, ctMessageBox
};


class CWindow
{
public:
    CWindow(CWindow* pParent_ = nullptr, int nX_ = 0, int nY_ = 0, int nWidth_ = 0, int nHeight_ = 0, int nType_ = ctUnknown);
    CWindow(const CWindow&) = delete;
    void operator= (const CWindow&) = delete;
    virtual ~CWindow();

public:
    bool IsEnabled() const { return m_fEnabled; }
    bool IsOver() const { return m_fHover; }
    bool IsActive() const { return m_pParent && m_pParent->m_pActive == this; }
    int GetType() const { return m_nType; }
    int GetWidth() const { return m_nWidth; }
    int GetHeight() const { return m_nHeight; }
    int GetTextWidth(size_t nOffset_ = 0, size_t nMaxLength_ = -1) const;
    int GetTextWidth(const char* pcsz_) const;

    CWindow* GetParent() { return m_pParent; }
    CWindow* GetChildren() { return m_pChildren; }
    CWindow* GetSiblings() { return GetParent() ? GetParent()->GetChildren() : nullptr; }
    CWindow* GetGroup();

    CWindow* GetNext(bool fWrap_ = false);
    CWindow* GetPrev(bool fWrap_ = false);

    void SetParent(CWindow* pParent_);
    void Destroy();
    void Enable(bool fEnable_ = true) { m_fEnabled = fEnable_; }
    void Move(int nX_, int nY_);
    void Offset(int ndX_, int ndY_);
    void SetSize(int nWidth_, int nHeight_);
    void Inflate(int ndW_, int ndH_);

public:
    virtual bool IsTabStop() const { return false; }

    virtual const char* GetText() const { return m_pszText; }
    virtual const GUIFONT* GetFont() const { return m_pFont; }
    virtual UINT GetValue() const;
    virtual void SetText(const char* pcszText_);
    virtual void SetFont(const GUIFONT* pFont_) { m_pFont = pFont_; }
    virtual void SetValue(UINT u_);

    virtual void Activate();
    virtual bool HitTest(int nX_, int nY_);
    virtual void EraseBackground(CScreen* /*pScreen_*/) { }
    virtual void Draw(CScreen* pScreen_) = 0;

    virtual void NotifyParent(int nParam_ = 0);
    virtual void OnNotify(CWindow* /*pWindow_*/, int /*nParam_*/) { }
    virtual bool OnMessage(int nMessage_, int nParam1_ = 0, int nParam2_ = 0);

protected:
    void RemoveChild();
    void MoveRecurse(CWindow* pWindow_, int ndX_, int ndY_);
    bool RouteMessage(int nMessage_, int nParam1_, int nParam2_);

protected:
    int m_nX = 0, m_nY = 0;
    int m_nWidth = 0, m_nHeight = 0;
    int m_nType = 0;

    char* m_pszText = nullptr;
    const GUIFONT* m_pFont = nullptr;

    bool m_fEnabled = true;
    bool m_fHover = false;

    CWindow* m_pParent = nullptr;
    CWindow* m_pChildren = nullptr;
    CWindow* m_pNext = nullptr;
    CWindow* m_pActive = nullptr;

    friend class GUI;
    friend class CDialog;
};


class CTextControl : public CWindow
{
public:
    CTextControl(CWindow* pParent_ = nullptr, int nX_ = 0, int nY_ = 0, const char* pcszText_ = "", BYTE bColour = WHITE, BYTE bBackColour = 0);

public:
    void Draw(CScreen* pScreen_) override;
    void SetTextAndColour(const char* pcszText_, BYTE bColour_);

protected:
    BYTE m_bColour = WHITE, m_bBackColour = 0;
};


class CButton : public CWindow
{
public:
    CButton(CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);

public:
    bool IsTabStop() const override { return true; }
    bool IsPressed() const { return m_fPressed; }

    void Draw(CScreen* pScreen_) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    bool m_fPressed = false;
};


class CTextButton : public CButton
{
public:
    CTextButton(CWindow* pParent_, int nX_, int nY_, const char* pcszText_ = "", int nMinWidth_ = 0);

public:
    void SetText(const char* pcszText_) override;
    void Draw(CScreen* pScreen_) override;

protected:
    int m_nMinWidth = 0;
};


class CImageButton : public CButton
{
public:
    CImageButton(CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_,
        const GUI_ICON* pIcon_, int nDX_ = 0, int nDY_ = 0);
    CImageButton(const CImageButton&) = delete;
    void operator= (const CImageButton&) = delete;

public:
    void Draw(CScreen* pScreen_) override;

protected:
    const GUI_ICON* m_pIcon = nullptr;
    int m_nDX = 0, m_nDY = 0;
};


class CUpButton : public CButton
{
public:
    CUpButton(CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);
    void Draw(CScreen* pScreen_) override;
};

class CDownButton : public CButton
{
public:
    CDownButton(CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);
    void Draw(CScreen* pScreen_) override;
};


class CCheckBox : public CWindow
{
public:
    CCheckBox(CWindow* pParent_, int nX_, int nY_, const char* pcszText_ = "", BYTE bColour_ = WHITE, BYTE bBackColour_ = 0);

public:
    bool IsTabStop() const override { return true; }
    bool IsChecked() const { return m_fChecked; }
    void SetChecked(bool fChecked_ = true) { m_fChecked = fChecked_; }

    void SetText(const char* pcszText_) override;
    void Draw(CScreen* pScreen_) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    bool m_fChecked = false;
    BYTE m_bColour = 0, m_bBackColour = 0;
};


class CEditControl : public CWindow
{
public:
    CEditControl(CWindow* pParent_, int nX_, int nY_, int nWidth_, const char* pcszText_ = "");
    CEditControl(CWindow* pParent_, int nX_, int nY_, int nWidth_, UINT u_);

public:
    bool IsTabStop() const override { return true; }
    void Activate() override;

    void SetSelectedText(const char* pcszText_, bool fSelected_);
    void Draw(CScreen* pScreen_) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    size_t m_nViewOffset = 0;
    size_t m_nCaretStart = 0, m_nCaretEnd = 0;
    DWORD m_dwCaretTime = 0;
};

class CNumberEditControl : public CEditControl
{
public:
    using CEditControl::CEditControl;

    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;
};


class CRadioButton : public CWindow
{
public:
    CRadioButton(CWindow* pParent_ = nullptr, int nX_ = 0, int nY_ = 0, const char* pcszText_ = "", int nWidth_ = 0);

public:
    bool IsTabStop() const override { return IsSelected(); }
    bool IsSelected() const { return m_fSelected; }
    void Select(bool fSelected_ = true);
    void SetText(const char* pcszText_) override;

    void Draw(CScreen* pScreen_) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    bool m_fSelected = false;
};


class CMenu : public CWindow
{
public:
    CMenu(CWindow* pParent_ = nullptr, int nX_ = 0, int nY_ = 0, const char* pcszText_ = "");

public:
    int GetSelected() const { return m_nSelected; }
    void Select(int nItem_);
    void SetText(const char* pcszText_) override;

    void Draw(CScreen* pScreen_) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    int m_nItems = 0;
    int m_nSelected = -1;
    bool m_fPressed = false;
};


class CDropList : public CMenu
{
public:
    CDropList(CWindow* pParent_ = nullptr, int nX_ = 0, int nY_ = 0, const char* pcszText_ = "", int nMinWidth_ = 0);

public:
    void SetText(const char* pcszText_) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    int m_nMinWidth = 0;
};


class CComboBox : public CWindow
{
public:
    CComboBox(CWindow* pParent_, int nX_, int nY_, const char* pcszText_, int nWidth_);
    CComboBox(const CComboBox&) = delete;
    void operator= (const CComboBox&) = delete;

public:
    bool IsTabStop() const override { return true; }
    int GetSelected() const { return m_nSelected; }
    const char* GetSelectedText();
    void Select(int nSelected_);
    void Select(const char* pcszItem_);
    void SetText(const char* pcszText_) override;

    void Draw(CScreen* pScreen_) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;
    void OnNotify(CWindow* pWindow_, int nParam_) override;

protected:
    int m_nItems = 0, m_nSelected = 0;
    bool m_fPressed = false;
    CDropList* m_pDropList = nullptr;
};


class CScrollBar : public CWindow
{
public:
    CScrollBar(CWindow* pParent_, int nX_, int nY_, int nHeight, int nMaxPos_, int nStep_ = 1);
    CScrollBar(const CScrollBar&) = delete;
    void operator= (const CScrollBar&) = delete;

public:
    bool IsTabStop() const override { return true; }
    int GetPos() const { return m_nPos; }
    void SetPos(int nPosition_);
    void SetMaxPos(int nMaxPos_);

    void Draw(CScreen* pScreen_) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;
    void OnNotify(CWindow* pWindow_, int nParam_) override;

protected:
    int m_nPos = 0, m_nMaxPos = 0, m_nStep = 0;
    int m_nScrollHeight = 0, m_nThumbSize = 0;
    bool m_fDragging = false;
    CButton* m_pUp = nullptr;
    CButton* m_pDown = nullptr;
};


class CIconControl : public CWindow
{
public:
    CIconControl(CWindow* pParent_, int nX_, int nY_, const GUI_ICON* pIcon_);
    CIconControl(const CIconControl&) = delete;
    void operator= (const CIconControl&) = delete;

public:
    void Draw(CScreen* pScreen_) override;

protected:
    const GUI_ICON* m_pIcon = nullptr;
};


class CFrameControl : public CWindow
{
public:
    CFrameControl(CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_ = WHITE, BYTE bFill_ = 0);

public:
    bool HitTest(int /*nX_*/, int /*nY_*/) override { return false; }
    void Draw(CScreen* pScreen_) override;

public:
    BYTE m_bColour = 0, m_bFill = 0;
};


class CListViewItem
{
public:
    CListViewItem(const GUI_ICON* pIcon_, const char* pcszLabel_, CListViewItem* pNext_ = nullptr) :
        m_pIcon(pIcon_), m_pszLabel(nullptr), m_pNext(pNext_) {
        m_pszLabel = strdup(pcszLabel_);
    }
    CListViewItem(const CListViewItem&) = delete;
    void operator= (const CListViewItem&) = delete;
    virtual ~CListViewItem() { free(m_pszLabel); }

public:
    const GUI_ICON* m_pIcon = nullptr;
    char* m_pszLabel = nullptr;
    CListViewItem* m_pNext = nullptr;
};

class CListView : public CWindow
{
public:
    CListView(CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_, int nItemOffset = 0);
    CListView(const CListView&) = delete;
    void operator= (const CListView&) = delete;
    ~CListView();

public:
    bool IsTabStop() const override { return true; }

    int GetSelected() const { return m_nSelected; }
    void Select(int nItem_);

    const CListViewItem* GetItem(int nItem_ = -1) const;
    int FindItem(const char* pcszLabel_, int nStart_ = 0);
    void SetItems(CListViewItem* pItems_);

    void EraseBackground(CScreen* pScreen_) override;
    void Draw(CScreen* pScreen_) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

    virtual void DrawItem(CScreen* pScreen_, int nItem_, int nX_, int nY_, const CListViewItem* pItem_);

protected:
    int m_nItems = 0, m_nSelected = 0, m_nHoverItem = 0;
    int m_nAcross = 0, m_nDown = 0, m_nItemOffset = 0;

    CListViewItem* m_pItems = nullptr;
    CScrollBar* m_pScrollBar = nullptr;
};


class CDialog : public CWindow
{
public:
    CDialog(CWindow* pParent_, int nWidth_, int nHeight_, const char* pcszCaption_);
    ~CDialog();

public:
    bool IsActiveDialog() const;
    void SetColours(int nTitle_, int nBody_) { m_nTitleColour = nTitle_; m_nBodyColour = nBody_; }

    void Centre();
    void Activate() override;
    bool HitTest(int nX_, int nY_) override;
    void Draw(CScreen* pScreen_) override;
    void EraseBackground(CScreen* pScreen_) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    bool m_fDragging = false;
    int m_nDragX = 0, m_nDragY = 0;
    int m_nTitleColour = 0, m_nBodyColour = 0;
};

////////////////////////////////////////////////////////////////////////////////

enum { mbOk, mbOkCancel, mbYesNo, mbYesNoCancel, mbRetryCancel, mbInformation = 0x10, mbWarning = 0x20, mbError = 0x30 };

class CMessageBox : public CDialog
{
public:
    CMessageBox(CWindow* pParent_, const char* pcszBody_, const char* pcszCaption_, int nFlags_);
    CMessageBox(const CMessageBox&) = delete;
    void operator= (const CMessageBox&) = delete;
    ~CMessageBox() { if (m_pszBody) free(m_pszBody); }

public:
    void OnNotify(CWindow* /*pWindow_*/, int /*nParam_*/) override { Destroy(); }
    void Draw(CScreen* pScreen_) override;

protected:
    int m_nLines = 0;
    char* m_pszBody = nullptr;
    CIconControl* m_pIcon = nullptr;
};

class CFileView : public CListView
{
public:
    CFileView(CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);
    CFileView(const CFileView&) = delete;
    void operator= (const CFileView&) = delete;
    ~CFileView();

public:
    const char* GetFullPath() const;
    const char* GetPath() const { return m_pszPath; }
    const char* GetFilter() const { return m_pszFilter; }
    void SetPath(const char* pcszPath_);
    void SetFilter(const char* pcszFilter_);
    void ShowHidden(bool fShow_);

    void Refresh();
    void NotifyParent(int nParam_) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

    static const GUI_ICON* GetFileIcon(const char* pcszFile_);

protected:
    char* m_pszPath = nullptr;
    char* m_pszFilter = nullptr;
    bool m_fShowHidden = false;
};
