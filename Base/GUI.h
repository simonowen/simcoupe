// Part of SimCoupe - A SAM Coupé emulator
//
// GUI.h: GUI and controls for on-screen interface
//
//  Copyright (c) 1999-2002  Simon Owen
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

const int GM_BUTTONDOWN = GM_MOUSE_MESSAGE | 1;
const int GM_BUTTONUP   = GM_MOUSE_MESSAGE | 2;
const int GM_MOUSEMOVE  = GM_MOUSE_MESSAGE | 3;
const int GM_MOUSEWHEEL = GM_MOUSE_MESSAGE | 4;

const int GM_CHAR       = GM_KEYBOARD_MESSAGE | 1;

enum { GK_NULL, GK_LEFT, GK_RIGHT, GK_UP, GK_DOWN };        // Cursor key constants


enum
{
    BLUE_1 = N_PALETTE_COLOURS, BLUE_2, BLUE_3, BLUE_4, BLUE_5, BLUE_6, BLUE_7, BLUE_8,
    RED_1, RED_2, RED_3, RED_4, RED_5, RED_6, RED_7, RED_8,
    MAGENTA_1, MAGENTA_2, MAGENTA_3, MAGENTA_4, MAGENTA_5, MAGENTA_6, MAGENTA_7, MAGENTA_8,
    GREEN_1, GREEN_2, GREEN_3, GREEN_4, GREEN_5, GREEN_6, GREEN_7, GREEN_8,
    CYAN_1, CYAN_2, CYAN_3, CYAN_4, CYAN_5, CYAN_6, CYAN_7, CYAN_8,
    YELLOW_1, YELLOW_2, YELLOW_3, YELLOW_4, YELLOW_5, YELLOW_6, YELLOW_7, YELLOW_8,
    GREY_1, GREY_2, GREY_3, GREY_4, GREY_5, GREY_6, GREY_7, GREY_8,
    CUSTOM_1, CUSTOM_2,
    TOTAL_COLOURS,

    BLACK = GREY_1, WHITE = GREY_8      // Useful aliases
};

const int N_GUI_COLOURS = TOTAL_COLOURS-N_PALETTE_COLOURS;


class CWindow;
class CDialog;

class GUI
{
    public:
        static bool IsActive () { return s_pGUI != NULL; }
        static bool IsModal () { return IsActive() && s_fModal; }
        static const RGBA* GetPalette ();

    public:
        static void Start (CWindow* pGUI_, bool fModal_=true);
        static void Stop ();

        static void Draw (CScreen* pScreen_);
        static bool SendMessage (int nMessage_, int nParam1_=0, int nParam2_=0);

    protected:
        static CWindow* s_pGUI;
        static int s_nX, s_nY;
        static int s_nUsage;
        static bool s_fModal;

        friend class CWindow;
        friend class CDialog;     // only needed for test cross-hair to access cursor position
};

////////////////////////////////////////////////////////////////////////////////

enum { ctUnknown, ctText, ctButton, ctCheckBox, ctEdit, ctRadio, ctMenu, ctImage, ctDialog };


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

        void Destroy ();
        void Enable (bool fEnable_=true) { m_fEnabled = fEnable_; }
        void Move (int nX_, int nY_);

    public:
        virtual bool IsTabStop () { return true; }

        virtual const char* GetText () const { return m_pcszText; }
        virtual void SetText (const char* pcszText_);
        int GetTextWidth () const { return CScreen::GetStringWidth(m_pcszText); }

        virtual void Activate ();
        virtual bool HitTest (int nX_, int nY_);
        virtual bool OnMessage (int nMessage_, int nParam1_=0, int nParam2_=0);
        virtual void OnCommand (CWindow* pWindow_) { }
        virtual void Draw (CScreen* pScreen_) = 0;

    protected:
        void AddChild (CWindow* pChild_);
        void MoveRecurse (CWindow* pWindow_, int ndX_, int ndY_);

    protected:
        int m_nX, m_nY;
        int m_nWidth, m_nHeight;
        int m_nType;

        char* m_pcszText;

        bool m_fEnabled, m_fHover;

        CWindow *m_pParent, *m_pChildren, *m_pNext, *m_pActive;
};


class CTextControl : public CWindow
{
    public:
        CTextControl (CWindow* pParent_=NULL, int nX_=0, int nY_=0, const char* pcszText_="", BYTE bColour=WHITE);

    public:
        bool IsTabStop () { return false; }
        void Draw (CScreen* pScreen_);

    protected:
        BYTE m_bColour;
};


class CButton : public CWindow
{
    public:
        CButton (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);

    public:
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


class CUpButton : public CButton
{
    public:
        CUpButton (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);

    public:
        void Draw (CScreen* pScreen_);
};

class CDownButton : public CButton
{
    public:
        CDownButton (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);

    public:
        void Draw (CScreen* pScreen_);
};


class CCheckBox : public CWindow
{
    public:
        CCheckBox (CWindow* pParent_, int nX_, int nY_, const char* pcszText_="");

    public:
        bool IsChecked () const { return m_fChecked; }
        void SetChecked (bool fChecked_=true) { m_fChecked = fChecked_; }

        void SetText (const char* pcszText_);
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        bool m_fChecked;
};


class CEditControl : public CWindow
{
    public:
        CEditControl (CWindow* pParent_, int nX_, int nY_, int nWidth_, const char* pcszText_="");

    public:
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);
};


class CRadioButton : public CWindow
{
    public:
        CRadioButton (CWindow* pParent_=NULL, int nX_=0, int nY_=0, const char* pcszText_="", int nWidth_=0);

    public:
        bool IsTabStop () { return IsSelected(); }
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
        int GetSelected () const { return m_nSelected; }
        void Select (int nSelected_);

        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);
        void OnCommand (CWindow* pWindow_);

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
        int GetPos () const { return m_nPos; }
        void SetPos (int nPosition_);
        void SetMaxPos (int nMaxPos_);

        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);
        void OnCommand (CWindow* pWindow_);

    protected:
        int m_nPos, m_nMaxPos, m_nStep;
        int m_nScrollHeight, m_nThumbSize;
        bool m_fDragging;
        CWindow *m_pUp, *m_pDown;
};


class CIconControl : public CWindow
{
    public:
        CIconControl (CWindow* pParent_, int nX_, int nY_, const GUI_ICON& rsIcon_, bool fSmall_=false);

    public:
        bool IsTabStop () { return false; }
        void Draw (CScreen* pScreen_);

    protected:
        GUI_ICON m_sIcon;
        bool m_fSmall;
};


class CDialog : public CWindow
{
    public:
        CDialog (int nWidth_=0, int nHeight_=0, const char* pcszCaption_="");

    public:
        bool HitTest (int nX_, int nY_);
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);
};


class CListView : public CWindow
{
    public:
        CListView (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);

    public:
        int GetSelected () const { return m_nSelected; }
        void Select (int nItem_);
        void SetItems (int nItems_=0);

        virtual void DrawItem (CScreen* pScreen_, int nItem_, int nX_, int nY_, const GUI_ICON* pIcon_=NULL, const char* pcsz_=NULL);
        void Draw (CScreen* pScreen_);
        bool OnMessage (int nMessage_, int nParam1_, int nParam2_);

    protected:
        int m_nItems, m_nSelected, m_nHoverItem;
        int m_nAcross, m_nDown;

        CScrollBar* m_pScrollBar;
};

////////////////////////////////////////////////////////////////////////////////

enum { ftDrive, ftDir, ftFile };

struct FileEntry
{
    char* psz;
    int nType;

    FileEntry* pNext;
};

class CFileView : public CListView
{
    public:
        CFileView (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_);

    public:
        void DrawItem (CScreen* pScreen_, int nItem_, int nX_, int nY_, const GUI_ICON* pIcon_=NULL, const char* pcsz_=NULL);

    protected:
        FileEntry* m_pFiles;
};

////////////////////////////////////////////////////////////////////////////////

class CTestDialog : public CDialog
{
    public:
        CTestDialog() : CDialog(205, 198, "GUI Test")
        {
            m_apControls[0] = new CEditControl(this, 8, 8, 190, "Edit control");

            m_apControls[1] = new CCheckBox(this, 8, 38, "Checked check-box");
            m_apControls[2] = new CCheckBox(this, 8, 54, "Unchecked check-box");
            reinterpret_cast<CCheckBox*>(m_apControls[1])->SetChecked();

            m_apControls[3] = new CRadioButton(this, 8, 78, "First option");
            m_apControls[4] = new CRadioButton(this, 8, 94, "Second option");
            m_apControls[5] = new CRadioButton(this, 8, 110, "Third option");
            reinterpret_cast<CRadioButton*>(m_apControls[3])->Select();

            m_apControls[6] = new CComboBox(this, 105,78, "Coch|Gwyn|Glas|Melyn", 70);
            m_apControls[7] = new CTextButton(this, 105, 103, "Button", 50);
            m_apControls[8] = new CScrollBar(this, 183,38,110,400);

            m_apControls[9] = new CIconControl(this, 8, 133, sDisplayIcon);

            m_apControls[10] = new CTextControl(this, 40, 133, "<- Icon control");
            m_apControls[11] = new CTextControl(this, 45, 149, "Coloured text control", GREEN_7);

            m_pEnable = new CCheckBox(this, 8, m_nHeight-20, "Controls enabled");
            reinterpret_cast<CCheckBox*>(m_pEnable)->SetChecked();

            m_pClose = new CTextButton(this, m_nWidth-55, m_nHeight-22, "Close", 50);

            m_pEnable->Activate();
        }

    public:
        void OnCommand (CWindow* pWindow_)
        {
            if (pWindow_ == m_pClose)
                GUI::Stop();

            else if (pWindow_ == m_pEnable)
            {
                bool fIsChecked = reinterpret_cast<CCheckBox*>(m_pEnable)->IsChecked();

                for (int i = 0 ; i < 12 ; i++)
                    m_apControls[i]->Enable(fIsChecked);
            }
        }

    public:
        CWindow *m_pEnable, *m_pClose, *m_apControls[12];
};


class CAboutDialog : public CDialog
{
    public:
        CAboutDialog () : CDialog(260, 184,  "About SimCoupe")
        {
            new CIconControl(this, 6, 6, sSamIcon);

            new CTextControl(this, 51, 8,   "SimCoupe - a SAM Coupe emulator", YELLOW_8);
            new CTextControl(this, 51, 22,  "Version 0.90 WIP (5)", YELLOW_8);

            new CTextControl(this, 16, 44,  "Win32/SDL versions and general overhaul:");
            new CTextControl(this, 26, 57,  "Simon Owen (simon.owen@simcoupe.org)", GREY_7);

            new CTextControl(this, 16, 76,  "Based on original DOS/X SimCoupe versions by:");
            new CTextControl(this, 26, 89,  "Allan Skillman (allan.skillman@arm.com)", GREY_7);

            new CTextControl(this, 16, 108,  "Additional technical enhancements:");
            new CTextControl(this, 26, 121,  "Dave Laundon (dave.laundon@simcoupe.org)", GREY_7);

            new CTextControl(this, 16, 142, "See SimCoupe.txt for additional information.", YELLOW_8);

            m_pCloseButton = new CTextButton(this, 105, 162, "Close", 50);
        }

    public:
        void OnCommand (CWindow* pWindow_) { if (pWindow_ == m_pCloseButton) GUI::Stop(); }

    protected:
        CWindow* m_pCloseButton;
};


class CFileDialog : public CDialog
{
    public:
        CFileDialog() : CDialog(485, 300, "Open Disk Image")
        {
            (new CFileView(this, 0, 0, m_nWidth, m_nHeight-20))->Activate();
            new CEditControl(this, 0, m_nHeight-14, m_nWidth-110, "C:\\Sam Coupe\\Disks\\");
            new CTextButton(this, m_nWidth - 99, m_nHeight-17, "OK", 46);
            m_pCancel = new CTextButton(this, m_nWidth - 50, m_nHeight-17, "Cancel", 46);
        }

    public:
        void CFileDialog::OnCommand (CWindow* pWindow_) { if (pWindow_ == m_pCancel) GUI::Stop(); }

    protected:
        CTextButton* m_pCancel;
};

#endif
