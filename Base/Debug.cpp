// Part of SimCoupe - A SAM Coupe emulator
//
// Debug.cpp: Integrated Z80 debugger
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

#include "SimCoupe.h"
#include "Debug.h"

#include "CPU.h"
#include "Disassem.h"
#include "Events.h"
#include "Frame.h"
#include "Keyboard.h"
#include "Memory.h"
#include "Options.h"
#include "Symbol.h"


// SAM BASIC keywords tokens
static const std::vector<std::string_view> basic_keywords
{
    "PI", "RND", "POINT", "FREE", "LENGTH", "ITEM", "ATTR", "FN", "BIN",
    "XMOUSE", "YMOUSE", "XPEN", "YPEN", "RAMTOP", "-", "INSTR", "INKEY$",
    "SCREEN$", "MEM$", "-", "PATH$", "STRING$", "-", "-", "SIN", "COS", "TAN",
    "ASN", "ACS", "ATN", "LN", "EXP", "ABS", "SGN", "SQR", "INT", "USR", "IN",
    "PEEK", "DPEEK", "DVAR", "SVAR", "BUTTON", "EOF", "PTR", "-", "UDG", "-",
    "LEN", "CODE", "VAL$", "VAL", "TRUNC$", "CHR$", "STR$", "BIN$", "HEX$",
    "USR$", "-", "NOT", "-", "-", "-", "MOD", "DIV", "BOR", "-", "BAND", "OR",
    "AND", "<>", "<=", ">=", "USING", "WRITE", "AT", "TAB", "OFF", "WHILE",
    "UNTIL", "LINE", "THEN", "TO", "STEP", "DIR", "FORMAT", "ERASE", "MOVE",
    "SAVE", "LOAD", "MERGE", "VERIFY", "OPEN", "CLOSE", "CIRCLE", "PLOT",
    "LET", "BLITZ", "BORDER", "CLS", "PALETTE", "PEN", "PAPER", "FLASH",
    "BRIGHT", "INVERSE", "OVER", "FATPIX", "CSIZE", "BLOCKS", "MODE", "GRAB",
    "PUT", "BEEP", "SOUND", "NEW", "RUN", "STOP", "CONTINUE", "CLEAR",
    "GO TO", "GO SUB", "RETURN", "REM", "READ", "DATA", "RESTORE", "PRINT",
    "LPRINT", "LIST", "LLIST", "DUMP", "FOR", "NEXT", "PAUSE", "DRAW",
    "DEFAULT", "DIM", "INPUT", "RANDOMIZE", "DEF FN", "DEF KEYCODE",
    "DEF PROC", "END PROC", "RENUM", "DELETE", "REF", "COPY", "-", "KEYIN",
    "LOCAL", "LOOP IF", "DO", "LOOP", "EXIT IF", "IF", "IF", "ELSE", "ELSE",
    "END IF", "KEY", "ON ERROR", "ON", "GET", "OUT", "POKE", "DPOKE",
    "RENAME", "CALL", "ROLL", "SCROLL", "SCREEN", "DISPLAY", "BOOT", "LABEL",
    "FILL", "WINDOW", "AUTO", "POP", "RECORD", "DEVICE", "PROTECT", "HIDE",
    "ZAP", "POW", "BOOM", "ZOOM", "-", "-", "-", "-", "-", "-", "-", "-", "INK"
};

constexpr auto BASE_KEYWORD = 60;

// Changed value colour (light red)
#define CHG_COL  'r'

constexpr auto ROW_GAP = 2;
constexpr auto ROW_HEIGHT = ROW_GAP + Font::FIXED_FONT_HEIGHT + ROW_GAP;
constexpr auto CHR_WIDTH = Font::FIXED_FONT_WIDTH + Font::CHAR_SPACING;

struct TRACEDATA
{
    uint16_t wPC;
    uint8_t abInstr[MAX_Z80_INSTR_LEN];
    sam_cpu::z80_state regs;
};


Debugger* pDebugger;

// Stack position used to track stepping out
int nStepOutSP = -1;

// Last position of debugger window and last register values
int nDebugX, nDebugY;
sam_cpu::z80_state sLastRegs, sCurrRegs;
uint8_t bLastStatus;
uint32_t dwLastCycle;
int nLastFrames;
auto nLastView = ViewType::Dis;
uint16_t wLastAddr;

// Instruction tracing
#define TRACE_SLOTS 1000
TRACEDATA aTrace[TRACE_SLOTS];
int nNumTraces;


namespace Debug
{

// Activate the debug GUI, if not already active
bool Start(std::optional<int> bp_index)
{
    Memory::full_contention = true;
    Memory::UpdateContention();

    // Reset the last entry counters, unless we're started from a triggered breakpoint
    if (!bp_index.has_value() && nStepOutSP == -1)
    {
        nLastFrames = 0;
        dwLastCycle = CPU::frame_cycles;

        sLastRegs = sCurrRegs = cpu;
        bLastStatus = IO::State().status;

        // If there's no breakpoint set any existing trace is meaningless
        if (Breakpoint::breakpoints.empty())
        {
            // Set the previous entry to have the current register values
            aTrace[nNumTraces = 0].regs = cpu;

            // Add the current location as the only entry
            AddTraceRecord();
        }

        UpdateSymbols();
    }

    // Stop any existing debugger instance
    GUI::Stop();

    // Create the main debugger window, passing any breakpoint
    if (!GUI::Start(pDebugger = new Debugger(bp_index)))
        pDebugger = nullptr;

    return true;
}

void Stop()
{
    if (pDebugger)
    {
        pDebugger->Destroy();
        pDebugger = nullptr;
    }
}

void FrameEnd()
{
    nLastFrames++;
}

void Refresh()
{
    if (pDebugger)
    {
        // Set the address without forcing it to the top of the window
        pDebugger->SetAddress((nLastView == ViewType::Dis) ? cpu.get_pc() : wLastAddr, false);
    }

    if (auto bp_index = Breakpoint::Hit())
    {
        Debug::Start(bp_index);
    }
}

void UpdateSymbols()
{
    fs::path map_path;
    if (GetOption(drive1) == drvFloppy && pFloppy1->HasDisk())
        map_path = fs::path(pFloppy1->DiskPath()).replace_extension(".map");

    Symbol::Update(map_path);
}

// Called on every RETurn, for step-out implementation
void OnRet()
{
    // Step-out in progress?
    if (nStepOutSP != -1)
    {
        // If the stack is at or just above the starting position, it should mean we've returned
        // Allow some generous slack for data that may have been on the stack above the address
        if ((cpu.get_sp() - nStepOutSP) >= 0 && (cpu.get_sp() - nStepOutSP) < 64)
            Start();
    }
}

void RetZHook()
{
    // Are we in in HDNSTP in ROM1, about to start an auto-executing code file?
    if (cpu.get_pc() == 0xe294 && GetSectionPage(Section::D) == ROM1 && !(cpu.get_f() & cpu.zf_mask))
    {
        // If the option is enabled, set a temporary breakpoint for the start
        if (GetOption(breakonexec))
            Breakpoint::AddTemp(nullptr, Expr::Compile("autoexec"));
    }
}


// Return whether the debug GUI is active
bool IsActive()
{
    return pDebugger != nullptr;
}

void AddTraceRecord()
{
    if (aTrace[nNumTraces % TRACE_SLOTS].wPC != cpu.get_pc() && !cpu.is_halted())
    {
        TRACEDATA* p = &aTrace[(++nNumTraces) % TRACE_SLOTS];
        p->wPC = cpu.get_pc();
        p->abInstr[0] = read_byte(p->wPC);
        p->abInstr[1] = read_byte(p->wPC + 1);
        p->abInstr[2] = read_byte(p->wPC + 2);
        p->abInstr[3] = read_byte(p->wPC + 3);
        p->regs = cpu;
    }
}

} // namespace Debug

////////////////////////////////////////////////////////////////////////////////

// Find the longest instruction that ends before a given address
uint16_t GetPrevInstruction(uint16_t wAddr_)
{
    // Start 4 bytes back as that's the longest instruction length
    for (unsigned int u = 4; u; u--)
    {
        uint16_t w = wAddr_ - u;
        uint8_t ab[] = { read_byte(w), read_byte(w + 1), read_byte(w + 2), read_byte(w + 3) };

        // Check that the instruction length leads to the required address
        if (w + Disassemble(ab) == wAddr_)
            return w;
    }

    // No match found, so return 1 byte back instead
    return wAddr_ - 1;
}

////////////////////////////////////////////////////////////////////////////////

void cmdStep(int nCount_ = 1, bool fCtrl_ = false)
{
    void* pPhysAddr = nullptr;
    uint8_t bOpcode;
    uint16_t wPC;

    // Skip any index prefixes on the instruction to reach the real opcode or a CD/ED prefix
    for (wPC = cpu.get_pc(); ((bOpcode = read_byte(wPC)) == IX_PREFIX || bOpcode == IY_PREFIX); wPC++);

    // Stepping into a HALT (with interrupt enabled) will enter the appropriate interrupt handler
    // This is much friendlier than single-stepping NOPs up to the next interrupt!
    if (nCount_ == 1 && bOpcode == OP_HALT && cpu.get_iff1() && !fCtrl_)
    {
        // For IM 2, form the address of the handler and break there
        if (cpu.get_int_mode() == 2)
            pPhysAddr = AddrReadPtr(read_word((cpu.get_i() << 8) | 0xff));

        // IM 0 and IM1 both use the handler at 0x0038
        else
            pPhysAddr = AddrReadPtr(IM1_INTERRUPT_HANDLER);
    }

    // If an address has been set, execute up to it
    if (pPhysAddr)
        Breakpoint::AddTemp(pPhysAddr);

    // Otherwise execute the requested number of instructions
    else
    {
        Expr::count = nCount_;
        auto expr = Expr::Counter;
        Breakpoint::AddTemp(nullptr, expr);
    }

    Debug::Stop();
}

void cmdStepOver(bool fCtrl_ = false)
{
    void* pPhysAddr = nullptr;
    uint8_t bOpcode, bOperand;
    uint16_t wPC;

    // Ctrl+StepOver performs a code timing measurement with minimal contention and interrupts disabled.
    // This provides a pure SAM execution environment, eliminating avoidable runtime variations.
    if (fCtrl_)
    {
        Memory::full_contention = false;
        cpu.on_mreq_wait(cpu.get_pc()); // remove opcode fetch slack
    }

    // Skip any index prefixes on the instruction to reach a CB/ED prefix or the real opcode
    for (wPC = cpu.get_pc(); ((bOpcode = read_byte(wPC)) == IX_PREFIX || bOpcode == IY_PREFIX); wPC++);
    bOperand = read_byte(wPC + 1);

    // 1-byte HALT or RST ?
    if (bOpcode == OP_HALT || (bOpcode & 0xc7) == 0xc7)
        pPhysAddr = AddrReadPtr(wPC + 1);

    // 2-byte backwards DJNZ/JR cc, or (LD|CP|IN|OT)[I|D]R ?
    else if (((bOpcode == OP_DJNZ || (bOpcode & 0xe7) == 0x20) && (bOperand & 0x80))
        || (bOpcode == ED_PREFIX && (bOperand & 0xf4) == 0xb0))
        pPhysAddr = AddrReadPtr(wPC + 2);

    // 3-byte CALL, CALL cc or backwards JP cc?
    else if (bOpcode == OP_CALL || (bOpcode & 0xc7) == 0xc4 ||
        ((bOpcode & 0xc7) == 0xc2 && read_word(wPC + 1) <= wPC))
        pPhysAddr = AddrReadPtr(wPC + 3);

    // Single step if no instruction-specific breakpoint is set
    if (!pPhysAddr)
        cmdStep();
    else
    {
        Breakpoint::AddTemp(pPhysAddr);
        Debug::Stop();
    }
}

void cmdStepOut()
{
    // Store the current stack pointer, for checking on RETurn calls
    nStepOutSP = cpu.get_sp();
    Debug::Stop();
}

////////////////////////////////////////////////////////////////////////////////

InputDialog::InputDialog(Window* pParent_/*=nullptr*/, const std::string& caption, const std::string& prompt, PFNINPUTPROC pfnNotify_)
    : Dialog(pParent_, 0, 0, caption), m_pfnNotify(pfnNotify_)
{
    // Get the length of the prompt string, so we can position the edit box correctly
    int n = GetTextWidth(prompt);

    // Create the prompt text control and input edit control
    new TextControl(this, 5, 10, prompt, WHITE);
    m_pInput = new NumberEditControl(this, 5 + n + 5, 6, 120);

    // Size the dialog to fit the prompt and edit control
    SetSize(8 + n + 120 + 8, 30);
    Centre();
}

void InputDialog::OnNotify(Window* pWindow_, int nParam_)
{
    if (pWindow_ == m_pInput && nParam_)
    {
        // Fetch and compile the input expression
        auto expr_str = m_pInput->GetText();
        auto expr = Expr::Compile(expr_str);

        // Close the dialog if the input was blank, or the notify handler tells us
        if (expr_str.empty() || (expr && m_pfnNotify(expr)))
        {
            Destroy();
            pDebugger->Refresh();
        }
    }
}


// Notify handler for New Address input
static bool OnAddressNotify(const Expr& expr)
{
    int nAddr = expr.Eval();
    pDebugger->SetAddress(nAddr, true);
    return true;
}

// Notify handler for Execute Until expression
static bool OnUntilNotify(const Expr& expr)
{
    Breakpoint::AddTemp(nullptr, expr);
    Debug::Stop();
    return false;
}

// Notify handler for Change Lmpr input
static bool OnLmprNotify(const Expr& expr)
{
    int page = expr.Eval() & LMPR_PAGE_MASK;
    IO::out_lmpr((IO::State().lmpr & ~LMPR_PAGE_MASK) | page);
    return true;
}

// Notify handler for Change Hmpr input
static bool OnHmprNotify(const Expr& expr)
{
    int page = expr.Eval() & HMPR_PAGE_MASK;
    IO::out_hmpr((IO::State(). hmpr & ~HMPR_PAGE_MASK) | page);
    return true;
}

// Notify handler for Change Lmpr input
static bool OnLeprNotify(const Expr& expr)
{
    IO::out_lepr(expr.Eval());
    return true;
}

// Notify handler for Change Hepr input
static bool OnHeprNotify(const Expr& expr)
{
    IO::out_hepr(expr.Eval());
    return true;
}

// Notify handler for Change Vmpr input
static bool OnVmprNotify(const Expr& expr)
{
    int page = expr.Eval() & VMPR_PAGE_MASK;
    IO::out_vmpr((IO::State().vmpr & VMPR_MODE_MASK) | page);
    return true;
}

// Notify handler for Change Mode input
static bool OnModeNotify(const Expr& expr)
{
    int nMode = expr.Eval();
    if (nMode < 1 || nMode > 4)
        return false;

    IO::out_vmpr(((nMode - 1) << 5) | (IO::State().vmpr & VMPR_PAGE_MASK));
    return true;
}

////////////////////////////////////////////////////////////////////////////////

uint16_t View::GetAddress() const
{
    return m_wAddr;
}

void View::SetAddress(uint16_t wAddr_, bool /*fForceTop_*/)
{
    m_wAddr = wAddr_;
    wLastAddr = wAddr_;
}

bool View::cmdNavigate(int nKey_, int nMods_)
{
    auto ctrl = (nMods_ == HM_CTRL);

    switch (nKey_)
    {
    case HK_KP7:  cmdStep(1, nMods_ != HM_NONE); break;
    case HK_KP8:  cmdStepOver(ctrl); break;
    case HK_KP9:  cmdStepOut();     break;
    case HK_KP4:  cmdStep(ctrl ? 10'000 : 10);      break;
    case HK_KP5:  cmdStep(ctrl ? 100'000 : 100);     break;
    case HK_KP6:  cmdStep(ctrl ? 1'000'000 : 1000);    break;

    default:      return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////

TextView::TextView(Window* pParent_)
    : View(pParent_), m_nRows(m_nHeight / ROW_HEIGHT)
{
    SetFont(sFixedFont);
}

void TextView::Draw(FrameBuffer& fb)
{
    // Draw each line, always drawing line 0 to allow an empty message
    for (int i = m_nTopLine; i < m_nTopLine + m_nRows && (!i || i < m_nLines); i++)
    {
        int nX = m_nX;
        int nY = m_nY + ROW_HEIGHT * (i - m_nTopLine);

        DrawLine(fb, nX, nY, i);
    }

    DisView::DrawRegisterPanel(fb, m_nX + m_nWidth - 6 * 16, m_nY);
}

bool TextView::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    switch (nMessage_)
    {
    case GM_BUTTONDBLCLK:
    {
        int nIndex = (nParam2_ - m_nY) / ROW_HEIGHT;

        if (IsOver() && nIndex >= 0 && nIndex < m_nLines)
            OnDblClick(nIndex);

        break;
    }

    case GM_CHAR:
        return cmdNavigate(nParam1_, nParam2_);

    case GM_MOUSEWHEEL:
        return cmdNavigate((nParam1_ < 0) ? HK_UP : HK_DOWN, 0);
    }

    return false;
}

bool TextView::cmdNavigate(int nKey_, int /*nMods_*/)
{
    switch (nKey_)
    {
    case HK_HOME:   m_nTopLine = 0; break;
    case HK_END:    m_nTopLine = m_nLines; break;

    case HK_UP:     m_nTopLine--; break;
    case HK_DOWN:   m_nTopLine++; break;

    case HK_PGUP:   m_nTopLine -= m_nRows; break;
    case HK_PGDN:   m_nTopLine += m_nRows; break;

    case HK_DELETE: OnDelete(); break;

    default:
        return false;
    }

    if (m_nTopLine > m_nLines - m_nRows)
        m_nTopLine = m_nLines - m_nRows;

    if (m_nTopLine < 0)
        m_nTopLine = 0;

    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool Debugger::s_fTransparent = false;

Debugger::Debugger(std::optional<int> bp_index)
    : Dialog(nullptr, 500, 260 + 36 + 2, "")
{
    // Move to the last display position, if any
    if (nDebugX | nDebugY)
        Move(nDebugX, nDebugY);

    // Create the status text control
    m_pStatus = new TextControl(this, 4, m_nHeight - ROW_HEIGHT, "");

    // If a breakpoint was supplied, report that it was triggered
    if (bp_index.has_value())
    {
        auto& bp = Breakpoint::breakpoints[*bp_index];
        std::stringstream ss;

        if (bp.type != BreakType::Temp)
        {
            ss << fmt::format("\aYBreakpoint {} hit:  {}", *bp_index, to_string(bp));
        }
        else if (bp.expr && bp.expr.str != "(counter)")
        {
            ss << fmt::format("\aYUNTIL condition met:  {}", bp.expr.str);
        }

        SetStatus(ss.str(), true, sPropFont);
        nLastView = ViewType::Dis;
    }

    Breakpoint::RemoveType(BreakType::Temp);

    // Clear step-out stack watch
    nStepOutSP = -1;

    // Force a break from the main CPU loop, and refresh the debugger display
    g_fBreak = true;
}

Debugger::~Debugger()
{
    // Remember the dialog position for next time
    nDebugX = m_nX;
    nDebugY = m_nY;

    // Remember the current register values so we know what's changed next time
    sLastRegs = cpu;
    bLastStatus = IO::State().status;

    // Save the cycle counter for timing comparisons
    dwLastCycle = CPU::frame_cycles;
    nLastFrames = 0;

    // Clear any cached data that could cause an immediate retrigger
    CPU::last_in_port = CPU::last_out_port = 0;
    Memory::last_phys_read1 = Memory::last_phys_read2 = Memory::last_phys_write1 = Memory::last_phys_write2 = nullptr;

    // Debugger is gone
    pDebugger = nullptr;
}

void Debugger::SetSubTitle(const std::string& sub_title)
{
    std::string title = "SimICE";

    if (!sub_title.empty())
    {
        title += " -- " + sub_title;
    }

    SetText(title);
}

void Debugger::SetAddress(uint16_t wAddr_, bool fForceTop_/*=false*/)
{
    m_pView->SetAddress(wAddr_, fForceTop_);
}

void Debugger::SetView(ViewType nView_)
{
    View* pNewView = nullptr;

    // Create the new view
    switch (nView_)
    {
    case ViewType::Dis:
        pNewView = new DisView(this);
        break;

    case ViewType::Txt:
        pNewView = new TxtView(this);
        break;

    case ViewType::Hex:
        pNewView = new HexView(this);
        break;

    case ViewType::Gfx:
        pNewView = new GfxView(this);
        break;

    case ViewType::Bpt:
        pNewView = new BptView(this);
        break;

    case ViewType::Trc:
        pNewView = new TrcView(this);
        break;
    }

    // New view created?
    if (pNewView)
    {
        SetSubTitle(pNewView->GetText());

        if (!m_pView)
        {
            pNewView->SetAddress((nView_ == ViewType::Dis) ? cpu.get_pc() : wLastAddr);
        }
        else
        {
            // Transfer the current address select, then replace the old one
            pNewView->SetAddress(m_pView->GetAddress());
            m_pView->Destroy();
            m_pView = nullptr;
        }

        m_pView = pNewView;
        nLastView = nView_;
    }
}

void Debugger::SetStatus(const std::string& status, bool fOneShot_, std::shared_ptr<Font> font)
{
    if (m_pStatus)
    {
        // One-shot status messages get priority
        if (fOneShot_)
            m_sStatus = status;
        else if (!m_sStatus.empty())
            return;

        if (font)
            m_pStatus->SetFont(font);

        m_pStatus->SetText(status);
    }
}

void Debugger::SetStatusByte(uint16_t addr)
{
    auto b = read_byte(addr);
    char ch = (b >= sFixedFont->first_chr && b <= sFixedFont->last_chr) ? b : ' ';
    auto keyword = (b >= BASE_KEYWORD) ? basic_keywords[b - BASE_KEYWORD] : "";

    auto status = fmt::format("{:04X}  {:02X}  {:03}  {:08b}  {}  {}", addr, b, b, b, ch, keyword);
    SetStatus(status, false, sFixedFont);
}

// Refresh the current debugger view
void Debugger::Refresh()
{
    // Re-set the view to the same address, to force a refresh
    m_pView->SetAddress(m_pView->GetAddress());
}


// Dialog override for background painting
void Debugger::EraseBackground(FrameBuffer& fb)
{
    // If we're not in transparent mode, call the base to draw the normal dialog background
    if (!s_fTransparent)
        Dialog::EraseBackground(fb);
}

// Dialog override for dialog drawing
void Debugger::Draw(FrameBuffer& fb)
{
    // First draw?
    if (!m_pView)
    {
        sCurrRegs = cpu;
        SetView(nLastView);
    }

    Dialog::Draw(fb);
}

bool Debugger::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    bool fRet = false;

    if (!fRet && nMessage_ == GM_CHAR)
    {
        fRet = true;

        bool fCtrl = !!(nParam2_ & HM_CTRL);
        bool fShift = !!(nParam2_ & HM_SHIFT);

        switch (nParam1_)
        {
        case HK_ESC:
            if (m_pCommandEdit)
            {
                m_pCommandEdit->Destroy();
                m_pCommandEdit = nullptr;
            }
            else if (nLastView != ViewType::Dis)
            {
                SetView(ViewType::Dis);
                SetAddress(cpu.get_pc());
            }
            else
                fRet = false;
            break;

        case HK_RETURN:
            if (!m_pCommandEdit)
            {
                m_pCommandEdit = new NumberEditControl(this, -1, m_nHeight - 16, m_nWidth + 2);
                m_pCommandEdit->SetFont(sPropFont);
            }
            break;

        case 'a':
            new InputDialog(this, "New location", "Address:", OnAddressNotify);
            break;

        case 'b':
            SetView(ViewType::Bpt);
            break;

        case 'c':
            SetView(ViewType::Trc);
            break;

        case 'd':
            SetView(ViewType::Dis);
            break;

        case 't':
            if (fCtrl)
                s_fTransparent = !s_fTransparent;
            else
                SetView(ViewType::Txt);
            break;

        case 'n':
            SetView(ViewType::Hex);
            break;

        case 'g':
            SetView(ViewType::Gfx);
            break;

        case 'l':
            if (fShift)
            {
                auto caption = fmt::format("Change LEPR [{:02X}]:", IO::State().lepr);
                new InputDialog(this, caption, "New Page:", OnLeprNotify);
            }
            else
            {
                auto caption = fmt::format("Change LMPR [{:02X}]:", IO::State().lmpr & LMPR_PAGE_MASK);
                new InputDialog(this, caption, "New Page:", OnLmprNotify);
            }
            break;

        case 'h':
            if (fShift)
            {
                auto caption = fmt::format("Change HEPR [{:02X}]:", IO::State().hepr);
                new InputDialog(this, caption, "New Page:", OnHeprNotify);
            }
            else
            {
                auto caption = fmt::format("Change HMPR [{:02X}]:", IO::State().hmpr & HMPR_PAGE_MASK);
                new InputDialog(this, caption, "New Page:", OnHmprNotify);
            }
            break;

        case 'v':
        {
            auto caption = fmt::format("Change VMPR [{:02X}]:", IO::State().vmpr & VMPR_PAGE_MASK);
            new InputDialog(this, caption, "New Page:", OnVmprNotify);
            break;
        }

        case 'm':
        {
            auto caption = fmt::format("Change Mode [{:X}]:", ((IO::State().vmpr & VMPR_MODE_MASK) >> 5) + 1);
            new InputDialog(this, caption, "New Mode:", OnModeNotify);
            break;
        }

        case 'u':
            new InputDialog(this, "Execute until", "Expression:", OnUntilNotify);
            break;

        case HK_KP0:
            IO::out_lmpr(IO::State().lmpr ^ LMPR_ROM0_OFF);
            break;

        case HK_KP1:
            IO::out_lmpr(IO::State().lmpr ^ LMPR_ROM1);
            break;

        case HK_KP2:
            IO::out_lmpr(IO::State().lmpr ^ LMPR_WPROT);
            break;

        case HK_KP3:
            IO::out_hmpr(IO::State().hmpr ^ HMPR_MCNTRL_MASK);
            break;

        default:
            fRet = false;
            break;
        }
    }

    // Refresh to reflect any changes if processed, otherwise pass to dialog base
    if (fRet)
        Refresh();
    else
        fRet = Dialog::OnMessage(nMessage_, nParam1_, nParam2_);

    return fRet;
}


void Debugger::OnNotify(Window* pWindow_, int nParam_)
{
    if (pWindow_ == m_pCommandEdit && nParam_ == 1)
    {
        auto command = m_pCommandEdit->GetText();

        if (command.empty())
        {
            m_pCommandEdit->Destroy();
            m_pCommandEdit = nullptr;
        }
        else if (Execute(command))
        {
            m_pCommandEdit->SetText("");
            Refresh();
        }
    }
}


std::optional<AccessType> GetAccessParam(std::string access)
{
    access = tolower(access);

    if (access == "r")
        return AccessType::Read;
    else if (access == "w")
        return AccessType::Write;
    else if (access == "rw")
        return AccessType::ReadWrite;

    return std::nullopt;
}

std::string Arg(const std::string& str, std::string& remain)
{
    std::string arg;
    std::istringstream ss(str);

    if (std::getline(ss, arg, ' '))
    {
        remain.clear();
        std::getline(ss, remain);
    }

    return arg;
}

std::optional<int> ArgValue(const std::string& str, std::string& remain)
{
    if (auto expr = Expr::Compile(str, remain))
    {
        if (auto value = expr.Eval(); value >= 0)
        {
            return value;
        }
    }

    return std::nullopt;
}

std::optional<int> ArgValue(const std::string& str)
{
    std::string remain;
    if (auto value = ArgValue(str, remain); value && remain.empty())
    {
        return value;
    }

    return std::nullopt;
}

void* ArgPhys(const std::string str, std::string& remain)
{
    std::string remain2;
    if (auto page = ArgValue(str, remain2))
    {
        if (remain2.length() >= 2 && remain2.front() == ':')
        {
            if (auto offset = ArgValue(remain2.substr(1), remain))
            {
                if (*page >= 0 && *page < NUM_INTERNAL_PAGES && *offset >= 0 && *offset < MEM_PAGE_SIZE)
                    return PageReadPtr(*page) + *offset;
            }
        }
        else
        {
            remain = remain2;
            auto addr = *page;
            return AddrReadPtr(addr);
        }
    }

    return nullptr;
}

std::optional<AccessType> ArgAccess(const std::string str, std::string& remain)
{
    std::string remain2 = str;

    if (auto arg = Arg(str, remain); !arg.empty())
    {
        if (arg == "r")
            return AccessType::Read;
        else if (arg == "w")
            return AccessType::Write;
        else if (arg == "rw")
            return AccessType::ReadWrite;
    }

    std::swap(remain, remain2);
    return std::nullopt;
}

bool Debugger::Execute(const std::string& cmdline)
{
    std::string remain;
    auto command = tolower(Arg(cmdline, remain));
    if (command.empty())
        return false;

    bool no_args = remain.empty();

    // di
    if (command == "di" && no_args)
    {
        cpu.set_iff1(false);
        cpu.set_iff2(false);
        return true;
    }

    // ei
    else if (command == "ei"  && no_args)
    {
        cpu.set_iff1(true);
        cpu.set_iff2(true);
        return true;
    }

    // im 0|1|2
    else if (command == "im")
    {
        if (auto im_mode = ArgValue(remain); im_mode && *im_mode >= 0 && *im_mode <= 2)
        {
            cpu.set_int_mode(*im_mode);
            return true;
        }
    }

    // reset
    else if (command == "reset" && no_args)
    {
        CPU::Reset(true);
        CPU::Reset(false);

        nLastFrames = 0;
        dwLastCycle = CPU::frame_cycles;

        SetAddress(cpu.get_pc());
        return true;
    }

    // nmi
    else if (command == "nmi" && no_args)
    {
        CPU::NMI();
        SetAddress(cpu.get_pc());
        return true;
    }

    // zap
    else if (command == "zap" && no_args)
    {
        uint8_t instr[MAX_Z80_INSTR_LEN]{};
        for (auto i = 0; i < MAX_Z80_INSTR_LEN; ++i)
            instr[i] = read_byte(cpu.get_pc() + i);

        auto len = static_cast<int>(Disassemble(instr));

        for (auto i = 0; i < len; ++i)
            write_byte(cpu.get_pc() + i, OP_NOP);

        return true;
    }

    // call addr
    else if (command == "call")
    {
        if (auto addr = ArgValue(remain))
        {
            cpu.set_sp(cpu.get_sp() - 2);
            write_word(cpu.get_sp(), cpu.get_pc());
            cpu.set_pc(*addr);
            SetAddress(cpu.get_pc());
            return true;
        }
    }

    // ret
    else if (command == "ret" && no_args)
    {
        cpu.set_sp(read_word(cpu.get_sp()));
        SetAddress(cpu.get_pc());
        cpu.set_sp(cpu.get_sp() + 2);
        return true;
    }

    // push value
    else if (command == "push")
    {
        if (auto value = ArgValue(remain))
        {
            cpu.set_sp(cpu.get_sp() - 2);
            write_word(cpu.get_sp(), *value);
            return true;
        }
    }

    // pop [register]
    else if (command == "pop")
    {
        if (no_args)
        {
            cpu.set_sp(cpu.get_sp() + 2);
            return true;
        }
        else if (auto reg_expr = Expr::Compile(remain, remain); reg_expr && remain.empty())
        {
            if (auto val = reg_expr.TokenValue(Expr::TokenType::Register))
            {
                if (auto reg = std::get_if<Expr::Token>(&*val))
                {
                    Expr::SetReg(*reg, read_word(cpu.get_sp()));
                    cpu.set_sp(cpu.get_sp() + 2);
                    return true;
                }
            }
        }
    }

    // break
    else if (command == "break" && no_args)
    {
        cpu.set_iff1(true);
        cpu.set_int_mode(1);
        cpu.set_pc(NMI_INTERRUPT_HANDLER);

        // Set up SAM BASIC paging
        IO::Out(LMPR_PORT, 0x1f);
        IO::Out(HMPR_PORT, 0x01);

        Debug::Stop();
        return false;
    }

    // x [count|until cond]
    else if (command == "x")
    {
        std::string remain2;

        // x
        if (remain.empty())
        {
            Debug::Stop();
            return false;
        }
        // x until cond
        else if (tolower(Arg(remain, remain2)) == "until")
        {
            if (auto expr = Expr::Compile(remain2))
            {
                Breakpoint::AddTemp(nullptr, expr);
                return true;
            }
        }
        // x 123  (instruction count)
        else if (auto count = ArgValue(remain))
        {
            Expr::count = *count;
            Breakpoint::AddTemp(nullptr, Expr::Counter);
            Debug::Stop();
            return false;
        }
    }

    // until expr
    else if (command == "u" || command == "until")
    {
        if (auto expr = Expr::Compile(remain))
        {
            Breakpoint::AddTemp(nullptr, expr);
            Debug::Stop();
            return false;
        }
    }

    // bpu cond
    else if (command == "bpu")
    {
        if (auto expr = Expr::Compile(remain))
        {
            Breakpoint::AddUntil(expr);
            return true;
        }
    }

    // bpx addr [if cond]
    else if (command == "bpx")
    {
        if (auto phys_addr = ArgPhys(remain, remain))
        {
            if (tolower(Arg(remain, remain)) == "if")
            {
                if (auto expr = Expr::Compile(remain))
                {
                    Breakpoint::AddExec(phys_addr, expr);
                    return true;
                }
            }
            else if (remain.empty())
            {
                Breakpoint::AddExec(phys_addr);
                return true;
            }
        }
    }

    // bpm addr [rw|r|w] [if cond]
    else if (command == "bpm")
    {
        if (auto phys_addr = ArgPhys(remain, remain))
        {
            auto access = AccessType::ReadWrite;
            if (auto access_arg = ArgAccess(remain, remain))
            {
                access = *access_arg;
            }

            if (tolower(Arg(remain, remain)) == "if")
            {
                if (auto expr = Expr::Compile(remain))
                {
                    Breakpoint::AddMemory(phys_addr, access, expr);
                    return true;
                }
            }
            else if (remain.empty())
            {
                Breakpoint::AddMemory(phys_addr, access);
                return true;
            }
        }
    }

    // bpmr addr length [rw|r|w] [if cond]
    else if (command == "bpmr")
    {
        if (auto phys_addr = ArgPhys(remain, remain))
        {
            if (auto length = ArgValue(remain, remain))
            {
                auto access = AccessType::ReadWrite;
                if (auto access_arg = ArgAccess(remain, remain))
                {
                    access = *access_arg;
                }

                if (tolower(Arg(remain, remain)) == "if")
                {
                    if (auto expr = Expr::Compile(remain))
                    {
                        Breakpoint::AddMemory(phys_addr, access, expr, *length);
                        return true;
                    }
                }
                else if (remain.empty())
                {
                    Breakpoint::AddMemory(phys_addr, access, {}, *length);
                    return true;
                }
            }
        }
    }

    // bpio port [rw|r|w] [if cond]
    else if (command == "bpio")
    {
        if (auto port = ArgValue(remain, remain))
        {
            auto access = AccessType::ReadWrite;
            if (auto access_arg = ArgAccess(remain, remain))
            {
                access = *access_arg;
            }

            if (tolower(Arg(remain, remain)) == "if")
            {
                if (auto expr = Expr::Compile(remain))
                {
                    Breakpoint::AddPort(*port, access, expr);
                    return true;
                }
            }
            else if (remain.empty())
            {
                Breakpoint::AddPort(*port, access);
                return true;
            }
        }
    }

    // bpint frame|line|midi [if cond]
    else if (command == "bpint")
    {
        uint8_t mask{};
        Expr expr;

        if (no_args)
        {
            mask = 0x1f;
        }
        else
        {
            while (!remain.empty())
            {
                if (auto arg = tolower(Arg(remain, remain)); !arg.empty())
                {
                    if (arg == "frame" || arg == "f")
                        mask |= STATUS_INT_FRAME;
                    else if (arg == "line" || arg == "l")
                        mask |= STATUS_INT_LINE;
                    else if (arg == "midi" || arg == "m")
                        mask |= STATUS_INT_MIDIIN | STATUS_INT_MIDIOUT;
                    else if (arg == "midiin" || arg == "mi")
                        mask |= STATUS_INT_MIDIIN;
                    else if (arg == "midiout" || arg == "mo")
                        mask |= STATUS_INT_MIDIOUT;
                    else if (arg == "if")
                    {
                        if (auto expr = Expr::Compile(remain))
                        {
                            Breakpoint::AddInterrupt(mask, expr);
                            return true;
                        }
                    }
                    else
                    {
                        return false;
                    }
                }
            }
        }

        Breakpoint::AddInterrupt(mask);
        return true;
    }

    // flag [+|-][sz5h3vnc]
    else if ((command == "flag" || command == "f") && !no_args)
    {
        bool set = true;
        uint8_t new_f = cpu.get_f();

        for (auto ch : tolower(remain))
        {
            uint8_t flag{};

            switch (ch)
            {
                case '+': set = true;  continue;
                case '-': set = false; continue;

                case 's': flag = cpu.sf_mask; break;
                case 'z': flag = cpu.zf_mask; break;
                case '5': flag = cpu.yf_mask; break;
                case 'h': flag = cpu.hf_mask; break;
                case '3': flag = cpu.xf_mask; break;
                case 'v': flag = cpu.pf_mask; break;
                case 'n': flag = cpu.nf_mask; break;
                case 'c': flag = cpu.cf_mask; break;

                default: return false;
            }

            if (set)
                new_f |= flag;
            else
                new_f &= ~flag;
        }

        cpu.set_f(new_f);
        return true;
    }

    // bc n  or  bc *
    else if (command == "bc")
    {
        if (remain == "*")
        {
            Breakpoint::RemoveAll();
            return true;
        }
        if (auto index = ArgValue(remain))
        {
            Breakpoint::Remove(*index);
            return true;
        }
    }

    // bd n  or  bd *  or  be n  or  be *
    else if (command == "bd" || command == "be")
    {
        bool enable = (command == "be");

        if (remain == "*")
        {
            for (auto& bp : Breakpoint::breakpoints)
                bp.enabled = enable;
            return true;
        }
        else if (auto index = ArgValue(remain))
        {
            if (auto pBreak = Breakpoint::GetAt(*index))
            {
                pBreak->enabled = enable;
                return true;
            }
        }
    }

    // exx
    else if (command == "exx" && no_args)
    {
        cpu.exx_regs();
        return true;
    }

    // ex de,hl
    else if (command == "ex")
    {
        if (auto arg = tolower(Arg(remain, remain)); arg == "de,hl" && remain.empty())
        {
            cpu.on_ex_de_hl_regs();
            return true;
        }
    }

    // ld reg,value  or  r reg=value  or  r reg value
    else if (command == "r" || command == "ld")
    {
        if (auto expr = Expr::Compile(remain, remain, Expr::regOnly))
        {
            if (auto reg = expr.TokenValue(Expr::TokenType::Register))
            {
                // Expect  ld hl,123  or r hl=123  syntax
                if (remain.empty() || (remain.front() != ',' && remain.front() != '='))
                {
                    return false;
                }

                if (auto value = ArgValue(remain.substr(1)))
                {
                    if (auto token = std::get_if<Expr::Token>(&*reg))
                    {
                        Expr::SetReg(*token, *value);
                        return true;
                    }
                }
            }
        }
    }

    // out port,value
    else if (command == "out")
    {
        if (auto port = ArgValue(remain, remain))
        {
            if (remain.empty() || remain.front() != ',')
            {
                return false;
            }

            if (auto value = ArgValue(remain.substr(1)))
            {
                IO::Out(*port, *value);
                return true;
            }
        }
    }

    // poke addr,val[,val,...]
    else if (command == "poke")
    {
        if (auto addr = ArgValue(remain, remain); addr && !remain.empty())
        {
            std::vector<uint8_t> values;

            while (!remain.empty())
            {
                if (remain.front() != ',')
                {
                    return false;
                }

                if (auto value = ArgValue(remain.substr(1), remain))
                {
                    if (*value >= 0x100)
                        values.push_back(static_cast<uint8_t>(*value >> 8));

                    values.push_back(static_cast<uint8_t>(*value));
                }
                else
                {
                    return false;
                }
            }

            auto offset = 0;
            for (auto b : values)
                write_byte(*addr + offset++, b);

            return true;
        }
    }

    return false;
}


////////////////////////////////////////////////////////////////////////////////
// Disassembler

#define MAX_LABEL_LEN  25
#define BAR_CHAR_LEN   65

uint16_t DisView::s_wAddrs[64];
bool DisView::m_fUseSymbols = true;

DisView::DisView(Window* pParent_)
    : View(pParent_)
{
    SetText("Disassemble");
    SetFont(sFixedFont);

    // Calculate the number of rows and columns in the view
    m_uRows = m_nHeight / ROW_HEIGHT;
    m_uColumns = m_nWidth / CHR_WIDTH;

    // Allocate enough for a full screen of characters, plus room for colour codes
    m_pszData = new char[m_uRows * m_uColumns * 2];
}

void DisView::SetAddress(uint16_t wAddr_, bool fForceTop_)
{
    View::SetAddress(wAddr_);

    // Update the control flow + data target address hints
    SetCodeTarget();
    SetDataTarget();

    // Show any data target in the status line
    pDebugger->SetStatus(m_data_target, false, sFixedFont);

    if (!fForceTop_)
    {
        // If the address is on-screen, but not first or last row, don't refresh
        for (unsigned int u = 1; u < m_uRows - 1; u++)
        {
            if (s_wAddrs[u] == wAddr_)
            {
                wAddr_ = s_wAddrs[0];
                break;
            }
        }
    }

    char* psz = m_pszData;
    for (unsigned int u = 0; u < m_uRows; u++)
    {
        s_wAddrs[u] = wAddr_;

        // Display the instruction address
        psz += sprintf(psz, "%04X ", wAddr_);

        // Disassemble the current instruction
        char szDisassem[64];
        uint8_t ab[MAX_Z80_INSTR_LEN] = { read_byte(wAddr_), read_byte(wAddr_ + 1), read_byte(wAddr_ + 2), read_byte(wAddr_ + 3) };
        auto uLen = Disassemble(ab, wAddr_, szDisassem, sizeof(szDisassem), m_fUseSymbols ? MAX_LABEL_LEN : 0);

        // Are we to use symbols?
        if (m_fUseSymbols)
        {
            // Look-up the symbol name to use as a label
            auto sName = Symbol::LookupAddr(wAddr_, wAddr_, MAX_LABEL_LEN);

            // Right-justify the label against the disassembly
            psz += sprintf(psz, "\ab%*s\aX", MAX_LABEL_LEN, sName.c_str());
        }
        else
        {
            *psz++ = ' ';

            // Show the instruction bytes between the address and the disassembly
            for (unsigned int v = 0; v < MAX_Z80_INSTR_LEN; v++)
            {
                if (v < uLen)
                    psz += sprintf(psz, " %02X", read_byte(wAddr_ + v));
                else
                    psz += sprintf(psz, "   ");
            }

            // Pad with spaces to keep disassembly in space position as symbol version
            psz += sprintf(psz, "%*s", MAX_LABEL_LEN - (1 + 2 + 1 + 2 + 1 + 2 + 1 + 2 + 1), "");
        }

        // Add the disassembly
        psz += sprintf(psz, " %s", szDisassem);

        // Terminate the line
        *psz++ = '\0';

        // Advance to the next instruction address
        wAddr_ += uLen;
    }

    // Terminate the line list
    *psz = '\0';
}


void DisView::Draw(FrameBuffer& fb)
{
    unsigned int u = 0;
    for (char* psz = (char*)m_pszData; *psz; psz += strlen(psz) + 1, u++)
    {
        int nX = m_nX;
        int nY = m_nY + ROW_HEIGHT * u;

        auto colour = 'W';

        if (s_wAddrs[u] == cpu.get_pc())
        {
            // The location bar is green for a change in code flow or yellow otherwise, with black text
            uint8_t bBarColour = (m_uCodeTarget != INVALID_TARGET) ? GREEN_7 : YELLOW_7;
            fb.FillRect(nX - 1, nY - 1, BAR_CHAR_LEN * CHR_WIDTH + 1, ROW_HEIGHT - 3, bBarColour);
            colour = 'k';

            // Add a direction arrow if we have a code target
            if (m_uCodeTarget != INVALID_TARGET)
                fb.DrawString(nX + CHR_WIDTH * (BAR_CHAR_LEN - 3), nY, (m_uCodeTarget <= cpu.get_pc()) ? "\ak\x80\x80\x80" : "\ak\x81\x81\x81");
        }

        // Check for a breakpoint at the current address.
        // Show conditional breakpoints in magenta, unconditional in red.
        auto pPhysAddr = AddrReadPtr(s_wAddrs[u]);
        if (auto index_val = Breakpoint::GetExecIndex(pPhysAddr))
        {
            colour = Breakpoint::GetAt(*index_val)->expr ? 'M' : 'R';
        }

        // Show the current entry normally if it's not the current code target, otherwise show all
        // in black text with an arrow instead of the address, indicating it's the code target.
        if (m_uCodeTarget == INVALID_TARGET || s_wAddrs[u] != m_uCodeTarget)
            fb.DrawString(nX, nY, "\a{}\a{}{}", colour, (colour != 'W') ? '0' : colour, psz);
        else
            fb.DrawString(nX, nY, "\a{}===>\a{}{}", (colour == 'k') ? 'k' : 'G', (colour == 'k') ? '0' : colour, psz + 4);
    }

    DrawRegisterPanel(fb, m_nX + m_nWidth - 6 * 16, m_nY);
}

/*static*/ void DisView::DrawRegisterPanel(FrameBuffer& fb, int nX_, int nY_)
{
    int i;
    int nX = nX_;
    int nY = nY_;

#define DoubleReg(dx,dy,name,reg) \
    { \
        fb.DrawString(nX+dx, nY+dy, "\ag{:<3s}\a{}{:02X}\a{}{:02X}", name, \
                    (z80::get_high8(cpu.get_##reg()) != z80::get_high8(sLastRegs.get_##reg()))?CHG_COL:'X', z80::get_high8(cpu.get_##reg()),  \
                    (z80::get_low8(cpu.get_##reg()) != z80::get_low8(sLastRegs.get_##reg()))?CHG_COL:'X', z80::get_low8(cpu.get_##reg())); \
    }

#define SingleReg(dx,dy,name,reg) \
    { \
        fb.DrawString(nX+dx, nY+dy, "\ag{:<2s}\a{}{:02X}", name, \
                    (cpu.get_##reg() != sLastRegs.get_##reg())?CHG_COL:'X', cpu.get_##reg()); \
    }

    DoubleReg(0, 0, "AF", af);    DoubleReg(54, 0, "AF'", alt_af);
    DoubleReg(0, 12, "BC", bc);    DoubleReg(54, 12, "BC'", alt_bc);
    DoubleReg(0, 24, "DE", de);    DoubleReg(54, 24, "DE'", alt_de);
    DoubleReg(0, 36, "HL", hl);    DoubleReg(54, 36, "HL'", alt_hl);

    DoubleReg(0, 52, "IX", ix);    DoubleReg(54, 52, "IY", iy);
    DoubleReg(0, 64, "PC", pc);    DoubleReg(54, 64, "SP", sp);

    SingleReg(0, 80, "I", i);      SingleReg(36, 80, "R", r);

    fb.DrawString(nX + 80, nY + 74, "\aK\x81\x81");

    for (i = 0; i < 4; i++)
        fb.DrawString(nX + 72, nY + 84 + i * 12, "{:04X}", read_word(cpu.get_sp() + i * 2));

    fb.DrawString(nX, nY + 96, "\agIM \a{}{}", (cpu.get_int_mode() != sLastRegs.get_int_mode()) ? CHG_COL : 'X', cpu.get_int_mode());
    fb.DrawString(nX + 18, nY + 96, "  \a{}{}I", (cpu.get_iff1() != sLastRegs.get_iff1()) ? CHG_COL : 'X', cpu.get_iff1() ? 'E' : 'D');

    char bIntDiff = IO::State().status ^ bLastStatus;
    auto status = IO::State().status;
    fb.DrawString(nX, nY + 108, "\agStat \a{}{}\a{}{}\a{}{}\a{}{}\a{}{}",
        (bIntDiff & 0x10) ? CHG_COL : (status & 0x10) ? 'K' : 'X', (status & 0x10) ? '-' : 'O',
        (bIntDiff & 0x08) ? CHG_COL : (status & 0x08) ? 'K' : 'X', (status & 0x08) ? '-' : 'F',
        (bIntDiff & 0x04) ? CHG_COL : (status & 0x04) ? 'K' : 'X', (status & 0x04) ? '-' : 'I',
        (bIntDiff & 0x02) ? CHG_COL : (status & 0x02) ? 'K' : 'X', (status & 0x02) ? '-' : 'M',
        (bIntDiff & 0x01) ? CHG_COL : (status & 0x01) ? 'K' : 'X', (status & 0x01) ? '-' : 'L');

    char bFlagDiff = cpu.get_f() ^ sLastRegs.get_f();
    fb.DrawString(nX, nY + 132, "\agFlag \a{}{}\a{}{}\a{}{}\a{}{}\a{}{}\a{}{}\a{}{}\a{}{}",
        (bFlagDiff & cpu.sf_mask) ? CHG_COL : (cpu.get_f() & cpu.sf_mask) ? 'X' : 'K', (cpu.get_f() & cpu.sf_mask) ? 'S' : '-',
        (bFlagDiff & cpu.zf_mask) ? CHG_COL : (cpu.get_f() & cpu.zf_mask) ? 'X' : 'K', (cpu.get_f() & cpu.zf_mask) ? 'Z' : '-',
        (bFlagDiff & cpu.yf_mask) ? CHG_COL : (cpu.get_f() & cpu.yf_mask) ? 'X' : 'K', (cpu.get_f() & cpu.yf_mask) ? '5' : '-',
        (bFlagDiff & cpu.hf_mask) ? CHG_COL : (cpu.get_f() & cpu.hf_mask) ? 'X' : 'K', (cpu.get_f() & cpu.hf_mask) ? 'H' : '-',
        (bFlagDiff & cpu.xf_mask) ? CHG_COL : (cpu.get_f() & cpu.xf_mask) ? 'X' : 'K', (cpu.get_f() & cpu.xf_mask) ? '3' : '-',
        (bFlagDiff & cpu.pf_mask) ? CHG_COL : (cpu.get_f() & cpu.pf_mask) ? 'X' : 'K', (cpu.get_f() & cpu.pf_mask) ? 'V' : '-',
        (bFlagDiff & cpu.nf_mask) ? CHG_COL : (cpu.get_f() & cpu.nf_mask) ? 'X' : 'K', (cpu.get_f() & cpu.nf_mask) ? 'N' : '-',
        (bFlagDiff & cpu.cf_mask) ? CHG_COL : (cpu.get_f() & cpu.cf_mask) ? 'X' : 'K', (cpu.get_f() & cpu.cf_mask) ? 'C' : '-');


    int nLine = (CPU::frame_cycles < CPU_CYCLES_PER_SIDE_BORDER) ? GFX_HEIGHT_LINES - 1 : (CPU::frame_cycles - CPU_CYCLES_PER_SIDE_BORDER) / CPU_CYCLES_PER_LINE;
    int nLineCycle = (CPU::frame_cycles + CPU_CYCLES_PER_LINE - CPU_CYCLES_PER_SIDE_BORDER) % CPU_CYCLES_PER_LINE;

    fb.DrawString(nX, nY + 148, "\agScan\aX {:03}:{:03}", nLine, nLineCycle);
    fb.DrawString(nX, nY + 160, "\agT\aX {}", CPU::frame_cycles);

    uint32_t dwCycleDiff = ((nLastFrames * CPU_CYCLES_PER_FRAME) + CPU::frame_cycles) - dwLastCycle;
    if (dwCycleDiff)
        fb.DrawString(nX + 12, nY + 172, "+{}", dwCycleDiff);

    fb.DrawString(nX, nY + 188, "\agA \a{}{}", ReadOnlyAddr(0x0000) ? 'c' : 'X', Memory::PageDesc(GetSectionPage(Section::A)));
    fb.DrawString(nX, nY + 200, "\agB \a{}{}", ReadOnlyAddr(0x4000) ? 'c' : 'X', Memory::PageDesc(GetSectionPage(Section::B)));
    fb.DrawString(nX, nY + 212, "\agC \a{}{}", ReadOnlyAddr(0x8000) ? 'c' : 'X', Memory::PageDesc(GetSectionPage(Section::C)));
    fb.DrawString(nX, nY + 224, "\agD \a{}{}", ReadOnlyAddr(0xc000) ? 'c' : 'X', Memory::PageDesc(GetSectionPage(Section::D)));

    fb.DrawString(nX + 66, nY + 188, "\agL\aX {:02X}", IO::State().lmpr);
    fb.DrawString(nX + 66, nY + 200, "\agH\aX {:02X}", IO::State().hmpr);
    fb.DrawString(nX + 66, nY + 212, "\agV\aX {:02X}", IO::State().vmpr);
    fb.DrawString(nX + 66, nY + 224, "\agM\aX {:X}", IO::ScreenMode());

    fb.DrawString(nX, nY + 240, "\agEvents");

    i = 0;
    for (auto pEvent = head_ptr; pEvent; pEvent = pEvent->next_ptr)
    {
        const char* pcszEvent = "????";
        switch (pEvent->type)
        {
        case EventType::FrameInterrupt:     pcszEvent = "FINT"; break;
        case EventType::FrameInterruptEnd:  pcszEvent = "FEND"; break;
        case EventType::LineInterrupt:      pcszEvent = "LINT"; break;
        case EventType::LineInterruptEnd:   pcszEvent = "LEND"; break;
        case EventType::MidiOutStart:       pcszEvent = "MIDI"; break;
        case EventType::MidiOutEnd:         pcszEvent = "MEND"; break;
        case EventType::MidiTxfmstEnd:      pcszEvent = "MTXF"; break;
        case EventType::MouseReset:         pcszEvent = "MOUS"; break;
        case EventType::BlueAlphaClock:     pcszEvent = "BLUE"; break;
        case EventType::TapeEdge:           pcszEvent = "TAPE"; break;
        case EventType::AsicReady:          pcszEvent = "ASIC"; break;

        case EventType::InputUpdate:
        case EventType::None:
            continue;
        }

        fb.DrawString(nX, nY + 252 + i * 12, "{:<4s} \a{}{:6}\aXT", pcszEvent, CHG_COL, pEvent->due_time - CPU::frame_cycles);
        if (++i == 3)
            break;
    }
}

bool DisView::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    switch (nMessage_)
    {
    case GM_BUTTONDBLCLK:
    {
        unsigned int uRow = (nParam2_ - m_nY) / ROW_HEIGHT;

        if (IsOver() && uRow < m_uRows)
        {
            // Find any existing execution breakpoint
            auto pPhysAddr = AddrReadPtr(s_wAddrs[uRow]);
            if (auto index_val = Breakpoint::GetExecIndex(pPhysAddr))
                Breakpoint::Remove(*index_val);
            else
                Breakpoint::AddExec(pPhysAddr);
        }
        break;
    }

    case GM_CHAR:
    {
        // Clear any one-shot status
        pDebugger->SetStatus("", true);

        switch (nParam1_)
        {
        case HK_SPACE:
            if (nParam2_ == HM_NONE)
                cmdStep();
            else if (nParam2_ == HM_SHIFT)
                cmdStepOut();
            else if (nParam2_ == HM_CTRL)
                cmdStepOver();
            break;

        case 's':
            m_fUseSymbols = !m_fUseSymbols;
            pDebugger->Refresh();
            break;

        case 'd': case 'D':
            break;

        default:
            return cmdNavigate(nParam1_, nParam2_);
        }
        break;
    }

    case GM_MOUSEWHEEL:
        return cmdNavigate((nParam1_ < 0) ? HK_UP : HK_DOWN, 0);

    default:
        return false;
    }

    return true;
}

bool DisView::cmdNavigate(int nKey_, int nMods_)
{
    auto wAddr = GetAddress();
    bool fCtrl = (nMods_ & HM_CTRL) != 0;

    switch (nKey_)
    {
    case HK_HOME:
        if (!fCtrl)
            wAddr = cpu.get_pc();
        else
            SetAddress(wAddr = 0, true);
        break;

    case HK_END:
        if (fCtrl)
        {
            // Set top address to 0000, then page up to leave FFFF at the bottom
            SetAddress(0, true);
            return cmdNavigate(HK_PGUP, 0);
        }
        break;

    case HK_UP:
        if (!fCtrl)
            wAddr = GetPrevInstruction(s_wAddrs[0]);
        else
        {
            wAddr = GetPrevInstruction(cpu.get_pc());
            cpu.set_pc(wAddr);
        }
        break;

    case HK_DOWN:
        if (!fCtrl)
            wAddr = s_wAddrs[1];
        else
        {
            uint8_t ab[MAX_Z80_INSTR_LEN];
            for (unsigned int u = 0; u < sizeof(ab); u++)
                ab[u] = read_byte(cpu.get_pc() + u);

            wAddr = cpu.get_pc() + Disassemble(ab);
            cpu.set_pc(wAddr);
        }
        break;

    case HK_LEFT:
        if (!fCtrl)
            wAddr = s_wAddrs[0] - 1;
        else
        {
            cpu.set_pc(cpu.get_pc() - 1);
            wAddr = cpu.get_pc();
        }
        break;

    case HK_RIGHT:
        if (!fCtrl)
            wAddr = s_wAddrs[0] + 1;
        else
        {
            cpu.set_pc(cpu.get_pc() + 1);
            wAddr = cpu.get_pc();
        }
        break;

    case HK_PGDN:
    {
        wAddr = s_wAddrs[m_uRows - 1];
        uint8_t ab[] = { read_byte(wAddr), read_byte(wAddr + 1), read_byte(wAddr + 2), read_byte(wAddr + 3) };
        wAddr += Disassemble(ab);
        break;
    }

    case HK_PGUP:
    {
        // Aim to have the current top instruction at the bottom
        auto w = s_wAddrs[0];

        // Start looking a screenful of single-byte instructions back
        for (wAddr = w - m_uRows; ; wAddr--)
        {
            auto w2 = wAddr;

            // Disassemble a screenful of instructions
            for (unsigned int u = 0; u < m_uRows - 1; u++)
            {
                uint8_t ab[] = { read_byte(w2), read_byte(w2 + 1), read_byte(w2 + 2), read_byte(w2 + 3) };
                w2 += Disassemble(ab);
            }

            // Check for a suitable ending position
            if (++w2 == w) break;
            if (++w2 == w) break;
            if (++w2 == w) break;
            if (++w2 == w) break;
        }
        break;
    }

    default:
        return View::cmdNavigate(nKey_, nMods_);
    }

    SetAddress(wAddr, !fCtrl);
    return true;
}

// Determine the code target address, if any
bool DisView::SetCodeTarget()
{
    // Extract the two bytes at PC, which we'll assume are single byte opcode and operand
    auto wPC = cpu.get_pc();
    auto bOpcode = read_byte(wPC);
    auto bOperand = read_byte(wPC + 1);
    uint8_t bFlags = cpu.get_f(), bCond = 0xff;

    // Work out the possible next instruction addresses, which depend on the instruction found
    auto wJpTarget = read_word(wPC + 1);
    uint16_t wJrTarget = wPC + 2 + static_cast<signed char>(read_byte(wPC + 1));
    auto wRetTarget = read_word(cpu.get_sp());
    uint16_t wRstTarget = bOpcode & 0x38;

    // No instruction target or conditional jump helper string yet
    m_uCodeTarget = INVALID_TARGET;

    // Examine the current opcode to check for flow changing instructions
    switch (bOpcode)
    {
    case OP_DJNZ:
        // Set a pretend zero flag if B is 1 and would be decremented to zero
        bFlags = (cpu.get_b() == 1) ? cpu.zf_mask : 0;
        bCond = 0;
        // Fall through...

    case OP_JR:     m_uCodeTarget = wJrTarget;  break;
    case OP_RET:    m_uCodeTarget = wRetTarget; break;
    case OP_JP:
    case OP_CALL:   m_uCodeTarget = wJpTarget;  break;
    case OP_JPHL:   m_uCodeTarget = cpu.get_hl();  break;

    case ED_PREFIX:
    {
        // RETN or RETI?
        if ((bOperand & 0xc7) == 0x45)
            m_uCodeTarget = wRetTarget;

        break;
    }

    case IX_PREFIX: if (bOperand == OP_JPHL) m_uCodeTarget = cpu.get_ix();  break;  // JP (IX)
    case IY_PREFIX: if (bOperand == OP_JPHL) m_uCodeTarget = cpu.get_iy();  break;  // JP (IY)

    default:
        // JR cc ?
        if ((bOpcode & 0xe7) == 0x20)
        {
            // Extract the 2-bit condition code and set the possible target
            bCond = (bOpcode >> 3) & 0x03;
            m_uCodeTarget = wJrTarget;
            break;
        }

        // Mask to check for certain groups we're interested in
        switch (bOpcode & 0xc7)
        {
        case 0xc0:  m_uCodeTarget = wRetTarget; break;  // RET cc
        case 0xc2:                                      // JP cc
        case 0xc4:  m_uCodeTarget = wJpTarget;  break;  // CALL cc
        case 0xc7:  m_uCodeTarget = wRstTarget; break;  // RST
        }

        // For all but RST, extract the 3-bit condition code
        if (m_uCodeTarget != INVALID_TARGET && (bOpcode & 0xc7) != 0xc7)
            bCond = (bOpcode >> 3) & 0x07;

        break;
    }

    // Have we got a condition to test?
    if (bCond <= 0x07)
    {
        static const uint8_t abFlags[] = {
            cpu.zf_mask, cpu.cf_mask, cpu.pf_mask, cpu.sf_mask };

        // Invert the 'not' conditions to give a set bit for a mask
        bFlags ^= (bCond & 1) ? 0x00 : 0xff;

        // Condition not met by flags?
        if (!(abFlags[bCond >> 1] & bFlags))
            m_uCodeTarget = INVALID_TARGET;
    }

    // Return whether a target has been set
    return m_uCodeTarget != INVALID_TARGET;
}

// Determine the target address
bool DisView::SetDataTarget()
{
    bool f16Bit = false;

    // No target or helper string yet
    m_uDataTarget = INVALID_TARGET;
    m_data_target.clear();

    // Extract potential instruction bytes
    auto wPC = cpu.get_pc();
    auto bOp0 = read_byte(wPC);
    auto bOp1 = read_byte(wPC + 1);
    auto bOp2 = read_byte(wPC + 2);
    auto bOp3 = read_byte(wPC + 3);
    auto bOpcode = bOp0;

    // Adjust for any index prefix
    bool fIndex = bOp0 == 0xdd || bOp0 == 0xfd;
    if (fIndex) bOpcode = bOp1;

    // Calculate potential operand addresses
    uint16_t wAddr12 = (bOp2 << 8) | bOp1;
    uint16_t wAddr23 = (bOp3 << 8) | bOp2;
    uint16_t wAddr = fIndex ? wAddr23 : wAddr12;
    uint16_t wHLIXIYd = !fIndex ? cpu.get_hl() : (((bOp0 == 0xdd) ? cpu.get_ix() : cpu.get_iy()) + bOp2);


    // 000r0010 = LD (BC/DE),A
    // 000r1010 = LD A,(BC/DE)
    if ((bOpcode & 0xe7) == 0x02)
        m_uDataTarget = (bOpcode & 0x10) ? cpu.get_de() : cpu.get_bc();

    // 00110010 = LD (nn),A
    // 00111010 = LD A,(nn)
    else if ((bOpcode & 0xf7) == 0x32)
        m_uDataTarget = wAddr;

    // [DD/FD] 0011010x = [INC|DEC] (HL/IX+d/IY+d)
    // [DD/FD] 01110rrr = LD (HL/IX+d/IY+d),r
    else if ((bOpcode & 0xfe) == 0x34 || bOpcode == 0x36)
        m_uDataTarget = wHLIXIYd;

    // [DD/FD] 00110rrr = LD (HL/IX+d/IY+d),n
    else if (bOpcode != OP_HALT && (bOpcode & 0xf8) == 0x70)
        m_uDataTarget = wHLIXIYd;

    // [DD/FD] 01rrr110 = LD r,(HL/IX+d/IY+d)
    else if (bOpcode != OP_HALT && (bOpcode & 0xc7) == 0x46)
        m_uDataTarget = wHLIXIYd;

    // [DD/FD] 10xxx110 = ADD|ADC|SUB|SBC|AND|XOR|OR|CP (HL/IX+d/IY+d)
    else if ((bOpcode & 0xc7) == 0x86)
        m_uDataTarget = wHLIXIYd;

    // (DD) E3 = EX (SP),HL/IX/IY
    else if (bOpcode == 0xe3)
    {
        m_uDataTarget = cpu.get_sp();
        f16Bit = true;
    }
    /*
        // 11rr0101 = PUSH rr
        else if ((bOpcode & 0xcf) == 0xc5)
        {
            m_uDataTarget = SP-2;
            f16Bit = true;
        }
    */
    // 11rr0001 = POP rr
    else if ((bOpcode & 0xcf) == 0xc1)
    {
        m_uDataTarget = cpu.get_sp();
        f16Bit = true;
    }

    // [DD/FD] 00100010 = LD (nn),HL/IX/IY
    // [DD/FD] 00101010 = LD HL/IX/IY,(nn)
    else if ((bOpcode & 0xf7) == 0x22)
    {
        m_uDataTarget = wAddr;
        f16Bit = true;
    }

    // ED 01dd1011 = LD [BC|DE|HL|SP],(nn)
    // ED 01dd0011 = LD (nn),[BC|DE|HL|SP]
    else if (bOpcode == ED_PREFIX && (bOp1 & 0xc7) == 0x43)
    {
        m_uDataTarget = wAddr23;
        f16Bit = true;
    }

    // ED 0110x111 = RRD/RLD
    else if (bOpcode == ED_PREFIX && (bOp1 & 0xf7) == 0x67)
    {
        m_uDataTarget = cpu.get_hl();
    }

    // ED 101000xx = LDI/CPI/INI/OUTI
    // ED 101010xx = LDD/CPD/IND/OUTD
    // ED 101100xx = LDIR/CPIR/INIR/OTIR
    // ED 101110xx = LDDR/CPDR/INDR/OTDR
    else if (bOpcode == ED_PREFIX && (bOp1 & 0xe4) == 0xa0)
    {
        m_uDataTarget = cpu.get_hl();
    }

    // CB prefix?
    else if (bOpcode == CB_PREFIX)
    {
        // DD/FD CB d 00xxxrrr = LD r, RLC|RRC|RL|RR|SLA|SRA|SLL|SRL (IX+d/IY+d)
        // DD/FD CB d xxbbbrrr = [_|BIT|RES|SET] b,(IX+d/IY+d)
        // DD/FD CB d 1xbbbrrr = LD r,[RES|SET] b,(IX+d/IY+d)
        if (fIndex)
            m_uDataTarget = wHLIXIYd;

        // CB 00ooo110 = RLC|RRC|RL|RR|SLA|SRA|SLL|SRL (HL)
        // CB oobbbrrr = [_|BIT|RES|SET] b,(HL)
        else if ((bOp1 & 0x07) == 0x06)
            m_uDataTarget = cpu.get_hl();
    }

    // Do we have something to display?
    if (m_uDataTarget != INVALID_TARGET)
    {
        if (f16Bit)
        {
            m_data_target = fmt::format("{:04X}  \aK{:04X} {:04X} {:04X}\aX {:04X} \aK{:04X} {:04X} {:04X}",
                m_uDataTarget,
                read_word(m_uDataTarget - 6), read_word(m_uDataTarget - 4), read_word(m_uDataTarget - 2),
                read_word(m_uDataTarget),
                read_word(m_uDataTarget + 2), read_word(m_uDataTarget + 4), read_word(m_uDataTarget + 6));
        }
        else
        {
            m_data_target = fmt::format("{:04X}  \aK{:02X} {:02X} {:02X} {:02X} {:02X}\aX {:02X} \aK{:02X} {:02X} {:02X} {:02X} {:02X}",
                m_uDataTarget,
                read_byte(m_uDataTarget - 5), read_byte(m_uDataTarget - 4), read_byte(m_uDataTarget - 3),
                read_byte(m_uDataTarget - 2), read_byte(m_uDataTarget - 1),
                read_byte(m_uDataTarget),
                read_byte(m_uDataTarget + 1), read_byte(m_uDataTarget + 2), read_byte(m_uDataTarget + 3),
                read_byte(m_uDataTarget + 4), read_byte(m_uDataTarget + 5));
        }
    }

    // Return whether a target has been set
    return m_uDataTarget != INVALID_TARGET;
}

////////////////////////////////////////////////////////////////////////////////

static const int TXT_COLUMNS = 64;

TxtView::TxtView(Window* pParent_)
    : View(pParent_), m_nRows(m_nHeight / ROW_HEIGHT), m_nColumns(80)
{
    SetText("Text");
    SetFont(sFixedFont);

    // Allocate enough for a full screen of characters
    m_pszData = new char[m_nRows * m_nColumns + 1];
}

void TxtView::SetAddress(uint16_t wAddr_, bool /*fForceTop_*/)
{
    View::SetAddress(wAddr_);
    m_aAccesses.clear();

    char* psz = m_pszData;
    for (int i = 0; i < m_nRows; i++)
    {
        memset(psz, ' ', m_nColumns);
        psz += sprintf(psz, "%04X", wAddr_);
        *psz++ = ' ';
        *psz++ = ' ';

        for (int j = 0; j < 64; j++)
        {
            // Remember addresses matching the last read/write access.
            if ((AddrReadPtr(wAddr_) == Memory::last_phys_read1 || AddrReadPtr(wAddr_) == Memory::last_phys_read2) ||
                (AddrWritePtr(wAddr_) == Memory::last_phys_write1 || AddrReadPtr(wAddr_) == Memory::last_phys_write2))
            {
                m_aAccesses.push_back(wAddr_);
            }

            auto b = read_byte(wAddr_++);
            *psz++ = (b >= ' ' && b <= 0x7f) ? b : '.';
        }

        *psz++ = '\0';
    }

    *psz = '\0';
}

bool TxtView::GetAddrPosition(uint16_t wAddr_, int& x_, int& y_)
{
    uint16_t wOffset = wAddr_ - GetAddress();
    int nRow = wOffset / TXT_COLUMNS;
    int nCol = wOffset % TXT_COLUMNS;

    if (nRow >= m_nRows)
        return false;

    x_ = m_nX + (4 + 2 + nCol) * CHR_WIDTH;
    y_ = m_nY + nRow * ROW_HEIGHT;

    return true;
}

void TxtView::Draw(FrameBuffer& fb)
{
    int nX, nY;

    // Change the background colour of locations matching the last read/write access.
    for (auto wAddr : m_aAccesses)
    {
        bool fRead = AddrReadPtr(wAddr) == Memory::last_phys_read1 || AddrReadPtr(wAddr) == Memory::last_phys_read2;
        bool fWrite = AddrWritePtr(wAddr) == Memory::last_phys_write1 || AddrWritePtr(wAddr) == Memory::last_phys_write2;
        uint8_t bColour = (fRead && fWrite) ? YELLOW_3 : fWrite ? RED_3 : GREEN_3;
        if (GetAddrPosition(wAddr, nX, nY))
        {
            fb.FillRect(nX - 1, nY - 1, CHR_WIDTH + 1, ROW_HEIGHT - 3, bColour);
        }
    }

    unsigned int u = 0;
    for (char* psz = m_pszData; *psz; psz += strlen(psz) + 1, u++)
    {
        int nX = m_nX;
        int nY = m_nY + ROW_HEIGHT * u;

        fb.DrawString(nX, nY, psz);
    }

    if (m_fEditing && GetAddrPosition(m_wEditAddr, nX, nY))
    {
        auto b = read_byte(m_wEditAddr);
        char ch = (b >= ' ' && b <= 0x7f) ? b : '.';

        fb.FillRect(nX - 1, nY - 1, CHR_WIDTH + 1, ROW_HEIGHT - 3, YELLOW_8);
        fb.DrawString(nX, nY, "\ak{}", ch);

        pDebugger->SetStatusByte(m_wEditAddr);
    }
    else
    {
        pDebugger->SetStatus("");
    }
}

bool TxtView::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    switch (nMessage_)
    {
    case GM_CHAR:
        return cmdNavigate(nParam1_, nParam2_);

    case GM_MOUSEWHEEL:
        return cmdNavigate((nParam1_ < 0) ? HK_UP : HK_DOWN, 0);
    }

    return false;
}

bool TxtView::cmdNavigate(int nKey_, int nMods_)
{
    auto wAddr = GetAddress();
    auto wEditAddr = m_wEditAddr;

    bool fCtrl = (nMods_ & HM_CTRL) != 0;
    bool fShift = (nMods_ & HM_SHIFT) != 0;

    switch (nKey_)
    {
        // Eat requests to select the same view
    case 't': case 'T':
        return true;

    case HK_ESC:
    case HK_RETURN:
        if (nKey_ == HK_ESC && !m_fEditing)
            return false;

        m_fEditing = !m_fEditing;
        wEditAddr = wAddr;
        break;

    case HK_HOME:
        wEditAddr = wAddr = fCtrl ? 0 : (fShift && m_fEditing) ? wEditAddr : cpu.get_pc();
        break;

    case HK_END:
        wAddr = fCtrl ? (0 - m_nRows * TXT_COLUMNS) : cpu.get_pc();
        wEditAddr = (fCtrl ? 0 : cpu.get_pc() + m_nRows * TXT_COLUMNS) - 1;
        break;

    case HK_UP:
        if (m_fEditing && !fCtrl)
            wEditAddr -= TXT_COLUMNS;
        else
            wAddr -= TXT_COLUMNS;
        break;

    case HK_DOWN:
        if (m_fEditing && !fCtrl)
            wEditAddr += TXT_COLUMNS;
        else
            wAddr += TXT_COLUMNS;
        break;

    case HK_BACKSPACE:
    case HK_LEFT:
        if (m_fEditing && !fCtrl)
            wEditAddr--;
        else
            wAddr--;
        break;

    case HK_RIGHT:
        if (m_fEditing && !fCtrl)
            wEditAddr++;
        else
            wAddr++;
        break;

    case HK_PGUP:
        wAddr -= m_nRows * TXT_COLUMNS;
        wEditAddr -= m_nRows * TXT_COLUMNS;
        break;

    case HK_PGDN:
        wAddr += m_nRows * TXT_COLUMNS;
        wEditAddr += m_nRows * TXT_COLUMNS;
        break;

    default:
    {
        // In editing mode allow new hex values to be typed
        if (m_fEditing && nKey_ >= ' ' && nKey_ <= 0x7f)
        {
            write_byte(wEditAddr, nKey_);
            wEditAddr++;
            break;
        }

        return View::cmdNavigate(nKey_, nMods_);
    }
    }

    if (m_fEditing)
    {
        if (wEditAddr != wAddr && static_cast<uint16_t>(wAddr - wEditAddr) <= TXT_COLUMNS)
            wAddr -= TXT_COLUMNS;
        else if (static_cast<uint16_t>(wEditAddr - wAddr) >= m_nRows * TXT_COLUMNS)
            wAddr += TXT_COLUMNS;

        if (m_wEditAddr != wEditAddr)
            m_wEditAddr = wEditAddr;
    }

    SetAddress(wAddr);
    return true;
}

////////////////////////////////////////////////////////////////////////////////

static const int HEX_COLUMNS = 16;

HexView::HexView(Window* pParent_)
    : View(pParent_)
{
    SetText("Numeric");
    SetFont(sFixedFont);

    m_nRows = m_nHeight / ROW_HEIGHT;
    m_nColumns = 80;

    // Allocate enough for a full screen of characters, plus null terminators
    m_pszData = new char[m_nRows * (m_nColumns + 1) + 2];
}

void HexView::SetAddress(uint16_t wAddr_, bool /*fForceTop_*/)
{
    View::SetAddress(wAddr_);
    m_aAccesses.clear();

    char* psz = m_pszData;
    for (int i = 0; i < m_nRows; i++)
    {
        memset(psz, ' ', m_nColumns);
        psz[m_nColumns - 1] = '\0';

        psz += sprintf(psz, "%04X", wAddr_);

        *psz++ = ' ';
        *psz++ = ' ';

        for (int j = 0; j < HEX_COLUMNS; j++)
        {
            // Remember addresses matching the last read/write access.
            if ((AddrReadPtr(wAddr_) == Memory::last_phys_read1 || AddrReadPtr(wAddr_) == Memory::last_phys_read2) ||
                (AddrWritePtr(wAddr_) == Memory::last_phys_write1 || AddrReadPtr(wAddr_) == Memory::last_phys_write2))
            {
                m_aAccesses.push_back(wAddr_);
            }

            auto b = read_byte(wAddr_++);
            psz[(HEX_COLUMNS - j) * 3 + 1 + j] = (b >= ' ' && b <= 0x7f) ? b : '.';
            psz += sprintf(psz, "%02X ", b);
            *psz = ' ';
        }

        psz += strlen(psz) + 1;
    }

    *psz = '\0';

    if (m_fEditing)
        pDebugger->SetStatusByte(m_wEditAddr);
    else
        pDebugger->SetStatus("");
}

bool HexView::GetAddrPosition(uint16_t wAddr_, int& x_, int& y_, int& textx_)
{
    uint16_t wOffset = wAddr_ - GetAddress();
    int nRow = wOffset / HEX_COLUMNS;
    int nCol = wOffset % HEX_COLUMNS;

    if (nRow >= m_nRows)
        return false;

    x_ = m_nX + (4 + 2 + nCol * 3) * CHR_WIDTH;
    y_ = m_nY + ROW_HEIGHT * nRow;
    textx_ = m_nX + (4 + 2 + HEX_COLUMNS * 3 + 1 + nCol) * CHR_WIDTH;

    return true;
}

void HexView::Draw(FrameBuffer& fb)
{
    int nX, nY, nTextX;

    // Change the background colour of locations matching the last read/write access.
    for (auto wAddr : m_aAccesses)
    {
        bool fRead = AddrReadPtr(wAddr) == Memory::last_phys_read1 || AddrReadPtr(wAddr) == Memory::last_phys_read2;
        bool fWrite = AddrWritePtr(wAddr) == Memory::last_phys_write1 || AddrWritePtr(wAddr) == Memory::last_phys_write2;
        uint8_t bColour = (fRead && fWrite) ? YELLOW_3 : fWrite ? RED_3 : GREEN_3;
        if (GetAddrPosition(wAddr, nX, nY, nTextX))
        {
            fb.FillRect(nX - 1, nY - 1, CHR_WIDTH * 2 + 1, ROW_HEIGHT - 3, bColour);
            fb.FillRect(nTextX - 1, nY - 1, CHR_WIDTH + 1, ROW_HEIGHT - 3, bColour);
        }
    }

    unsigned int u = 0;
    for (char* psz = m_pszData; *psz; psz += strlen(psz) + 1, u++)
    {
        int nX = m_nX;
        int nY = m_nY + ROW_HEIGHT * u;

        fb.DrawString(nX, nY, psz);
    }

    if (m_fEditing && GetAddrPosition(m_wEditAddr, nX, nY, nTextX))
    {
        auto b = read_byte(m_wEditAddr);
        auto str = fmt::format("{:02X}", b);

        if (m_fRightNibble)
            nX += CHR_WIDTH;

        fb.FillRect(nX - 1, nY - 1, CHR_WIDTH + 1, ROW_HEIGHT - 3, YELLOW_8);
        fb.DrawString(nX, nY, "\ak{}", str[m_fRightNibble]);

        char ch = (b >= ' ' && b <= 0x7f) ? b : '.';
        fb.FillRect(nTextX - 1, nY - 1, CHR_WIDTH + 1, ROW_HEIGHT - 3, GREY_6);
        fb.DrawString(nTextX, nY, "\ak{}", ch);
    }
}

bool HexView::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    switch (nMessage_)
    {
    case GM_CHAR:
        return cmdNavigate(nParam1_, nParam2_);

    case GM_MOUSEWHEEL:
        return cmdNavigate((nParam1_ < 0) ? HK_UP : HK_DOWN, 0);
    }

    return false;
}

bool HexView::cmdNavigate(int nKey_, int nMods_)
{
    auto wAddr = GetAddress();
    auto wEditAddr = m_wEditAddr;

    bool fCtrl = (nMods_ & HM_CTRL) != 0;
    bool fShift = (nMods_ & HM_SHIFT) != 0;

    switch (nKey_)
    {
        // Eat requests to select the same view
    case 'n': case 'N':
        return true;

    case HK_ESC:
    case HK_RETURN:
        if (nKey_ == HK_ESC && !m_fEditing)
            return false;

        m_fEditing = !m_fEditing;
        wEditAddr = wAddr;
        m_fRightNibble = false;
        break;

    case HK_HOME:
        wEditAddr = wAddr = fCtrl ? 0 : (fShift && m_fEditing) ? wEditAddr : cpu.get_pc();
        break;

    case HK_END:
        wAddr = fCtrl ? (0 - m_nRows * HEX_COLUMNS) : cpu.get_pc();
        wEditAddr = (fCtrl ? 0 : cpu.get_pc() + m_nRows * HEX_COLUMNS) - 1;
        break;

    case HK_UP:
        if (m_fEditing && !fCtrl)
            wEditAddr -= HEX_COLUMNS;
        else
            wAddr -= HEX_COLUMNS;
        break;

    case HK_DOWN:
        if (m_fEditing && !fCtrl)
            wEditAddr += HEX_COLUMNS;
        else
            wAddr += HEX_COLUMNS;
        break;

    case HK_BACKSPACE:
    case HK_LEFT:
        if (m_fRightNibble)
            m_fRightNibble = false;
        else if (m_fEditing && !fCtrl)
            wEditAddr--;
        else
            wAddr--;
        break;

    case HK_RIGHT:
        if (m_fEditing && !fCtrl)
            wEditAddr++;
        else
            wAddr++;
        break;

    case HK_PGUP:
        wAddr -= m_nRows * HEX_COLUMNS;
        wEditAddr -= m_nRows * HEX_COLUMNS;
        break;

    case HK_PGDN:
        wAddr += m_nRows * HEX_COLUMNS;
        wEditAddr += m_nRows * HEX_COLUMNS;
        break;

    default:
    {
        // In editing mode allow new hex values to be typed
        if (m_fEditing && nKey_ >= '0' && nKey_ <= 'f' && isxdigit(nKey_))
        {
            uint8_t bNibble = isdigit(nKey_) ? nKey_ - '0' : 10 + tolower(nKey_) - 'a';

            // Modify using the new nibble
            if (m_fRightNibble)
                write_byte(wEditAddr, (read_byte(wEditAddr) & 0xf0) | bNibble);
            else
                write_byte(wEditAddr, (read_byte(wEditAddr) & 0x0f) | (bNibble << 4));

            // Change nibble
            m_fRightNibble = !m_fRightNibble;

            // Advance to next byte if we've just completed one
            if (!m_fRightNibble)
                wEditAddr++;

            break;
        }

        return View::cmdNavigate(nKey_, nMods_);
    }
    }

    if (m_fEditing)
    {
        if (wEditAddr != wAddr && static_cast<uint16_t>(wAddr - wEditAddr) <= HEX_COLUMNS)
            wAddr -= HEX_COLUMNS;
        else if (static_cast<uint16_t>(wEditAddr - wAddr) >= m_nRows * HEX_COLUMNS)
            wAddr += HEX_COLUMNS;

        if (m_wEditAddr != wEditAddr)
        {
            m_wEditAddr = wEditAddr;
            m_fRightNibble = false;
        }
    }

    SetAddress(wAddr);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
/*
CMemView::CMemView (Window* pParent_)
    : View(pParent_)
{
}

void CMemView::SetAddress (uint16_t wAddr_, bool fForceTop_)
{
    auto psz = (uint8_t*)szDisassem;

    unsigned int len = 16384, uBlock = len >> 8;
    for (int index = 0 ; index < 256 ; index++)
    {
        unsigned int uCount = 0;
        for (unsigned int u = uBlock ; u-- ; uCount += !!read_byte(wAddr_++));
        *psz++ = (uCount + uBlock/50) * 100 / uBlock;
    }
}


void CMemView::Draw (FrameBuffer& fb)
{
    fb.SetFont(sFixedFont);

    unsigned int uGap = 12;

    for (unsigned int u = 0 ; u < 256 ; u++)
    {
        unsigned int len = (m_nHeight - uGap) * ((uint8_t*)szDisassem)[u] / 100;
        fb.DrawLine(m_nX+u, m_nY+m_nHeight-uGap-len, 0, len, (u & 16) ? WHITE : GREY_7);
    }

    fb.DrawString(m_nX, m_nY+m_nHeight-10, "Page 0: 16K in 1K units");

    fb.SetFont(sGUIFont);
}
*/

////////////////////////////////////////////////////////////////////////////////
// Graphics View

static const int STRIP_GAP = 8;
unsigned int GfxView::s_uWidth = 8, GfxView::s_uZoom = 1;
int GfxView::s_mode = 4;

GfxView::GfxView(Window* pParent_)
    : View(pParent_)
{
    SetText("Graphics");
    SetFont(sFixedFont);

    // Allocate enough space for a double-width window, at 1 byte per pixel
    m_pbData = new uint8_t[m_nWidth * m_nHeight * 2];

    s_mode = IO::ScreenMode();
}

void GfxView::SetAddress(uint16_t wAddr_, bool /*fForceTop_*/)
{
    static const unsigned int auPPB[] = { 8, 8, 2, 2 };   // Pixels Per Byte in each mode

    View::SetAddress(wAddr_);

    m_uStripWidth = s_uWidth * s_uZoom * auPPB[s_mode - 1];
    m_uStripLines = m_nHeight / s_uZoom;
    m_uStrips = (m_nWidth + STRIP_GAP + m_uStripWidth + STRIP_GAP - 1) / (m_uStripWidth + STRIP_GAP);

    auto pb = m_pbData;
    for (unsigned int u = 0; u < ((m_uStrips + 1) * m_uStripLines); u++)
    {
        switch (s_mode)
        {
        case 1:
        case 2:
        {
            for (unsigned int v = 0; v < s_uWidth; v++)
            {
                auto b = read_byte(wAddr_++);

                uint8_t bGridBg = m_fGrid ? BLUE_1 : BLACK;
                uint8_t bBg0 = (u & 1) ? BLACK : bGridBg;
                uint8_t bBg1 = (u & 1) ? bGridBg : BLACK;

                memset(pb, (b & 0x80) ? WHITE : bBg0, s_uZoom); pb += s_uZoom;
                memset(pb, (b & 0x40) ? WHITE : bBg1, s_uZoom); pb += s_uZoom;
                memset(pb, (b & 0x20) ? WHITE : bBg0, s_uZoom); pb += s_uZoom;
                memset(pb, (b & 0x10) ? WHITE : bBg1, s_uZoom); pb += s_uZoom;
                memset(pb, (b & 0x08) ? WHITE : bBg0, s_uZoom); pb += s_uZoom;
                memset(pb, (b & 0x04) ? WHITE : bBg1, s_uZoom); pb += s_uZoom;
                memset(pb, (b & 0x02) ? WHITE : bBg0, s_uZoom); pb += s_uZoom;
                memset(pb, (b & 0x01) ? WHITE : bBg1, s_uZoom); pb += s_uZoom;
            }
            break;
        }

        case 3:
        {
            for (unsigned int v = 0; v < s_uWidth; v++)
            {
                auto b = read_byte(wAddr_++);

                // To keep things simple, draw only the odd pixels
                
                memset(pb, IO::Mode3Clut((b & 0x30) >> 4), s_uZoom); pb += s_uZoom;
                memset(pb, IO::Mode3Clut((b & 0x03) >> 0), s_uZoom); pb += s_uZoom;
            }
            break;
        }

        case 4:
        {
            for (unsigned int v = 0; v < s_uWidth; v++)
            {
                auto b = read_byte(wAddr_++);

                memset(pb, IO::State().clut[b >> 4], s_uZoom); pb += s_uZoom;
                memset(pb, IO::State().clut[b & 0xf], s_uZoom); pb += s_uZoom;
            }
            break;
        }
        }
    }

    auto status = fmt::format("{:04X}  Mode {}  Width {}  Zoom {}x", GetAddress(), s_mode, s_uWidth, s_uZoom);
    pDebugger->SetStatus(status, false, sFixedFont);

}

void GfxView::Draw(FrameBuffer& fb)
{
    fb.ClipTo(m_nX, m_nY, m_nWidth, m_nHeight);

    auto pb = m_pbData;

    for (unsigned int u = 0; u < m_uStrips + 1; u++)
    {
        int nX = m_nX + u * (m_uStripWidth + STRIP_GAP);
        int nY = m_nY;

        for (unsigned int v = 0; v < m_uStripLines; v++, pb += m_uStripWidth)
        {
            for (unsigned int w = 0; w < s_uZoom; w++, nY++)
            {
                fb.Poke(nX, nY, pb, m_uStripWidth);
            }
        }
    }

    fb.ClipNone();
}

bool GfxView::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    switch (nMessage_)
    {
    case GM_CHAR:
        return cmdNavigate(nParam1_, nParam2_);

    case GM_MOUSEWHEEL:
        return cmdNavigate((nParam1_ < 0) ? HK_PGUP : HK_PGDN, 0);
    }

    return false;
}

bool GfxView::cmdNavigate(int nKey_, int nMods_)
{
    auto wAddr = GetAddress();
    bool fCtrl = (nMods_ & HM_CTRL) != 0;

    switch (nKey_)
    {
        // Keys 1 to 4 select the screen mode
    case '1': case '2': case '3': case '4':
        s_mode = nKey_ - '0';
        if (s_mode < 3 && s_uWidth > 32U) s_uWidth = 32U;  // Clip width in modes 1+2
        break;

        // Toggle grid view in modes 1+2
    case 'g': case 'G':
        m_fGrid = !m_fGrid;
        break;

    case HK_HOME:
        wAddr = fCtrl ? 0 : cpu.get_pc();
        break;

    case HK_END:
        wAddr = fCtrl ? (static_cast<uint16_t>(0) - m_uStrips * m_uStripLines * s_uWidth) : cpu.get_pc();
        break;

    case HK_UP:
        if (!fCtrl)
            wAddr -= s_uWidth;
        else if (s_uZoom < 16)
            s_uZoom++;
        break;

    case HK_DOWN:
        if (!fCtrl)
            wAddr += s_uWidth;
        else if (s_uZoom > 1)
            s_uZoom--;
        break;

    case HK_LEFT:
        if (!fCtrl)
            wAddr--;
        else if (s_uWidth > 1)
            s_uWidth--;
        break;

    case HK_RIGHT:
        if (!fCtrl)
            wAddr++;
        else if (s_uWidth < ((s_mode < 3) ? 32U : 128U))   // Restrict byte width to mode limit
            s_uWidth++;
        break;

    case HK_PGUP:
        if (!fCtrl)
            wAddr -= m_uStrips * m_uStripLines * s_uWidth;
        else
            wAddr -= m_uStripLines * s_uWidth;
        break;

    case HK_PGDN:
        if (!fCtrl)
            wAddr += m_uStrips * m_uStripLines * s_uWidth;
        else
            wAddr += m_uStripLines * s_uWidth;
        break;

    default:
        return View::cmdNavigate(nKey_, nMods_);
    }

    SetAddress(wAddr, true);
    return true;
}


////////////////////////////////////////////////////////////////////////////////
// Breakpoint View

BptView::BptView(Window* pParent_)
    : View(pParent_)
{
    SetText("Breakpoints");
    SetFont(sFixedFont);

    m_nRows = (m_nHeight / ROW_HEIGHT) - 1;

    // Allocate enough for a full screen of characters, plus null terminators
    m_pszData = new char[m_nRows * 81 + 2];
    m_pszData[0] = '\0';
}

void BptView::SetAddress(uint16_t wAddr_, bool /*fForceTop_*/)
{
    View::SetAddress(wAddr_);

    char* psz = m_pszData;

    if (Breakpoint::breakpoints.empty())
        psz += sprintf(psz, "No breakpoints") + 1;
    else
    {
        int index = 0;

        m_nLines = 0;
        m_nActive = -1;

        for (const auto& bp : Breakpoint::breakpoints)
        {
            psz += sprintf(psz, "%2d: %s", index, to_string(bp).c_str()) + 1;
            m_nLines++;

            // Check if we're on an execution breakpoint (ignoring condition)
            auto pPhys = AddrReadPtr(cpu.get_pc());
            if (bp.type == BreakType::Execute)
            {
                if (auto exec = std::get_if<BreakExec>(&bp.data))
                {
                    if (exec->phys_addr == pPhys)
                        m_nActive = index;
                }
            }

            index++;
        }
    }

    // Double null to terminate text
    *psz++ = '\0';
}


void BptView::Draw(FrameBuffer& fb)
{
    int i;
    char* psz = m_pszData;

    for (i = 0; i < m_nTopLine && *psz; i++)
        psz += strlen(psz) + 1;

    for (i = 0; i < m_nRows && *psz; i++)
    {
        int nX = m_nX;
        int nY = m_nY + ROW_HEIGHT * i;

        auto pBreak = Breakpoint::GetAt(m_nTopLine + i);
        auto colour = (m_nTopLine + i == m_nActive) ? CYAN_7 : (pBreak && !pBreak->enabled) ? GREY_4 : WHITE;
        fb.DrawString(nX, nY, colour, psz);
        psz += strlen(psz) + 1;
    }

    DisView::DrawRegisterPanel(fb, m_nX + m_nWidth - 6 * 16, m_nY);
}

bool BptView::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    switch (nMessage_)
    {
    case GM_BUTTONDBLCLK:
    {
        int nIndex = (nParam2_ - m_nY) / ROW_HEIGHT;

        if (IsOver() && nIndex >= 0 && nIndex < m_nLines)
        {
            auto pBreak = Breakpoint::GetAt(nIndex);
            pBreak->enabled = !pBreak->enabled;
        }
        break;
    }

    case GM_CHAR:
        return cmdNavigate(nParam1_, nParam2_);

    case GM_MOUSEWHEEL:
        return cmdNavigate((nParam1_ < 0) ? HK_UP : HK_DOWN, 0);
    }

    return false;
}

bool BptView::cmdNavigate(int nKey_, int nMods_)
{
    switch (nKey_)
    {
        // Eat requests to select the same view
    case 'b': case 'B':
        return true;

    case HK_HOME:   m_nTopLine = 0; break;
    case HK_END:    m_nTopLine = m_nLines; break;

    case HK_UP:     m_nTopLine--; break;
    case HK_DOWN:   m_nTopLine++; break;

    case HK_PGUP:   m_nTopLine -= m_nRows; break;
    case HK_PGDN:   m_nTopLine += m_nRows; break;

    default:
        return View::cmdNavigate(nKey_, nMods_);
    }

    if (m_nTopLine < 0)
        m_nTopLine = 0;
    else if (m_nTopLine > m_nLines - m_nRows)
        m_nTopLine = m_nLines - m_nRows;

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Trace View

constexpr auto TRACE_ADDR_LABEL_LENGTH = 16;
constexpr auto TRACE_DIS_LABEL_LENGTH = 12;
constexpr auto TRACE_DIS_LENGTH = 18;

TrcView::TrcView(Window* pParent_)
    : TextView(pParent_)
{
    SetText("Trace");
    SetLines(std::min(nNumTraces, TRACE_SLOTS));
    cmdNavigate(HK_END, 0);
}

void TrcView::DrawLine(FrameBuffer& fb, int nX_, int nY_, int nLine_)
{
    if (GetLines() <= 1)
        fb.DrawString(nX_, nY_, "No instruction trace");
    else
    {
        char szDis[32]{};
        char sz[128]{}, *psz = sz;

        int nPos = (nNumTraces - GetLines() + 1 + nLine_ + TRACE_SLOTS) % TRACE_SLOTS;
        auto pTD = &aTrace[nPos];

        Disassemble(pTD->abInstr, pTD->wPC, szDis, sizeof(szDis), m_use_symbols ? TRACE_DIS_LABEL_LENGTH : 0);
        auto colour_len = static_cast<int>(strlen(szDis)) - fb.StringLength(szDis);

        if (m_use_symbols)
        {
            auto sName = Symbol::LookupAddr(pTD->wPC, pTD->wPC, TRACE_ADDR_LABEL_LENGTH);
            psz += sprintf(psz, "\ab%*s\aX %-*s", TRACE_ADDR_LABEL_LENGTH, sName.c_str(), TRACE_DIS_LENGTH + colour_len, szDis);
        }
        else
        {
            psz += sprintf(psz, " %04X            %-*s", pTD->wPC, TRACE_DIS_LENGTH + colour_len, szDis);
        }

        if (nLine_ != GetLines() - 1)
        {
            int nPos0 = (nPos + 0 + TRACE_SLOTS) % TRACE_SLOTS;
            int nPos1 = (nPos + 1 + TRACE_SLOTS) % TRACE_SLOTS;
            TRACEDATA* p0 = &aTrace[nPos0];
            TRACEDATA* p1 = &aTrace[nPos1];

            // Macro-tastic!
#define CHG(r)    (p1->regs.get_##r() != p0->regs.get_##r())
#define CHG_H(rr)  (z80::get_high8(p1->regs.get_##rr()) != z80::get_high8(p0->regs.get_##rr()))
#define CHG_L(rr)  (z80::get_low8(p1->regs.get_##rr()) != z80::get_low8(p0->regs.get_##rr()))

#define CHK_S(r,RL)     if (CHG(r)) PRINT_S(RL,r);
#define CHK(rr,RH,RL)   if (CHG_H(rr) && !CHG_L(rr)) PRINT_H(RH,rr);      \
                        else if (!CHG_H(rr) && CHG_L(rr)) PRINT_L(RL,rr); \
                        else if (CHG(rr)) PRINT_D(RH RL,rr)

#define PRINT_H(N,rr)  psz += sprintf(psz, "\ag" N "\aX %02X->%02X  ", static_cast<unsigned>(z80::get_high8(p0->regs.get_##rr())), static_cast<unsigned>(z80::get_high8(p1->regs.get_##rr())))
#define PRINT_L(N,rr)  psz += sprintf(psz, "\ag" N "\aX %02X->%02X  ", static_cast<unsigned>(z80::get_low8(p0->regs.get_##rr())), static_cast<unsigned>(z80::get_low8(p1->regs.get_##rr())))
#define PRINT_S(N,r)    psz += sprintf(psz, "\ag" N "\aX %02X->%02X  ", static_cast<unsigned>(p0->regs.get_##r()), static_cast<unsigned>(p1->regs.get_##r()))
#define PRINT_D(N,rr)    psz += sprintf(psz, "\ag" N "\aX %04X->%04X  ", static_cast<unsigned>(p0->regs.get_##rr()), static_cast<unsigned>(p1->regs.get_##rr()))

            // Special check for EXX, as we don't have room for all changes
            if ((CHG(bc) && CHG(alt_bc)) ||
                (CHG(de) && CHG(alt_de)) ||
                (CHG(hl) && CHG(alt_hl)))
            {
                psz += sprintf(psz, "BC/DE/HL <=> BC'/DE'/HL'");
            }
            // Same for BC+DE+HL changing in block instructions
            else if (CHG(bc) && CHG(de) && CHG(hl))
            {
                psz += sprintf(psz, "\agBC\aX->%04X \agDE\aX->%04X \agHL\aX->%04X",
                    static_cast<unsigned>(cpu.get_bc()),
                    static_cast<unsigned>(cpu.get_de()),
                    static_cast<unsigned>(cpu.get_hl()));
            }
            else
            {
                if (CHG(sp))
                {
                    if (CHG(af)) PRINT_D("AF", af);
                    if (CHG(bc)) PRINT_D("BC", bc);
                    if (CHG(de)) PRINT_D("DE", de);
                    if (CHG(hl)) PRINT_D("HL", hl);
                }
                else
                {
                    if (CHG_H(af)) PRINT_H("A", af);

                    if (m_double_regs)
                    {
                        if (CHG(bc)) PRINT_D("BC", bc);
                        if (CHG(de)) PRINT_D("DE", de);
                        if (CHG(hl)) PRINT_D("HL", hl);
                    }
                    else
                    {
                        CHK(bc, "B", "C");
                        CHK(de, "D", "E");
                        CHK(hl, "H", "L");
                    }
                }

                if (CHG(ix)) PRINT_D("IX", ix);
                if (CHG(iy)) PRINT_D("IY", iy);
                if (CHG(sp)) PRINT_D("SP", sp);

                CHK_S(i, "I");
                CHK_S(int_mode, "IM");
            }
        }

        auto colour = "W";

        if (nLine_ == GetLines() - 1)
        {
            fb.FillRect(nX_ - 1, nY_ - 1, m_nWidth - 112, ROW_HEIGHT - 3, YELLOW_7);
            colour = "k\a0";
        }

        fb.DrawString(nX_, nY_, "\a{}{}", colour, sz);
    }
}

bool TrcView::cmdNavigate(int nKey_, int nMods_)
{
    if (nKey_ == HK_SPACE)
        m_double_regs = !m_double_regs;
    else if (nKey_ == 's')
        m_use_symbols = !m_use_symbols;

    return TextView::cmdNavigate(nKey_, nMods_);
}

void TrcView::OnDblClick(int nLine_)
{
    int nPos = (nNumTraces - GetLines() + 1 + GetTopLine() + nLine_ + TRACE_SLOTS) % TRACE_SLOTS;
    TRACEDATA* pTD = &aTrace[nPos];

    View::SetAddress(pTD->wPC, true);
    pDebugger->SetView(ViewType::Dis);
}

void TrcView::OnDelete()
{
    nNumTraces = 0;
    SetLines(0);
}
