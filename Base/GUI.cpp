// Part of SimCoupe - A SAM Coupe emulator
//
// GUI.cpp: GUI and controls for on-screen interface
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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

//  ToDo:
//   - CFileView derived class needed to supply file icons
//   - button repeat on scrollbar
//   - add extra message box buttons (yes/no/cancel, etc.)
//   - regular list box?
//   - use icon for button arrows?
//   - edit box cursor positioning

#include "SimCoupe.h"
#include "GUI.h"

#include <ctype.h>

#include "Display.h"
#include "Font.h"
#include "Frame.h"
#include "Input.h"
#include "Keyboard.h"
#include "Sound.h"
#include "UI.h"
#include "Video.h"

CWindow *GUI::s_pGUI, *GUI::s_pGarbage;
int GUI::s_nX, GUI::s_nY;
bool GUI::s_fModal;

static DWORD dwLastClick = 0;   // Time of last double-click


bool GUI::SendMessage (int nMessage_, int nParam1_/*=0*/, int nParam2_/*=0*/)
{
    // We're not interested in messages when we're inactive
    if (!s_pGUI)
        return false;

    // Keep track of the mouse
    if (nMessage_ == GM_MOUSEMOVE)
    {
        s_nX = nParam1_;
        s_nY = nParam2_;
    }

    // Check for double-clicks
    else if (nMessage_ == GM_BUTTONDOWN)
    {
        static int nLastX, nLastY;
        static bool fDouble = false;

        // Work out how long it's been since the last click, and how much the mouse has moved
        DWORD dwNow = OSD::GetTime();
        int nMovedSquared = (nLastX - nParam1_)*(nLastX - nParam1_) + (nLastY - nParam2_)*(nLastY - nParam2_);

        // If the click is close enough to the last click (in space and time), convert it to a double-click
        if (!fDouble && dwNow-dwLastClick < DOUBLE_CLICK_TIME &&
            nMovedSquared < (DOUBLE_CLICK_THRESHOLD*DOUBLE_CLICK_THRESHOLD))
            nMessage_ = GM_BUTTONDBLCLK;

        // Remember the last time and position of the click
        dwLastClick = dwNow;
        nLastX = nParam1_;
        nLastY = nParam2_;

        // Remember whether we've processed a double-click, so a third click isn't another one
        fDouble = (nMessage_ == GM_BUTTONDBLCLK);
    }

    // Pass the message to the active GUI component
    s_pGUI->OnMessage(nMessage_, nParam1_, nParam2_);

    // Send a move after a button up, to give a hit test after an effective mouse capture
    if (s_pGUI && nMessage_ == GM_BUTTONUP)
        s_pGUI->OnMessage(GM_MOUSEMOVE, s_nX, s_nY);

    // Stop the GUI if it was deleted during the last message
    if (s_pGarbage)
    {
        // Delete the destroyed window tree
        delete s_pGarbage;
        s_pGarbage = NULL;

        // If the GUI still exists, update the activation state
        if (s_pGUI)
            s_pGUI->OnMessage(GM_MOUSEMOVE, s_nX, s_nY);

        // Otherwise we're done
        else
            Stop();
    }

    return true;
}


bool GUI::Start (CWindow* pGUI_)
{
    // Reject the new GUI if it's already running, or if the emulator is paused
    if (s_pGUI || g_fPaused)
    {
        // Delete the supplied object tree and return failure
        delete pGUI_;
        return false;
    }

    // Set the top level window and clear any last click time
    s_pGUI = pGUI_;
    dwLastClick = 0;

    // Position the cursor off-screen, to ensure the first drawn position matches the native OS position
    s_nX = s_nY = -ICON_SIZE;

    // Silence sound playback
    Sound::Silence();
    Display::SetDirty();

    return true;
}

void GUI::Stop ()
{
    // Delete any existing GUI object
    delete s_pGUI;
    s_pGUI = NULL;

    Display::SetDirty();
}

void GUI::Delete (CWindow* pWindow_)
{
    // Link up the existing window to the garbage chain
    if (!s_pGarbage)
        s_pGarbage = pWindow_;
    else
    {
        s_pGarbage->SetParent(pWindow_);
        s_pGarbage = pWindow_;
    }

    // If the top-most window is being removed, invalidate the main GUI pointer
    if (pWindow_ == s_pGUI)
        s_pGUI = NULL;
}

void GUI::Draw (CScreen* pScreen_)
{
    if (s_pGUI)
    {
        CScreen::SetFont(&sGUIFont);
        s_pGUI->Draw(pScreen_);

        pScreen_->DrawImage(s_nX, s_nY, ICON_SIZE, ICON_SIZE,
                    reinterpret_cast<const BYTE*>(sMouseCursor.abData), sMouseCursor.abPalette);
    }
}


bool GUI::IsModal ()
{
    return s_pGUI && s_pGUI->GetType() >= ctDialog && reinterpret_cast<CDialog*>(s_pGUI)->IsModal();
}

////////////////////////////////////////////////////////////////////////////////

CWindow::CWindow (CWindow* pParent_/*=NULL*/, int nX_/*=0*/, int nY_/*=0*/, int nWidth_/*=0*/, int nHeight_/*=0*/, int nType_/*=ctUnknown*/)
    : m_nX(nX_), m_nY(nY_), m_nWidth(nWidth_), m_nHeight(nHeight_), m_nType(nType_), m_pszText(NULL), m_fEnabled(true), m_fHover(false),
        m_pParent(NULL), m_pChildren(NULL), m_pNext(NULL), m_pActive(NULL)
{
    if (pParent_)
    {
        // Set the parent window
        SetParent(pParent_);

        // Adjust our position to be relative to the parent
        m_nX += pParent_->m_nX;
        m_nY += pParent_->m_nY;
    }
}

CWindow::~CWindow ()
{
    // Delete any child controls
    while (m_pChildren)
    {
        CWindow* pChild = m_pChildren;
        m_pChildren = m_pChildren->m_pNext;
        delete pChild;
    }

    delete[] m_pszText;
}


// Test whether the given point is inside the current control
bool CWindow::HitTest (int nX_, int nY_)
{
    return (nX_ >= m_nX) && (nX_ < (m_nX+m_nWidth)) && (nY_ >= m_nY) && (nY_ < (m_nY+m_nHeight));
}

// Draw the child controls on the current window
void CWindow::Draw (CScreen* pScreen_)
{
    for (CWindow* p = m_pChildren ; p ; p = p->m_pNext)
    {
        if (p != m_pActive)
            p->Draw(pScreen_);
    }

    // Draw the active control last to ensure it's shown above any other controls
    if (m_pActive)
        m_pActive->Draw(pScreen_);
}

// Notify the parent something has changed
void CWindow::NotifyParent (int nParam_/*=0*/)
{
    if (m_pParent)
        m_pParent->OnNotify(this, nParam_);
}

bool CWindow::OnMessage (int nMessage_, int nParam1_/*=0*/, int nParam2_/*=0*/)
{
    bool fProcessed = false;

    // The active child gets first go at the message
    if (m_pActive)
    {
        m_pActive->m_fHover = m_pActive->HitTest(nParam1_, nParam2_);
        fProcessed =  m_pActive->OnMessage(nMessage_, nParam1_, nParam2_);
    }

    // Give the remaining child controls a chance to process the message
    for (CWindow* p = m_pChildren ; !fProcessed && p ; )
    {
        CWindow* pChild = p;
        p = p->m_pNext;

        // Skip disabled windows, and the active window, as we're already done it
        if (pChild->IsEnabled() && pChild != m_pActive)
        {
            // If we're clicking on a control, auto-activate it
            if ((nMessage_ == GM_BUTTONDOWN) && (pChild->m_fHover = pChild->HitTest(nParam1_, nParam2_)))
                pChild->Activate();

            fProcessed = pChild->OnMessage(nMessage_, nParam1_, nParam2_);
        }
    }

    m_fHover = HitTest(nParam1_, nParam2_);

    // Return whether the message was processed
    return fProcessed;
}


void CWindow::SetParent (CWindow* pParent_/*=NULL*/)
{
    // Unlink from any existing parent
    if (m_pParent)
    {
        CWindow* pPrev = GetPrev();

        if (!pPrev)
            m_pParent->m_pChildren = GetNext();
        else
            pPrev->m_pNext = GetNext();

        if (m_pParent->m_pActive == this)
            m_pParent->m_pActive = NULL;

        m_pParent = m_pNext = NULL;
    }

    // Set the new parent, if any
    if (pParent_ && pParent_ != this)
    {
        m_pParent = pParent_;

        if (!pParent_->m_pChildren)
            pParent_->m_pChildren = this;
        else
        {
            CWindow* p;
            for (p = pParent_->m_pChildren ; p->GetNext() ; p = p->GetNext());
            p->m_pNext = this;
        }
    }
}


void CWindow::Destroy ()
{
    if (m_pParent)
    {
        // Unlink us from the parent, keeping the local pointer for use in the destructor
        CWindow* pParent = m_pParent;
        SetParent(NULL);
        m_pParent = pParent;

        // Re-activate the parent now we're gone
        pParent->Activate();
    }

    // Schedule the object to be deleted when safe
    GUI::Delete(this);
}


void CWindow::Activate ()
{
    if (m_pParent)
        m_pParent->m_pActive = this;
}


void CWindow::SetText (const char* pcszText_)
{
    // Delete any old string and make a copy of the new one (take care in case the new string is the old one)
    char* pcszOld = m_pszText;
    strcpy(m_pszText = new char[strlen(pcszText_)+1], pcszText_);
    delete[] pcszOld;
}

void CWindow::SetValue (UINT u_)
{
    char sz[16];
    sprintf(sz, "%u", u_);
    SetText(sz);
}

CWindow* CWindow::GetNext (bool fWrap_/*=false*/)
{
    return m_pNext ? m_pNext : (fWrap_ ? GetSiblings() : NULL);
}

CWindow* CWindow::GetPrev (bool fWrap_/*=false*/)
{
    CWindow* pLast = NULL;

    for (CWindow* p = GetSiblings() ; p ; pLast = p, p = p->m_pNext)
        if (p->m_pNext == this)
            return p;

    return fWrap_ ? pLast : NULL;
}

// Return the start of the control group containing the current control
CWindow* CWindow::GetGroup ()
{
    // Search our sibling controls
    for (CWindow* p = GetSiblings() ; p ; p = p->GetNext())
    {
        // Continue looking if it's not a radio button
        if (p->GetType() != GetType())
            continue;

        // Search the rest of the radio group
        for (CWindow* p2 = p ; p2 && p2->GetType() == GetType() ; p2 = p2->GetNext())
        {
            // If we've found ourselves, return the start of the group
            if (p2 == this)
                return p;
        }
    }

    return NULL;
}


void CWindow::MoveRecurse (CWindow* pWindow_, int ndX_, int ndY_)
{
    // Move our window by the specified offset
    pWindow_->m_nX += ndX_;
    pWindow_->m_nY += ndY_;

    // Move and child windows
    for (CWindow* p = pWindow_->m_pChildren ; p ; p = p->m_pNext)
        MoveRecurse(p, ndX_, ndY_);
}

void CWindow::Move (int nX_, int nY_)
{
    // Perform a recursive relative move of the window and all children
    MoveRecurse(this, nX_ - m_nX, nY_ - m_nY);
}

void CWindow::Offset (int ndX_, int ndY_)
{
    // Perform a recursive relative move of the window and all children
    MoveRecurse(this, ndX_, ndY_);
}


void CWindow::SetSize (int nWidth_, int nHeight_)
{
    if (nWidth_) m_nWidth = nWidth_;
    if (nHeight_) m_nHeight = nHeight_;
}

void CWindow::Inflate (int ndW_, int ndH_)
{
    m_nWidth += ndW_;
    m_nHeight += ndH_;
}

////////////////////////////////////////////////////////////////////////////////

CTextControl::CTextControl (CWindow* pParent_/*=NULL*/, int nX_/*=0*/, int nY_/*=0*/, const char* pcszText_/*=""*/,
    BYTE bColour_/*=WHITE*/, BYTE bBackColour_/*=0*/)
    : CWindow(pParent_, nX_, nY_, 0, 0, ctText), m_bColour(bColour_), m_bBackColour(bBackColour_)
{
    SetText(pcszText_);
    m_nWidth = GetTextWidth();
}

void CTextControl::Draw (CScreen* pScreen_)
{
    if (m_bBackColour)
        pScreen_->FillRect(m_nX-1, m_nY-1, GetTextWidth()+2, 14, m_bBackColour);

    pScreen_->DrawString(m_nX, m_nY, GetText(), IsEnabled() ? m_bColour : GREY_5);
}

////////////////////////////////////////////////////////////////////////////////

const int BUTTON_BORDER = 3;
const int BUTTON_HEIGHT = BUTTON_BORDER + CHAR_HEIGHT + BUTTON_BORDER;

CButton::CButton (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
    : CWindow(pParent_, nX_, nY_, nWidth_, nHeight_ ? nHeight_ : BUTTON_HEIGHT, ctButton), m_fPressed(false)
{
}

void CButton::Draw (CScreen* pScreen_)
{
    bool fPressed = m_fPressed && IsOver();

    // Fill the main button background
    pScreen_->FillRect(m_nX+1, m_nY+1, m_nWidth-2, m_nHeight-2, IsActive() ? YELLOW_8 : GREY_7);

    // Draw the edge highlight for the top and left
    pScreen_->DrawLine(m_nX, m_nY, m_nWidth, 0, fPressed ? GREY_5 : WHITE);
    pScreen_->DrawLine(m_nX, m_nY, 0, m_nHeight, fPressed ? GREY_5 : WHITE);

    // Draw the edge highlight for the bottom and right
    pScreen_->DrawLine(m_nX+1, m_nY+m_nHeight-1, m_nWidth-2, 0, fPressed ? WHITE : GREY_5);
    pScreen_->DrawLine(m_nX+m_nWidth-1, m_nY+1, 0, m_nHeight-1, fPressed ? WHITE : GREY_5);
}

bool CButton::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    switch (nMessage_)
    {
        case GM_CHAR:
            if (!IsActive())
                break;

            switch (nParam1_)
            {
                case ' ':
                case '\r':
                    NotifyParent(nParam1_ == '\r');
                    return true;
            }
            break;

        case GM_BUTTONDOWN:
        case GM_BUTTONDBLCLK:
            // If the click was over us, flag the button as pressed
            if (IsOver())
                return m_fPressed = true;
            break;

        case GM_BUTTONUP:
            // If we were depressed and the mouse has been released over us, register a button press
            if (IsOver() && m_fPressed)
                NotifyParent();

            // Ignore button ups that aren't for us
            else if (!m_fPressed)
                return false;

            // Unpress the button and return that we processed the message
            m_fPressed = false;
            return true;

        case GM_MOUSEMOVE:
            return m_fPressed;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

CTextButton::CTextButton (CWindow* pParent_, int nX_, int nY_, const char* pcszText_/*=""*/, int nMinWidth_/*=0*/)
    : CButton(pParent_, nX_, nY_, 0, BUTTON_HEIGHT), m_nMinWidth(nMinWidth_)
{
    SetText(pcszText_);
}

void CTextButton::SetText (const char* pcszText_)
{
    CWindow::SetText(pcszText_);

    // Set the control width to be just enough to contain the text and a border
    m_nWidth = BUTTON_BORDER + GetTextWidth() + BUTTON_BORDER;

    // If we're below the minimum width, set to the minimum
    if (m_nWidth < m_nMinWidth)
        m_nWidth = m_nMinWidth;
}

void CTextButton::Draw (CScreen* pScreen_)
{
    CButton::Draw(pScreen_);

    bool fPressed = m_fPressed && IsOver();

    // Centralise the text in the button, and offset down and right if it's pressed
    int nX = m_nX + fPressed + (m_nWidth - CScreen::GetStringWidth(GetText()))/2;
    int nY = m_nY + fPressed + (m_nHeight-CHAR_HEIGHT)/2 + 1;
    pScreen_->DrawString(nX, nY, GetText(), IsEnabled() ? (IsActive() ? BLACK : BLACK) : GREY_5);
}

////////////////////////////////////////////////////////////////////////////////

CImageButton::CImageButton (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_,
    const GUI_ICON* pIcon_, int nDX_/*=0*/, int nDY_/*=0*/)
    : CButton(pParent_, nX_, nY_, nWidth_, nHeight_), m_pIcon(pIcon_), m_nDX(nDX_), m_nDY(nDY_)
{
    m_nType = ctImageButton;
}

void CImageButton::Draw (CScreen* pScreen_)
{
    CButton::Draw(pScreen_);

    bool fPressed = m_fPressed && IsOver();
    int nX = m_nX + m_nDX + fPressed, nY = m_nY + m_nDY + fPressed;

    pScreen_->DrawImage(nX, nY, ICON_SIZE, ICON_SIZE, reinterpret_cast<const BYTE*>(m_pIcon->abData),
                        IsEnabled() ? m_pIcon->abPalette : m_pIcon->abPalette);
}

////////////////////////////////////////////////////////////////////////////////

CUpButton::CUpButton (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
    : CButton(pParent_, nX_, nY_, nWidth_, nHeight_)
{
}

void CUpButton::Draw (CScreen* pScreen_)
{
    CButton::Draw(pScreen_);

    bool fPressed = m_fPressed && IsOver();

    int nX = m_nX + 2 + fPressed, nY = m_nY + 3 + fPressed;
    BYTE bColour = GetParent()->IsEnabled() ? BLACK : GREY_5;
    pScreen_->DrawLine(nX+5, nY,   1, 0, bColour);
    pScreen_->DrawLine(nX+4, nY+1, 3, 0, bColour);
    pScreen_->DrawLine(nX+3, nY+2, 2, 0, bColour);  pScreen_->DrawLine(nX+6, nY+2, 2, 0, bColour);
    pScreen_->DrawLine(nX+2, nY+3, 2, 0, bColour);  pScreen_->DrawLine(nX+7, nY+3, 2, 0, bColour);
    pScreen_->DrawLine(nX+1, nY+4, 2, 0, bColour);  pScreen_->DrawLine(nX+8, nY+4, 2, 0, bColour);
}

////////////////////////////////////////////////////////////////////////////////

CDownButton::CDownButton (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
    : CButton(pParent_, nX_, nY_, nWidth_, nHeight_)
{
}

void CDownButton::Draw (CScreen* pScreen_)
{
    CButton::Draw(pScreen_);

    bool fPressed = m_fPressed && IsOver();

    int nX = m_nX + 2 + fPressed, nY = m_nY + 5 + fPressed;
    BYTE bColour = GetParent()->IsEnabled() ? BLACK : GREY_5;
    pScreen_->DrawLine(nX+5, nY+5,   1, 0, bColour);
    pScreen_->DrawLine(nX+4, nY+4, 3, 0, bColour);
    pScreen_->DrawLine(nX+3, nY+3, 2, 0, bColour);  pScreen_->DrawLine(nX+6, nY+3, 2, 0, bColour);
    pScreen_->DrawLine(nX+2, nY+2, 2, 0, bColour);  pScreen_->DrawLine(nX+7, nY+2, 2, 0, bColour);
    pScreen_->DrawLine(nX+1, nY+1, 2, 0, bColour);  pScreen_->DrawLine(nX+8, nY+1, 2, 0, bColour);
}

////////////////////////////////////////////////////////////////////////////////

const int PRETEXT_GAP = 5;
const int BOX_SIZE = 11;

CCheckBox::CCheckBox (CWindow* pParent_/*=NULL*/, int nX_/*=0*/, int nY_/*=0*/, const char* pcszText_/*=""*/,
    BYTE bColour_/*=WHITE*/, BYTE bBackColour_/*=0*/)
    : CWindow(pParent_, nX_, nY_, 0, BOX_SIZE, ctCheckBox),
    m_fChecked(false), m_bColour(bColour_), m_bBackColour(bBackColour_)
{
    SetText(pcszText_);
}

void CCheckBox::SetText (const char* pcszText_)
{
    CWindow::SetText(pcszText_);

    // Set the control width to be just enough to contain the text
    m_nWidth = 1 + BOX_SIZE + PRETEXT_GAP + GetTextWidth();
}

void CCheckBox::Draw (CScreen* pScreen_)
{
    // Draw the text to the right of the box, grey if the control is disabled
    int nX = m_nX + BOX_SIZE + PRETEXT_GAP;
    int nY = m_nY + (BOX_SIZE-CHAR_HEIGHT)/2 + 1;

    // Fill the background if required
    if (m_bBackColour)
        pScreen_->FillRect(m_nX-1, m_nY-1, BOX_SIZE + PRETEXT_GAP+GetTextWidth()+2, BOX_SIZE+2, m_bBackColour);

    // Draw the label text
    pScreen_->DrawString(nX, nY, GetText(), IsEnabled() ? (IsActive() ? YELLOW_8 : m_bColour) : GREY_5);

    // Draw the empty check box
    pScreen_->FrameRect(m_nX, m_nY, BOX_SIZE, BOX_SIZE, !IsEnabled() ? GREY_5 : IsActive() ? YELLOW_8 : GREY_7);

    BYTE abEnabled[] = { 0, GREY_7 }, abDisabled[] = { 0, GREY_5 };

    static BYTE abCheck[11][11] =
    {
        { 0,0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,1,0,0 },
        { 0,0,0,0,0,0,0,1,1,0,0 },
        { 0,0,0,0,0,0,1,1,1,0,0 },
        { 0,0,1,0,0,1,1,1,0,0,0 },
        { 0,0,1,1,1,1,1,0,0,0,0 },
        { 0,0,1,1,1,1,0,0,0,0,0 },
        { 0,0,0,1,1,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0,0 },
        { 0,0,0,0,0,0,0,0,0,0,0 },
    };

    // Box checked?
    if (m_fChecked)
        pScreen_->DrawImage(m_nX, m_nY, 11, 11, reinterpret_cast<BYTE*>(abCheck), IsEnabled() ? abEnabled : abDisabled);
}

bool CCheckBox::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    static bool fPressed = false;

    switch (nMessage_)
    {
        case GM_CHAR:
        {
            if (!IsActive())
                break;

            switch (nParam1_)
            {
                case ' ':
                case '\r':
                    SetChecked(!IsChecked());
                    NotifyParent();
                    return true;
            }
            break;
        }
            
        case GM_BUTTONDOWN:
        case GM_BUTTONDBLCLK:
            // Was the click over us?
            if (IsOver())
            {
                SetChecked(!IsChecked());
                NotifyParent();
                return fPressed = true;
            }
            break;

        case GM_BUTTONUP:
            if (fPressed)
            {
                fPressed = false;
                return true;
            }
            break;

        case GM_MOUSEMOVE:
            return fPressed;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

const size_t MAX_EDIT_LENGTH = 250;

CEditControl::CEditControl (CWindow* pParent_, int nX_, int nY_, int nWidth_, const char* pcszText_/*=""*/)
    : CWindow(pParent_, nX_, nY_, nWidth_, BUTTON_HEIGHT, ctEdit)
{
    SetText(pcszText_);
}

CEditControl::CEditControl (CWindow* pParent_, int nX_, int nY_, int nWidth_, UINT u_)
    : CWindow(pParent_, nX_, nY_, nWidth_, BUTTON_HEIGHT, ctEdit)
{
    SetValue(u_);
}

void CEditControl::Draw (CScreen* pScreen_)
{
    // Fill overall control background, and draw a frame round it
    pScreen_->FillRect(m_nX+1, m_nY+1, m_nWidth-2, m_nHeight-2, IsEnabled() ? (IsActive() ? YELLOW_8 : WHITE) : GREY_7);
    pScreen_->FrameRect(m_nX, m_nY, m_nWidth, m_nHeight, GREY_7);

    // Draw a light edge highlight for the bottom and right
    pScreen_->DrawLine(m_nX+1, m_nY+m_nHeight-1, m_nWidth-1, 0, GREY_7);
    pScreen_->DrawLine(m_nX+m_nWidth-1, m_nY+1, 0, m_nHeight-1, GREY_7);

    // The text could be too long for the control, so find the longest tail-segment that fits
    const char* pcsz = GetText();
    while (CScreen::GetStringWidth(pcsz) >= (m_nWidth - 4))
        pcsz++;

    int nY = m_nY + (m_nHeight - CHAR_HEIGHT)/2;

    // If the control is enabled and focussed we'll show a flashing caret after the text
    if (IsEnabled() && IsActive())
    {
        bool fCaretOn = (OSD::GetTime() % 800) < 400;

        // Draw a character-height vertical bar after the text
        int nX = m_nX + CScreen::GetStringWidth(pcsz)+ 4;
        pScreen_->DrawLine(nX, nY, 0, CHAR_HEIGHT+1, fCaretOn ? BLUE_4 : WHITE);
    }

    pScreen_->DrawString(m_nX+3, nY+1, pcsz, IsEnabled() ? BLACK : GREY_5);
}

bool CEditControl::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    switch (nMessage_)
    {
        case GM_BUTTONDOWN:
        case GM_BUTTONDBLCLK:
            // Was the click over us?
            if (IsOver())
                return true;
            break;

        case GM_CHAR:
            // Reject key presses if we're not active and the cursor isn't over us
            if (!IsActive())
                break;

            switch (nParam1_)
            {
                case HK_UP:
                case HK_DOWN:
                case HK_LEFT:
                case HK_RIGHT:
                    // Eat these for future cursor handling
                    return true;

                // Return possibly submits the dialog contents
                case '\r':
                    NotifyParent(1);
                    break;

                // Backspace deletes the last character
                case '\b':
                    if (*m_pszText)
                    {
                        m_pszText[strlen(m_pszText)-1] = '\0';
                        NotifyParent();
                    }
                    return true;

                default:
                    // Only accept printable characters
                    if (nParam1_ >= ' ' && nParam1_ <= 0x7f)
                    {
                        // Only add a character if we're not at the maximum length yet
                        if (strlen(GetText()) < MAX_EDIT_LENGTH)
                        {
                            char sz[MAX_EDIT_LENGTH+1], szNew[2] = { nParam1_, '\0' };
                            SetText(strcat(strcpy(sz, GetText()), szNew));
                            NotifyParent();
                        }

                        return true;
                    }
                    break;
            }
            break;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

const char* const RADIO_DELIMITER = "|";
const int RADIO_PRETEXT_GAP = 16;

CRadioButton::CRadioButton (CWindow* pParent_/*=NULL*/, int nX_/*=0*/, int nY_/*=0*/, const char* pcszText_/*=""*/, int nWidth_/*=0*/)
    : CWindow(pParent_, nX_, nY_, nWidth_, CHAR_HEIGHT+2, ctRadio), m_fSelected(false)
{
    SetText(pcszText_);
}

void CRadioButton::SetText (const char* pcszText_)
{
    CWindow::SetText(pcszText_);

    // Set the control width to be just enough to contain the text
    m_nWidth = 1 + RADIO_PRETEXT_GAP + GetTextWidth();
}

void CRadioButton::Draw (CScreen* pScreen_)
{
    int nX = m_nX+1, nY = m_nY;

    BYTE abActive[] = { 0, GREY_5, GREY_7, YELLOW_8 };
    BYTE abEnabled[] = { 0, GREY_5, GREY_7, GREY_7 };
    BYTE abDisabled[] = { 0, GREY_3, GREY_5, GREY_5 };

    static BYTE abSelected[10][10] = 
    {
        { 0,0,0,3,3,3,3 },
        { 0,0,3,0,0,0,0,3 },
        { 0,3,0,1,2,2,1,0,3 },
        { 3,0,1,2,2,2,2,1,0,3 },
        { 3,0,2,2,2,2,2,2,0,3 },
        { 3,0,2,2,2,2,2,2,0,3 },
        { 3,0,1,2,2,2,2,1,0,3 },
        { 0,3,0,1,2,2,1,0,3 },
        { 0,0,3,0,0,0,0,3 },
        { 0,0,0,3,3,3,3 }
    };

    static BYTE abUnselected[10][10] = 
    {
        { 0,0,0,3,3,3,3 },
        { 0,0,3,0,0,0,0,3 },
        { 0,3,0,0,0,0,0,0,3 },
        { 3,0,0,0,0,0,0,0,0,3 },
        { 3,0,0,0,0,0,0,0,0,3 },
        { 3,0,0,0,0,0,0,0,0,3 },
        { 3,0,0,0,0,0,0,0,0,3 },
        { 0,3,0,0,0,0,0,0,3 },
        { 0,0,3,0,0,0,0,3 },
        { 0,0,0,3,3,3,3 }
    };

    // Draw the radio button image in the current state
    pScreen_->DrawImage (nX, nY, 10, 10, reinterpret_cast<BYTE*>(m_fSelected ? abSelected : abUnselected),
                            !IsEnabled() ? abDisabled : IsActive() ? abActive : abEnabled);

    // Draw the text to the right of the button, grey if the control is disabled
    pScreen_->DrawString(nX+RADIO_PRETEXT_GAP, nY+1, GetText(), IsEnabled() ? (IsActive() ? YELLOW_8 : GREY_7) : GREY_5);
}

bool CRadioButton::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    static bool fPressed = false;

    switch (nMessage_)
    {
        case GM_CHAR:
        {
            if (!IsActive())
                break;

            switch (nParam1_)
            {
                case HK_LEFT:
                case HK_UP:
                {
                    CWindow* pPrev = GetPrev();
                    if (pPrev && pPrev->GetType() == GetType())
                    {
                        pPrev->Activate();
                        reinterpret_cast<CRadioButton*>(pPrev)->Select();
                        NotifyParent();
                    }
                    return true;
                }

                case HK_RIGHT:
                case HK_DOWN:
                {
                    CWindow* pNext = GetNext();
                    if (pNext && pNext->GetType() == GetType())
                    {
                        pNext->Activate();
                        reinterpret_cast<CRadioButton*>(pNext)->Select();
                        NotifyParent();
                    }
                    return true;
                }

                case '\r':
                    NotifyParent(1);
                    return true;
            }
            break;
        }

        case GM_BUTTONDOWN:
        case GM_BUTTONDBLCLK:
            // Was the click over us?
            if (IsOver())
            {
                Select();
                NotifyParent();
                return fPressed = true;
            }
            break;

        case GM_BUTTONUP:
            if (fPressed)
            {
                fPressed = false;
                return true;
            }
            break;
    }

    return fPressed;
}

void CRadioButton::Select (bool fSelected_/*=true*/)
{
    // Remember the new status
    m_fSelected = fSelected_;

    // Of it's a selection we have more work to do...
    if (m_fSelected)
    {
        // Search the control group
        for (CWindow* p = GetGroup() ; p && p->GetType() == ctRadio ; p = p->GetNext())
        {
            // Deselect the button if it's not us
            if (p != this)
                reinterpret_cast<CRadioButton*>(p)->Select(false);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

const char* const MENU_DELIMITERS = "|";
const int MENU_TEXT_GAP = 5;
const int MENU_ITEM_HEIGHT = 2+CHAR_HEIGHT+2;

CMenu::CMenu (CWindow* pParent_/*=NULL*/, int nX_/*=0*/, int nY_/*=0*/, const char* pcszText_/*=""*/)
    : CWindow(pParent_, nX_, nY_, 0, 0, ctMenu), m_nSelected(-1), m_fPressed(false)
{
    SetText(pcszText_);
    Activate();
}

void CMenu::Select (int nItem_)
{
    m_nSelected = (nItem_ < 0) ? 0 : (nItem_ >= m_nItems) ? m_nItems-1 : nItem_;
}

void CMenu::SetText (const char* pcszText_)
{
    CWindow::SetText(pcszText_);

    int nMaxLen = 0;
    char sz[256], *psz = strtok(strcpy(sz, GetText()), MENU_DELIMITERS);

    for (m_nItems = 0 ; psz ; psz = strtok(NULL, MENU_DELIMITERS), m_nItems++)
    {
        int nLen = CScreen::GetStringWidth(psz);
        if (nLen > nMaxLen)
            nMaxLen = nLen;
    }

    // Set the control width to be just enough to contain the text
    m_nWidth = MENU_TEXT_GAP + nMaxLen + MENU_TEXT_GAP;
    m_nHeight = MENU_ITEM_HEIGHT * m_nItems;
}

void CMenu::Draw (CScreen* pScreen_)
{
    // Fill overall control background and frame it
    pScreen_->FillRect(m_nX, m_nY, m_nWidth, m_nHeight, YELLOW_8);
    pScreen_->FrameRect(m_nX-1, m_nY-1, m_nWidth+2, m_nHeight+2, GREY_7);

    // Make a copy of the menu item list as strtok() munges as it iterates
    char sz[256], *psz = strtok(strcpy(sz, GetText()), MENU_DELIMITERS);

    // Loop through the items on the menu
    for (int i = 0 ; psz ; psz = strtok(NULL, MENU_DELIMITERS), i++)
    {
        int nX = m_nX, nY = m_nY + (MENU_ITEM_HEIGHT * i);

        // Draw the string over the default background if not selected
        if (i != m_nSelected)
            pScreen_->DrawString(nX+MENU_TEXT_GAP, nY+2, psz, BLACK);

        // Otherwise draw the selected item as white on black
        else
        {
            pScreen_->FillRect(nX, nY, m_nWidth, MENU_ITEM_HEIGHT, BLACK);
            pScreen_->DrawString(nX+MENU_TEXT_GAP, nY+2, psz, WHITE);
        }
    }
}

bool CMenu::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    switch (nMessage_)
    {
        case GM_CHAR:
            switch (nParam1_)
            {
                // Return uses the current selection
                case '\r':
                    NotifyParent();
                    Destroy();
                    return true;

                // Esc cancels
                case '\x1b':
                    m_nSelected = -1;
                    NotifyParent();
                    Destroy();
                    return true;

                // Move to the previous selection, wrapping to the bottom if necessary
                case HK_UP:
                    Select((m_nSelected-1 + m_nItems) % m_nItems);
                    break;

                // Move to the next selection, wrapping to the top if necessary
                case HK_DOWN:
                    Select((m_nSelected+1) % m_nItems);
                    break;
            }
            return true;

        case GM_BUTTONDBLCLK:
        case GM_BUTTONDOWN:
            // Button clicked (held?) on the menu, so remember it's happened
            if (IsOver())
                return m_fPressed = true;

            // Button clicked away from the menu, which cancels it
            m_nSelected = -1;
            NotifyParent();
            Destroy();
            return true;

        case GM_BUTTONUP:
            // Cursor not over the control?
            if (!IsOver())
            {
                // If it wasn't a drag from the menu, ignore it
                if (!m_fPressed)
                    break;

                // Was a drag, so treat this as a cancel
                m_nSelected = -1;
            }

            // Return the current selection
            NotifyParent();
            Destroy();
            return true;

        case GM_MOUSEMOVE:
            // Determine the menu item we're above, if any
            m_nSelected = IsOver() ? ((nParam2_-m_nY) / MENU_ITEM_HEIGHT) : -1;

            // Treat the movement as an effective drag, for the case of a combo-box drag
            return m_fPressed = true;

        case GM_MOUSEWHEEL:
            if (IsActive())
            {
                Select((m_nSelected + nParam1_ + m_nItems) % m_nItems);
                return true;
            }
            break;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

CDropList::CDropList (CWindow* pParent_/*=NULL*/, int nX_/*=0*/, int nY_/*=0*/, const char* pcszText_/*=""*/, int nMinWidth_/*=0*/)
    : CMenu(pParent_, nX_, nY_, pcszText_), m_nMinWidth(nMinWidth_)
{
    m_nSelected = 0;
    SetText(pcszText_);
}

void CDropList::SetText (const char* pcszText_)
{
    CMenu::SetText(pcszText_);

    if (m_nWidth < m_nMinWidth)
        m_nWidth = m_nMinWidth;
}

bool CDropList::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    // Eat movement messages that are not over the control
    if (nMessage_ == GM_MOUSEMOVE && !IsOver())
        return true;

    return CMenu::OnMessage(nMessage_, nParam1_, nParam2_);
}

////////////////////////////////////////////////////////////////////////////////

const int COMBO_TEXT_GAP = 5;
const int COMBO_BORDER = 3;
const int COMBO_HEIGHT = COMBO_BORDER + CHAR_HEIGHT + COMBO_BORDER;

CComboBox::CComboBox (CWindow* pParent_/*=NULL*/, int nX_/*=0*/, int nY_/*=0*/, const char* pcszText_/*=""*/, int nWidth_/*=0*/)
    : CWindow(pParent_, nX_, nY_, nWidth_, COMBO_HEIGHT, ctComboBox),
    m_nItems(0), m_nSelected(0), m_fPressed(false), m_pDropList(NULL)
{
    SetText(pcszText_);
}


void CComboBox::Select (int nSelected_)
{
    int nOldSelection = m_nSelected;
    m_nSelected = (nSelected_ < 0 || !m_nItems) ? 0 : (nSelected_ >= m_nItems) ? m_nItems-1 : nSelected_;

    // Nofify the parent if the selection has changed
    if (m_nSelected != nOldSelection)
        NotifyParent();
}

void CComboBox::Select (const char* pcszItem_)
{
    char sz[256];

    // Find the text for the current selection
    char* psz = strtok(strcpy(sz, GetText()), "|");
    for (int i = 0 ; psz && i < m_nSelected ; psz = strtok(NULL, "|"), i++)
    {
        // If we've found the item, select it
        if (!strcasecmp(psz, pcszItem_))
        {
            Select(i);
            break;
        }
    }
}


const char* CComboBox::GetSelectedText ()
{
    static char sz[256];

    // Find the text for the current selection
    char* psz = strtok(strcpy(sz, GetText()), "|");
    for (int i = 0 ; psz && i < m_nSelected ; psz = strtok(NULL, "|"), i++);

    // Return the item string if found
    return psz ? psz : "";
}

void CComboBox::SetText (const char* pcszText_)
{
    CWindow::SetText(pcszText_);

    // Count the number of items in the list, then select the first one
    for (m_nItems = !!*pcszText_ ; (pcszText_ = strchr(pcszText_,'|')) ; pcszText_++, m_nItems++);
    Select(0);
}

void CComboBox::Draw (CScreen* pScreen_)
{
    bool fPressed = m_fPressed;

    // Fill the main control background
    pScreen_->FrameRect(m_nX, m_nY, m_nWidth, m_nHeight, GREY_7);
    pScreen_->FillRect(m_nX+1, m_nY+1, m_nWidth-COMBO_HEIGHT-1, m_nHeight-2,
                        !IsEnabled() ? GREY_7 : (IsActive() && !fPressed) ? YELLOW_8 : WHITE);

    // Fill the main button background
    int nX = m_nX + m_nWidth - COMBO_HEIGHT, nY = m_nY + 1;
    pScreen_->FillRect(nX+1, nY+1, COMBO_HEIGHT-1, m_nHeight-3, GREY_7);

    // Draw the edge highlight for the top, left, right and bottom
    pScreen_->DrawLine(nX, nY, COMBO_HEIGHT, 0, fPressed ? GREY_5 : WHITE);
    pScreen_->DrawLine(nX, nY, 0, m_nHeight-2, fPressed ? GREY_5 : WHITE);
    pScreen_->DrawLine(nX+1, nY+m_nHeight-2, COMBO_HEIGHT-2, 0, fPressed ? WHITE : GREY_5);
    pScreen_->DrawLine(nX+COMBO_HEIGHT-1, nY+1, 0, m_nHeight-2, fPressed ? WHITE : GREY_5);

    // Show the arrow button, down a pixel if it's pressed
    nY += fPressed;
    BYTE bColour = IsEnabled() ? BLACK : GREY_5;
    pScreen_->DrawLine(nX+8, nY+9, 1, 0, bColour);
    pScreen_->DrawLine(nX+7, nY+8, 3, 0, bColour);
    pScreen_->DrawLine(nX+6, nY+7, 2, 0, bColour);  pScreen_->DrawLine(nX+9,  nY+7, 2, 0, bColour);
    pScreen_->DrawLine(nX+5, nY+6, 2, 0, bColour);  pScreen_->DrawLine(nX+10, nY+6, 2, 0, bColour);
    pScreen_->DrawLine(nX+4, nY+5, 2, 0, bColour);  pScreen_->DrawLine(nX+11, nY+5, 2, 0, bColour);


    // Find the text for the current selection
    char sz[256], *psz = strtok(strcpy(sz, GetText()), "|");
    for (int i = 0 ; psz && i < m_nSelected ; psz = strtok(NULL, "|"), i++);

    // Draw the current selection
    nX = m_nX + 5;
    nY = m_nY + (m_nHeight-CHAR_HEIGHT)/2 + 1;
    pScreen_->DrawString(nX, nY, psz ? psz : "", IsEnabled() ? (IsActive() ? BLACK : BLACK) : GREY_5);

    // Call the base to paint any child controls
    CWindow::Draw(pScreen_);
}

bool CComboBox::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    // Give child controls first go at the message
    if (CWindow::OnMessage(nMessage_, nParam1_, nParam2_))
        return true;

    switch (nMessage_)
    {
        case GM_CHAR:
            if (!IsActive())
                break;

            switch (nParam1_)
            {
                case ' ':
                case '\r':
                    m_fPressed = !m_fPressed;
                    if (m_fPressed)
                        (m_pDropList = new CDropList(this, 1, COMBO_HEIGHT, GetText(), m_nWidth-2))->Select(m_nSelected);
                    return true;

                case HK_UP:
                    Select(m_nSelected-1);
                    return true;

                case HK_DOWN:
                    Select(m_nSelected+1);
                    return true;

                case HK_HOME:
                    Select(0);
                    return true;

                case HK_END:
                    Select(m_nItems-1);
                    return true;
            }
            break;

        case GM_BUTTONDOWN:
        case GM_BUTTONDBLCLK:
            if (!IsOver())
                break;

            // If the click was over us, and the button is pressed, create the drop list
            if (IsOver() && (m_fPressed = !m_fPressed))
                (m_pDropList = new CDropList(this, 1, COMBO_HEIGHT, GetText(), m_nWidth-2))->Select(m_nSelected);

            return true;

        case GM_MOUSEWHEEL:
            if (IsActive())
            {
                Select(m_nSelected + nParam1_);
                return true;
            }
            break;
    }

    return false;
}

void CComboBox::OnNotify (CWindow* pWindow_, int nParam_/*=0*/)
{
    if (pWindow_ == m_pDropList)
    {
        int nSelected = m_pDropList->GetSelected();
        if (nSelected != -1)
            Select(nSelected);

        m_fPressed = false;
        m_pDropList = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////////

const int SCROLLBAR_WIDTH = 15;
const int SB_BUTTON_HEIGHT = 15;

CScrollBar::CScrollBar (CWindow* pParent_, int nX_, int nY_, int nHeight_, int nMaxPos_, int nStep_/*=1*/)
: CWindow(pParent_, nX_, nY_, SCROLLBAR_WIDTH, nHeight_), m_nStep(nStep_), m_fDragging(false)
{
    m_pUp = new CUpButton(this, 0, 0, m_nWidth, SB_BUTTON_HEIGHT);
    m_pDown = new CDownButton(this, 0, m_nHeight-SB_BUTTON_HEIGHT, m_nWidth, SB_BUTTON_HEIGHT);

    m_nScrollHeight = nHeight_ - SB_BUTTON_HEIGHT*2;
    SetMaxPos(nMaxPos_);
}

void CScrollBar::SetPos (int nPosition_)
{
    m_nPos = (nPosition_ < 0) ? 0 : (nPosition_ > m_nMaxPos) ? m_nMaxPos : nPosition_;
}

void CScrollBar::SetMaxPos (int nMaxPos_)
{
    m_nPos = 0;

    // Determine how much of the height is not covered by the current view
    m_nMaxPos = nMaxPos_ - m_nHeight;

    // If we have a scrollable portion, set the thumb size to indicate how much
    if (nMaxPos_ && m_nMaxPos > 0)
        m_nThumbSize = max(m_nHeight * m_nScrollHeight / nMaxPos_, 10);
}

void CScrollBar::Draw (CScreen* pScreen_)
{
    // Don't draw anything if we don't have a scroll range
    if (m_nMaxPos <= 0)
        return;

    // Fill the main button background
    pScreen_->FillRect(m_nX+1, m_nY+1, m_nWidth-2, m_nHeight-2, IsActive() ? YELLOW_8 : GREY_7);

    // Draw the edge highlight for the top, left, bottom and right
    pScreen_->DrawLine(m_nX, m_nY, m_nWidth, 0, WHITE);
    pScreen_->DrawLine(m_nX, m_nY, 0, m_nHeight, WHITE);
    pScreen_->DrawLine(m_nX+1, m_nY+m_nHeight-1, m_nWidth-2, 0, WHITE);
    pScreen_->DrawLine(m_nX+m_nWidth-1, m_nY+1, 0, m_nHeight-1, WHITE);

    int nHeight = m_nScrollHeight - m_nThumbSize, nPos = nHeight * m_nPos / m_nMaxPos;

    int nX = m_nX, nY = m_nY + SB_BUTTON_HEIGHT + nPos;

    // Fill the main button background
    pScreen_->FillRect(nX, nY, m_nWidth, m_nThumbSize, !IsEnabled() ? GREY_7 : GREY_7);

    pScreen_->DrawLine(nX, nY, m_nWidth, 0, WHITE);
    pScreen_->DrawLine(nX, nY, 0, m_nThumbSize, WHITE);
    pScreen_->DrawLine(nX+1, nY+m_nThumbSize-1, m_nWidth-1, 0, GREY_4);
    pScreen_->DrawLine(nX+m_nWidth-1, nY+1, 0, m_nThumbSize-1, GREY_4);

    CWindow::Draw(pScreen_);
}

bool CScrollBar::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    static int nDragOffset;
    static bool fDragging;

    // We're inert (and invisible) if there's no scroll range
    if (m_nMaxPos <= 0)
        return false;

    bool fRet = CWindow::OnMessage(nMessage_, nParam1_, nParam2_);

    // Stop the buttons remaining active
    m_pActive = NULL;

    if (fRet)
        return true;

    switch (nMessage_)
    {
        case GM_CHAR:
            if (!IsActive())
                break;

            switch (nParam1_)
            {
                case HK_UP:     SetPos(m_nPos - m_nStep);   break;
                case HK_DOWN:   SetPos(m_nPos + m_nStep);   break;

                default:        return false;
            }
            
            return true;

        case GM_BUTTONDOWN:
        case GM_BUTTONDBLCLK:
            // Was the click over us when we have an range to scroll
            if (IsOver() && m_nMaxPos > 0)
            {
                int nPos = (m_nScrollHeight - m_nThumbSize) * m_nPos / m_nMaxPos, nY = nParam2_ - (m_nY + SB_BUTTON_HEIGHT);

                // Page up if the click was above the thumb
                if (nY < nPos)
                    SetPos(m_nPos - m_nHeight);

                // Page down if below the thumb
                else if (nY >= (nPos+m_nThumbSize))
                    SetPos(m_nPos + m_nHeight);

                // Click is on the thumb
                else
                {
                    // Remember the vertical offset onto the thumb for drag calculation
                    nDragOffset = nParam2_ - (m_nY + SB_BUTTON_HEIGHT) - nPos;
                    fDragging = true;
                }

                return true;
            }
            break;

        case GM_BUTTONUP:
            if (fDragging)
            {
                fDragging = false;
                return true;
            }
            break;

        case GM_MOUSEMOVE:
            if (fDragging)
            {
                // Calculate the new position represented by the thumb, and limit it to the valid range
                SetPos((nParam2_ - (m_nY + SB_BUTTON_HEIGHT) - nDragOffset) * m_nMaxPos / (m_nScrollHeight - m_nThumbSize));

                return true;
            }
            break;

        case GM_MOUSEWHEEL:
            SetPos(m_nPos + m_nStep*nParam1_);
            return true;
    }
    return false;
}

void CScrollBar::OnNotify (CWindow* pWindow_, int nParam_/*=0*/)
{
    if (pWindow_ == m_pUp)
        SetPos(m_nPos - m_nStep);
    else if (pWindow_ == m_pDown)
        SetPos(m_nPos + m_nStep);
}

////////////////////////////////////////////////////////////////////////////////

const int ITEM_SIZE = 72;

// Helper to locate matches when a filename is typed
static bool IsPrefix (const char* pcszPrefix_, const char* pcszName_)
{
    // Skip any common characters at the start of the name
    while (*pcszPrefix_ && *pcszName_ && tolower(*pcszPrefix_) == tolower(*pcszName_))
        pcszPrefix_++, pcszName_++;

    // Return true if the full prefix was matched
    return !*pcszPrefix_;
}


CListView::CListView (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_, int nItemOffset_/*=0*/)
    : CWindow(pParent_, nX_, nY_, nWidth_, nHeight_, ctListView),
    m_nItems(0), m_nSelected(0), m_nHoverItem(0), m_nAcross(0), m_nDown(0), m_nItemOffset(nItemOffset_), m_pItems(NULL)
{
    // Create a scrollbar to cover the overall height, scrolling if necessary
    m_pScrollBar = new CScrollBar(this, m_nWidth-SCROLLBAR_WIDTH, 0, m_nHeight, 0, ITEM_SIZE);
}

CListView::~CListView ()
{
    // Free any existing items
    SetItems(NULL);
}


void CListView::Select (int nItem_)
{
    int nOldSelection = m_nSelected;
    m_nSelected = (nItem_ < 0) ? 0 : (nItem_ >= m_nItems) ? m_nItems-1 : nItem_;

    // Calculate the row containing the new item, and the vertical offset in the list overall
    int nRow = m_nSelected / m_nAcross, nOffset = nRow*ITEM_SIZE - m_pScrollBar->GetPos();

    // If the new item is not completely visible, scroll the list so it _just_ is
    if (nOffset < 0 || nOffset >= (m_nHeight-ITEM_SIZE))
        m_pScrollBar->SetPos(nRow*ITEM_SIZE - ((nOffset < 0) ? 0 : (m_nHeight-ITEM_SIZE)));

    // Inform the owner if the selection has changed
    if (m_nSelected != nOldSelection)
        NotifyParent();
}


// Return the entry for the specified item, or the current item if none was specified
const CListViewItem* CListView::GetItem (int nItem_/*=-1*/) const
{
    int i;

    // If no item is specified, return the default
    if (nItem_ == -1)
        nItem_ = GetSelected();

    // Linear search for the item - not so good for large directories!
    const CListViewItem* pItem = m_pItems;
    for (i = 0 ; pItem && i < nItem_ ; i++)
        pItem = pItem->m_pNext;

    // Return the item if found
    return (i == nItem_) ? pItem : NULL;
}

// Find the item with the specified label (not case-sensitive)
int CListView::FindItem (const char* pcszLabel_, int nStart_/*=0*/)
{
    // Linear search for the item - not so good for large directories!
    int nItem = 0;
    for (const CListViewItem* pItem = GetItem(nStart_) ; pItem ; pItem = pItem->m_pNext, nItem++)
    {
        if (!strcasecmp(pItem->m_pszLabel, pcszLabel_))
            return nItem;
    }

    // Not found
    return -1;
}


void CListView::SetItems (CListViewItem* pItems_)
{
    // Delete any existing list
    for (CListViewItem* pNext ; m_pItems ; m_pItems = pNext)
    {
        pNext = m_pItems->m_pNext;
        delete m_pItems;
    }

    // Count the number of items in the new list
    for (m_nItems = 0, m_pItems = pItems_ ; pItems_ ; pItems_ = pItems_->m_pNext, m_nItems++);

    // Calculate how many items on a row, and how many rows, and set the required scrollbar size
    m_nAcross = m_nWidth/ITEM_SIZE;
    m_nDown = (m_nItems+m_nAcross-1) / m_nAcross;
    m_pScrollBar->SetMaxPos(m_nDown*ITEM_SIZE);

    Select(0);
}

void CListView::DrawItem (CScreen* pScreen_, int nItem_, int nX_, int nY_, const CListViewItem* pItem_)
{
    // If this is the selected item, draw a box round it (darkened if the control isn't active)
    if (nItem_ == m_nSelected)
    {
        if (IsActive())
            pScreen_->FillRect(nX_+1, nY_+1, ITEM_SIZE-2, ITEM_SIZE-2, BLUE_2);

        pScreen_->FrameRect(nX_, nY_, ITEM_SIZE, ITEM_SIZE, IsActive() ? GREY_7 : GREY_5, true);
    }

    if (pItem_->m_pIcon)
        pScreen_->DrawImage(nX_+(ITEM_SIZE-ICON_SIZE)/2, nY_+m_nItemOffset+5, ICON_SIZE, ICON_SIZE,
                            reinterpret_cast<const BYTE*>(pItem_->m_pIcon->abData), pItem_->m_pIcon->abPalette);

    const char* pcsz = pItem_->m_pszLabel;
    if (pcsz)
    {
        int nLine = 0;
        const char *pszStart = pcsz, *pszBreak = NULL;
        char szLines[2][64], sz[64];
        *szLines[0] = *szLines[1] = '\0';

        // Spread the item text over up to 2 lines
        while (nLine < 2)
        {
            size_t uLen = pcsz-pszStart;
            strncpy(sz, pszStart, uLen)[uLen] = '\0';

            if (CScreen::GetStringWidth(sz) >= (ITEM_SIZE-9))
            {
                sz[uLen-1] = '\0';
                pcsz--;

                if (nLine == 1 || !pszBreak)
                {
                    if (nLine == 1)
                        strcpy(sz+(pcsz-pszStart-2), "...");
                    strcpy(szLines[nLine++], sz);
                    pszStart = pcsz;
                }
                else
                {
                    if (nLine == 1)
                        strcpy(sz+(pszBreak-pszStart-2), "...");
                    else
                        sz[pszBreak-pszStart] = '\0';

                    strcpy(szLines[nLine++], sz);
                    pszStart = pszBreak + (*pszBreak == ' ');
                    pszBreak = NULL;
                }
            }

            // Check for a break point position
            if ((*pcsz =='.' || *pcsz == ' ') && pcsz != pszStart)
                pszBreak = pcsz;

            if (!*pcsz++)
            {
                if (nLine < 2)
                    strcpy(szLines[nLine++], pszStart);
                break;
            }
        }

        // Output the two text lines using the small font, each centralised below the icon
        nY_ += m_nItemOffset+42;
        pScreen_->SetFont(&sOldFont);
        pScreen_->DrawString(nX_+(ITEM_SIZE-pScreen_->GetStringWidth(szLines[0]))/2, nY_, szLines[0], WHITE);
        pScreen_->DrawString(nX_+(ITEM_SIZE-pScreen_->GetStringWidth(szLines[1]))/2, nY_+12, szLines[1], WHITE);
        pScreen_->SetFont(&sGUIFont);
    }
}

// Erase the control background
void CListView::EraseBackground (CScreen* pScreen_)
{
    pScreen_->FillRect(m_nX, m_nY, m_nWidth, m_nHeight, BLUE_1);
}

void CListView::Draw (CScreen* pScreen_)
{
    // Fill the main background of the control
    EraseBackground(pScreen_);

    // Fetch the current scrollbar position
    int nScrollPos = m_pScrollBar->GetPos();

    // Calculate the range of icons that are visible and need drawing
    int nStart = nScrollPos/ITEM_SIZE * m_nAcross, nOffset = nScrollPos % ITEM_SIZE;
    int nDepth = (m_nHeight + nOffset + ITEM_SIZE-1) / ITEM_SIZE;
    int nEnd = min(m_nItems, nStart + m_nAcross*nDepth);

    // Clip to the main control, to keep partly drawn icons within our client area
    pScreen_->SetClip(m_nX, m_nY, m_nWidth, m_nHeight);

    const CListViewItem* pItem = GetItem(nStart);
    for (int i = nStart ; pItem && i < nEnd ; pItem=pItem->m_pNext, i++)
    {
        int x = m_nX + ((i % m_nAcross) * ITEM_SIZE), y = m_nY + (((i-nStart) / m_nAcross) * ITEM_SIZE) - nOffset;
        DrawItem(pScreen_, i, x, y, pItem);
    }

    // Restore the default clip area
    pScreen_->SetClip();
    CWindow::Draw(pScreen_);
}


bool CListView::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    static char szPrefix[16] = "";

    // Give the scrollbar first look at the message, but prevent it remaining active
    bool fRet = CWindow::OnMessage(nMessage_, nParam1_, nParam2_);
    m_pActive = NULL;
    if (fRet)
        return fRet;

    switch (nMessage_)
    {
        case GM_CHAR:
            if (!IsActive())
                break;

            switch (nParam1_)
            {
                case HK_LEFT:   Select(m_nSelected-1);  break;
                case HK_RIGHT:  Select(m_nSelected+1);  break;

                case HK_UP:
                    // Only move up if we're not already on the top line
                    if (m_nSelected >= m_nAcross)
                        Select(m_nSelected-m_nAcross);
                    break;

                case HK_DOWN:
                {
                    // Calculate the row the new item would be on
                    int nNewRow = min(m_nSelected+m_nAcross,m_nItems-1) / m_nAcross;

                    // Only move down if we're not already on the bottom row
                    if (nNewRow != m_nSelected/m_nAcross)
                        Select(m_nSelected+m_nAcross);
                    break;
                }

                // Move up one screen full, staying on the same column
                case HK_PGUP:
                {
                    int nUp = min(m_nHeight/ITEM_SIZE,m_nSelected/m_nAcross) * m_nAcross;
                    Select(m_nSelected-nUp);
                    break;
                }

                // Move down one screen full, staying on the same column
                case HK_PGDN:
                {
                    int nDown = min(m_nHeight/ITEM_SIZE,(m_nItems-m_nSelected-1)/m_nAcross) * m_nAcross;
                    Select(m_nSelected+nDown);
                    break;
                }

                // Move to first item
                case HK_HOME:
                    Select(0);
                    break;

                // Move to last item
                case HK_END:
                    Select(m_nItems-1);
                    break;

                // Return selects the current item - like a double-click
                case '\r':
                    szPrefix[0] = '\0';
                    NotifyParent(1);
                    break;

                default:
                {
                    static DWORD dwLastChar = 0;
                    DWORD dwNow = OSD::GetTime();

                    // Clear the buffer on any non-printing characters or if too long since the last one
                    if (!isprint(nParam1_) || (dwNow - dwLastChar > 1000))
                        szPrefix[0] = '\0';

                    // Ignore non-printable characters, or if the buffer is full
                    if (!isprint(nParam1_) || strlen(szPrefix) >= 15)
                        return false;

                    // Ignore duplicates of the same first character, to skip to the next match
                    if (!(strlen(szPrefix) == 1 && szPrefix[0] == nParam1_))
                    {
                        // Add the new character to the buffer
                        char* psz = szPrefix + strlen(szPrefix);
                        *psz++ = nParam1_;
                        *psz = '\0';

                        dwLastChar = dwNow;
                    }

                    // Look for a match, starting *after* the current selection if this is the first character
                    int nItem = GetSelected() + (strlen(szPrefix) == 1);
                    const CListViewItem* p = GetItem(nItem);

                    bool fFound = false;
                    for ( ; p && !(fFound = IsPrefix(szPrefix, p->m_pszLabel)) ; p = p->m_pNext, nItem++);

                    // Nothing found from the item to the end of the list
                    if (!fFound)
                    {
                        // Wrap to search from the start
                        p = GetItem(nItem = 0);
                        for ( ; p && !(fFound = IsPrefix(szPrefix, p->m_pszLabel)) ; p = p->m_pNext, nItem++);

                        // No match, so give up
                        if (!fFound)
                            break;
                    }

                    // Select the matching item
                    Select(nItem);
                    break;
                }
            }

            m_nHoverItem = -1;
            return true;

        case GM_MOUSEMOVE:
        {
            if (!IsOver())
            {
                m_nHoverItem = -1;
                return false;
            }

            int nAcross = (nParam1_ - m_nX) / ITEM_SIZE, nDown = (nParam2_ - m_nY + m_pScrollBar->GetPos()) / ITEM_SIZE;

            // Calculate the item we're above, if any
            int nHoverItem = nAcross + (nDown * m_nAcross);
            m_nHoverItem = (nAcross < m_nAcross && nHoverItem < m_nItems) ? nHoverItem : -1;

            break;
        }

        case GM_BUTTONDOWN:
            if (!IsOver())
                break;

            if (m_nHoverItem != -1)
                Select(m_nHoverItem);
            return true;

        case GM_BUTTONDBLCLK:
            if (!IsOver())
                break;

            NotifyParent(1);
            return true;

        case GM_MOUSEWHEEL:
            m_nHoverItem = -1;
            return false;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

// Compare two filenames, returning true if the 1st entry comes after the 2nd
static bool SortCompare (const char* pcsz1_, const char* pcsz2_)
{
    // Skip any common characters at the start of the name
    while (*pcsz1_ && *pcsz2_ && tolower(*pcsz1_) == tolower(*pcsz2_))
        pcsz1_++, pcsz2_++;

    // Compare the first character that differs
    return tolower(*pcsz1_) > tolower(*pcsz2_);
}

// Compare two file entries, returning true if the 1st entry comes after the 2nd
static bool SortCompare (CListViewItem* p1_, CListViewItem* p2_)
{
    // Drives come before directories, which come before files
    if ((p1_->m_pIcon == &sFolderIcon) ^ (p2_->m_pIcon == &sFolderIcon))
        return p2_->m_pIcon == &sFolderIcon;

    // Compare the filenames
    return SortCompare(p1_->m_pszLabel, p2_->m_pszLabel);
}


CFileView::CFileView (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
    : CListView(pParent_, nX_, nY_, nWidth_, nHeight_), m_pszPath(NULL), m_pszFilter(NULL), m_fShowHidden(false)
{
}

CFileView::~CFileView()
{
    delete[] m_pszPath;
    delete[] m_pszFilter;
}


bool CFileView::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    bool fRet = CListView::OnMessage(nMessage_, nParam1_, nParam2_);

    // Backspace moves up a directory
    if (!fRet && nMessage_ == GM_CHAR && nParam1_ == '\b')
    {
        // We can only move up if there's a .. entry
        int nItem = FindItem("..");
        if (nItem != -1)
        {
            Select(nItem);
            NotifyParent(1);
            fRet = true;
        }
    }

    return fRet;
}

void CFileView::NotifyParent (int nParam_)
{
    const CListViewItem* pItem = GetItem();

    if (pItem && nParam_)
    {
        // Double-clicking to open a directory?
        if (pItem->m_pIcon == &sFolderIcon)
        {
            // Make a copy of the current path to modify
            char szPath[MAX_PATH];
            strcpy(szPath, m_pszPath);

            // Stepping up a level?
            if (!strcmp(pItem->m_pszLabel, ".."))
            {
                // Strip the trailing [back]slash
                szPath[strlen(szPath)-1] = '\0';

                // Strip the current directory name, if we find another separator
                char* psz = strrchr(szPath, PATH_SEPARATOR);
                if (psz)
                    psz[1] = '\0';

                // Otherwise remove the drive letter to leave a blank path for a virtual drive list (DOS/Win32 only)
                else
                    szPath[0] = '\0';
            }

            // Change to a sub-directory
            else
            {
                // Add the sub-directory name and a trailing backslash
                char szSep[2] = { PATH_SEPARATOR, '\0' };
                strcat(strcat(szPath, pItem->m_pszLabel), szSep);
            }

            // Make sure we have access to the path before setting it
            if (!szPath[0] || OSD::CheckPathAccess(szPath))
                SetPath(szPath);
            else
            {
                char szError[256];
                sprintf(szError, "%s%c\n\nCan't access directory.", pItem->m_pszLabel, PATH_SEPARATOR);
                new CMessageBox(this, szError, "Access Denied", mbError);
            }
        }
    }

    // Base handling, to notify parent
    CWindow::NotifyParent(nParam_);
}

// Determine an appropriate icon for the supplied file name/extension
const GUI_ICON* CFileView::GetFileIcon (const char* pcszFile_)
{
    // Determine the main file extension
    char sz[MAX_PATH];
    char* pszExt = strrchr(strcpy(sz, pcszFile_), '.');

    int nCompressType = 0;
    if (pszExt)
    {
        if (!strcasecmp(pszExt, ".gz")) nCompressType = 1;
        if (!strcasecmp(pszExt, ".zip")) nCompressType = 2;

        // Strip off the main extension and look for another
        if (nCompressType)
        {
            *pszExt = '\0';
            pszExt = strrchr(sz, '.');
        }
    }

    static const char* aExts[] = { ".dsk", ".sad", ".td0", ".sbt", ".mgt", ".img", ".sdf", ".cpm" };
    bool fDiskImage = false;

    for (UINT u = 0 ; !fDiskImage && pszExt && u < sizeof(aExts)/sizeof(aExts[0]) ; u++)
        fDiskImage = !strcasecmp(pszExt, aExts[u]);

    return nCompressType ? &sCompressedIcon : fDiskImage ? &sDiskIcon : &sDocumentIcon;
}


// Get the full path of the current item
const char* CFileView::GetFullPath () const
{
    static char szPath[MAX_PATH];
    const CListViewItem* pItem;

    if (!m_pszPath || !(pItem = GetItem()))
        return NULL;

    return strcat(strcpy(szPath, m_pszPath), pItem->m_pszLabel);
}

// Set a new path to browse
void CFileView::SetPath (const char* pcszPath_)
{
    if (pcszPath_)
    {
        delete[] m_pszPath;
        strcpy(m_pszPath = new char[strlen(pcszPath_)+1], pcszPath_);

        // If the path doesn't end in a separator, we've got a filename too
        const char* pcszFile = strrchr(pcszPath_, PATH_SEPARATOR);
        if (pcszFile && *++pcszFile)
            m_pszPath[pcszFile-pcszPath_] = '\0';

        // Fill the file list
        Refresh();

        // If we can find the file in the list, select it
        int nItem;
        if (pcszFile && (nItem = FindItem(pcszFile)) != -1)
            Select(nItem);
    }
}

// Set a new file filter
void CFileView::SetFilter (const char* pcszFilter_)
{
    if (pcszFilter_)
    {
        delete[] m_pszFilter;
        strcpy(m_pszFilter = new char[strlen(pcszFilter_)+1], pcszFilter_);
        Refresh();
    }
}

// Set whether hidden files should be shown
void CFileView::ShowHidden (bool fShow_)
{
    // Update the option and refresh the file list
    m_fShowHidden = fShow_;
    Refresh();
}


// Populate the list view with items from the path matching the current file filter
void CFileView::Refresh ()
{
    // Return unless we've got both a path and a file filter
    if (!m_pszPath || !m_pszFilter)
        return;

    // Make a copy of the current selection label name
    const CListViewItem* pItem = GetItem();
    char* pszLabel = pItem ? strdup(pItem->m_pszLabel) : NULL;

    // Free any existing list before we allocate a new one
    SetItems(NULL);
    CListViewItem* pItems = NULL;

    // An empty path gives a virtual drive list (only possible on DOS/Win32)
    if (!m_pszPath[0])
    {
        // Work through the letters backwards as we add to the head of the file chain
        for (int chDrive = 'Z' ; chDrive >= 'A' ; chDrive--)
        {
            char szRoot[] = { chDrive, ':', '\\', '\0' };

            // Can we access the root directory?
            if (OSD::CheckPathAccess(szRoot))
            {
                // Remove the backslash to leave just X:, and add to the list
                szRoot[2] = '\0';
                pItems = new CListViewItem(&sFolderIcon, szRoot, pItems);
            }
        }
    }
    else
    {
        DIR* dir = opendir(m_pszPath);
        if (dir)
        {
            // Count the number of filter items to apply
            int nFilters = *m_pszFilter ? 1 : 0;
            char szFilters[256];
            for (char* psz = strtok(strcpy(szFilters, m_pszFilter), ";") ; psz && (psz = strtok(NULL, ";")) ; nFilters++);

            for (struct dirent* entry ; (entry = readdir(dir)) ; )
            {
                char sz[MAX_PATH];
                struct stat st;

                // Ignore . and .. for now (we'll add .. back later if required)
                if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
                    continue;

                // Should we remove hidden files from the listing?
                if (!m_fShowHidden)
                {
                    // Form the full path of the current file
                    char szPath[MAX_PATH];
                    strcat(strcpy(szPath , m_pszPath), entry->d_name);

                    // Skip the file if it's considered hidden
                    if (OSD::IsHidden(szPath))
                        continue;
                }

                // Examine the entry to see what it is
                stat(strcat(strcpy(sz, m_pszPath), entry->d_name), &st);

                // Only regular files are affected by the file filter
                if (S_ISREG(st.st_mode))
                {
                    // If we have a filter list, apply it now
                    if (nFilters)
                    {
                        int i;

                        // Ignore files with no extension
                        char* pszExt = strrchr(entry->d_name, '.');
                        if (!pszExt)
                            continue;

                        // Compare the extension with each of the filters
                        char* pszFilter = szFilters;
                        for (i = 0 ; i < nFilters && strcasecmp(pszFilter, pszExt) ; i++, pszFilter += strlen(pszFilter)+1);

                        // Ignore the entry if we didn't match it
                        if (i == nFilters)
                            continue;
                    }
                }

                // Ignore anything that isn't a directory or a block device (or a symbolic link to one)
                else if (!S_ISDIR(st.st_mode) && !S_ISBLK(st.st_mode))
                    continue;

                // Create a new list entry for the current item
                CListViewItem* pNew = new CListViewItem(S_ISDIR(st.st_mode) ? &sFolderIcon :
                                                        S_ISBLK(st.st_mode) ? &sMiscIcon :
                                                        GetFileIcon(entry->d_name), entry->d_name);

                // Insert the item into the correct sort position
                CListViewItem *p = pItems, *pPrev = NULL;
                while (p && SortCompare(pNew, p))
                {
                    pPrev = p;
                    p = p->m_pNext;
                }

                // Adjust the links to any neighbouring entries
                if (pPrev) pPrev->m_pNext = pNew;
                pNew->m_pNext = p;
                if (pItems == p) pItems = pNew;
            }

            closedir(dir);
        }

        // If we're not a top-level directory, add a .. entry to the head of the list
        // This prevents non-DOS/Win32 machines stepping back up to the device list level
        if (strlen(m_pszPath) > 1)
            pItems = new CListViewItem(&sFolderIcon, "..", pItems);
    }

    // Give the item list to the list control
    SetItems(pItems);

    // Was there a previous selection?
    if (pszLabel)
    {
        // Look for it in the new list
        int nItem = FindItem(pszLabel);
        free(pszLabel);

        // If found, select it
        if (nItem != -1)
            Select(nItem);
    }
}

////////////////////////////////////////////////////////////////////////////////

CIconControl::CIconControl (CWindow* pParent_, int nX_, int nY_, const GUI_ICON* pIcon_, bool fSmall_)
    : CWindow(pParent_,nX_,nY_,0,0,ctImage), m_pIcon(pIcon_)
{
}

void CIconControl::Draw (CScreen* pScreen_)
{
    BYTE abGreyed[ICON_PALETTE_SIZE];

    // Is the control to be drawn disabled?
    if (!IsEnabled())
    {
        // Make a copy of the palette
        memcpy(abGreyed, m_pIcon->abPalette, sizeof(m_pIcon->abPalette));

        // Grey the icon by using shades of grey with approximate intensity
        for (int i = 0 ; i < ICON_PALETTE_SIZE ; i++)
        {
            // Only change non-zero colours, to keep the background transparent
            if (abGreyed[i])
                abGreyed[i] = GREY_1 + (abGreyed[i] & 0x07);
        }
    }

    // Draw the icon, using the greyed palette if disabled
    pScreen_->DrawImage(m_nX, m_nY, ICON_SIZE, ICON_SIZE, reinterpret_cast<const BYTE*>(m_pIcon->abData),
                        IsEnabled() ? m_pIcon->abPalette : abGreyed);
}

////////////////////////////////////////////////////////////////////////////////

CFrameControl::CFrameControl (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_, BYTE bColour_/*=WHITE*/, BYTE bFill_/*=0*/)
    : CWindow(pParent_, nX_, nY_, nWidth_, nHeight_, ctFrame), m_bColour(bColour_), m_bFill(bFill_)
{
}

void CFrameControl::Draw (CScreen* pScreen_)
{
    // If we've got a fill colour, use it now
    if (m_bFill)
        pScreen_->FillRect(m_nX, m_nY, m_nWidth, m_nHeight, m_bFill);

    // Draw the frame around the area
    pScreen_->FrameRect(m_nX, m_nY, m_nWidth, m_nHeight, m_bColour, true);
}

////////////////////////////////////////////////////////////////////////////////

const int TITLE_TEXT_COLOUR = WHITE;
const int TITLE_BACK_COLOUR = BLUE_3;
const int DIALOG_BACK_COLOUR = BLUE_2;
const int DIALOG_FRAME_COLOUR = GREY_7;

const int TITLE_HEIGHT = 4 + CHAR_HEIGHT + 5;

CWindow* CDialog::s_pActive;


CDialog::CDialog (CWindow* pParent_, int nWidth_, int nHeight_, const char* pcszCaption_, bool fModal_/*=true*/)
    : CWindow(pParent_, 0, 0, nWidth_, nHeight_, ctDialog), m_fModal(fModal_), m_fDragging(false), m_nDragX(0),
        m_nDragY(0), m_nTitleColour(TITLE_BACK_COLOUR), m_nBodyColour(DIALOG_BACK_COLOUR)
{
    SetText(pcszCaption_);

    Centre();
    CWindow::Activate();

    // Inherit dialog activation from the parent
    if (s_pActive == GetParent())
        s_pActive = this;
}

CDialog::~CDialog ()
{
    // Pass the dialog activation back to the parent window
    if (s_pActive == this)
        s_pActive = GetParent();
}

void CDialog::Centre ()
{
    // Position the window slightly above centre
    Move((Frame::GetWidth() - m_nWidth) >> 1, ((Frame::GetHeight() - m_nHeight) * 9/10) >> 1);
}

// Activating the dialog will activate the first child control that accepts focus
void CDialog::Activate ()
{
    // If there's no active window on the dialog, activate the tab-stop
    if (m_pChildren)
    {
        // Look for the first control with a tab-stop
        CWindow* p;
        for (p = m_pChildren ; p && !p->IsTabStop() ; p = p->GetNext());

        // If we found one, activate it
        if (p)
            p->Activate();
    }
}

bool CDialog::HitTest (int nX_, int nY_)
{
    // The caption is outside the original dimensions, so we need a special test
    return (nX_ >= m_nX-1) && (nX_ < (m_nX+m_nWidth+1)) && (nY_ >= m_nY-TITLE_HEIGHT) && (nY_ < (m_nY+1));
}

// Fill the dialog background
void CDialog::EraseBackground (CScreen* pScreen_)
{
    pScreen_->FillRect(m_nX, m_nY, m_nWidth, m_nHeight, IsActiveDialog() ? m_nBodyColour : (m_nBodyColour & ~0x7));
}

void CDialog::Draw (CScreen* pScreen_)
{
    // Make sure there's always an active control
    if (!m_pActive)
        Activate();

#if 0
    // Debug crosshairs to track mapped GUI cursor position
    pScreen_->DrawLine(GUI::s_nX, 0, 0, pScreen_->GetHeight(), WHITE);
    pScreen_->DrawLine(0, GUI::s_nY, pScreen_->GetPitch(), 0, WHITE);
#endif

    // Fill dialog background and draw a 3D frame
    EraseBackground(pScreen_);
    pScreen_->FrameRect(m_nX-2, m_nY-TITLE_HEIGHT-2, m_nWidth+3, m_nHeight+TITLE_HEIGHT+3, DIALOG_FRAME_COLOUR);
    pScreen_->FrameRect(m_nX-1, m_nY-TITLE_HEIGHT-1, m_nWidth+3, m_nHeight+TITLE_HEIGHT+3, DIALOG_FRAME_COLOUR-2);
    pScreen_->Plot(m_nX+m_nWidth+1, m_nY-TITLE_HEIGHT-2, DIALOG_FRAME_COLOUR);
    pScreen_->Plot(m_nX-2, m_nY+m_nHeight+1, DIALOG_FRAME_COLOUR-2);

    // Fill caption background and draw the diving line at the bottom of it
    pScreen_->FillRect(m_nX, m_nY-TITLE_HEIGHT, m_nWidth, TITLE_HEIGHT-1, IsActiveDialog() ? m_nTitleColour : (m_nTitleColour & ~0x7));
    pScreen_->DrawLine(m_nX, m_nY-1, m_nWidth, 0, DIALOG_FRAME_COLOUR);

    // Draw caption text in the centre
    pScreen_->DrawString(m_nX + (m_nWidth - CScreen::GetStringWidth(GetText(),true))/2,
                         m_nY-TITLE_HEIGHT+5, GetText(), TITLE_TEXT_COLOUR, true);

    // Call the base to draw any child controls
    CWindow::Draw(pScreen_);
}


bool CDialog::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    // Pass to the active control, then the base implementation for the remaining child controls
    if (CWindow::OnMessage(nMessage_, nParam1_, nParam2_))
        return true;

    switch (nMessage_)
    {
        case GM_CHAR:
        {
            switch (nParam1_)
            {
                // Tab or Shift-Tab moves between any controls that are tab-stops
                case '\t':
                {
                    // Loop until we find an enabled control to stop on
                    while (m_pActive)
                    {
                        m_pActive = nParam2_ ? m_pActive->GetPrev(true) : m_pActive->GetNext(true);

                        // Stop once we find a suitable control
                        if (m_pActive->IsTabStop() && m_pActive->IsEnabled())
                            break;
                    }

                    return true;
                }

                // Cursor left or right moves between controls of the same type
                case HK_LEFT:
                case HK_RIGHT:
                case HK_UP:
                case HK_DOWN:
                {
                    // Determine the next control 
                    bool fPrev = (nParam1_ == HK_LEFT) || (nParam1_ == HK_UP);

                    // Look for the next enabled/tabstop control of the same type
                    
                    for (CWindow* p = m_pActive ; p ; )
                    {
                        p = fPrev ? p->GetPrev(true) : p->GetNext(true);
                        
                        // Stop if we're found a different control type
                        if (p->GetType() != m_pActive->GetType())
                            break;

                        // If we've found a suitable control, activate it
                        else if (p->IsEnabled() && p->IsTabStop())
                        {
                            p->Activate();
                            break;
                        }
                    }

                    return true;
                }

                // Esc cancels the dialog
                case '\x1b':
                    Destroy();
                    break;
            }
            break;
        }

        case GM_BUTTONDOWN:
        case GM_BUTTONDBLCLK:
            // Button down on the caption?
            if (IsOver() && nParam2_ < (m_nY+TITLE_HEIGHT))
            {
                // Remember the offset from the window position to the drag location
                m_nDragX = nParam1_ - m_nX;
                m_nDragY = nParam2_ - m_nY;

                // Flag we're dragging and return the message as processed
                return m_fDragging = true;
            }
            break;

        case GM_BUTTONUP:
            // If this is the button up after finishing a drag, clear the flag
            if (m_fDragging)
            {
                m_fDragging = false;
                return true;
            }
            break;

        case GM_MOUSEMOVE:
            // If we're dragging, move the window to it's new position
            if (m_fDragging)
            {
                Move(nParam1_-m_nDragX, nParam2_-m_nDragY);
                return true;
            }
            break;
    }

    // If we're modal, absorb all messages to prevent any parent processing
    return m_fModal;
}

////////////////////////////////////////////////////////////////////////////////

const int MSGBOX_NORMAL_COLOUR = BLUE_2;
const int MSGBOX_ERROR_COLOUR = RED_2;
const int MSGBOX_BUTTON_SIZE = 50;
const int MSGBOX_LINE_HEIGHT = 15;
const int MSGBOX_GAP = 13;

CMessageBox::CMessageBox (CWindow* pParent_, const char* pcszBody_, const char* pcszCaption_, int nFlags_)
    : CDialog(pParent_, 0, 0, pcszCaption_, true), m_nLines(0), m_pIcon(NULL)
{
    // We need to be recognisably different from a regular dialog, despite being based on one
    m_nType = ctMessageBox;

    // Break the body text into lines
    for (char *psz = m_pszBody = strdup(pcszBody_), *pszEOL = psz ; pszEOL && *psz ; psz += strlen(psz)+1, m_nLines++)
    {
        if ((pszEOL = strchr(psz, '\n')))
            *pszEOL = '\0';

        // Keep track of the maximum line width
        int nLen = CScreen::GetStringWidth(psz);
        if (nLen > m_nWidth)
            m_nWidth = nLen;
    }

    // Calculate the text area height
    m_nHeight = (MSGBOX_LINE_HEIGHT * m_nLines);

    // Work out the icon to use, if any
    const GUI_ICON* apIcons[] = { NULL, &sInformationIcon, &sWarningIcon, &sErrorIcon };
    const GUI_ICON* pIcon = apIcons[(nFlags_ & 0x30) >> 4];

    // Calculate the text area height, and increase to allow space for the icon if necessary
    if (pIcon)
    {
        // Update the body width and height to allow for the icon
        m_nWidth += ICON_SIZE + MSGBOX_GAP/2;

        // Add the icon to the top-left of the dialog
        m_pIcon = new CIconControl(this, MSGBOX_GAP/2, MSGBOX_GAP/2, pIcon);
    }

    // Work out the width of the button block, which depends on how many buttons are needed
    int nButtons = 1;
    int nButtonWidth = ((MSGBOX_BUTTON_SIZE+MSGBOX_GAP) * nButtons) - MSGBOX_GAP;

    // Allow for a surrounding border
    m_nWidth  += MSGBOX_GAP << 1;
    m_nHeight += MSGBOX_GAP << 1;

    // Centre the button block at the bottom of the dialog [ToDo: add remaining buttons]
    int nButtonOffset = (m_nWidth-nButtonWidth) >> 1;
    (new CTextButton(this, nButtonOffset, m_nHeight, "OK", MSGBOX_BUTTON_SIZE))->Activate();

    // Allow for the button height and a small gap underneath it
    m_nHeight += BUTTON_HEIGHT + MSGBOX_GAP/2;

    // Centralise the dialog on the screen by default
    int nX = (Frame::GetWidth() - m_nWidth) >> 1, nY = (Frame::GetHeight() - m_nHeight)*2/5;
    Move(nX, nY);

    // Error boxes are shown in red
    if (pIcon == &sInformationIcon)
        CDialog::SetColours(MSGBOX_NORMAL_COLOUR+2, MSGBOX_NORMAL_COLOUR);
    else
        CDialog::SetColours(MSGBOX_ERROR_COLOUR+1, MSGBOX_ERROR_COLOUR);
}

void CMessageBox::Draw (CScreen* pScreen_)
{
    CDialog::Draw(pScreen_);

    // Calculate the x-offset to the body text
    int nX = m_nX + MSGBOX_GAP + (m_pIcon ? ICON_SIZE + MSGBOX_GAP/2 : 0);

    // Draw each line in the body text
    const char* psz = m_pszBody;
    for (int i = 0 ; i < m_nLines ; psz += strlen(psz)+1, i++)
        pScreen_->DrawString(nX, m_nY+MSGBOX_GAP+(MSGBOX_LINE_HEIGHT*i), psz, WHITE);
}
