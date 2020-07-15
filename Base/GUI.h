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
#include "FrameBuffer.h"

const int GM_MOUSE_MESSAGE = 0x40000000;
const int GM_KEYBOARD_MESSAGE = 0x20000000;
const int GM_TYPE_MASK = 0x60000000;

const int GM_BUTTONUP = GM_MOUSE_MESSAGE | 1;
const int GM_BUTTONDOWN = GM_MOUSE_MESSAGE | 2;
const int GM_BUTTONDBLCLK = GM_MOUSE_MESSAGE | 3;
const int GM_MOUSEMOVE = GM_MOUSE_MESSAGE | 4;
const int GM_MOUSEWHEEL = GM_MOUSE_MESSAGE | 5;

const int GM_CHAR = GM_KEYBOARD_MESSAGE | 1;

const uint32_t DOUBLE_CLICK_TIME = 400;    // Under 400ms between consecutive clicks counts as a double-click
const int DOUBLE_CLICK_THRESHOLD = 5;   // Distance between clicks for double-clicks to be recognised


class Window;
class Dialog;

class GUI
{
public:
    static bool IsActive() { return s_pGUI != nullptr; }
    static bool IsModal();

public:
    static bool Start(Window* pGUI_);
    static void Stop();

    static void Draw(FrameBuffer& fb);
    static bool SendMessage(int nMessage_, int nParam1_ = 0, int nParam2_ = 0);
    static void Delete(Window* pWindow_);

protected:
    static Window* s_pGUI;
    static std::queue<Window*> s_garbageQueue;
    static std::stack<Window*> s_dialogStack;
    static int s_nX, s_nY;

    friend class Window;
    friend class Dialog;     // only needed for test cross-hair to access cursor position
};

////////////////////////////////////////////////////////////////////////////////

// Control types, as returned by GetType() from any base Window pointer
enum {
    ctUnknown, ctText, ctButton, ctImageButton, ctCheckBox, ctComboBox, ctEdit, ctRadio,
    ctMenu, ctImage, ctFrame, ctListView, ctDialog, ctMessageBox
};


class Window
{
public:
    Window(Window* pParent_ = nullptr, int nX_ = 0, int nY_ = 0, int nWidth_ = 0, int nHeight_ = 0, int nType_ = ctUnknown);
    Window(const Window&) = delete;
    void operator= (const Window&) = delete;
    virtual ~Window();

public:
    bool IsEnabled() const { return m_fEnabled; }
    bool IsOver() const { return m_fHover; }
    bool IsActive() const { return m_pParent && m_pParent->m_pActive == this; }
    int GetType() const { return m_nType; }
    int GetWidth() const { return m_nWidth; }
    int GetHeight() const { return m_nHeight; }
    int GetTextWidth(size_t offset=0, size_t max_length=-1) const;
    int GetTextWidth(const char* pcsz_) const;

    Window* GetParent() { return m_pParent; }
    Window* GetChildren() { return m_pChildren; }
    Window* GetSiblings() { return GetParent() ? GetParent()->GetChildren() : nullptr; }
    Window* GetGroup();

    Window* GetNext(bool fWrap_ = false);
    Window* GetPrev(bool fWrap_ = false);

    void SetParent(Window* pParent_);
    void Destroy();
    void Enable(bool fEnable_ = true) { m_fEnabled = fEnable_; }
    void Move(int nX_, int nY_);
    void Offset(int ndX_, int ndY_);
    void SetSize(int nWidth_, int nHeight_);
    void Inflate(int ndW_, int ndH_);

public:
    virtual bool IsTabStop() const { return false; }

    virtual const char* GetText() const { return m_pszText; }
    virtual std::shared_ptr<Font> GetFont() const { return m_pFont; }
    virtual unsigned int GetValue() const;
    virtual void SetText(const char* pcszText_);
    virtual void SetFont(std::shared_ptr<Font>& font) { m_pFont = font; }
    virtual void SetValue(unsigned int u_);

    virtual void Activate();
    virtual bool HitTest(int nX_, int nY_);
    virtual void EraseBackground(FrameBuffer& /*fb*/) { }
    virtual void Draw(FrameBuffer& fb) = 0;

    virtual void NotifyParent(int nParam_ = 0);
    virtual void OnNotify(Window* /*pWindow_*/, int /*nParam_*/) { }
    virtual bool OnMessage(int nMessage_, int nParam1_ = 0, int nParam2_ = 0);

protected:
    void MoveRecurse(Window* pWindow_, int ndX_, int ndY_);
    bool RouteMessage(int nMessage_, int nParam1_, int nParam2_);

protected:
    int m_nX = 0, m_nY = 0;
    int m_nWidth = 0, m_nHeight = 0;
    int m_nType = 0;

    char* m_pszText = nullptr;
    std::shared_ptr<Font> m_pFont{ sGUIFont };

    bool m_fEnabled = true;
    bool m_fHover = false;

    Window* m_pParent = nullptr;
    Window* m_pChildren = nullptr;
    Window* m_pNext = nullptr;
    Window* m_pActive = nullptr;

    friend class GUI;
    friend class Dialog;
};


class TextControl : public Window
{
public:
    TextControl(Window* pParent_ = nullptr, int nX_ = 0, int nY_ = 0, const char* pcszText_ = "", uint8_t bColour = WHITE, uint8_t bBackColour = 0);

public:
    void Draw(FrameBuffer& fb) override;
    void SetTextAndColour(const char* pcszText_, uint8_t bColour_);

protected:
    uint8_t m_bColour = WHITE, m_bBackColour = 0;
};


class Button : public Window
{
public:
    Button(Window* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);

public:
    bool IsTabStop() const override { return true; }
    bool IsPressed() const { return m_fPressed; }

    void Draw(FrameBuffer& fb) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    bool m_fPressed = false;
};


class TextButton : public Button
{
public:
    TextButton(Window* pParent_, int nX_, int nY_, const char* pcszText_ = "", int nMinWidth_ = 0);

public:
    void SetText(const char* pcszText_) override;
    void Draw(FrameBuffer& fb) override;

protected:
    int m_nMinWidth = 0;
};


class ImageButton : public Button
{
public:
    ImageButton(Window* pParent_, int nX_, int nY_, int nWidth_, int nHeight_,
        const GUI_ICON* pIcon_, int nDX_ = 0, int nDY_ = 0);
    ImageButton(const ImageButton&) = delete;
    void operator= (const ImageButton&) = delete;

public:
    void Draw(FrameBuffer& fb) override;

protected:
    const GUI_ICON* m_pIcon = nullptr;
    int m_nDX = 0, m_nDY = 0;
};


class UpButton : public Button
{
public:
    UpButton(Window* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);
    void Draw(FrameBuffer& fb) override;
};

class DownButton : public Button
{
public:
    DownButton(Window* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);
    void Draw(FrameBuffer& fb) override;
};


class CheckBox : public Window
{
public:
    CheckBox(Window* pParent_, int nX_, int nY_, const char* pcszText_ = "", uint8_t bColour_ = WHITE, uint8_t bBackColour_ = 0);

public:
    bool IsTabStop() const override { return true; }
    bool IsChecked() const { return m_fChecked; }
    void SetChecked(bool fChecked_ = true) { m_fChecked = fChecked_; }

    void SetText(const char* pcszText_) override;
    void Draw(FrameBuffer& fb) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    bool m_fChecked = false;
    uint8_t m_bColour = 0, m_bBackColour = 0;
};


class EditControl : public Window
{
public:
    EditControl(Window* pParent_, int nX_, int nY_, int nWidth_, const char* pcszText_ = "");
    EditControl(Window* pParent_, int nX_, int nY_, int nWidth_, unsigned int u_);

public:
    bool IsTabStop() const override { return true; }
    void Activate() override;

    void SetText(const char* pcszText_) override;
    void SetSelectedText(const char* pcszText_, bool fSelected_);
    void Draw(FrameBuffer& fb) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    size_t m_nViewOffset = 0;
    size_t m_nCaretStart = 0, m_nCaretEnd = 0;
    uint32_t m_dwCaretTime = 0;
};

class NumberEditControl : public EditControl
{
public:
    using EditControl::EditControl;

    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;
};


class RadioButton : public Window
{
public:
    RadioButton(Window* pParent_ = nullptr, int nX_ = 0, int nY_ = 0, const char* pcszText_ = "", int nWidth_ = 0);

public:
    bool IsTabStop() const override { return IsSelected(); }
    bool IsSelected() const { return m_fSelected; }
    void Select(bool fSelected_ = true);
    void SetText(const char* pcszText_) override;

    void Draw(FrameBuffer& fb) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    bool m_fSelected = false;
};


class Menu : public Window
{
public:
    Menu(Window* pParent_ = nullptr, int nX_ = 0, int nY_ = 0, const char* pcszText_ = "");

public:
    int GetSelected() const { return m_nSelected; }
    void Select(int nItem_);
    void SetText(const char* pcszText_) override;

    void Draw(FrameBuffer& fb) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    int m_nItems = 0;
    int m_nSelected = -1;
    bool m_fPressed = false;
};


class DropList : public Menu
{
public:
    DropList(Window* pParent_ = nullptr, int nX_ = 0, int nY_ = 0, const char* pcszText_ = "", int nMinWidth_ = 0);

public:
    void SetText(const char* pcszText_) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    int m_nMinWidth = 0;
};


class ComboBox : public Window
{
public:
    ComboBox(Window* pParent_, int nX_, int nY_, const char* pcszText_, int nWidth_);
    ComboBox(const ComboBox&) = delete;
    void operator= (const ComboBox&) = delete;

public:
    bool IsTabStop() const override { return true; }
    int GetSelected() const { return m_nSelected; }
    const char* GetSelectedText();
    void Select(int nSelected_);
    void Select(const char* pcszItem_);
    void SetText(const char* pcszText_) override;

    void Draw(FrameBuffer& fb) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;
    void OnNotify(Window* pWindow_, int nParam_) override;

protected:
    int m_nItems = 0, m_nSelected = 0;
    bool m_fPressed = false;
    DropList* m_pDropList = nullptr;
};


class ScrollBar : public Window
{
public:
    ScrollBar(Window* pParent_, int nX_, int nY_, int nHeight, int nMaxPos_, int nStep_ = 1);
    ScrollBar(const ScrollBar&) = delete;
    void operator= (const ScrollBar&) = delete;

public:
    bool IsTabStop() const override { return true; }
    int GetPos() const { return m_nPos; }
    void SetPos(int nPosition_);
    void SetMaxPos(int nMaxPos_);

    void Draw(FrameBuffer& fb) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;
    void OnNotify(Window* pWindow_, int nParam_) override;

protected:
    int m_nPos = 0, m_nMaxPos = 0, m_nStep = 0;
    int m_nScrollHeight = 0, m_nThumbSize = 0;
    bool m_fDragging = false;
    Button* m_pUp = nullptr;
    Button* m_pDown = nullptr;
};


class IconControl : public Window
{
public:
    IconControl(Window* pParent_, int nX_, int nY_, const GUI_ICON* pIcon_);
    IconControl(const IconControl&) = delete;
    void operator= (const IconControl&) = delete;

public:
    void Draw(FrameBuffer& fb) override;

protected:
    const GUI_ICON* m_pIcon = nullptr;
};


class FrameControl : public Window
{
public:
    FrameControl(Window* pParent_, int nX_, int nY_, int nWidth_, int nHeight_, uint8_t bColour_ = WHITE, uint8_t bFill_ = 0);

public:
    bool HitTest(int /*nX_*/, int /*nY_*/) override { return false; }
    void Draw(FrameBuffer& fb) override;

public:
    uint8_t m_bColour = 0, m_bFill = 0;
};


class ListViewItem
{
public:
    ListViewItem(const GUI_ICON* pIcon_, const char* pcszLabel_, ListViewItem* pNext_ = nullptr) :
        m_pIcon(pIcon_), m_pszLabel(nullptr), m_pNext(pNext_) {
        m_pszLabel = strdup(pcszLabel_);
    }
    ListViewItem(const ListViewItem&) = delete;
    void operator= (const ListViewItem&) = delete;
    virtual ~ListViewItem() { free(m_pszLabel); }

public:
    const GUI_ICON* m_pIcon = nullptr;
    char* m_pszLabel = nullptr;
    ListViewItem* m_pNext = nullptr;
};

class ListView : public Window
{
public:
    ListView(Window* pParent_, int nX_, int nY_, int nWidth_, int nHeight_, int nItemOffset = 0);
    ListView(const ListView&) = delete;
    void operator= (const ListView&) = delete;
    ~ListView();

public:
    bool IsTabStop() const override { return true; }

    int GetSelected() const { return m_nSelected; }
    void Select(int nItem_);

    const ListViewItem* GetItem(int nItem_ = -1) const;
    int FindItem(const char* pcszLabel_, int nStart_ = 0);
    void SetItems(ListViewItem* pItems_);

    void EraseBackground(FrameBuffer& fb) override;
    void Draw(FrameBuffer& fb) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

    virtual void DrawItem(FrameBuffer& fb, int nItem_, int nX_, int nY_, const ListViewItem* pItem_);

protected:
    int m_nItems = 0, m_nSelected = 0, m_nHoverItem = 0;
    int m_nAcross = 0, m_nDown = 0, m_nItemOffset = 0;

    ListViewItem* m_pItems = nullptr;
    ScrollBar* m_pScrollBar = nullptr;
};


class Dialog : public Window
{
public:
    Dialog(Window* pParent_, int nWidth_, int nHeight_, const char* pcszCaption_);
    ~Dialog();

public:
    bool IsActiveDialog() const;
    void SetColours(int nTitle_, int nBody_) { m_nTitleColour = nTitle_; m_nBodyColour = nBody_; }

    void Centre();
    void Activate() override;
    bool HitTest(int nX_, int nY_) override;
    void Draw(FrameBuffer& fb) override;
    void EraseBackground(FrameBuffer& fb) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    bool m_fDragging = false;
    int m_nDragX = 0, m_nDragY = 0;
    int m_nTitleColour = 0, m_nBodyColour = 0;
};

////////////////////////////////////////////////////////////////////////////////

enum { mbOk, mbOkCancel, mbYesNo, mbYesNoCancel, mbRetryCancel, mbInformation = 0x10, mbWarning = 0x20, mbError = 0x30 };

class MsgBox : public Dialog
{
public:
    MsgBox(Window* pParent_, const char* pcszBody_, const char* pcszCaption_, int nFlags_);
    MsgBox(const MsgBox&) = delete;
    void operator= (const MsgBox&) = delete;
    ~MsgBox() { if (m_pszBody) free(m_pszBody); }

public:
    void OnNotify(Window* /*pWindow_*/, int /*nParam_*/) override { Destroy(); }
    void Draw(FrameBuffer& fb) override;

protected:
    int m_nLines = 0;
    char* m_pszBody = nullptr;
    IconControl* m_pIcon = nullptr;
};

class FileView : public ListView
{
public:
    FileView(Window* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);
    FileView(const FileView&) = delete;
    void operator= (const FileView&) = delete;
    ~FileView();

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
