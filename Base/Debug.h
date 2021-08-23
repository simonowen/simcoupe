// Part of SimCoupe - A SAM Coupe emulator
//
// Debug.h: Integrated Z80 debugger
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

#include "Breakpoint.h"
#include "GUI.h"
#include "FrameBuffer.h"

namespace Debug
{
bool Start(std::optional<int> bp_index = std::nullopt);
void Stop();
void FrameEnd();
void Refresh();
void UpdateSymbols();

void OnRet();
void RetZHook();

bool IsActive();
void AddTraceRecord();
}

enum class ViewType { Dis, Txt, Hex, Gfx, Bpt, Trc };

class View : public Window
{
public:
    View(Window* pParent_)
        : Window(pParent_, 4, 5, pParent_->GetWidth() - 8, pParent_->GetHeight() - 10 - 12), m_wAddr(0) { }

public:
    bool OnMessage(int /*nMessage_*/, int /*nParam1_*/, int /*nParam2_*/) override { return false; }

    uint16_t GetAddress() const;
    virtual void SetAddress(uint16_t wAddr_, bool /*fForceTop_*/ = false);
    virtual bool cmdNavigate(int nKey_, int nMods_);

private:
    uint16_t m_wAddr = 0;
};

class TextView : public View
{
public:
    TextView(Window* pParent_);

public:
    int GetLines() const { return m_nLines; }
    void SetLines(int nLines_) { m_nLines = nLines_; }
    int GetTopLine() const { return m_nTopLine; }

    virtual void DrawLine(FrameBuffer& fb, int nX_, int nY_, int nLine_) = 0;
    virtual bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;
    virtual bool cmdNavigate(int nKey_, int nMods_) override;
    virtual void OnDblClick(int /*nLine_*/) { }
    virtual void OnDelete() { }

protected:
    void Draw(FrameBuffer& fb) override;

private:
    int m_nLines = 0;    // Lines of content to display
    int m_nTopLine = 0;  // Line number of first visible line
    int m_nRows = 0;     // Maximum visible rows
};

class DisView : public View
{
public:
    static const unsigned int INVALID_TARGET = 0U - 1;

public:
    DisView(Window* pParent_);
    DisView(const DisView&) = delete;
    void operator= (const DisView&) = delete;
    ~DisView() { delete[] m_pszData; }

public:
    void SetAddress(uint16_t wAddr_, bool fForceTop_ = false) override;
    void Draw(FrameBuffer& fb) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

public:
    static void DrawRegisterPanel(FrameBuffer& fb, int nX_, int nY_);

protected:
    bool SetCodeTarget();
    bool SetDataTarget();
    bool cmdNavigate(int nKey_, int nMods_) override;

private:
    unsigned int m_uRows = 0, m_uColumns = 0;
    unsigned int m_uCodeTarget = INVALID_TARGET;
    unsigned int m_uDataTarget = INVALID_TARGET;
    std::string m_data_target;
    char* m_pszData = nullptr;

    static uint16_t s_wAddrs[];
    static bool m_fUseSymbols;
};


class TxtView : public View
{
public:
    TxtView(Window* pParent_);
    TxtView(const TxtView&) = delete;
    void operator= (const TxtView&) = delete;
    ~TxtView() { delete[] m_pszData; }

public:
    bool GetAddrPosition(uint16_t wAddr_, int& x_, int& y_);
    void SetAddress(uint16_t wAddr_, bool fForceTop_ = false) override;
    void Draw(FrameBuffer& fb) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    bool cmdNavigate(int nKey_, int nMods_) override;

private:
    int m_nRows = 0, m_nColumns = 0;
    char* m_pszData = nullptr;
    std::vector<uint16_t> m_aAccesses{};

    bool m_fEditing = false;
    uint16_t m_wEditAddr = 0;
};

class HexView : public View
{
public:
    HexView(Window* pParent_);
    HexView(const HexView&) = delete;
    void operator= (const HexView&) = delete;
    ~HexView() { delete[] m_pszData; }

public:
    bool GetAddrPosition(uint16_t wAddr_, int& x_, int& y_, int& textx_);
    void SetAddress(uint16_t wAddr_, bool fForceTop_ = false) override;
    void Draw(FrameBuffer& fb) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    bool cmdNavigate(int nKey_, int nMods_) override;

private:
    int m_nRows = 0, m_nColumns = 0;
    char* m_pszData = nullptr;
    std::vector<uint16_t> m_aAccesses{};

    bool m_fEditing = false, m_fRightNibble = false;
    uint16_t m_wEditAddr = 0;
};

/*
class MemView : public View
{
    public:
        MemView (Window* pParent_);

    public:
        void SetAddress (uint16_t wAddr_, bool fForceTop_=false) override;
        void Draw (FrameBuffer& fb) override;
};
*/

class GfxView : public View
{
public:
    GfxView(Window* pParent_);
    GfxView(const GfxView&) = delete;
    void operator= (const GfxView&) = delete;
    ~GfxView() { delete[] m_pbData; }

public:
    void SetAddress(uint16_t wAddr_, bool fForceTop_ = false) override;
    void Draw(FrameBuffer& fb) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    bool cmdNavigate(int nKey_, int nMods_) override;

protected:
    bool m_fGrid = true;
    unsigned int m_uStrips = 0, m_uStripWidth = 0, m_uStripLines = 0;
    uint8_t* m_pbData = nullptr;

    static unsigned int s_uWidth, s_uZoom;
    static int s_mode;
};

class BptView : public View
{
public:
    BptView(Window* pParent_);
    BptView(const BptView&) = delete;
    void operator= (const BptView&) = delete;
    ~BptView() { delete[] m_pszData; }

public:
    void SetAddress(uint16_t wAddr_, bool fForceTop_ = false) override;
    void Draw(FrameBuffer& fb) override;
    bool OnMessage(int nMessage_, int nParam1_, int nParam2_) override;

protected:
    bool cmdNavigate(int nKey_, int nMods_) override;

private:
    int m_nRows = 0, m_nLines = 0, m_nTopLine = 0;
    int m_nActive = -1;
    char* m_pszData = nullptr;
};

class TrcView final : public TextView
{
public:
    TrcView(Window* pParent_);
    TrcView(const TrcView&) = delete;
    void operator= (const TrcView&) = delete;

public:
    void DrawLine(FrameBuffer& fb, int nX_, int nY_, int nLine_) override;
    bool cmdNavigate(int nKey_, int nMods_) override;
    void OnDblClick(int nLine_) override;
    void OnDelete() override;

private:
    bool m_double_regs = true;
    bool m_use_symbols = true;
};


class Debugger final : public Dialog
{
public:
    Debugger(std::optional<int> bp_index = std::nullopt);
    Debugger(const Debugger&) = delete;
    void operator= (const Debugger&) = delete;
    ~Debugger();

    bool OnMessage(int nMessage_, int nParam1_ = 0, int nParam2_ = 0) override;
    void OnNotify(Window* pWindow_, int nParam_) override;
    void EraseBackground(FrameBuffer& fb) override;
    void Draw(FrameBuffer& fb) override;

    void Refresh();
    void SetSubTitle(const std::string& sub_title);
    void SetAddress(uint16_t wAddr_, bool fForceTop_ = false);
    void SetView(ViewType nView);
    void SetStatus(const std::string& status, bool fOneShot_ = false, std::shared_ptr<Font> font = {});
    void SetStatusByte(uint16_t addr);
    bool Execute(const std::string& cmdline);

protected:
    View* m_pView = nullptr;
    EditControl* m_pCommandEdit = nullptr;
    TextControl* m_pStatus = nullptr;

    std::string m_sStatus{};

    static bool s_fTransparent;
};


typedef bool (*PFNINPUTPROC)(const Expr& expr);

class InputDialog final : public Dialog
{
public:
    InputDialog(Window* pParent_, const std::string& caption, const std::string& prompt, PFNINPUTPROC pfn_);
    InputDialog(const InputDialog&) = delete;
    void operator= (const InputDialog&) = delete;

public:
    void OnNotify(Window* pWindow_, int nParam_) override;

protected:
    EditControl* m_pInput = nullptr;
    PFNINPUTPROC m_pfnNotify = nullptr;
};
