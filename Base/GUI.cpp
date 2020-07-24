// Part of SimCoupe - A SAM Coupe emulator
//
// GUI.cpp: GUI and controls for on-screen interface
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

//  ToDo:
//   - FileView derived class needed to supply file icons
//   - button repeat on scrollbar
//   - add extra message box buttons (yes/no/cancel, etc.)
//   - regular list box?
//   - use icon for button arrows?
//   - edit box cursor positioning

#include "SimCoupe.h"
#include "GUI.h"

#include <ctype.h>

#include "Expr.h"
#include "Font.h"
#include "Frame.h"
#include "Input.h"
#include "Keyboard.h"
#include "Sound.h"
#include "UI.h"
#include "Video.h"

Window* GUI::s_pGUI;
int GUI::s_nX, GUI::s_nY;

static uint32_t dwLastClick = 0;   // Time of last double-click

std::queue<Window*> GUI::s_garbageQueue;
std::stack<Window*> GUI::s_dialogStack;

bool GUI::SendMessage(int nMessage_, int nParam1_/*=0*/, int nParam2_/*=0*/)
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
        auto dwNow = OSD::GetTime();
        int nMovedSquared = (nLastX - nParam1_) * (nLastX - nParam1_) + (nLastY - nParam2_) * (nLastY - nParam2_);

        // If the click is close enough to the last click (in space and time), convert it to a double-click
        if (!fDouble && nMovedSquared < (DOUBLE_CLICK_THRESHOLD * DOUBLE_CLICK_THRESHOLD) &&
            dwNow != dwLastClick && dwNow - dwLastClick < DOUBLE_CLICK_TIME)
            nMessage_ = GM_BUTTONDBLCLK;

        // Remember the last time and position of the click
        dwLastClick = dwNow;
        nLastX = nParam1_;
        nLastY = nParam2_;

        // Remember whether we've processed a double-click, so a third click isn't another one
        fDouble = (nMessage_ == GM_BUTTONDBLCLK);
    }

    // Pass the message to the active GUI component
    s_pGUI->RouteMessage(nMessage_, nParam1_, nParam2_);

    // Send a move after a button up, to give a hit test after an effective mouse capture
    if (s_pGUI && nMessage_ == GM_BUTTONUP)
        s_pGUI->RouteMessage(GM_MOUSEMOVE, s_nX, s_nY);

    // Clear out the garbage
    while (!s_garbageQueue.empty())
    {
        delete s_garbageQueue.front();
        s_garbageQueue.pop();
    }

    // If the GUI still exists, update the activation state
    if (s_pGUI)
        s_pGUI->RouteMessage(GM_MOUSEMOVE, s_nX, s_nY);
    // Otherwise we're done
    else
        Stop();

    return true;
}


bool GUI::Start(Window* pGUI_)
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

    return true;
}

void GUI::Stop()
{
    // Delete any existing GUI object
    if (s_pGUI)
    {
        delete s_pGUI;
        s_pGUI = nullptr;
    }

    Input::Purge();
}

void GUI::Delete(Window* pWindow_)
{
    s_garbageQueue.push(pWindow_);

    // If the top-most window is being removed, invalidate the main GUI pointer
    if (pWindow_ == s_pGUI)
        s_pGUI = nullptr;
}

void GUI::Draw(FrameBuffer& fb)
{
    if (s_pGUI)
    {
        fb.SetFont(s_pGUI->GetFont());
        s_pGUI->Draw(fb);

        // Use hardware cursor on Win32, software cursor on everything else (for now).
#ifndef WIN32
        fb.DrawImage(s_nX, s_nY, ICON_SIZE, ICON_SIZE,
            reinterpret_cast<const uint8_t*>(sMouseCursor.abData), sMouseCursor.abPalette);
#endif
    }
}


bool GUI::IsModal()
{
    return !s_dialogStack.empty();
}

////////////////////////////////////////////////////////////////////////////////

Window::Window(Window* pParent_/*=nullptr*/, int nX_/*=0*/, int nY_/*=0*/, int nWidth_/*=0*/, int nHeight_/*=0*/, int nType_/*=ctUnknown*/)
    : m_nX(nX_), m_nY(nY_), m_nWidth(nWidth_), m_nHeight(nHeight_), m_nType(nType_), m_pFont(sGUIFont)
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

Window::~Window()
{
    // Delete any child controls
    while (m_pChildren)
    {
        Window* pChild = m_pChildren;
        m_pChildren = m_pChildren->m_pNext;
        delete pChild;
    }
}


// Test whether the given point is inside the current control
bool Window::HitTest(int nX_, int nY_)
{
    return (nX_ >= m_nX) && (nX_ < (m_nX + m_nWidth)) && (nY_ >= m_nY) && (nY_ < (m_nY + m_nHeight));
}

// Draw the child controls on the current window
void Window::Draw(FrameBuffer& fb)
{
    for (Window* p = m_pChildren; p; p = p->m_pNext)
    {
        if (p != m_pActive)
        {
            fb.SetFont(p->m_pFont);
            p->Draw(fb);
        }
    }

    // Draw the active control last to ensure it's shown above any other controls
    if (m_pActive)
    {
        fb.SetFont(m_pActive->m_pFont);
        m_pActive->Draw(fb);
    }
}

// Notify the parent something has changed
void Window::NotifyParent(int nParam_/*=0*/)
{
    if (m_pParent)
        m_pParent->OnNotify(this, nParam_);
}

bool Window::RouteMessage(int nMessage_, int nParam1_/*=0*/, int nParam2_/*=0*/)
{
    bool fProcessed = false;
    bool fMouseMessage = (nMessage_ & GM_TYPE_MASK) == GM_MOUSE_MESSAGE;

    // The active child gets first go at the message
    if (m_pActive)
    {
        // If it's a mouse message, update the hit status for the active child
        if (fMouseMessage)
            m_pActive->m_fHover = m_pActive->HitTest(nParam1_, nParam2_);

        fProcessed = m_pActive->RouteMessage(nMessage_, nParam1_, nParam2_);
    }

    // Give the remaining child controls a chance to process the message
    for (Window* p = m_pChildren; !fProcessed && p; )
    {
        Window* pChild = p;
        p = p->m_pNext;

        // If it's a mouse message, update the child control hit status
        if (fMouseMessage)
            pChild->m_fHover = pChild->HitTest(nParam1_, nParam2_);

        // Skip the active window and disabled windows
        if (pChild != m_pActive && pChild->IsEnabled())
        {
            // If we're clicking on a control, activate it
            if ((nMessage_ == GM_BUTTONDOWN) && pChild->IsTabStop() && pChild->m_fHover)
                pChild->Activate();

            fProcessed = pChild->RouteMessage(nMessage_, nParam1_, nParam2_);
        }
    }

    // After the children have had a look, allow the current window a chance to process
    if (!fProcessed)
        fProcessed = OnMessage(nMessage_, nParam1_, nParam2_);

    // If it's a mouse message, update the hit status for this window
    if (fMouseMessage)
        m_fHover = HitTest(nParam1_, nParam2_);

    // Return whether the message was processed
    return fProcessed;
}

bool Window::OnMessage(int /*nMessage_*/, int /*nParam1_=0*/, int /*nParam2_=0*/)
{
    return false;
}


void Window::SetParent(Window* pParent_/*=nullptr*/)
{
    // Unlink from any existing parent
    if (m_pParent)
    {
        Window* pPrev = GetPrev();

        if (!pPrev)
            m_pParent->m_pChildren = GetNext();
        else
            pPrev->m_pNext = GetNext();

        if (m_pParent->m_pActive == this)
            m_pParent->m_pActive = nullptr;

        m_pParent = m_pNext = nullptr;
    }

    // Set the new parent, if any
    if (pParent_ && pParent_ != this)
    {
        m_pParent = pParent_;

        if (!pParent_->m_pChildren)
            pParent_->m_pChildren = this;
        else
        {
            Window* p;
            for (p = pParent_->m_pChildren; p->GetNext(); p = p->GetNext());
            p->m_pNext = this;
        }
    }
}


void Window::Destroy()
{
    // Destroy any child windows first
    while (m_pChildren)
        m_pChildren->Destroy();

    if (m_pParent)
    {
        // Unlink us from the parent, but remember what it was
        Window* pParent = m_pParent;
        SetParent(nullptr);

        // Re-activate the parent now we're gone
        pParent->Activate();
    }

    // Schedule the object to be deleted when safe
    GUI::Delete(this);
}


void Window::Activate()
{
    if (m_pParent)
        m_pParent->m_pActive = this;
}


void Window::SetText(const std::string& str)
{
    m_text = str;
}

unsigned int Window::GetValue() const
{
    char* pEnd = nullptr;
    unsigned long ulValue = std::strtoul(m_text.c_str(), &pEnd, 0);
    return *pEnd ? 0 : static_cast<unsigned int>(ulValue);
}

void Window::SetValue(unsigned int u_)
{
    SetText(fmt::format("{}", u_).c_str());
}

int Window::GetTextWidth(size_t offset, size_t max_length) const
{
    return m_pFont->StringWidth(GetText().substr(offset), static_cast<int>(max_length));
}

int Window::GetTextWidth(const std::string& str) const
{
    return m_pFont->StringWidth(str);
}


Window* Window::GetNext(bool fWrap_/*=false*/)
{
    return m_pNext ? m_pNext : (fWrap_ ? GetSiblings() : nullptr);
}

Window* Window::GetPrev(bool fWrap_/*=false*/)
{
    Window* pLast = nullptr;

    for (Window* p = GetSiblings(); p; pLast = p, p = p->m_pNext)
        if (p->m_pNext == this)
            return p;

    return fWrap_ ? pLast : nullptr;
}

// Return the start of the control group containing the current control
Window* Window::GetGroup()
{
    // Search our sibling controls
    for (Window* p = GetSiblings(); p; p = p->GetNext())
    {
        // Continue looking if it's not a radio button
        if (p->GetType() != GetType())
            continue;

        // Search the rest of the radio group
        for (Window* p2 = p; p2 && p2->GetType() == GetType(); p2 = p2->GetNext())
        {
            // If we've found ourselves, return the start of the group
            if (p2 == this)
                return p;
        }
    }

    return nullptr;
}


void Window::MoveRecurse(Window* pWindow_, int ndX_, int ndY_)
{
    // Move our window by the specified offset
    pWindow_->m_nX += ndX_;
    pWindow_->m_nY += ndY_;

    // Move and child windows
    for (Window* p = pWindow_->m_pChildren; p; p = p->m_pNext)
        MoveRecurse(p, ndX_, ndY_);
}

void Window::Move(int nX_, int nY_)
{
    // Perform a recursive relative move of the window and all children
    MoveRecurse(this, nX_ - m_nX, nY_ - m_nY);
}

void Window::Offset(int ndX_, int ndY_)
{
    // Perform a recursive relative move of the window and all children
    MoveRecurse(this, ndX_, ndY_);
}


void Window::SetSize(int nWidth_, int nHeight_)
{
    if (nWidth_) m_nWidth = nWidth_;
    if (nHeight_) m_nHeight = nHeight_;
}

void Window::Inflate(int ndW_, int ndH_)
{
    m_nWidth += ndW_;
    m_nHeight += ndH_;
}

////////////////////////////////////////////////////////////////////////////////

TextControl::TextControl(Window* pParent_, int nX_, int nY_, const std::string& str,
    uint8_t bColour_/*=WHITE*/, uint8_t bBackColour_/*=0*/)
    : Window(pParent_, nX_, nY_, 0, 0, ctText), m_bColour(bColour_), m_bBackColour(bBackColour_)
{
    SetTextAndColour(str, bColour_);
    m_nWidth = GetTextWidth();
}

void TextControl::Draw(FrameBuffer& fb)
{
    if (m_bBackColour)
        fb.FillRect(m_nX - 1, m_nY - 1, GetTextWidth() + 2, 14, m_bBackColour);

    auto colour = IsEnabled() ? m_bColour : GREY_5;
    fb.DrawString(m_nX, m_nY, colour, GetText());
}

void TextControl::SetTextAndColour(const std::string& str, uint8_t colour)
{
    m_bColour = colour;
    Window::SetText(str);
}

////////////////////////////////////////////////////////////////////////////////

const int BUTTON_BORDER = 3;
const int BUTTON_HEIGHT = BUTTON_BORDER + Font::CHAR_HEIGHT + BUTTON_BORDER;

Button::Button(Window* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
    : Window(pParent_, nX_, nY_, nWidth_, nHeight_ ? nHeight_ : BUTTON_HEIGHT, ctButton)
{
}

void Button::Draw(FrameBuffer& fb)
{
    bool fPressed = m_fPressed && IsOver();

    // Fill the main button background
    fb.FillRect(m_nX + 1, m_nY + 1, m_nWidth - 2, m_nHeight - 2, IsActive() ? YELLOW_8 : GREY_7);

    // Draw the edge highlight for the top and left
    fb.DrawLine(m_nX, m_nY, m_nWidth, 0, fPressed ? GREY_5 : WHITE);
    fb.DrawLine(m_nX, m_nY, 0, m_nHeight, fPressed ? GREY_5 : WHITE);

    // Draw the edge highlight for the bottom and right
    fb.DrawLine(m_nX + 1, m_nY + m_nHeight - 1, m_nWidth - 2, 0, fPressed ? WHITE : GREY_5);
    fb.DrawLine(m_nX + m_nWidth - 1, m_nY + 1, 0, m_nHeight - 1, fPressed ? WHITE : GREY_5);
}

bool Button::OnMessage(int nMessage_, int nParam1_, int /*nParam2_*/)
{
    switch (nMessage_)
    {
    case GM_CHAR:
        if (!IsActive())
            break;

        switch (nParam1_)
        {
        case HK_SPACE:
        case HK_RETURN:
            NotifyParent(nParam1_ == HK_RETURN);
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

TextButton::TextButton(Window* pParent_, int nX_, int nY_, const std::string& str, int nMinWidth_/*=0*/)
    : Button(pParent_, nX_, nY_, 0, BUTTON_HEIGHT), m_nMinWidth(nMinWidth_)
{
    SetText(str);
}

void TextButton::SetText(const std::string& str)
{
    Window::SetText(str);

    // Set the control width to be just enough to contain the text and a border
    m_nWidth = BUTTON_BORDER + GetTextWidth() + BUTTON_BORDER;

    // If we're below the minimum width, set to the minimum
    if (m_nWidth < m_nMinWidth)
        m_nWidth = m_nMinWidth;
}

void TextButton::Draw(FrameBuffer& fb)
{
    Button::Draw(fb);

    bool fPressed = m_fPressed && IsOver();

    // Centralise the text in the button, and offset down and right if it's pressed
    int nX = m_nX + fPressed + (m_nWidth - GetTextWidth()) / 2;
    int nY = m_nY + fPressed + (m_nHeight - Font::CHAR_HEIGHT) / 2 + 1;
    auto colour = IsEnabled() ? (IsActive() ? BLACK : BLACK) : GREY_5;
    fb.DrawString(nX, nY, colour, GetText());
}

////////////////////////////////////////////////////////////////////////////////

ImageButton::ImageButton(Window* pParent_, int nX_, int nY_, int nWidth_, int nHeight_,
    const GUI_ICON* pIcon_, int nDX_/*=0*/, int nDY_/*=0*/)
    : Button(pParent_, nX_, nY_, nWidth_, nHeight_), m_pIcon(pIcon_), m_nDX(nDX_), m_nDY(nDY_)
{
    m_nType = ctImageButton;
}

void ImageButton::Draw(FrameBuffer& fb)
{
    Button::Draw(fb);

    bool fPressed = m_fPressed && IsOver();
    int nX = m_nX + m_nDX + fPressed, nY = m_nY + m_nDY + fPressed;

    fb.DrawImage(nX, nY, ICON_SIZE, ICON_SIZE, reinterpret_cast<const uint8_t*>(m_pIcon->abData),
        IsEnabled() ? m_pIcon->abPalette : m_pIcon->abPalette);
}

////////////////////////////////////////////////////////////////////////////////

UpButton::UpButton(Window* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
    : Button(pParent_, nX_, nY_, nWidth_, nHeight_)
{
}

void UpButton::Draw(FrameBuffer& fb)
{
    Button::Draw(fb);

    bool fPressed = m_fPressed && IsOver();

    int nX = m_nX + 2 + fPressed, nY = m_nY + 3 + fPressed;
    uint8_t bColour = GetParent()->IsEnabled() ? BLACK : GREY_5;
    fb.DrawLine(nX + 5, nY, 1, 0, bColour);
    fb.DrawLine(nX + 4, nY + 1, 3, 0, bColour);
    fb.DrawLine(nX + 3, nY + 2, 2, 0, bColour);  fb.DrawLine(nX + 6, nY + 2, 2, 0, bColour);
    fb.DrawLine(nX + 2, nY + 3, 2, 0, bColour);  fb.DrawLine(nX + 7, nY + 3, 2, 0, bColour);
    fb.DrawLine(nX + 1, nY + 4, 2, 0, bColour);  fb.DrawLine(nX + 8, nY + 4, 2, 0, bColour);
}

////////////////////////////////////////////////////////////////////////////////

DownButton::DownButton(Window* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
    : Button(pParent_, nX_, nY_, nWidth_, nHeight_)
{
}

void DownButton::Draw(FrameBuffer& fb)
{
    Button::Draw(fb);

    bool fPressed = m_fPressed && IsOver();

    int nX = m_nX + 2 + fPressed, nY = m_nY + 5 + fPressed;
    uint8_t bColour = GetParent()->IsEnabled() ? BLACK : GREY_5;
    fb.DrawLine(nX + 5, nY + 5, 1, 0, bColour);
    fb.DrawLine(nX + 4, nY + 4, 3, 0, bColour);
    fb.DrawLine(nX + 3, nY + 3, 2, 0, bColour);  fb.DrawLine(nX + 6, nY + 3, 2, 0, bColour);
    fb.DrawLine(nX + 2, nY + 2, 2, 0, bColour);  fb.DrawLine(nX + 7, nY + 2, 2, 0, bColour);
    fb.DrawLine(nX + 1, nY + 1, 2, 0, bColour);  fb.DrawLine(nX + 8, nY + 1, 2, 0, bColour);
}

////////////////////////////////////////////////////////////////////////////////

const int PRETEXT_GAP = 5;
const int BOX_SIZE = 11;

CheckBox::CheckBox(Window* pParent_/*=nullptr*/, int nX_/*=0*/, int nY_/*=0*/, const std::string& str/*=""*/,
    uint8_t bColour_/*=WHITE*/, uint8_t bBackColour_/*=0*/)
    : Window(pParent_, nX_, nY_, 0, BOX_SIZE, ctCheckBox), m_bColour(bColour_), m_bBackColour(bBackColour_)
{
    SetText(str);
}

void CheckBox::SetText(const std::string& str)
{
    Window::SetText(str);

    // Set the control width to be just enough to contain the text
    m_nWidth = 1 + BOX_SIZE + PRETEXT_GAP + GetTextWidth();
}

void CheckBox::Draw(FrameBuffer& fb)
{
    // Draw the text to the right of the box, grey if the control is disabled
    int nX = m_nX + BOX_SIZE + PRETEXT_GAP;
    int nY = m_nY + (BOX_SIZE - Font::CHAR_HEIGHT) / 2 + 1;

    // Fill the background if required
    if (m_bBackColour)
        fb.FillRect(m_nX - 1, m_nY - 1, BOX_SIZE + PRETEXT_GAP + GetTextWidth() + 2, BOX_SIZE + 2, m_bBackColour);

    // Draw the label text
    auto colour = IsEnabled() ? (IsActive() ? YELLOW_8 : m_bColour) : GREY_5;
    fb.DrawString(nX, nY, colour, GetText());

    // Draw the empty check box
    fb.FrameRect(m_nX, m_nY, BOX_SIZE, BOX_SIZE, !IsEnabled() ? GREY_5 : IsActive() ? YELLOW_8 : GREY_7);

    uint8_t abEnabled[] = { 0, GREY_7 }, abDisabled[] = { 0, GREY_5 };

    static const std::vector<uint8_t> check_mark
    {
        0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,1,0,0,
        0,0,0,0,0,0,0,1,1,0,0,
        0,0,0,0,0,0,1,1,1,0,0,
        0,0,1,0,0,1,1,1,0,0,0,
        0,0,1,1,1,1,1,0,0,0,0,
        0,0,1,1,1,1,0,0,0,0,0,
        0,0,0,1,1,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,
    };

    // Box checked?
    if (m_fChecked)
        fb.DrawImage(m_nX, m_nY, 11, 11, check_mark.data(), IsEnabled() ? abEnabled : abDisabled);
}

bool CheckBox::OnMessage(int nMessage_, int nParam1_, int /*nParam2_*/)
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
        case HK_SPACE:
        case HK_RETURN:
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

const int EDIT_BORDER = 3;
const int EDIT_HEIGHT = EDIT_BORDER + Font::CHAR_HEIGHT + EDIT_BORDER;

const size_t MAX_EDIT_LENGTH = 250;

EditControl::EditControl(Window* pParent_, int nX_, int nY_, int nWidth_, const std::string& str)
    : Window(pParent_, nX_, nY_, nWidth_, EDIT_HEIGHT, ctEdit)
{
    SetText(str);
}

EditControl::EditControl(Window* pParent_, int nX_, int nY_, int nWidth_, unsigned int u_)
    : Window(pParent_, nX_, nY_, nWidth_, EDIT_HEIGHT, ctEdit)
{
    SetValue(u_);
}

void EditControl::Activate()
{
    Window::Activate();

    m_nCaretStart = 0;
    m_nCaretEnd = GetText().size();
}

void EditControl::SetText(const std::string& str)
{
    SetSelectedText(str, false);
}

void EditControl::SetSelectedText(const std::string& str, bool fSelected_)
{
    Window::SetText(str);

    // Select the text or position the caret at the end, as requested
    m_nCaretEnd = str.length();
    m_nCaretStart = fSelected_ ? 0 : m_nCaretEnd;
}

void EditControl::Draw(FrameBuffer& fb)
{
    int nWidth = m_nWidth - 2 * EDIT_BORDER;

    // Fill overall control background, and draw a frame round it
    fb.FillRect(m_nX + 1, m_nY + 1, m_nWidth - 2, m_nHeight - 2, IsEnabled() ? (IsActive() ? YELLOW_8 : WHITE) : GREY_7);
    fb.FrameRect(m_nX, m_nY, m_nWidth, m_nHeight, GREY_7);

    // Draw a light edge highlight for the bottom and right
    fb.DrawLine(m_nX + 1, m_nY + m_nHeight - 1, m_nWidth - 1, 0, GREY_7);
    fb.DrawLine(m_nX + m_nWidth - 1, m_nY + 1, 0, m_nHeight - 1, GREY_7);

    // If the caret is before the current view start we need to shift the view back
    if (m_nCaretEnd < m_nViewOffset)
    {
        for (m_nViewOffset = m_nCaretEnd; m_nViewOffset > 0; m_nViewOffset--)
        {
            // Stop if we've exposed 1/4 of the edit control length
            if (GetTextWidth(m_nViewOffset, m_nCaretEnd - m_nViewOffset) >= (nWidth / 4))
            {
                m_nViewOffset--;
                break;
            }
        }
    }
    // If the caret is beyond the end of the control we need to shift the view forwards
    else if (GetTextWidth(m_nViewOffset, m_nCaretEnd - m_nViewOffset) > nWidth)
    {
        // Loop while the view string is still too long for the control
        for (; GetTextWidth(m_nViewOffset) > nWidth; m_nViewOffset++)
        {
            // Stop if caret is within 3/4 of the edit control length
            if (GetTextWidth(m_nViewOffset, m_nCaretEnd - m_nViewOffset) < (nWidth * 3 / 4))
                break;
        }
    }

    // Top-left of text region within edit control
    int nX = m_nX + EDIT_BORDER;
    int nY = m_nY + EDIT_BORDER;

    // Determine the visible length of the string within the control
    size_t nViewLength = 0;
    for (; GetText()[m_nViewOffset + nViewLength]; nViewLength++)
    {
        // Too long for control?
        if (GetTextWidth(m_nViewOffset, nViewLength) > nWidth)
        {
            // Remove a character to bring within width, and finish
            nViewLength--;
            break;
        }
    }

    // Draw the visible text
    fb.DrawString(nX, nY + 1, "\a{}{}", IsEnabled() ? 'k' : 'K', GetText().substr(m_nViewOffset, nViewLength));

    // Is the control focussed with an active selection?
    if (IsActive() && m_nCaretStart != m_nCaretEnd)
    {
        size_t nStart = m_nCaretStart, nEnd = m_nCaretEnd;
        if (nStart > nEnd)
            std::swap(nStart, nEnd);

        // Clip start to visible selection
        if (nStart < m_nViewOffset)
            nStart = m_nViewOffset;

        // Clip end to visible selection
        if (nEnd - nStart > nViewLength)
            nEnd = nStart + nViewLength;

        // Determine offset and pixel width of highlighted selection
        int dx = GetTextWidth(m_nViewOffset, nStart - m_nViewOffset);
        int wx = GetTextWidth(nStart, nEnd - nStart);

        // Draw the black selection highlight and white text over it
        fb.FillRect(nX + dx + !!dx - 1, nY - 1, 1 + wx + 1, 1 + Font::CHAR_HEIGHT + 1, IsEnabled() ? (IsActive() ? BLACK : GREY_4) : GREY_6);
        fb.DrawString(nX + dx + !!dx, nY + 1, "{}", GetText().substr(nStart, nEnd - nStart));
    }

    // If the control is enabled and focussed we'll show a flashing caret after the text
    if (IsEnabled() && IsActive())
    {
        bool fCaretOn = ((OSD::GetTime() - m_dwCaretTime) % 800) < 400;
        int dx = GetTextWidth(m_nViewOffset, m_nCaretEnd - m_nViewOffset);

        // Draw a character-height vertical bar after the text
        fb.DrawLine(nX + dx - !dx, nY - 1, 0, 1 + Font::CHAR_HEIGHT + 1, fCaretOn ? BLACK : YELLOW_8);
    }
}

bool EditControl::OnMessage(int nMessage_, int nParam1_, int nParam2_)
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

        // Reset caret blink time so it's visible
        m_dwCaretTime = OSD::GetTime();

        bool fCtrl = !!(nParam2_ & HM_CTRL);
        bool fShift = !!(nParam2_ & HM_SHIFT);

        switch (nParam1_)
        {
        case HK_HOME:
            // Start of line, with optional select
            m_nCaretEnd = fShift ? 0 : m_nCaretStart = 0;
            return true;

        case HK_END:
            // End of line, with optional select
            m_nCaretEnd = fShift ? GetText().size() : m_nCaretStart = GetText().size();
            return true;

        case HK_LEFT:
            // Previous word
            if (fCtrl)
            {
                // Step back over whitespace
                while (m_nCaretEnd > 0 && GetText()[m_nCaretEnd - 1] == ' ')
                    m_nCaretEnd--;

                // Step back until whitespace
                while (m_nCaretEnd > 0 && GetText()[m_nCaretEnd - 1] != ' ')
                    m_nCaretEnd--;
            }
            // Previous character
            else if (m_nCaretEnd > 0)
                m_nCaretEnd--;

            // Not extending selection?
            if (!fShift)
                m_nCaretStart = m_nCaretEnd;

            return true;

        case HK_RIGHT:
            // Next word
            if (fCtrl)
            {
                // Step back until whitespace
                while (GetText()[m_nCaretEnd] && GetText()[m_nCaretEnd] != ' ')
                    m_nCaretEnd++;

                // Step back over whitespace
                while (GetText()[m_nCaretEnd] && GetText()[m_nCaretEnd] == ' ')
                    m_nCaretEnd++;
            }
            // Next character
            else if (GetText()[m_nCaretEnd])
                m_nCaretEnd++;

            // Not extending selection?
            if (!fShift)
                m_nCaretStart = m_nCaretEnd;

            return true;

            // Return possibly submits the dialog contents
        case HK_RETURN:
        case HK_KPENTER:
            NotifyParent(1);
            return true;

        default:
            // Ctrl combination?
            if (nParam2_ & HM_CTRL)
            {
                // Ctrl-A is select all
                if (nParam1_ == 'A')
                {
                    m_nCaretStart = 0;
                    m_nCaretEnd = GetText().length();
                    return true;
                }

                break;
            }

            // No selection?
            if (m_nCaretStart == m_nCaretEnd)
            {
                // For backspace and delete, create a single character selection, if not at the ends
                if (nParam1_ == HK_BACKSPACE && m_nCaretStart > 0)
                    m_nCaretStart--;
                else if (nParam1_ == HK_DELETE && m_nCaretEnd < GetText().length())
                    m_nCaretEnd++;
            }

            // Ignore anything that isn't a printable character, backspace or delete
            if (nParam1_ != HK_BACKSPACE && nParam1_ != HK_DELETE && (nParam1_ < ' ' || nParam1_ > 0x7f))
                break;

            // Active selection?
            if (m_nCaretStart != m_nCaretEnd)
            {
                // Ensure start < end
                if (m_nCaretStart > m_nCaretEnd)
                    std::swap(m_nCaretStart, m_nCaretEnd);

                // Remove the selection, and set the text back
                auto new_text = GetText().substr(0, m_nCaretStart) + GetText().substr(m_nCaretEnd);
                Window::SetText(new_text);

                // Move the end selection to the previous selection start position
                m_nCaretEnd = m_nCaretStart;
            }

            // Only accept printable characters
            if (nParam1_ >= ' ' && nParam1_ <= 0x7f)
            {
                // Only add a character if we're not at the maximum length yet
                if (GetText().length() < MAX_EDIT_LENGTH)
                {
                    // Insert the new character at the caret position
                    auto new_text = GetText().substr(0, m_nCaretStart) + static_cast<char>(nParam1_) + GetText().substr(m_nCaretEnd);
                    m_nCaretStart = ++m_nCaretEnd;
                    Window::SetText(new_text);
                }
            }

            // Content changed
            NotifyParent();
            return true;
        }
        break;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

bool NumberEditControl::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    if (nMessage_ == GM_CHAR)
    {
        switch (nParam1_)
        {
        case HK_KPDECIMAL:  nParam1_ = '.'; break;
        case HK_KPPLUS:     nParam1_ = '+'; break;
        case HK_KPMINUS:    nParam1_ = '-'; break;
        case HK_KPMULT:     nParam1_ = '*'; break;
        case HK_KPDIVIDE:   nParam1_ = '/'; break;
        default:
            if (nParam1_ >= HK_KP0 && nParam1_ <= HK_KP9)
                nParam1_ = (nParam1_ - HK_KP0) + '0';
            break;
        }
    }

    return EditControl::OnMessage(nMessage_, nParam1_, nParam2_);
}

////////////////////////////////////////////////////////////////////////////////

const int RADIO_PRETEXT_GAP = 16;

RadioButton::RadioButton(Window* pParent_, int nX_, int nY_, const std::string& str, int nWidth_/*=0*/)
    : Window(pParent_, nX_, nY_, nWidth_, Font::CHAR_HEIGHT + 2, ctRadio)
{
    SetText(str);
}

void RadioButton::SetText(const std::string& str)
{
    Window::SetText(str);

    // Set the control width to be just enough to contain the text
    m_nWidth = 1 + RADIO_PRETEXT_GAP + GetTextWidth();
}

void RadioButton::Draw(FrameBuffer& fb)
{
    int nX = m_nX + 1, nY = m_nY;

    uint8_t abActive[] = { 0, GREY_5, GREY_7, YELLOW_8 };
    uint8_t abEnabled[] = { 0, GREY_5, GREY_7, GREY_7 };
    uint8_t abDisabled[] = { 0, GREY_3, GREY_5, GREY_5 };

    static const std::vector<uint8_t> selected
    {
        0,0,0,3,3,3,3,0,0,0,
        0,0,3,0,0,0,0,3,0,0,
        0,3,0,1,2,2,1,0,3,0,
        3,0,1,2,2,2,2,1,0,3,
        3,0,2,2,2,2,2,2,0,3,
        3,0,2,2,2,2,2,2,0,3,
        3,0,1,2,2,2,2,1,0,3,
        0,3,0,1,2,2,1,0,3,0,
        0,0,3,0,0,0,0,3,0,0,
        0,0,0,3,3,3,3,0,0,0,
    };

    static const std::vector<uint8_t> unselected
    {
        0,0,0,3,3,3,3,0,0,0,
        0,0,3,0,0,0,0,3,0,0,
        0,3,0,0,0,0,0,0,3,0,
        3,0,0,0,0,0,0,0,0,3,
        3,0,0,0,0,0,0,0,0,3,
        3,0,0,0,0,0,0,0,0,3,
        3,0,0,0,0,0,0,0,0,3,
        0,3,0,0,0,0,0,0,3,0,
        0,0,3,0,0,0,0,3,0,0,
        0,0,0,3,3,3,3,0,0,0,
    };

    // Draw the radio button image in the current state
    fb.DrawImage(nX, nY, 10, 10, (m_fSelected ? selected : unselected).data(),
        !IsEnabled() ? abDisabled : IsActive() ? abActive : abEnabled);

    // Draw the text to the right of the button, grey if the control is disabled
    auto colour = IsEnabled() ? (IsActive() ? YELLOW_8 : GREY_7) : GREY_5;
    fb.DrawString(nX + RADIO_PRETEXT_GAP, nY + 1, colour, GetText());
}

bool RadioButton::OnMessage(int nMessage_, int nParam1_, int /*nParam2_*/)
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
            Window* pPrev = GetPrev();
            if (pPrev && pPrev->GetType() == GetType())
            {
                pPrev->Activate();
                reinterpret_cast<RadioButton*>(pPrev)->Select();
                NotifyParent();
            }
            return true;
        }

        case HK_RIGHT:
        case HK_DOWN:
        {
            Window* pNext = GetNext();
            if (pNext && pNext->GetType() == GetType())
            {
                pNext->Activate();
                reinterpret_cast<RadioButton*>(pNext)->Select();
                NotifyParent();
            }
            return true;
        }

        case HK_SPACE:
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

void RadioButton::Select(bool fSelected_/*=true*/)
{
    // Remember the new status
    m_fSelected = fSelected_;

    // Of it's a selection we have more work to do...
    if (m_fSelected)
    {
        // Search the control group
        for (Window* p = GetGroup(); p && p->GetType() == ctRadio; p = p->GetNext())
        {
            // Deselect the button if it's not us
            if (p != this)
                reinterpret_cast<RadioButton*>(p)->Select(false);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

const int MENU_TEXT_GAP = 5;
const int MENU_ITEM_HEIGHT = 2 + Font::CHAR_HEIGHT + 2;

Menu::Menu(Window* pParent_/*=nullptr*/, int nX_/*=0*/, int nY_/*=0*/, const std::string& str)
    : Window(pParent_, nX_, nY_, 0, 0, ctMenu)
{
    SetText(str);
    Activate();
}

void Menu::Select(int index)
{
    auto num_items = static_cast<int>(m_items.size());

    if (index >= 0 && index < num_items)
        m_nSelected = index;
    else if (index < 0)
        m_nSelected = num_items - 1;
    else if (index > num_items)
        m_nSelected = 0;
}

void Menu::SetText(const std::string& str)
{
    Window::SetText(str);
    m_items = split(str, '|');

    auto it = std::max_element(m_items.begin(), m_items.end(),
        [&](auto& a, auto& b) { return GetTextWidth(a) < GetTextWidth(b); });

    auto maxlen = (it != m_items.end()) ? GetTextWidth(*it) : 0;
    m_nWidth = MENU_TEXT_GAP + maxlen + MENU_TEXT_GAP;
    m_nHeight = MENU_ITEM_HEIGHT * static_cast<int>(m_items.size());
}

void Menu::Draw(FrameBuffer& fb)
{
    fb.FillRect(m_nX, m_nY, m_nWidth, m_nHeight, YELLOW_8);
    fb.FrameRect(m_nX - 1, m_nY - 1, m_nWidth + 2, m_nHeight + 2, GREY_7);

    auto index = 0;
    for (const auto& item_str : m_items)
    {
        auto nX = m_nX;
        auto nY = m_nY + (MENU_ITEM_HEIGHT * index);

        if (index != m_nSelected)
            fb.DrawString(nX + MENU_TEXT_GAP, nY + 2, BLACK, item_str);
        else
        {
            fb.FillRect(nX, nY, m_nWidth, MENU_ITEM_HEIGHT, BLACK);
            fb.DrawString(nX + MENU_TEXT_GAP, nY + 2, item_str);
        }

        index++;
    }
}

bool Menu::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    switch (nMessage_)
    {
    case GM_CHAR:
        switch (nParam1_)
        {
            // Return uses the current selection
        case HK_RETURN:
            NotifyParent();
            Destroy();
            return true;

            // Esc cancels
        case HK_ESC:
            m_nSelected = -1;
            NotifyParent();
            Destroy();
            return true;

            // Move to the previous selection, wrapping to the bottom if necessary
        case HK_UP:
            Select(static_cast<int>((m_nSelected - 1 + m_items.size()) % m_items.size()));
            break;

            // Move to the next selection, wrapping to the top if necessary
        case HK_DOWN:
            Select(static_cast<int>((m_nSelected + 1) % m_items.size()));
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
        m_nSelected = IsOver() ? ((nParam2_ - m_nY) / MENU_ITEM_HEIGHT) : -1;

        // Treat the movement as an effective drag, for the case of a combo-box drag
        return m_fPressed = true;

    case GM_MOUSEWHEEL:
        if (IsActive())
        {
            Select(static_cast<int>((m_nSelected + nParam1_ + m_items.size()) % m_items.size()));
            return true;
        }
        break;
    }

    return false;
}

////////////////////////////////////////////////////////////////////////////////

DropList::DropList(Window* pParent_, int nX_, int nY_, const std::string& str, int nMinWidth_)
    : Menu(pParent_, nX_, nY_, str), m_nMinWidth(nMinWidth_)
{
    m_nSelected = 0;
    SetText(str);
}

void DropList::SetText(const std::string& str)
{
    Menu::SetText(str);

    if (m_nWidth < m_nMinWidth)
        m_nWidth = m_nMinWidth;
}

bool DropList::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    // Eat movement messages that are not over the control
    if (nMessage_ == GM_MOUSEMOVE && !IsOver())
        return true;

    return Menu::OnMessage(nMessage_, nParam1_, nParam2_);
}

////////////////////////////////////////////////////////////////////////////////

const int COMBO_BORDER = 3;
const int COMBO_HEIGHT = COMBO_BORDER + Font::CHAR_HEIGHT + COMBO_BORDER;

ComboBox::ComboBox(Window* pParent_, int nX_, int nY_, const std::string& items_str, int nWidth_)
    : Window(pParent_, nX_, nY_, nWidth_, COMBO_HEIGHT, ctComboBox)
{
    SetText(items_str);
}


void ComboBox::Select(int index)
{
    auto prev_selected = m_nSelected;
    int num_items = static_cast<int>(m_items.size());

    m_nSelected = std::min(std::max(0, index), num_items - 1);

    if (m_nSelected != prev_selected)
        NotifyParent();
}

void ComboBox::Select(const std::string& item_str)
{
    auto find_str = tolower(item_str);

    auto it = std::find_if(m_items.begin(), m_items.end(),
        [&](auto& str) { return tolower(str) == find_str; });

    if (it != m_items.end())
        Select(static_cast<int>(std::distance(m_items.begin(), it)));
}


std::string ComboBox::GetSelectedText()
{
    if (m_nSelected >= 0 && m_nSelected < static_cast<int>(m_items.size()))
        return m_items[m_nSelected];

    return "";
}

void ComboBox::SetText(const std::string& items_str)
{
    Window::SetText(items_str);
    m_items = split(items_str, '|');
    Select(0);
}

void ComboBox::Draw(FrameBuffer& fb)
{
    bool fPressed = m_fPressed;

    // Fill the main control background
    fb.FrameRect(m_nX, m_nY, m_nWidth, m_nHeight, GREY_7);
    fb.FillRect(m_nX + 1, m_nY + 1, m_nWidth - COMBO_HEIGHT - 1, m_nHeight - 2,
        !IsEnabled() ? GREY_7 : (IsActive() && !fPressed) ? YELLOW_8 : WHITE);

    // Fill the main button background
    int nX = m_nX + m_nWidth - COMBO_HEIGHT, nY = m_nY + 1;
    fb.FillRect(nX + 1, nY + 1, COMBO_HEIGHT - 1, m_nHeight - 3, GREY_7);

    // Draw the edge highlight for the top, left, right and bottom
    fb.DrawLine(nX, nY, COMBO_HEIGHT, 0, fPressed ? GREY_5 : WHITE);
    fb.DrawLine(nX, nY, 0, m_nHeight - 2, fPressed ? GREY_5 : WHITE);
    fb.DrawLine(nX + 1, nY + m_nHeight - 2, COMBO_HEIGHT - 2, 0, fPressed ? WHITE : GREY_5);
    fb.DrawLine(nX + COMBO_HEIGHT - 1, nY + 1, 0, m_nHeight - 2, fPressed ? WHITE : GREY_5);

    // Show the arrow button, down a pixel if it's pressed
    nY += fPressed;
    uint8_t bColour = IsEnabled() ? BLACK : GREY_5;
    fb.DrawLine(nX + 8, nY + 9, 1, 0, bColour);
    fb.DrawLine(nX + 7, nY + 8, 3, 0, bColour);
    fb.DrawLine(nX + 6, nY + 7, 2, 0, bColour);  fb.DrawLine(nX + 9, nY + 7, 2, 0, bColour);
    fb.DrawLine(nX + 5, nY + 6, 2, 0, bColour);  fb.DrawLine(nX + 10, nY + 6, 2, 0, bColour);
    fb.DrawLine(nX + 4, nY + 5, 2, 0, bColour);  fb.DrawLine(nX + 11, nY + 5, 2, 0, bColour);

    auto item_str = (m_nSelected >= 0 && m_nSelected < static_cast<int>(m_items.size())) ?
        m_items[m_nSelected] : "";

    nX = m_nX + 5;
    nY = m_nY + (m_nHeight - Font::CHAR_HEIGHT) / 2 + 1;
    auto colour = IsEnabled() ? (IsActive() ? BLACK : BLACK) : GREY_5;
    fb.DrawString(nX, nY, colour, item_str);

    // Call the base to paint any child controls
    Window::Draw(fb);
}

bool ComboBox::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    // Give child controls first go at the message
    if (Window::OnMessage(nMessage_, nParam1_, nParam2_))
        return true;

    switch (nMessage_)
    {
    case GM_CHAR:
        if (!IsActive())
            break;

        switch (nParam1_)
        {
        case HK_SPACE:
        case HK_RETURN:
            m_fPressed = !m_fPressed;
            if (m_fPressed)
                (m_pDropList = new DropList(this, 1, COMBO_HEIGHT, GetText(), m_nWidth - 2))->Select(m_nSelected);
            return true;

        case HK_UP:
            Select(m_nSelected - 1);
            return true;

        case HK_DOWN:
            Select(m_nSelected + 1);
            return true;

        case HK_HOME:
            Select(0);
            return true;

        case HK_END:
            Select(static_cast<int>(m_items.size() - 1));
            return true;
        }
        break;

    case GM_BUTTONDOWN:
    case GM_BUTTONDBLCLK:
        if (!IsOver())
            break;

        // If the click was over us, and the button is pressed, create the drop list
        if (IsOver() && (m_fPressed = !m_fPressed))
            (m_pDropList = new DropList(this, 1, COMBO_HEIGHT, GetText(), m_nWidth - 2))->Select(m_nSelected);

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

void ComboBox::OnNotify(Window* pWindow_, int /*nParam_=0*/)
{
    if (pWindow_ == m_pDropList)
    {
        int nSelected = m_pDropList->GetSelected();
        if (nSelected != -1)
            Select(nSelected);

        m_fPressed = false;
        m_pDropList = nullptr;
    }
}

////////////////////////////////////////////////////////////////////////////////

const int SCROLLBAR_WIDTH = 15;
const int SB_BUTTON_HEIGHT = 15;

ScrollBar::ScrollBar(Window* pParent_, int nX_, int nY_, int nHeight_, int nMaxPos_, int nStep_/*=1*/)
    : Window(pParent_, nX_, nY_, SCROLLBAR_WIDTH, nHeight_), m_nStep(nStep_)
{
    m_pUp = new UpButton(this, 0, 0, m_nWidth, SB_BUTTON_HEIGHT);
    m_pDown = new DownButton(this, 0, m_nHeight - SB_BUTTON_HEIGHT, m_nWidth, SB_BUTTON_HEIGHT);

    m_nScrollHeight = nHeight_ - SB_BUTTON_HEIGHT * 2;
    SetMaxPos(nMaxPos_);
}

void ScrollBar::SetPos(int nPosition_)
{
    m_nPos = (nPosition_ < 0) ? 0 : (nPosition_ > m_nMaxPos) ? m_nMaxPos : nPosition_;
}

void ScrollBar::SetMaxPos(int nMaxPos_)
{
    m_nPos = 0;

    // Determine how much of the height is not covered by the current view
    m_nMaxPos = nMaxPos_ - m_nHeight;

    // If we have a scrollable portion, set the thumb size to indicate how much
    if (nMaxPos_ && m_nMaxPos > 0)
        m_nThumbSize = std::max(m_nHeight * m_nScrollHeight / nMaxPos_, 10);
}

void ScrollBar::Draw(FrameBuffer& fb)
{
    // Don't draw anything if we don't have a scroll range
    if (m_nMaxPos <= 0)
        return;

    // Fill the main button background
    fb.FillRect(m_nX + 1, m_nY + 1, m_nWidth - 2, m_nHeight - 2, GREY_7);

    // Draw the edge highlight for the top, left, bottom and right
    fb.DrawLine(m_nX, m_nY, m_nWidth, 0, WHITE);
    fb.DrawLine(m_nX, m_nY, 0, m_nHeight, WHITE);
    fb.DrawLine(m_nX + 1, m_nY + m_nHeight - 1, m_nWidth - 2, 0, WHITE);
    fb.DrawLine(m_nX + m_nWidth - 1, m_nY + 1, 0, m_nHeight - 1, WHITE);

    int nHeight = m_nScrollHeight - m_nThumbSize, nPos = nHeight * m_nPos / m_nMaxPos;

    int nX = m_nX, nY = m_nY + SB_BUTTON_HEIGHT + nPos;

    // Fill the main button background
    fb.FillRect(nX, nY, m_nWidth, m_nThumbSize, !IsEnabled() ? GREY_7 : GREY_7);

    fb.DrawLine(nX, nY, m_nWidth, 0, WHITE);
    fb.DrawLine(nX, nY, 0, m_nThumbSize, WHITE);
    fb.DrawLine(nX + 1, nY + m_nThumbSize - 1, m_nWidth - 1, 0, GREY_4);
    fb.DrawLine(nX + m_nWidth - 1, nY + 1, 0, m_nThumbSize - 1, GREY_4);

    Window::Draw(fb);
}

bool ScrollBar::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    static int nDragOffset;
    static bool fDragging;

    // We're inert (and invisible) if there's no scroll range
    if (m_nMaxPos <= 0)
        return false;

    bool fRet = Window::OnMessage(nMessage_, nParam1_, nParam2_);

    // Stop the buttons remaining active
    m_pActive = nullptr;

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
            else if (nY >= (nPos + m_nThumbSize))
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
        SetPos(m_nPos + m_nStep * nParam1_);
        return true;
    }
    return false;
}

void ScrollBar::OnNotify(Window* pWindow_, int /*nParam_=0*/)
{
    if (pWindow_ == m_pUp)
        SetPos(m_nPos - m_nStep);
    else if (pWindow_ == m_pDown)
        SetPos(m_nPos + m_nStep);
}

////////////////////////////////////////////////////////////////////////////////

const int ITEM_SIZE = 72;

// Helper to locate matches when a filename is typed
static bool IsPrefix(const char* pcszPrefix_, const char* pcszName_)
{
    // Skip any common characters at the start of the name
    while (*pcszPrefix_ && *pcszName_ && tolower(*pcszPrefix_) == tolower(*pcszName_))
    {
        pcszPrefix_++;
        pcszName_++;
    }

    // Return true if the full prefix was matched
    return !*pcszPrefix_;
}


ListView::ListView(Window* pParent_, int nX_, int nY_, int nWidth_, int nHeight_, int nItemOffset_/*=0*/)
    : Window(pParent_, nX_, nY_, nWidth_, nHeight_, ctListView), m_nItemOffset(nItemOffset_)
{
    // Create a scrollbar to cover the overall height, scrolling if necessary
    m_pScrollBar = new ScrollBar(this, m_nWidth - SCROLLBAR_WIDTH, 0, m_nHeight, 0, ITEM_SIZE);
}

void ListView::Select(int index)
{
    auto prev_selected = m_nSelected;
    auto num_items = static_cast<int>(m_items.size());
    m_nSelected = (index < 0) ? 0 : (index >= num_items) ? num_items - 1 : index;

    // Calculate the row containing the new item, and the vertical offset in the list overall
    auto row = m_nSelected / m_nAcross;
    auto offset = row * ITEM_SIZE - m_pScrollBar->GetPos();

    // If the new item is not completely visible, scroll the list so it _just_ is
    if (offset < 0 || offset >= (m_nHeight - ITEM_SIZE))
        m_pScrollBar->SetPos(row * ITEM_SIZE - ((offset < 0) ? 0 : (m_nHeight - ITEM_SIZE)));

    if (m_nSelected != prev_selected)
        NotifyParent();
}


// Return the entry for the specified item, or the current item if none was specified
const ListViewItem* ListView::GetItem(int index/*=-1*/) const
{
    if (index == -1)
        index = GetSelected();

    if (index >= 0 && index < static_cast<int>(m_items.size()))
    {
        return &m_items[index];
    }

    return nullptr;
}

// Find the item with the specified label (not case-sensitive)
std::optional<int> ListView::FindItem(const std::string& label, int nStart_/*=0*/)
{
    auto label_lower = tolower(label);

    auto it = std::find_if(m_items.begin(), m_items.end(),
        [&](ListViewItem& item) { return tolower(item.m_label) == label_lower; });

    if (it != m_items.end())
    {
        auto index = std::distance(m_items.begin(), it);
        return static_cast<int>(index);
    }

    return std::nullopt;
}


void ListView::SetItems(std::vector<ListViewItem>&& items)
{
    m_items = std::move(items);
    m_num_items = static_cast<int>(m_items.size());

    // Calculate how many items on a row, and how many rows, and set the required scrollbar size
    m_nAcross = m_nWidth / ITEM_SIZE;
    m_nDown = static_cast<int>((m_items.size() + m_nAcross - 1) / m_nAcross);
    m_pScrollBar->SetMaxPos(m_nDown * ITEM_SIZE);

    Select(0);
}

void ListView::DrawItem(FrameBuffer& fb, int nItem_, int nX_, int nY_, const ListViewItem* pItem_)
{
    // If this is the selected item, draw a box round it (darkened if the control isn't active)
    if (nItem_ == m_nSelected)
    {
        if (IsActive())
            fb.FillRect(nX_ + 1, nY_ + 1, ITEM_SIZE - 2, ITEM_SIZE - 2, BLUE_2);

        fb.FrameRect(nX_, nY_, ITEM_SIZE, ITEM_SIZE, IsActive() ? GREY_7 : GREY_5, true);
    }

    auto& icon = pItem_->m_pIcon.get();
    fb.DrawImage(nX_ + (ITEM_SIZE - ICON_SIZE) / 2, nY_ + m_nItemOffset + 5, ICON_SIZE, ICON_SIZE,
        reinterpret_cast<const uint8_t*>(icon.abData), icon.abPalette);

    auto& label = pItem_->m_label;
    if (!label.empty())
    {
        fb.SetFont(sPropFont);

        int nLine = 0;
        auto pcsz = label.c_str();
        const char* pszStart = pcsz, * pszBreak = nullptr;
        char szLines[2][64], sz[64];
        *szLines[0] = *szLines[1] = '\0';

        // Spread the item text over up to 2 lines
        while (nLine < 2)
        {
            size_t uLen = pcsz - pszStart;
            strncpy(sz, pszStart, uLen)[uLen] = '\0';

            if (GetTextWidth(sz) >= (ITEM_SIZE - 9))
            {
                sz[uLen - 1] = '\0';
                pcsz--;

                if (nLine == 1 || !pszBreak)
                {
                    if (nLine == 1)
                        strcpy(sz + (pcsz - pszStart - 2), "...");
                    strcpy(szLines[nLine++], sz);
                    pszStart = pcsz;
                }
                else
                {
                    if (nLine == 1)
                        strcpy(sz + (pszBreak - pszStart - 2), "...");
                    else
                        sz[pszBreak - pszStart] = '\0';

                    strcpy(szLines[nLine++], sz);
                    pszStart = pszBreak + (*pszBreak == ' ');
                    pszBreak = nullptr;
                }
            }

            // Check for a break point position
            if ((*pcsz == '.' || *pcsz == ' ') && pcsz != pszStart)
                pszBreak = pcsz;

            if (!*pcsz++)
            {
                if (nLine < 2)
                    strcpy(szLines[nLine++], pszStart);
                break;
            }
        }

        // Output the two text lines using the small font, each centralised below the icon
        nY_ += m_nItemOffset + 42;

        fb.DrawString(nX_ + (ITEM_SIZE - fb.StringWidth(szLines[0])) / 2, nY_, szLines[0]);
        fb.DrawString(nX_ + (ITEM_SIZE - fb.StringWidth(szLines[1])) / 2, nY_ + 12, szLines[1]);

        fb.SetFont(sGUIFont);
    }
}

// Erase the control background
void ListView::EraseBackground(FrameBuffer& fb)
{
    fb.FillRect(m_nX, m_nY, m_nWidth, m_nHeight, BLUE_1);
}

void ListView::Draw(FrameBuffer& fb)
{
    // Fill the main background of the control
    EraseBackground(fb);

    // Fetch the current scrollbar position
    int nScrollPos = m_pScrollBar->GetPos();

    // Calculate the range of icons that are visible and need drawing
    auto num_items = static_cast<int>(m_items.size());
    int nStart = nScrollPos / ITEM_SIZE * m_nAcross, nOffset = nScrollPos % ITEM_SIZE;
    int nDepth = (m_nHeight + nOffset + ITEM_SIZE - 1) / ITEM_SIZE;
    int nEnd = std::min(num_items, nStart + m_nAcross * nDepth);

    // Clip to the main control, to keep partly drawn icons within our client area
    fb.ClipTo(m_nX, m_nY, m_nWidth, m_nHeight);

    for (int i = nStart; i < nEnd; ++i)
    {
        if (auto pItem = GetItem(i))
        {
            auto x = m_nX + ((i % m_nAcross) * ITEM_SIZE);
            auto y = m_nY + (((i - nStart) / m_nAcross) * ITEM_SIZE) - nOffset;
            DrawItem(fb, i, x, y, pItem);
        }
        else
        {
            break;
        }
    }

    // Restore the default clip area
    fb.ClipNone();
    Window::Draw(fb);
}


bool ListView::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    static std::string s_prefix;

    // Give the scrollbar first look at the message, but prevent it remaining active
    bool fRet = Window::OnMessage(nMessage_, nParam1_, nParam2_);
    m_pActive = nullptr;
    if (fRet)
        return fRet;

    switch (nMessage_)
    {
    case GM_CHAR:
        if (!IsActive())
            break;

        switch (nParam1_)
        {
        case HK_LEFT:   Select(m_nSelected - 1);  break;
        case HK_RIGHT:  Select(m_nSelected + 1);  break;

        case HK_UP:
            // Only move up if we're not already on the top line
            if (m_nSelected >= m_nAcross)
                Select(m_nSelected - m_nAcross);
            break;

        case HK_DOWN:
        {
            // Calculate the row the new item would be on
            int nNewRow = std::min(m_nSelected + m_nAcross, m_num_items - 1) / m_nAcross;

            // Only move down if we're not already on the bottom row
            if (nNewRow != m_nSelected / m_nAcross)
                Select(m_nSelected + m_nAcross);
            break;
        }

        // Move up one screen full, staying on the same column
        case HK_PGUP:
        {
            int nUp = std::min(m_nHeight / ITEM_SIZE, m_nSelected / m_nAcross) * m_nAcross;
            Select(m_nSelected - nUp);
            break;
        }

        // Move down one screen full, staying on the same column
        case HK_PGDN:
        {
            int nDown = std::min(m_nHeight / ITEM_SIZE, (m_num_items - m_nSelected - 1) / m_nAcross) * m_nAcross;
            Select(m_nSelected + nDown);
            break;
        }

        // Move to first item
        case HK_HOME:
            Select(0);
            break;

        // Move to last item
        case HK_END:
            Select(m_num_items - 1);
            break;

        // Return selects the current item - like a double-click
        case HK_RETURN:
            s_prefix.clear();
            NotifyParent(1);
            break;

        default:
        {
            static uint32_t dwLastChar = 0;
            auto dwNow = OSD::GetTime();

            // Clear the buffer on any non-printing characters or if too long since the last one
            if (nParam1_ < ' ' || nParam1_ > 0x7f || (dwNow - dwLastChar > 1000))
                s_prefix.clear();

            // Ignore non-printable characters, or if the buffer is full
            if (nParam1_ < ' ' || nParam1_ > 0x7f)
                return false;

            // Ignore duplicates of the same first character, to skip to the next match
            if (!(s_prefix.length() == 1 && s_prefix.front() == nParam1_))
            {
                s_prefix += std::tolower(nParam1_);
                dwLastChar = dwNow;
            }

            // Look for a match, starting *after* the current selection if this is the first character
            auto start_index = (GetSelected() + (s_prefix.length() == 1)) % static_cast<int>(m_items.size());

            auto prefix_pred = [&](const ListViewItem& item) { return tolower(item.m_label.substr(0, s_prefix.length())) == s_prefix; };
            auto it = std::find_if(m_items.begin() + start_index, m_items.end(), prefix_pred);
            if (it == m_items.end())
            {
                it = std::find_if(m_items.begin(), m_items.begin() + start_index, prefix_pred);
            }

            if (it != m_items.end())
            {
                auto index = static_cast<int>(std::distance(m_items.begin(), it));
                Select(index);
            }
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
        m_nHoverItem = (nAcross < m_nAcross && nHoverItem < m_num_items) ? nHoverItem : -1;

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

FileView::FileView(Window* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
    : ListView(pParent_, nX_, nY_, nWidth_, nHeight_)
{
}

FileView::~FileView()
{
    delete[] m_pszFilter;
}


bool FileView::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    bool fRet = ListView::OnMessage(nMessage_, nParam1_, nParam2_);

    // Backspace moves up a directory
    if (!fRet && nMessage_ == GM_CHAR && nParam1_ == HK_BACKSPACE)
    {
        if (auto index = FindItem(".."))
        {
            Select(*index);
            NotifyParent(1);
            fRet = true;
        }
    }

    return fRet;
}

void FileView::NotifyParent(int nParam_)
{
    const ListViewItem* pItem = GetItem();

    if (pItem && nParam_)
    {
        // Double-clicking to open a directory?
        if (std::addressof(pItem->m_pIcon.get()) == std::addressof(sFolderIcon))
        {
            auto path = m_path;

            if (pItem->m_label == "..")
            {
                if (path != path.root_path())
                    path = path.parent_path();
                else
                    path.clear();
            }
            else
            {
                path = path / pItem->m_label;
            }

            // Make sure we have access to the path before setting it
            if (path.empty() || OSD::CheckPathAccess(path.string()))
            {
                m_path = path;
                Refresh();
            }
            else
            {
                auto body = fmt::format("{}{}\n\nCan't access directory.", pItem->m_label, PATH_SEPARATOR);
                new MsgBox(this, body, "Access Denied", mbError);
            }
        }
    }

    // Base handling, to notify parent
    Window::NotifyParent(nParam_);
}

// Determine an appropriate icon for the supplied file name/extension
const GUI_ICON& FileView::GetFileIcon(const std::string& path_str)
{
    auto path = fs::path(path_str);
    auto file_ext = tolower(path.extension().string());

    static const std::set<std::string_view> archive_exts{ ".zip", ".gz" };
    bool is_archive = archive_exts.find(file_ext) != archive_exts.end();
    if (is_archive)
    {
        path = path.replace_extension();
        file_ext = tolower(path.extension().string());
    }

    static const std::set<std::string_view> disk_exts{ ".dsk", ".sad", ".sbt", ".mgt", ".img", ".cpm" };
    bool is_disk_image = disk_exts.find(file_ext) != disk_exts.end();

    return is_archive ? sCompressedIcon : is_disk_image ? sDiskIcon : sDocumentIcon;
}


// Get the full path of the current item
std::string FileView::GetFullPath() const
{
    const ListViewItem* pItem{};
    if (m_path.empty() || !(pItem = GetItem()))
        return "";

    return (m_path / pItem->m_label).string();
}

// Set a new path to browse
void FileView::SetPath(const std::string& filepath)
{
    auto path = fs::path(filepath);
    auto filename = path.filename().string();
    m_path = path.parent_path();

    Refresh();

    if (auto index = FindItem(filename))
        Select(*index);
}

// Set a new file filter
void FileView::SetFilter(const char* pcszFilter_)
{
    if (pcszFilter_)
    {
        delete[] m_pszFilter;
        strcpy(m_pszFilter = new char[strlen(pcszFilter_) + 1], pcszFilter_);
        Refresh();
    }
}

// Set whether hidden files should be shown
void FileView::ShowHidden(bool fShow_)
{
    // Update the option and refresh the file list
    m_fShowHidden = fShow_;
    Refresh();
}


// Populate the list view with items from the path matching the current file filter
void FileView::Refresh()
{
    if (!m_pszFilter)
        return;

    auto pItem = GetItem();
    auto label = pItem ? pItem->m_label : "";

    std::vector<ListViewItem> items;

    // An empty path gives a virtual drive list (only possible on DOS/Win32)
#ifdef _WIN32
    if (m_path.empty())
    {
        for (char chDrive = 'A'; chDrive <= 'Z'; ++chDrive)
        {
            auto root_path = fmt::format("{}:\\", chDrive);

            if (OSD::CheckPathAccess(root_path))
            {
                items.emplace_back(sFolderIcon, root_path);
            }
        }
    }
    else
#endif
    {
        auto filters = to_set(split(m_pszFilter, ';'));

        for (auto& entry : fs::directory_iterator(m_path))
        {
            auto file_path = entry.path();// m_path / entry->d_name;
            auto file_name = file_path.filename().string();
            auto file_ext = tolower(file_path.extension().string());

            if (file_name == "." || file_name == "..")
                continue;

            if (!m_fShowHidden && OSD::IsHidden(file_path.string()))
                continue;

            if (entry.is_regular_file() && !filters.empty())
            {
                if (filters.find(file_ext) == filters.end())
                    continue;
            }

            // Ignore anything that isn't a directory or a block device (or a symbolic link to one)
            else if (!entry.is_directory() && !entry.is_block_file())
            {
                continue;
            }

            items.emplace_back(
                entry.is_directory() ? sFolderIcon : entry.is_block_file() ? sMiscIcon : GetFileIcon(file_name),
                file_name);
        }

        // Sort by type (directories first) then filename.
        std::sort(items.begin(), items.end(),
            [](auto& lhs, auto& rhs)
            {
                auto ltype = (std::addressof(lhs.m_pIcon.get()) != std::addressof(sFolderIcon));
                auto rtype = (std::addressof(rhs.m_pIcon.get()) != std::addressof(sFolderIcon));
                return (ltype < rtype) || (ltype == rtype && lhs.m_label < rhs.m_label);
            });

        items.emplace(items.begin(), sFolderIcon, "..");
    }

    SetItems(std::move(items));

    if (!label.empty())
    {
        if (auto index = FindItem(label))
            Select(*index);
    }
}

////////////////////////////////////////////////////////////////////////////////

IconControl::IconControl(Window* pParent_, int nX_, int nY_, const GUI_ICON* pIcon_)
    : Window(pParent_, nX_, nY_, 0, 0, ctImage), m_pIcon(pIcon_)
{
}

void IconControl::Draw(FrameBuffer& fb)
{
    uint8_t abGreyed[ICON_PALETTE_SIZE];

    // Is the control to be drawn disabled?
    if (!IsEnabled())
    {
        // Make a copy of the palette
        memcpy(abGreyed, m_pIcon->abPalette, sizeof(m_pIcon->abPalette));

        // Grey the icon by using shades of grey with approximate intensity
        for (int i = 0; i < ICON_PALETTE_SIZE; i++)
        {
            // Only change non-zero colours, to keep the background transparent
            if (abGreyed[i])
                abGreyed[i] = GREY_1 + (abGreyed[i] & 0x07);
        }
    }

    // Draw the icon, using the greyed palette if disabled
    fb.DrawImage(m_nX, m_nY, ICON_SIZE, ICON_SIZE, reinterpret_cast<const uint8_t*>(m_pIcon->abData),
        IsEnabled() ? m_pIcon->abPalette : abGreyed);
}

////////////////////////////////////////////////////////////////////////////////

FrameControl::FrameControl(Window* pParent_, int nX_, int nY_, int nWidth_, int nHeight_, uint8_t bColour_/*=WHITE*/, uint8_t bFill_/*=0*/)
    : Window(pParent_, nX_, nY_, nWidth_, nHeight_, ctFrame), m_bColour(bColour_), m_bFill(bFill_)
{
}

void FrameControl::Draw(FrameBuffer& fb)
{
    // If we've got a fill colour, use it now
    if (m_bFill)
        fb.FillRect(m_nX, m_nY, m_nWidth, m_nHeight, m_bFill);

    // Draw the frame around the area
    fb.FrameRect(m_nX, m_nY, m_nWidth, m_nHeight, m_bColour, true);
}

////////////////////////////////////////////////////////////////////////////////

const int TITLE_TEXT_COLOUR = WHITE;
const int TITLE_BACK_COLOUR = BLUE_3;
const int DIALOG_BACK_COLOUR = BLUE_2;
const int DIALOG_FRAME_COLOUR = GREY_7;

const int TITLE_HEIGHT = 4 + Font::CHAR_HEIGHT + 5;


Dialog::Dialog(Window* pParent_, int nWidth_, int nHeight_, const std::string& caption)
    : Window(pParent_, 0, 0, nWidth_, nHeight_, ctDialog), m_nTitleColour(TITLE_BACK_COLOUR), m_nBodyColour(DIALOG_BACK_COLOUR)
{
    SetText(caption);

    Centre();
    Window::Activate();

    GUI::s_dialogStack.push(this);
}

Dialog::~Dialog()
{
    GUI::s_dialogStack.pop();
}

bool Dialog::IsActiveDialog() const
{
    return !GUI::s_dialogStack.empty() && GUI::s_dialogStack.top() == this;
}

void Dialog::Centre()
{
    // Position the window slightly above centre
    Move((Frame::Width() - m_nWidth) >> 1, ((Frame::Height() - m_nHeight) * 9 / 10) >> 1);
}

// Activating the dialog will activate the first child control that accepts focus
void Dialog::Activate()
{
    // If there's no active window on the dialog, activate the tab-stop
    if (m_pChildren)
    {
        // Look for the first control with a tab-stop
        Window* p;
        for (p = m_pChildren; p && !p->IsTabStop(); p = p->GetNext());

        // If we found one, activate it
        if (p)
            p->Activate();
    }
}

bool Dialog::HitTest(int nX_, int nY_)
{
    // The caption is outside the original dimensions, so we need a special test
    return (nX_ >= m_nX - 1) && (nX_ < (m_nX + m_nWidth + 1)) && (nY_ >= m_nY - TITLE_HEIGHT) && (nY_ < (m_nY + 1));
}

// Fill the dialog background
void Dialog::EraseBackground(FrameBuffer& fb)
{
    fb.FillRect(m_nX, m_nY, m_nWidth, m_nHeight, IsActiveDialog() ? m_nBodyColour : (m_nBodyColour & ~0x7));
}

void Dialog::Draw(FrameBuffer& fb)
{
    // Make sure there's always an active control
    if (!m_pActive)
        Activate();

#if 0
    // Debug crosshairs to track mapped GUI cursor position
    fb.DrawLine(GUI::s_nX, 0, 0, fb.Height(), WHITE);
    fb.DrawLine(0, GUI::s_nY, fb.Width(), 0, WHITE);
#endif

    // Fill dialog background and draw a 3D frame
    EraseBackground(fb);
    fb.FrameRect(m_nX - 2, m_nY - TITLE_HEIGHT - 2, m_nWidth + 3, m_nHeight + TITLE_HEIGHT + 3, DIALOG_FRAME_COLOUR);
    fb.FrameRect(m_nX - 1, m_nY - TITLE_HEIGHT - 1, m_nWidth + 3, m_nHeight + TITLE_HEIGHT + 3, DIALOG_FRAME_COLOUR - 2);
    fb.Plot(m_nX + m_nWidth + 1, m_nY - TITLE_HEIGHT - 2, DIALOG_FRAME_COLOUR);
    fb.Plot(m_nX - 2, m_nY + m_nHeight + 1, DIALOG_FRAME_COLOUR - 2);

    // Fill caption background and draw the diving line at the bottom of it
    fb.FillRect(m_nX, m_nY - TITLE_HEIGHT, m_nWidth, TITLE_HEIGHT - 1, IsActiveDialog() ? m_nTitleColour : (m_nTitleColour & ~0x7));
    fb.DrawLine(m_nX, m_nY - 1, m_nWidth, 0, DIALOG_FRAME_COLOUR);

    // Draw caption text on the left side
    fb.SetFont(sSpacedGUIFont);
    fb.DrawString(m_nX + 5, m_nY - TITLE_HEIGHT + 5, TITLE_TEXT_COLOUR, GetText());
    fb.DrawString(m_nX + 5 + 1, m_nY - TITLE_HEIGHT + 5, TITLE_TEXT_COLOUR, GetText());
    fb.SetFont(sGUIFont);

    // Call the base to draw any child controls
    Window::Draw(fb);
}


bool Dialog::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    // Pass to the active control, then the base implementation for the remaining child controls
    if (Window::OnMessage(nMessage_, nParam1_, nParam2_))
        return true;

    switch (nMessage_)
    {
    case GM_CHAR:
    {
        switch (nParam1_)
        {
            // Tab or Shift-Tab moves between any controls that are tab-stops
        case HK_TAB:
        {
            // Loop until we find an enabled control to stop on
            while (m_pActive)
            {
                m_pActive = nParam2_ ? m_pActive->GetPrev(true) : m_pActive->GetNext(true);

                // Stop once we find a suitable control
                if (m_pActive->IsTabStop() && m_pActive->IsEnabled())
                {
                    m_pActive->Activate();
                    break;
                }
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

            for (Window* p = m_pActive; p; )
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
        case HK_ESC:
            Destroy();
            break;
        }
        break;
    }

    case GM_BUTTONDOWN:
    case GM_BUTTONDBLCLK:
        // Button down on the caption?
        if (IsOver() && nParam2_ < (m_nY + TITLE_HEIGHT))
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
            Move(nParam1_ - m_nDragX, nParam2_ - m_nDragY);
            return true;
        }
        break;
    }

    // Absorb all other messages to prevent any parent processing
    return true;
}

////////////////////////////////////////////////////////////////////////////////

const int MSGBOX_NORMAL_COLOUR = BLUE_2;
const int MSGBOX_ERROR_COLOUR = RED_2;
const int MSGBOX_BUTTON_SIZE = 50;
const int MSGBOX_LINE_HEIGHT = 15;
const int MSGBOX_GAP = 13;

MsgBox::MsgBox(Window* pParent_, const std::string& body, const std::string& caption, int nFlags_)
    : Dialog(pParent_, 0, 0, caption), m_lines(split(body, '\n'))
{
    // We need to be recognisably different from a regular dialog, despite being based on one
    m_nType = ctMessageBox;

    auto it = std::max_element(m_lines.begin(), m_lines.end(),
        [&](auto& a, auto& b) { return GetTextWidth(a) < GetTextWidth(b); });

    auto maxlen = (it != m_lines.end()) ? GetTextWidth(*it) : 0;
    m_nWidth = std::max(m_nWidth, maxlen);

    // Calculate the text area height
    m_nHeight = static_cast<int>(MSGBOX_LINE_HEIGHT * m_lines.size());

    // Work out the icon to use, if any
    const GUI_ICON* apIcons[] = { nullptr, &sInformationIcon, &sWarningIcon, &sErrorIcon };
    const GUI_ICON* pIcon = apIcons[(nFlags_ & 0x30) >> 4];

    // Calculate the text area height, and increase to allow space for the icon if necessary
    if (pIcon)
    {
        // Update the body width and height to allow for the icon
        m_nWidth += ICON_SIZE + MSGBOX_GAP / 2;

        // Add the icon to the top-left of the dialog
        m_pIcon = new IconControl(this, MSGBOX_GAP / 2, MSGBOX_GAP / 2, pIcon);
    }

    // Work out the width of the button block, which depends on how many buttons are needed
    int nButtons = 1;
    int nButtonWidth = ((MSGBOX_BUTTON_SIZE + MSGBOX_GAP) * nButtons) - MSGBOX_GAP;

    // Allow for a surrounding border
    m_nWidth += MSGBOX_GAP << 1;
    m_nHeight += MSGBOX_GAP << 1;

    // Centre the button block at the bottom of the dialog [ToDo: add remaining buttons]
    int nButtonOffset = (m_nWidth - nButtonWidth) >> 1;
    (new TextButton(this, nButtonOffset, m_nHeight, "OK", MSGBOX_BUTTON_SIZE))->Activate();

    // Allow for the button height and a small gap underneath it
    m_nHeight += BUTTON_HEIGHT + MSGBOX_GAP / 2;

    // Centralise the dialog on the screen by default
    int nX = (Frame::Width() - m_nWidth) >> 1, nY = (Frame::Height() - m_nHeight) * 2 / 5;
    Move(nX, nY);

    // Error boxes are shown in red
    if (pIcon == &sInformationIcon)
        Dialog::SetColours(MSGBOX_NORMAL_COLOUR + 2, MSGBOX_NORMAL_COLOUR);
    else
        Dialog::SetColours(MSGBOX_ERROR_COLOUR + 1, MSGBOX_ERROR_COLOUR);
}

void MsgBox::Draw(FrameBuffer& fb)
{
    Dialog::Draw(fb);

    // Calculate the x-offset to the body text
    int nX = m_nX + MSGBOX_GAP + (m_pIcon ? ICON_SIZE + MSGBOX_GAP / 2 : 0);

    // Draw each line in the body text
    auto index = 0;
    for (auto& line : m_lines)
    {
        fb.DrawString(nX, m_nY + MSGBOX_GAP + (MSGBOX_LINE_HEIGHT * index++), line);
    }
}
