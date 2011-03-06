// Part of SimCoupe - A SAM Coupe emulator
//
// GUI.h: GUI and controls for on-screen interface
//
//  Copyright (c) 1999-2011  Simon Owen
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

#ifndef GUI_H
#define GUI_H

#include "CScreen.h"
#include "GUIIcons.h"
#include "IO.h"

const int GM_MOUSE_MESSAGE    = 0x40000000;
const int GM_KEYBOARD_MESSAGE = 0x20000000;

const int GM_BUTTONUP     = GM_MOUSE_MESSAGE | 1;
const int GM_BUTTONDOWN   = GM_MOUSE_MESSAGE | 2;
const int GM_BUTTONDBLCLK = GM_MOUSE_MESSAGE | 3;
const int GM_MOUSEMOVE    = GM_MOUSE_MESSAGE | 4;
const int GM_MOUSEWHEEL   = GM_MOUSE_MESSAGE | 5;

const int GM_CHAR         = GM_KEYBOARD_MESSAGE | 1;

const DWORD DOUBLE_CLICK_TIME = 400;    // Under 400ms between consecutive clicks counts as a double-click
const int DOUBLE_CLICK_THRESHOLD = 5;   // Distance between clicks for double-clicks to be recognised


class CWindow;
class CDialog;

class GUI
{
    public:
        static bool IsActive () { return s_pGUI != NULL; }
        static bool IsModal ();

    public:
        static bool Start (CWindow* pGUI_);
        static void Stop ();

        static void Draw (CScreen* pScreen_);
        static bool SendMessage (int nMessage_, int nParam1_=0, int nParam2_=0);
        static void Delete (CWindow* pWindow_);

    protected:
        static CWindow *s_pGUI, *s_pGarbage;
        static int s_nX, s_nY;
        static bool s_fModal;

        friend class CWindow;
        friend class CDialog;     // only needed for test cross-hair to access cursor position
};

////////////////////////////////////////////////////////////////////////////////

// Control types, as returned by GetType() from any base CWindow pointer
enum { ctUnknown, ctText, ctButton, ctImageButton, ctCheckBox, ctComboBox, ctEdit, ctRadio,
       ctMenu, ctImage, ctFrame, ctListView, ctDialog, ctMessageBox };


class CWindow
{
    public:
        CWindow (CWindow* pParent_=NULL, int nX_=0, int nY_=0, int nWidth_=0, int nHeight_=0, int nType_=ctUnknown);
        virtual ~CWindow ();

    public:
        bool IsEnabled () const { return m_fEnabled; }
        bool IsOver () const { return m_fHover; }
        bool IsActive () const { return m_pParent && m_pParent->m_pActive == this; }
        int GetType () const { return m_nType; }

        CWindow* GetParent () { return m_pParent; }
        CWindow* GetChildren () { return m_pChildren; }
        CWindow* GetSiblings () { return GetParent() ? GetParent()->GetChildren() : NULL; }
        CWindow* GetGroup ();

        CWindow* GetNext (bool fWrap_=false);
        CWindow* GetPrev (bool fWrap_=false);

        void SetParent (CWindow* pParent_);
        void Destroy ();
        void Enable (bool fEnable_=true) { m_fEnabled = fEnable_; }
        void Move (int nX_, int nY_);
        void Offset (int ndX_, int ndY_);
        void SetSize (int nWidth_, int nHeight_);
        void Inflate (int ndW_, int ndH_);

    public:
        virtual bool IsTabStop () const { return false; }

        virtual const char* GetText () const { return m_pszText; }
        virtual UINT GetValue () const { return strtoul(m_pszText, NULL, 0); }
        virtual void SetText (const char* pcszText_);
        virtual void SetValue (UINT u_);
        int GetTextWidth () const { return CScreen::GetStringWidth(m_pszText); }

        virtual void Activate ();
        virtual bool HitTest (int nX_, int nY_);
        virtual void EraseBackground (CScreen* pScreen_) { }
        virtual void Draw (CScreen* pScreen_) = 0;

        virtual void NotifyParent (int nParam_=0);
        virtual void OnNotify (CWindow* pWindow_, int nParam_) { }
        virtual bool OnMessage (int nMessage_, int nParam1_=0, int nParam2_=0);

    protected:
        void RemoveChild ();
        void MoveRecurse (CWindow* pWindow_, int ndX_, int ndY_);

    protected:
        int m_nX, m_nY;
        int m_nWidth, m_nHeight;
        int m_nType;

        char* m_pszText;

        bool m_fEnabled, m_fHover;

        CWindow *m_pParent, *m_pChildren, *m_pNext, *m_pActive;

        friend class CDialog;
};


class CTextControl : public CWindow
{
    public:
        CTextControl (CWindow* pParent_=NULL, int nX_=0, int nY_=0, const char* pcszText_="", BYTE bColour=WHITE, BYTE bBackColour=0);

    public:
        void Draw (CScreen* pScreen_);

    protected:
        BYTE m_bColour, m_bBackColour;
};


class CButton : public CWindow
{
    public:
        CButton (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);

    public:
        bool IsTabStop () const { return true; }
        bool IsPressed () const { return m_fPressed; }

        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        bool m_fPressed;
};


class CTextButton : public CButton
{
    public:
        CTextButton (CWindow* pParent_, int nX_, int nY_, const char* pcszText_="", int nMinWidth_=0);

    public:
        void SetText (const char* pcszText_);
        void Draw (CScreen* pScreen_);

    protected:
        int m_nMinWidth;
};


class CImageButton : public CButton
{
    public:
        CImageButton (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_,
                        const GUI_ICON* pIcon_, int nDX_=0, int nDY_=0);

    public:
        void Draw (CScreen* pScreen_);

    protected:
        const GUI_ICON* m_pIcon;
        int m_nDX, m_nDY;
};


class CUpButton : public CButton
{
    public:
        CUpButton (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);
        void Draw (CScreen* pScreen_);
};

class CDownButton : public CButton
{
    public:
        CDownButton (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);
        void Draw (CScreen* pScreen_);
};


class CCheckBox : public CWindow
{
    public:
        CCheckBox (CWindow* pParent_, int nX_, int nY_, const char* pcszText_="", BYTE bColour_=WHITE, BYTE bBackColour_=0);

    public:
        bool IsTabStop () const { return true; }
        bool IsChecked () const { return m_fChecked; }
        void SetChecked (bool fChecked_=true) { m_fChecked = fChecked_; }

        void SetText (const char* pcszText_);
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        bool m_fChecked;
        BYTE m_bColour, m_bBackColour;
};


class CEditControl : public CWindow
{
    public:
        CEditControl (CWindow* pParent_, int nX_, int nY_, int nWidth_, const char* pcszText_="");
        CEditControl (CWindow* pParent_, int nX_, int nY_, int nWidth_, UINT u_);

    public:
        bool IsTabStop () const { return true; }
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);
};


class CRadioButton : public CWindow
{
    public:
        CRadioButton (CWindow* pParent_=NULL, int nX_=0, int nY_=0, const char* pcszText_="", int nWidth_=0);

    public:
        bool IsTabStop () const { return IsSelected(); }
        bool IsSelected () const { return m_fSelected; }
        void Select (bool fSelected_=true);
        void SetText (const char* pcszText_);

        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        bool m_fSelected;
};


class CMenu : public CWindow
{
    public:
        CMenu (CWindow* pParent_=NULL, int nX_=0, int nY_=0, const char* pcszText_="");

    public:
        int GetSelected () const  { return m_nSelected; }
        void Select (int nItem_);
        void SetText (const char* pcszText_);

        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        int m_nItems, m_nSelected;
        bool m_fPressed;
};


class CDropList : public CMenu
{
    public:
        CDropList (CWindow* pParent_=NULL, int nX_=0, int nY_=0, const char* pcszText_="", int nMinWidth_=0);

    public:
        void SetText (const char* pcszText_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        int m_nMinWidth;
};


class CComboBox : public CWindow
{
    public:
        CComboBox (CWindow* pParent_, int nX_, int nY_, const char* pcszText_, int nWidth_);

    public:
        bool IsTabStop () const { return true; }
        int GetSelected () const { return m_nSelected; }
        const char* GetSelectedText ();
        void Select (int nSelected_);
        void Select (const char* pcszItem_);
        void SetText (const char* pcszText_);

        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);
        void OnNotify (CWindow* pWindow_, int nParam_);

    protected:
        int m_nItems, m_nSelected;
        bool m_fPressed;
        CDropList* m_pDropList;
};


class CScrollBar : public CWindow
{
    public:
        CScrollBar (CWindow* pParent_, int nX_, int nY_, int nHeight, int nMaxPos_, int nStep_=1);

    public:
        bool IsTabStop () const { return true; }
        int GetPos () const { return m_nPos; }
        void SetPos (int nPosition_);
        void SetMaxPos (int nMaxPos_);

        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);
        void OnNotify (CWindow* pWindow_, int nParam_);

    protected:
        int m_nPos, m_nMaxPos, m_nStep;
        int m_nScrollHeight, m_nThumbSize;
        bool m_fDragging;
        CButton *m_pUp, *m_pDown;
};


class CIconControl : public CWindow
{
    public:
        CIconControl (CWindow* pParent_, int nX_, int nY_, const GUI_ICON* pIcon_, bool fSmall_=false);

    public:
        void Draw (CScreen* pScreen_);

    protected:
        const GUI_ICON* m_pIcon;
        bool m_fSmall;
};


class CFrameControl : public CWindow
{
    public:
        CFrameControl (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_=WHITE, BYTE bFill_=0);

    public:
        bool HitTest (int nX_, int nY_) { return false; }
        void Draw (CScreen* pScreen_);

    public:
        BYTE m_bColour, m_bFill;
};


class CListViewItem
{
    public:
        CListViewItem (const GUI_ICON* pIcon_, const char* pcszLabel_, CListViewItem* pNext_=NULL) :
            m_pIcon(pIcon_), m_pNext(pNext_) { m_pszLabel = strdup(pcszLabel_); }
        virtual ~CListViewItem () { free(m_pszLabel); }

    public:
        bool IsTabStop () const { return true; }

    public:
        const GUI_ICON* m_pIcon;
        char* m_pszLabel;
        CListViewItem* m_pNext;
};

class CListView : public CWindow
{
    public:
        CListView (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_, int nItemOffset=0);
        ~CListView ();

    public:
        bool IsTabStop () const { return true; }
        int GetSelected () const { return m_nSelected; }
        void Select (int nItem_);

        const CListViewItem* GetItem (int nItem_=-1) const;
        int FindItem (const char* pcszLabel_, int nStart_=0);
        void SetItems (CListViewItem* pItems_);

        void EraseBackground (CScreen* pScreen_);
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

        virtual void DrawItem (CScreen* pScreen_, int nItem_, int nX_, int nY_, const CListViewItem* pItem_);

    protected:
        int m_nItems, m_nSelected, m_nHoverItem;
        int m_nAcross, m_nDown, m_nItemOffset;

        CListViewItem* m_pItems;
        CScrollBar* m_pScrollBar;
};


class CDialog : public CWindow
{
    public:
        CDialog (CWindow* pParent_, int nWidth_, int nHeight_, const char* pcszCaption_, bool fModal_=true);
        ~CDialog ();

    public:
        bool IsModal () const { return m_fModal; }
        bool IsActiveDialog () { return s_pActive == this; }
        void SetColours (int nTitle_, int nBody_) { m_nTitleColour = nTitle_; m_nBodyColour = nBody_; }

        void Centre ();
        void Activate ();
        bool HitTest (int nX_, int nY_);
        void Draw (CScreen* pScreen_);
        void EraseBackground (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        bool m_fModal, m_fDragging;
        int m_nDragX, m_nDragY;
        int m_nTitleColour, m_nBodyColour;

        static CWindow* s_pActive;
};

////////////////////////////////////////////////////////////////////////////////

enum { mbOk, mbOkCancel, mbYesNo, mbYesNoCancel, mbRetryCancel, mbInformation = 0x10, mbWarning = 0x20, mbError = 0x30 };

class CMessageBox : public CDialog
{
    public:
        CMessageBox (CWindow* pParent_, const char* pcszBody_, const char* pcszCaption_, int nFlags_);
        ~CMessageBox () { if (m_pszBody) free(m_pszBody); }

    public:
        void OnNotify (CWindow* pWindow_, int nParam_) { Destroy(); }
        void Draw (CScreen* pScreen_);

    protected:
        int m_nLines;
        char* m_pszBody;
        CIconControl* m_pIcon;
};

class CFileView : public CListView
{
    public:
        CFileView (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);
        ~CFileView ();

    public:
        const char* GetFullPath () const;
        const char* GetPath () const { return m_pszPath; }
        const char* GetFilter () const { return m_pszFilter; }
        void SetPath (const char* pcszPath_);
        void SetFilter (const char* pcszFilter_);
        void ShowHidden (bool fShow_);

        void Refresh ();
        void NotifyParent (int nParam_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

        static const GUI_ICON* GetFileIcon (const char* pcszFile_);

    protected:
        char *m_pszPath, *m_pszFilter;
        bool m_fShowHidden;
};


#endif // GUI_H
