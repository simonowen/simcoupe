// Part of SimCoupe - A SAM Coupé emulator
//
// GUI.cpp: GUI and controls for on-screen interface
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

//  ToDo:
//   - finish CFileView

#include "SimCoupe.h"
#include "GUI.h"

#include "Font.h"
#include "Frame.h"
#include "Input.h"
#include "Video.h"


// Array of SAM colours used by the GUI - allocated separately to support dimming the main palette
static const BYTE abGuiPalette [N_GUI_COLOURS] =
{
     1, 9, 16, 24, 17, 25, 113, 121,        // Blues
     2, 10, 32, 40, 34, 42, 114, 122,       // Reds
     3, 11, 48, 56, 51, 59, 115, 123,       // Magentas
     4, 12, 64, 72, 68, 76, 116, 124,       // Greens
     5, 13, 80, 88, 85, 93, 117, 125,       // Cyans
     6, 14, 96, 104, 102, 110, 118, 126,    // Yellows
     0, 8, 7, 15, 112, 120, 119, 127,       // Greys, from black to white
     0, 0                                   // Custom colours, defined in GetPalette() below
};

CWindow* GUI::s_pGUI;
int GUI::s_nX, GUI::s_nY;
int GUI::s_nUsage = 0;
bool GUI::s_fModal;


bool GUI::SendMessage (int nMessage_, int nParam1_/*=0*/, int nParam2_/*=0*/)
{
    // We're not interested in messages when we're inactive
    if (!IsActive())
        return false;

    // Increment the usage counter to prevent the GUI being deleted during message processing
    s_nUsage++;

    // Keep track of the mouse
    if (nMessage_ == GM_MOUSEMOVE)
    {
        s_nX = nParam1_;
        s_nY = nParam2_;
    }

    // Pass the message to the active GUI component
    s_pGUI->OnMessage(nMessage_, nParam1_, nParam2_);

    // Send a move after a button up, to give a hit test after an effective mouse capture
    if (nMessage_ == GM_BUTTONUP)
        s_pGUI->OnMessage(GM_MOUSEMOVE, nParam1_, nParam2_);

    // Stop the GUI if it was deleted during the last message
    if (!--s_nUsage)
        Stop();

    return true;
}


void GUI::Start (CWindow* pGUI_, bool fModal_/*=true*/)
{
    // Delete any previous GUI (this shouldn't ever happen though)
    if (s_pGUI)
        delete s_pGUI;

    s_pGUI = pGUI_;
    s_fModal = fModal_;
    s_nUsage = 1;

    // Dim the background SAM screen
    Video::CreatePalettes();

    // Steal all keyboard and mouse input
    Input::Acquire(false, false);
}

void GUI::Stop ()
{
    // We can't delete the GUI if it's still in use, so wait until any active call returns
    if (--s_nUsage > 0)
        return;

    delete s_pGUI;
    s_pGUI = NULL;

    // Restore the normal SAM palette
    Video::CreatePalettes();
    Frame::Clear();

    // Give keyboard input back to the emulation
    Input::Acquire(false, true);
}


void GUI::Draw (CScreen* pScreen_)
{
    if (s_pGUI)
    {
        CScreen::SetFont(&sNewFont);
        s_pGUI->Draw(pScreen_);
    }
}

const RGBA* GUI::GetPalette ()
{
    static RGBA asCustom[] = { {77,97,133,255}, { 202,217,253,255} };   // A couple of extra custom colours
    static RGBA asPalette[N_GUI_COLOURS];
    static bool fPrepared = false;

    // If we've already got what's needed, return the current setup
    if (fPrepared)
        return asPalette;

    // Most GUI palette colours are based on SAM colours, so get the original list
    const RGBA* pSAM = IO::GetPalette();

    for (int i = 0; i < N_GUI_COLOURS ; i++)
    {
        if (i >= (CUSTOM_1-N_PALETTE_COLOURS))
            asPalette[i] = asCustom[i-(CUSTOM_1-N_PALETTE_COLOURS)];
        else
            asPalette[i] = pSAM[abGuiPalette[i]];
    }

    // Flag that we've prepared it, and return the freshly generated palette
    fPrepared = true;
    return asPalette;
}

////////////////////////////////////////////////////////////////////////////////

CWindow::CWindow (CWindow* pParent_/*=NULL*/, int nX_/*=0*/, int nY_/*=0*/, int nWidth_/*=0*/, int nHeight_/*=0*/, int nType_/*=ctUnknown*/)
    : m_nX(nX_), m_nY(nY_), m_nWidth(nWidth_), m_nHeight(nHeight_), m_nType(nType_), m_pcszText(NULL), m_fEnabled(true), m_fHover(false),
        m_pParent(pParent_), m_pChildren(NULL), m_pNext(NULL), m_pActive(NULL)
{
    // If we've a parent, adjust our position to be relative to it
    if (m_pParent)
    {
        m_nX += m_pParent->m_nX;
        m_nY += m_pParent->m_nY;

        m_pParent->AddChild(this);
    }
}

CWindow::~CWindow ()
{
    delete m_pcszText;
    Destroy();
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
            // Hit test the control, and auto-activate it if we're over it
            if ((nMessage_ & GM_MOUSE_MESSAGE) && (pChild->m_fHover = pChild->HitTest(nParam1_, nParam2_)))
                pChild->Activate();

            fProcessed = pChild->OnMessage(nMessage_, nParam1_, nParam2_);
        }
    }

    // Hit test the current window
    m_fHover = HitTest(nParam1_, nParam2_);

    // Return whether the message was processed
    return fProcessed;
}


void CWindow::AddChild (CWindow* pChild_)
{
    if (!m_pChildren)
        m_pChildren = pChild_;
    else
    {
        for (CWindow* p = m_pChildren ; p != pChild_; p = p->GetNext())
        {
            if (!p->m_pNext)
                p->m_pNext = pChild_;
        }
    }
}

void CWindow::Destroy ()
{
    // Unlink from the parent, if any
    if (m_pParent)
    {
        CWindow* pPrev = GetPrev();

        if (!pPrev)
            m_pParent->m_pChildren = GetNext();
        else
            pPrev->m_pNext = GetNext();

        if (m_pParent->m_pActive == this)
            m_pParent->m_pActive = NULL;
    }

    // Delete any child controls we have
    while (m_pChildren)
    {
        CWindow* pDelete = m_pChildren;
        m_pChildren = m_pChildren->m_pNext;
        delete pDelete;
    }

    GUI::SendMessage(GM_MOUSEMOVE, GUI::s_nX, GUI::s_nY);
}


void CWindow::Activate ()
{
    if (m_pParent)
        m_pParent->m_pActive = this;
}


void CWindow::SetText (const char* pcszText_)
{
    if (!pcszText_)
        pcszText_ = "";

    // Delete any old string and make a copy of the new one (take care in case the new string is the old one)
    char* pcszOld = m_pcszText;
    strcpy(m_pcszText = new char[strlen(pcszText_)+1], pcszText_);
    delete pcszOld;
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

////////////////////////////////////////////////////////////////////////////////

CTextControl::CTextControl (CWindow* pParent_/*=NULL*/, int nX_/*=0*/, int nY_/*=0*/, const char* pcszText_/*=""*/, BYTE bColour_/*=WHITE*/)
    : CWindow(pParent_, nX_, nY_, 0, 0, ctText), m_bColour(bColour_)
{
    SetText(pcszText_);
    m_nWidth = GetTextWidth();
}

void CTextControl::Draw (CScreen* pScreen_)
{
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
    pScreen_->FillRect(m_nX+1, m_nY+1, m_nWidth-2, m_nHeight-2, IsActive() ? YELLOW_8 : CUSTOM_2);

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
                    GetParent()->OnCommand(this);
                    return true;
            }
            break;

        case GM_BUTTONDOWN:
            // If the click was over us, flag the button as pressed
            if (IsOver())
                return m_fPressed = true;
            break;

        case GM_BUTTONUP:
            // If we were depressed and the mouse has been released over us, register a button press
            if (IsOver() && m_fPressed)
                GetParent()->OnCommand(this);

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

CUpButton::CUpButton (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
    : CButton(pParent_, nX_, nY_, nWidth_, nHeight_)
{
}

void CUpButton::Draw (CScreen* pScreen_)
{
    CButton::Draw(pScreen_);

    bool fPressed = m_fPressed && IsOver();

    int nX = m_nX + 2 + fPressed, nY = m_nY + 3 + fPressed;
    BYTE bColour = GetParent()->IsEnabled() ? CUSTOM_1 : GREY_5;
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
    BYTE bColour = GetParent()->IsEnabled() ? CUSTOM_1 : GREY_5;
    pScreen_->DrawLine(nX+5, nY+5,   1, 0, bColour);
    pScreen_->DrawLine(nX+4, nY+4, 3, 0, bColour);
    pScreen_->DrawLine(nX+3, nY+3, 2, 0, bColour);  pScreen_->DrawLine(nX+6, nY+3, 2, 0, bColour);
    pScreen_->DrawLine(nX+2, nY+2, 2, 0, bColour);  pScreen_->DrawLine(nX+7, nY+2, 2, 0, bColour);
    pScreen_->DrawLine(nX+1, nY+1, 2, 0, bColour);  pScreen_->DrawLine(nX+8, nY+1, 2, 0, bColour);
}

////////////////////////////////////////////////////////////////////////////////

const int PRETEXT_GAP = 5;
const int BOX_SIZE = 11;

CCheckBox::CCheckBox (CWindow* pParent_/*=NULL*/, int nX_/*=0*/, int nY_/*=0*/, const char* pcszText_/*=""*/)
    : CWindow(pParent_, nX_, nY_, 0, BOX_SIZE, ctCheckBox), m_fChecked(false)
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
    // Draw the empty check box
    pScreen_->FrameRect(m_nX, m_nY, BOX_SIZE, BOX_SIZE, !IsEnabled() ? GREY_5 : IsActive() ? YELLOW_8 : CUSTOM_2);

    BYTE abEnabled[] = { 0, CUSTOM_2 }, abDisabled[] = { 0, GREY_5 };

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

    // Draw the text to the right of the box, grey if the control is disabled
    int nX = m_nX + BOX_SIZE + PRETEXT_GAP;
    int nY = m_nY + (BOX_SIZE-CHAR_HEIGHT)/2 + 1;
    pScreen_->DrawString(nX, nY, GetText(), IsEnabled() ? (IsActive() ? YELLOW_8 : GREY_7) : GREY_5);
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
                    GetParent()->OnCommand(this);
                    return true;
            }
            break;
        }
            
        case GM_BUTTONDOWN:
            // Was the click over us?
            if (IsOver())
            {
                SetChecked(!IsChecked());
                GetParent()->OnCommand(this);
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

const int MAX_EDIT_LENGTH = 256;

CEditControl::CEditControl (CWindow* pParent_, int nX_, int nY_, int nWidth_, const char* pcszText_/*=""*/)
    : CWindow(pParent_, nX_, nY_, nWidth_, CHAR_HEIGHT+5, ctEdit)
{
    // Allocate the fixed maximum size, and copy the initial text into it
    strcpy(m_pcszText = new char[MAX_EDIT_LENGTH+1], pcszText_);
}

void CEditControl::Draw (CScreen* pScreen_)
{
    // Fill overall control background, and draw a frame round it
    pScreen_->FillRect(m_nX+1, m_nY+1, m_nWidth-2, m_nHeight-2, IsEnabled() ? (IsActive() ? YELLOW_8 : WHITE) : GREY_7);
    pScreen_->FrameRect(m_nX, m_nY, m_nWidth, m_nHeight, CUSTOM_2);

    // Draw a light edge highlight for the bottom and right
    pScreen_->DrawLine(m_nX+1, m_nY+m_nHeight-1, m_nWidth-1, 0, GREY_7);
    pScreen_->DrawLine(m_nX+m_nWidth-1, m_nY+1, 0, m_nHeight-1, GREY_7);

    // The text could be too long for the control, so find the longest tail-segment that fits
    const char* pcsz = GetText();
    while (CScreen::GetStringWidth(pcsz) >= (m_nWidth - 2))
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
                // Backspace deletes the last character
                case '\b':
                    if (*m_pcszText)
                    {
                        m_pcszText[strlen(m_pcszText)-1] = '\0';
                        GetParent()->OnCommand(this);
                    }
                    return true;

                default:
                    // Only accept printable characters
                    if (nParam1_ >= ' ' && nParam1_ <= 0x7f)
                    {
                        // Only add a character if we're not at the maximum length yet
                        size_t nLen = strlen(GetText());
                        if (nLen < MAX_EDIT_LENGTH)
                        {
                            m_pcszText[nLen] = nParam1_;
                            m_pcszText[nLen+1] = '\0';
                            GetParent()->OnCommand(this);
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

    BYTE abActive[] = { 0, GREY_5, CUSTOM_2, YELLOW_8 };
    BYTE abEnabled[] = { 0, GREY_5, CUSTOM_2, CUSTOM_2 };
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
                case GK_UP:
                {
                    CWindow* pPrev = GetPrev();
                    if (pPrev && pPrev->GetType() == GetType())
                    {
                        pPrev->Activate();
                        reinterpret_cast<CRadioButton*>(pPrev)->Select();
                        GetParent()->OnCommand(this);
                    }
                    return true;
                }

                case GK_DOWN:
                {
                    CWindow* pNext = GetNext();
                    if (pNext && pNext->GetType() == GetType())
                    {
                        pNext->Activate();
                        reinterpret_cast<CRadioButton*>(pNext)->Select();
                        GetParent()->OnCommand(this);
                    }
                    return true;
                }
            }
            break;
        }

        case GM_BUTTONDOWN:
            // Was the click over us?
            if (IsOver())
            {
                Select();
                GetParent()->OnCommand(this);
                return fPressed = true;
            }
            break;

        case GM_BUTTONUP:
            if (fPressed)
                return !(fPressed = false);
            break;
    }

    return fPressed;
}

void CRadioButton::Select (bool fSelected_/*=true*/)
{
    // Remember the new status, and if it's a selection we have more work to do...
    if (m_fSelected = fSelected_)
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
    pScreen_->FrameRect(m_nX-1, m_nY-1, m_nWidth+2, m_nHeight+2, CUSTOM_2);

    // Make a copy of the menu item list as strtok() munges as it iterates
    char sz[256], *psz = strtok(strcpy(sz, GetText()), MENU_DELIMITERS);

    // Loop through the items on the menu
    for (int i = 0 ; psz ; psz = strtok(NULL, MENU_DELIMITERS), i++)
    {
        int nX = m_nX, nY = m_nY + (MENU_ITEM_HEIGHT * i);

        // Draw the string over the default background if not selected
        if (i != m_nSelected)
            pScreen_->DrawString(nX+MENU_TEXT_GAP, nY+1, psz, BLACK);

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
                    GetParent()->OnCommand(this);
                    delete this;
                    return true;

                // Esc cancels
                case '\x1b':
                    m_nSelected = -1;
                    GetParent()->OnCommand(this);
                    delete this;
                    return true;

                // Move to the previous selection, wrapping to the bottom if necessary
                case GK_UP:
                    Select((m_nSelected-1 + m_nItems) % m_nItems);
                    break;

                // Move to the next selection, wrapping to the top if necessary
                case GK_DOWN:
                    Select((m_nSelected+1) % m_nItems);
                    break;
            }
            return true;

        case GM_BUTTONDOWN:
            // Button clicked (held?) on the menu, so remember it's happened
            if (IsOver())
                return m_fPressed = true;

            // Button clicked away from the menu, which cancels it
            m_nSelected = -1;
            GetParent()->OnCommand(this);
            delete this;
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
            GetParent()->OnCommand(this);
            delete this;
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
    : CWindow(pParent_, nX_, nY_, nWidth_, COMBO_HEIGHT, ctButton), m_fPressed(false), m_pDropList(NULL), m_nItems(1)
{
    SetText(pcszText_);
    Select(0);

    for (const char* pcsz = GetText() ; pcsz=strchr(pcsz,'|') ; pcsz++, m_nItems++);
}

void CComboBox::Select (int nSelected_)
{
    m_nSelected = (nSelected_ < 0) ? 0 : (nSelected_ >= m_nItems) ? m_nItems-1 : nSelected_;
}

void CComboBox::Draw (CScreen* pScreen_)
{
    bool fPressed = m_fPressed;

    // Fill the main control background
    pScreen_->FrameRect(m_nX, m_nY, m_nWidth, m_nHeight, CUSTOM_2);
    pScreen_->FillRect(m_nX+1, m_nY+1, m_nWidth-COMBO_HEIGHT-1, m_nHeight-2,
                        !IsEnabled() ? GREY_7 : (IsActive() && !fPressed) ? YELLOW_8 : WHITE);

    // Fill the main button background
    int nX = m_nX + m_nWidth - COMBO_HEIGHT, nY = m_nY + 1;
    pScreen_->FillRect(nX+1, nY+1, COMBO_HEIGHT-1, m_nHeight-3, CUSTOM_2);

    // Draw the edge highlight for the top, left, right and bottom
    pScreen_->DrawLine(nX, nY, COMBO_HEIGHT, 0, fPressed ? GREY_5 : WHITE);
    pScreen_->DrawLine(nX, nY, 0, m_nHeight-2, fPressed ? GREY_5 : WHITE);
    pScreen_->DrawLine(nX+1, nY+m_nHeight-2, COMBO_HEIGHT-2, 0, fPressed ? WHITE : GREY_5);
    pScreen_->DrawLine(nX+COMBO_HEIGHT-1, nY+1, 0, m_nHeight-2, fPressed ? WHITE : GREY_5);

    // Show the arrow button, down a pixel if it's pressed
    nY += fPressed;
    BYTE bColour = IsEnabled() ? CUSTOM_1 : GREY_5;
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
                    if (m_fPressed = !m_fPressed)
                        (m_pDropList = new CDropList(this, 1, COMBO_HEIGHT, GetText(), m_nWidth-2))->Select(m_nSelected);
                    return true;

                case GK_UP:
                    Select(m_nSelected-1);
                    return true;

                case GK_DOWN:
                    Select(m_nSelected+1);
                    return true;
            }
            break;

        case GM_BUTTONDOWN:
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

void CComboBox::OnCommand (CWindow* pWindow_)
{
    if (pWindow_ == m_pDropList)
    {
        int nSelected = ((CDropList*)pWindow_)->GetSelected();
        if (nSelected != -1)
            m_nSelected = nSelected;

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

    // The scrollable area includes only what is not included in the current view
    if ((m_nMaxPos = nMaxPos_ - m_nHeight) > 0)
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
    pScreen_->FillRect(nX, nY, m_nWidth, m_nThumbSize, !IsEnabled() ? GREY_7 : CUSTOM_2);

    pScreen_->DrawLine(nX, nY, m_nWidth, 0, WHITE);
    pScreen_->DrawLine(nX, nY, 0, m_nThumbSize, WHITE);
    pScreen_->DrawLine(nX+1, nY+m_nThumbSize-1, m_nWidth-1, 0, GREY_5);
    pScreen_->DrawLine(nX+m_nWidth-1, nY+1, 0, m_nThumbSize-1, GREY_5);

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
                case GK_UP:     SetPos(m_nPos - m_nStep);   break;
                case GK_DOWN:   SetPos(m_nPos + m_nStep);   break;

                default:        return false;
            }
            
            return true;

        case GM_BUTTONDOWN:
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
                return !(fDragging = false);
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

void CScrollBar::OnCommand (CWindow* pWindow_)
{
    if (pWindow_ == m_pUp)
        SetPos(m_nPos - m_nStep);
    else if (pWindow_ == m_pDown)
        SetPos(m_nPos + m_nStep);
}

////////////////////////////////////////////////////////////////////////////////

const int ITEM_SIZE = 72;

CListView::CListView (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
    : CWindow(pParent_, nX_, nY_, nWidth_, nHeight_), m_nAcross(0), m_nDown(0)
{
    // Create a scrollbar to cover the overall height, scrolling if necessary
    m_pScrollBar = new CScrollBar(this, m_nWidth-SCROLLBAR_WIDTH, 0, m_nHeight, 0, ITEM_SIZE);

    SetItems();
}

void CListView::Select (int nItem_)
{
    m_nSelected = (nItem_ < 0) ? 0 : (nItem_ >= m_nItems) ? m_nItems-1 : nItem_;

    // Calculate the row containing the new item, and the vertical offset in the list overall
    int nRow = m_nSelected / m_nAcross, nOffset = nRow*ITEM_SIZE - m_pScrollBar->GetPos();

    // If the new item is not completely visible, scroll the list so it _just_ is
    if (nOffset < 0 || nOffset >= (m_nHeight-ITEM_SIZE))
        m_pScrollBar->SetPos(nRow*ITEM_SIZE - ((nOffset < 0) ? 0 : (m_nHeight-ITEM_SIZE)));
}

void CListView::SetItems (int nItems_/*=0*/)
{
    m_nItems = nItems_;

    // Calculate how many items can fit on a row, and how many rows are needed
    m_nAcross = m_nWidth/ITEM_SIZE;
    m_nDown = (m_nItems+m_nAcross-1) / m_nAcross;

    m_pScrollBar->SetMaxPos(m_nDown*ITEM_SIZE);
}

void CListView::DrawItem (CScreen* pScreen_, int nItem_, int nX_, int nY_, const GUI_ICON* pIcon_/*=NULL*/,
                            const char* pcsz_/*=NULL*/)
{
    if (nItem_ == m_nSelected)
        pScreen_->FrameRect(nX_, nY_, ITEM_SIZE, ITEM_SIZE, YELLOW_6);

    if (pIcon_)
        pScreen_->DrawImage(nX_+(ITEM_SIZE-32)/2, nY_+5, 32, 32, reinterpret_cast<const BYTE*>(pIcon_->abData), pIcon_->abPalette);

    if (pcsz_)
    {
        int nW = ITEM_SIZE - 6, nLine = 0;
        const char *pszStart = pcsz_, *pszSpace = NULL;
        char szLines[2][64] = { 0 }, sz[64];

        while (*pcsz_ && nLine < 2)
        {
            strncpy(sz, pszStart, pcsz_-pszStart)[pcsz_-pszStart] = '\0';
            int len = CScreen::GetStringWidth(sz);

            if (len > nW)
            {
                if (nLine == 1 || !pszSpace)
                {
                    if (nLine == 1)
                        strcpy(&sz[pcsz_-pszStart-2], "...");
                    strcpy(szLines[nLine++], sz);
                    pszStart = pcsz_;
                }
                else
                {
                    if (nLine == 1)
                        strcpy(&sz[pszSpace-pszStart-2], "...");
                    else
                        sz[pszSpace-pszStart] = '\0';
                    strcpy(szLines[nLine++], sz);
                    pszStart = pszSpace+1;
                    pszSpace = NULL;
                }
            }

            if (*pcsz_ == ' ')
                pszSpace = pcsz_;

            if (!*++pcsz_)
                strcpy(szLines[nLine++], pszStart);
        }

        pScreen_->DrawString(nX_+32-(pScreen_->GetStringWidth(szLines[0])/2), nY_+39, szLines[0], WHITE);
        pScreen_->DrawString(nX_+32-(pScreen_->GetStringWidth(szLines[1])/2), nY_+39+CHAR_HEIGHT+2, szLines[1], WHITE);
    }
}

void CListView::Draw (CScreen* pScreen_)
{
    pScreen_->FillRect(m_nX, m_nY, m_nWidth, m_nHeight, BLUE_1);

    m_nDown = (m_nItems+m_nAcross-1) / m_nAcross;

    int nScrollPos = m_pScrollBar->GetPos();

    int nStart = nScrollPos/ITEM_SIZE * m_nAcross, nOffset = nScrollPos % ITEM_SIZE;
    int nDepth = (m_nHeight + nOffset + ITEM_SIZE-1) / ITEM_SIZE;
    int nEnd = min(m_nItems, nStart + m_nAcross*nDepth);


    pScreen_->SetClip(m_nX, m_nY, m_nWidth, m_nHeight);

    for (int i = nStart ; i < nEnd ; i++)
    {
        int x = m_nX + ((i % m_nAcross) * ITEM_SIZE), y = m_nY + (((i-nStart) / m_nAcross) * ITEM_SIZE) - nOffset;
        DrawItem(pScreen_, i, x, y);
    }

    pScreen_->SetClip();

    CWindow::Draw(pScreen_);
}

bool CListView::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
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
                case GK_LEFT:   Select(m_nSelected-1);  break;
                case GK_RIGHT:  Select(m_nSelected+1);  break;

                case GK_UP:
                    // Only move up if we're not already on the top line
                    if (m_nSelected >= m_nAcross)
                        Select(m_nSelected-m_nAcross);
                    break;

                case GK_DOWN:
                {
                    // Calculate the row the new item would be on
                    int nNewRow = min(m_nSelected+m_nAcross,m_nItems-1) / m_nAcross;

                    // Only move down if we're not already on the bottom row
                    if (nNewRow != m_nSelected/m_nAcross)
                        Select(m_nSelected+m_nAcross);
                    break;
                }

                default:
                    return false;
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

        case GM_MOUSEWHEEL:
            m_nHoverItem = -1;
            return false;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

CFileView::CFileView (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
    : CListView(pParent_, nX_, nY_, nWidth_, nHeight_), m_pFiles(NULL)
{
    struct dirent* entry;

#define PPP   "c:\\"

    DIR* dir = opendir(PPP);
    if (dir)
    {
        int nItems = 0;

        while (entry = readdir(dir))
        {
            FileEntry* pEntry = new FileEntry;
            pEntry->psz = strdup(entry->d_name);

            char sz[MAX_PATH];
            struct stat st;
            stat(strcat(strcpy(sz, PPP), pEntry->psz), &st);
            pEntry->nType = (st.st_mode & _S_IFDIR) ? ftDir : ftFile;

            pEntry->pNext = m_pFiles;
            m_pFiles = pEntry;
                
            nItems++;
        }

        closedir(dir);

        SetItems(nItems);
    }
}

void CFileView::DrawItem (CScreen* pScreen_, int nItem_, int nX_, int nY_, const GUI_ICON* pIcon_, const char* pcsz_)
{
    FileEntry* pFile = m_pFiles;
    for (int i = 0 ; i < nItem_ ; i++)
        pFile = pFile->pNext;

    const GUI_ICON* pIcon = (pFile->nType) == ftDir ? &sFolderIcon : &sDiskIcon;

    CListView::DrawItem(pScreen_, nItem_, nX_, nY_, pIcon, pFile->psz);

    char* aszE[] = { "DSK", "SAD", "SDF", "DSK", "DSK", "DSK", "SDF", "DSK", "SAD" };
    if (nItem_ >= 2)
        pScreen_->DrawString(nX_+(ITEM_SIZE/2)-(pScreen_->GetStringWidth(aszE[nItem_%9])/2), nY_+22, aszE[nItem_%9], BLACK);
}

////////////////////////////////////////////////////////////////////////////////

CIconControl::CIconControl (CWindow* pParent_, int nX_, int nY_, const GUI_ICON& rsIcon_, bool fSmall_)
    : CWindow(pParent_,nX_,nY_,0,0,ctImage), m_sIcon(rsIcon_)
{
}

void CIconControl::Draw (CScreen* pScreen_)
{
    BYTE abGreyed[ICON_PALETTE_SIZE];

    // Is the control to be drawn disabled?
    if (!IsEnabled())
    {
        // Make a copy of the palette
        memcpy(abGreyed, m_sIcon.abPalette, sizeof m_sIcon.abPalette);

        // Grey the icon by using shades of grey with approximate intensity
        for (int i = 0 ; i < ICON_PALETTE_SIZE ; i++)
            abGreyed[i] = GREY_1 + (abGreyed[i] & 0x07);
    }

    // Draw the icon, using the greyed palette if disabled
    pScreen_->DrawImage(m_nX, m_nY, ICON_SIZE, ICON_SIZE, reinterpret_cast<BYTE*>(m_sIcon.abData),
                        IsEnabled() ? m_sIcon.abPalette : abGreyed);
}

////////////////////////////////////////////////////////////////////////////////

const int TITLE_TEXT_COLOUR = WHITE;
const int TITLE_BACK_COLOUR = BLUE_3;
const int DIALOG_BACK_COLOUR = BLUE_2;
const int DIALOG_FRAME_COLOUR = GREY_6;

const int TITLE_HEIGHT = 4 + CHAR_HEIGHT + 5;


CDialog::CDialog (int nWidth_/*=0*/, int nHeight_/*=0*/, const char* pcszCaption_/*=""*/)
    : CWindow(NULL, 0, 0, nWidth_, nHeight_, ctDialog)
{
    SetText(pcszCaption_);

    // Centralise the dialog on the screen by default (well, slightly above centre)
    int nW = Frame::GetWidth(), nH = Frame::GetHeight();
    m_nX = (nW - m_nWidth)/2;
    m_nY = (nH - m_nHeight)*3/8;
}

bool CDialog::HitTest (int nX_, int nY_)
{
    return (nX_ >= m_nX-1) && (nX_ < (m_nX+m_nWidth+1)) && (nY_ >= m_nY-TITLE_HEIGHT) && (nY_ < (m_nY+m_nHeight+1));
}

void CDialog::Draw (CScreen* pScreen_)
{
#if 0
    // Debug crosshairs to track mapped GUI cursor position
    pScreen_->DrawLine(GUI::s_nX, 0, 0, pScreen_->GetHeight(), WHITE);
    pScreen_->DrawLine(0, GUI::s_nY, pScreen_->GetPitch(), 0, WHITE);
#endif

    // Fill dialog background and draw frame
    pScreen_->FillRect(m_nX, m_nY, m_nWidth, m_nHeight, DIALOG_BACK_COLOUR);
    pScreen_->FrameRect(m_nX-1, m_nY-1, m_nWidth+2, m_nHeight+2, DIALOG_FRAME_COLOUR);

    // Fill caption background and draw frame
    pScreen_->FillRect(m_nX, m_nY-TITLE_HEIGHT, m_nWidth, TITLE_HEIGHT, TITLE_BACK_COLOUR);
    pScreen_->FrameRect(m_nX-1, m_nY-TITLE_HEIGHT, m_nWidth+2, TITLE_HEIGHT, DIALOG_FRAME_COLOUR);

    // Draw caption text in the centre
    pScreen_->DrawString(m_nX + (m_nWidth - GetTextWidth())/2, m_nY-TITLE_HEIGHT+5, GetText(), TITLE_TEXT_COLOUR, true);

    // Call the base to draw any child controls
    CWindow::Draw(pScreen_);
}

bool CDialog::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    // These statics are safe as only one dialog can be dragged at once
    static bool fDragging = false;
    static int nStartX, nStartY;

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
                    do {
                        m_pActive = !m_pActive ? GetChildren() : nParam2_ ? m_pActive->GetPrev(true) : m_pActive->GetNext(true);
                    }
                    while (!m_pActive->IsTabStop() || !m_pActive->IsEnabled());

                    // Activate the control to show it has the focus
                    m_pActive->Activate();
                    return true;
                }

                // Cursor left or right moves between controls of the same type
                case GK_LEFT:
                case GK_RIGHT:
                {
                    CWindow* pActive = !m_pActive ? GetChildren() :
                        (nParam1_ == GK_LEFT) ? m_pActive->GetPrev(true) : m_pActive->GetNext(true);

                    // Activate the next/previous control if it's the same type
                    if (pActive && pActive->GetType() == GetType())
                        (m_pActive = pActive)->Activate();
                    return true;
                }

                // Esc
                case '\x1b':
                    GUI::Stop();
                    break;
            }
            break;
        }

        case GM_BUTTONDOWN:
            // Button down on the caption?
            if (IsOver() && nParam2_ < (m_nY+TITLE_HEIGHT))
            {
                // Remember the offset from the window position to the drag location
                nStartX = nParam1_ - m_nX;
                nStartY = nParam2_ - m_nY;

                // Flag we're dragging and return the message as processed
                return fDragging = true;
            }
            break;

        case GM_BUTTONUP:
            // If this is the button up after finishing a drag, clear the flag
            if (fDragging)
                return !(fDragging = false);
            break;

        case GM_MOUSEMOVE:
            // If we're dragging, move the window to it's new position
            if (fDragging)
            {
                Move(nParam1_-nStartX, nParam2_-nStartY);
                return true;
            }
            break;
    }

    return false;
}
