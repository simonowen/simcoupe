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
#include "Frame.h"
#include "Keyboard.h"
#include "Memory.h"
#include "Options.h"
#include "Symbol.h"
#include "Util.h"


// SAM BASIC keywords tokens
static const std::vector<const char*> basic_keywords
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
    uint16_t wPC;                         // PC value
    uint8_t abInstr[MAX_Z80_INSTR_LEN];  // Instruction at PC
    Z80Regs regs;                     // Register values
};


Debugger* pDebugger;

// Stack position used to track stepping out
int nStepOutSP = -1;

// Last position of debugger window and last register values
int nDebugX, nDebugY;
Z80Regs sLastRegs, sCurrRegs;
uint8_t bLastStatus;
uint32_t dwLastCycle;
int nLastFrames;
ViewType nLastView = vtDis;
uint16_t wLastAddr;

// Instruction tracing
#define TRACE_SLOTS 1000
TRACEDATA aTrace[TRACE_SLOTS];
int nNumTraces;


namespace Debug
{

// Activate the debug GUI, if not already active
bool Start(BREAKPT* pBreak_)
{
    // Restore memory contention in case of a timing measurement
    CPU::UpdateContention();

    // Reset the last entry counters, unless we're started from a triggered breakpoint
    if (!pBreak_ && nStepOutSP == -1)
    {
        nLastFrames = 0;
        dwLastCycle = g_dwCycleCounter;

        R = (R7 & 0x80) | (R & 0x7f);
        sLastRegs = sCurrRegs = regs;
        bLastStatus = status_reg;

        // If there's no breakpoint set any existing trace is meaningless
        if (!Breakpoint::IsSet())
        {
            // Set the previous entry to have the current register values
            aTrace[nNumTraces = 0].regs = regs;

            // Add the current location as the only entry
            BreakpointHit();
        }

        // Is drive 1 a floppy drive with a disk in it?
        if (GetOption(drive1) == drvFloppy && pFloppy1->HasDisk())
        {
            std::string strPath = pFloppy1->DiskPath();

            // Strip any file extension from the end
            size_t nExt = strPath.rfind(".");
            if (nExt != std::string::npos)
                strPath = strPath.substr(0, nExt);

            // Attempt to load user symbols from a corresponding .map file
            Symbol::Update(strPath.append(".map").c_str());
        }
        else
        {
            // Unload user symbols if there's no drive or disk
            Symbol::Update(nullptr);
        }
    }

    // Stop any existing debugger instance
    GUI::Stop();

    // Create the main debugger window, passing any breakpoint
    if (!GUI::Start(pDebugger = new Debugger(pBreak_)))
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
        pDebugger->SetAddress((nLastView == vtDis) ? PC : wLastAddr, false);

        // Re-test breakpoints for the likely changed location
        BreakpointHit();
    }
}

// Called on every RETurn, for step-out implementation
void OnRet()
{
    // Step-out in progress?
    if (nStepOutSP != -1)
    {
        // If the stack is at or just above the starting position, it should mean we've returned
        // Allow some generous slack for data that may have been on the stack above the address
        if ((SP - nStepOutSP) >= 0 && (SP - nStepOutSP) < 64)
            Start();
    }
}

bool RetZHook()
{
    // Are we in in HDNSTP in ROM1, about to start an auto-executing code file?
    if (PC == 0xe294 && GetSectionPage(SECTION_D) == ROM1 && !(F & FLAG_Z))
    {
        // If the option is enabled, set a temporary breakpoint for the start
        if (GetOption(breakonexec))
            Breakpoint::AddTemp(nullptr, Expr::Compile("autoexec"));
    }

    // Continue normal processing
    return false;
}


// Return whether the debug GUI is active
bool IsActive()
{
    return pDebugger != nullptr;
}

// Return whether any breakpoints are active
bool IsBreakpointSet()
{
    return Breakpoint::IsSet();
}

// Return whether any of the active breakpoints have been hit
bool BreakpointHit()
{
    // Add a new trace entry if PC has changed
    if (aTrace[nNumTraces % TRACE_SLOTS].wPC != PC)
    {
        TRACEDATA* p = &aTrace[(++nNumTraces) % TRACE_SLOTS];
        p->wPC = PC;
        p->abInstr[0] = read_byte(PC);
        p->abInstr[1] = read_byte(PC + 1);
        p->abInstr[2] = read_byte(PC + 2);
        p->abInstr[3] = read_byte(PC + 3);
        p->regs = regs;
    }

    return Breakpoint::IsHit();
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
    for (wPC = PC; ((bOpcode = read_byte(wPC)) == IX_PREFIX || bOpcode == IY_PREFIX); wPC++);

    // Stepping into a HALT (with interrupt enabled) will enter the appropriate interrupt handler
    // This is much friendlier than single-stepping NOPs up to the next interrupt!
    if (nCount_ == 1 && bOpcode == OP_HALT && IFF1 && !fCtrl_)
    {
        // For IM 2, form the address of the handler and break there
        if (IM == 2)
            pPhysAddr = AddrReadPtr(read_word((I << 8) | 0xff));

        // IM 0 and IM1 both use the handler at 0x0038
        else
            pPhysAddr = AddrReadPtr(IM1_INTERRUPT_HANDLER);
    }

    // If an address has been set, execute up to it
    if (pPhysAddr)
        Breakpoint::AddTemp(pPhysAddr, nullptr);

    // Otherwise execute the requested number of instructions
    else
    {
        Expr::nCount = nCount_;
        Breakpoint::AddTemp(nullptr, &Expr::Counter);
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
        // Set minimal contention
        CPU::UpdateContention(false);

        // Round to the next 4T contention boundary to eliminate any slack on the next opcode fetch
        g_dwCycleCounter |= 3;
    }

    // Skip any index prefixes on the instruction to reach a CB/ED prefix or the real opcode
    for (wPC = PC; ((bOpcode = read_byte(wPC)) == IX_PREFIX || bOpcode == IY_PREFIX); wPC++);
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
        Breakpoint::AddTemp(pPhysAddr, nullptr);
        Debug::Stop();
    }
}

void cmdStepOut()
{
    // Store the current stack pointer, for checking on RETurn calls
    nStepOutSP = SP;
    Debug::Stop();
}

////////////////////////////////////////////////////////////////////////////////

InputDialog::InputDialog(Window* pParent_/*=nullptr*/, const char* pcszCaption_, const char* pcszPrompt_, PFNINPUTPROC pfnNotify_)
    : Dialog(pParent_, 0, 0, pcszCaption_), m_pfnNotify(pfnNotify_)
{
    // Get the length of the prompt string, so we can position the edit box correctly
    int n = GetTextWidth(pcszPrompt_);

    // Create the prompt text control and input edit control
    new TextControl(this, 5, 10, pcszPrompt_, WHITE);
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
        const char* pcszExpr = m_pInput->GetText();
        EXPR* pExpr = Expr::Compile(pcszExpr);

        // Close the dialog if the input was blank, or the notify handler tells us
        if (!*pcszExpr || (pExpr && m_pfnNotify(pExpr)))
        {
            Destroy();
            Expr::Release(pExpr);
            pDebugger->Refresh();
        }
    }
}


// Notify handler for New Address input
static bool OnAddressNotify(EXPR* pExpr_)
{
    int nAddr = Expr::Eval(pExpr_);
    pDebugger->SetAddress(nAddr, true);
    return true;
}

// Notify handler for Execute Until expression
static bool OnUntilNotify(EXPR* pExpr_)
{
    if (pExpr_->nType == T_NUMBER && !pExpr_->pNext)
    {
        std::stringstream ss;
        ss << "PC==" << std::hex << pExpr_->nValue;
        Expr::Release(pExpr_);
        pExpr_ = Expr::Compile(ss.str().c_str());
    }

    Breakpoint::AddTemp(nullptr, pExpr_);
    Debug::Stop();
    return false;
}

// Notify handler for Change Lmpr input
static bool OnLmprNotify(EXPR* pExpr_)
{
    int nPage = Expr::Eval(pExpr_) & LMPR_PAGE_MASK;
    IO::OutLmpr((lmpr & ~LMPR_PAGE_MASK) | nPage);
    return true;
}

// Notify handler for Change Hmpr input
static bool OnHmprNotify(EXPR* pExpr_)
{
    int nPage = Expr::Eval(pExpr_) & HMPR_PAGE_MASK;
    IO::OutHmpr((hmpr & ~HMPR_PAGE_MASK) | nPage);
    return true;
}

// Notify handler for Change Lmpr input
static bool OnLeprNotify(EXPR* pExpr_)
{
    IO::OutLepr(Expr::Eval(pExpr_));
    return true;
}

// Notify handler for Change Hepr input
static bool OnHeprNotify(EXPR* pExpr_)
{
    IO::OutHepr(Expr::Eval(pExpr_));
    return true;
}

// Notify handler for Change Vmpr input
static bool OnVmprNotify(EXPR* pExpr_)
{
    int nPage = Expr::Eval(pExpr_) & VMPR_PAGE_MASK;
    IO::OutVmpr(VMPR_MODE | nPage);
    return true;
}

// Notify handler for Change Mode input
static bool OnModeNotify(EXPR* pExpr_)
{
    int nMode = Expr::Eval(pExpr_);
    if (nMode < 1 || nMode > 4)
        return false;

    IO::OutVmpr(((nMode - 1) << 5) | VMPR_PAGE);
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
    switch (nKey_)
    {
    case HK_KP7:  cmdStep(1, nMods_ != HM_NONE); break;
    case HK_KP8:  cmdStepOver(nMods_ == HM_CTRL); break;
    case HK_KP9:  cmdStepOut();     break;
    case HK_KP4:  cmdStep(10);      break;
    case HK_KP5:  cmdStep(100);     break;
    case HK_KP6:  cmdStep(1000);    break;

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

Debugger::Debugger(BREAKPT* pBreak_/*=nullptr*/)
    : Dialog(nullptr, 433, 260 + 36 + 2, "")
{
    // Move to the last display position, if any
    if (nDebugX | nDebugY)
        Move(nDebugX, nDebugY);

    // Create the status text control
    m_pStatus = new TextControl(this, 4, m_nHeight - ROW_HEIGHT, "");

    // If a breakpoint was supplied, report that it was triggered
    if (pBreak_)
    {
        char sz[128] = {};

        if (pBreak_->nType != btTemp)
            snprintf(sz, sizeof(sz) - 1, "\aYBreakpoint %d hit:  %s", Breakpoint::GetIndex(pBreak_), Breakpoint::GetDesc(pBreak_));
        else
        {
            if (pBreak_->pExpr && pBreak_->pExpr != &Expr::Counter)
                snprintf(sz, sizeof(sz) - 1, "\aYUNTIL condition met:  %s", pBreak_->pExpr->pcszExpr);
        }

        SetStatus(sz, true, sPropFont);
    }

    // Remove all temporary breakpoints
    for (int i = 0; (pBreak_ = Breakpoint::GetAt(i)); i++)
    {
        if (pBreak_->nType == btTemp)
        {
            Breakpoint::RemoveAt(i);
            i--;
        }
    }

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
    sLastRegs = regs;
    bLastStatus = status_reg;

    // Save the cycle counter for timing comparisons
    dwLastCycle = g_dwCycleCounter;
    nLastFrames = 0;

    // Clear any cached data that could cause an immediate retrigger
    wPortRead = wPortWrite = 0;
    pbMemRead1 = pbMemRead2 = pbMemWrite1 = pbMemWrite2 = nullptr;

    // Debugger is gone
    pDebugger = nullptr;
}

void Debugger::SetSubTitle(const char* pcszSubTitle_)
{
    char szTitle[128] = "SimICE";

    if (pcszSubTitle_ && *pcszSubTitle_)
    {
        strcat(szTitle, " -- ");
        strncat(szTitle, pcszSubTitle_, sizeof(szTitle) - strlen(szTitle) - 1);
        szTitle[sizeof(szTitle) - 1] = '\0';
    }

    SetText(szTitle);
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
    case vtDis:
        pNewView = new DisView(this);
        break;

    case vtTxt:
        pNewView = new TxtView(this);
        break;

    case vtHex:
        pNewView = new HexView(this);
        break;

    case vtGfx:
        pNewView = new GfxView(this);
        break;

    case vtBpt:
        pNewView = new BptView(this);
        break;

    case vtTrc:
        pNewView = new TrcView(this);
        break;
    }

    // New view created?
    if (pNewView)
    {
        SetSubTitle(pNewView->GetText());

        if (!m_pView)
        {
            pNewView->SetAddress((nView_ == vtDis) ? PC : wLastAddr);
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

void Debugger::SetStatus(const char* pcsz_, bool fOneShot_, std::shared_ptr<Font> font)
{
    if (m_pStatus)
    {
        // One-shot status messages get priority
        if (fOneShot_)
            m_sStatus = pcsz_;
        else if (!m_sStatus.empty())
            return;

        if (font)
            m_pStatus->SetFont(font);

        m_pStatus->SetText(pcsz_);
    }
}

void Debugger::SetStatusByte(uint16_t wAddr_)
{
    size_t i;
    char szBinary[9] = {};
    const char* pcszKeyword = "";

    // Read byte at status location
    auto b = read_byte(wAddr_);

    // Change unprintable characters to a space
    char ch = (b >= ' ' && b <= 0x7f) ? b : ' ';

    // Keyword range?
    if (b >= BASE_KEYWORD)
        pcszKeyword = basic_keywords[b - BASE_KEYWORD];

    // Generate binary representation
    for (i = 128; i > 0; i >>= 1)
        strcat(szBinary, (b & i) ? "1" : "0");

    // Form full status line, and set it
    char sz[128] = {};
    snprintf(sz, sizeof(sz) - 1, "%04X  %02X  %03d  %s  %c  %s", wAddr_, b, b, szBinary, ch, pcszKeyword);
    SetStatus(sz, false, sFixedFont);
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
        // Form the R register from its working components, and save the current register state
        R = (R7 & 0x80) | (R & 0x7f);
        sCurrRegs = regs;

        // Set the last view
        SetView(nLastView);
    }

    Dialog::Draw(fb);
}

bool Debugger::OnMessage(int nMessage_, int nParam1_, int nParam2_)
{
    char sz[64];
    bool fRet = false;

    if (!fRet && nMessage_ == GM_CHAR)
    {
        fRet = true;

        // Force lower-case
        nParam1_ = tolower(nParam1_);

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
            else if (nLastView != vtDis)
            {
                SetView(vtDis);
                SetAddress(PC);
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
            SetView(vtBpt);
            break;

        case 'c':
            SetView(vtTrc);
            break;

        case 'd':
            SetView(vtDis);
            break;

        case 't':
            if (fCtrl)
                s_fTransparent = !s_fTransparent;
            else
                SetView(vtTxt);
            break;

        case 'n':
            SetView(vtHex);
            break;

        case 'g':
            SetView(vtGfx);
            break;

        case 'l':
            if (fShift)
            {
                sprintf(sz, "Change LEPR [%02X]:", lepr);
                new InputDialog(this, sz, "New Page:", OnLeprNotify);
            }
            else
            {
                sprintf(sz, "Change LMPR [%02X]:", lmpr & LMPR_PAGE_MASK);
                new InputDialog(this, sz, "New Page:", OnLmprNotify);
            }
            break;

        case 'h':
            if (fShift)
            {
                sprintf(sz, "Change HEPR [%02X]:", hepr);
                new InputDialog(this, sz, "New Page:", OnHeprNotify);
            }
            else
            {
                sprintf(sz, "Change HMPR [%02X]:", hmpr & HMPR_PAGE_MASK);
                new InputDialog(this, sz, "New Page:", OnHmprNotify);
            }
            break;

        case 'v':
            sprintf(sz, "Change VMPR [%02X]:", vmpr & VMPR_PAGE_MASK);
            new InputDialog(this, sz, "New Page:", OnVmprNotify);
            break;

        case 'm':
            sprintf(sz, "Change Mode [%X]:", ((vmpr & VMPR_MODE_MASK) >> 5) + 1);
            new InputDialog(this, sz, "New Mode:", OnModeNotify);
            break;

        case 'u':
            new InputDialog(this, "Execute until", "Expression:", OnUntilNotify);
            break;

        case HK_KP0:
            IO::OutLmpr(lmpr ^ LMPR_ROM0_OFF);
            break;

        case HK_KP1:
            IO::OutLmpr(lmpr ^ LMPR_ROM1);
            break;

        case HK_KP2:
            IO::OutLmpr(lmpr ^ LMPR_WPROT);
            break;

        case HK_KP3:
            IO::OutHmpr(hmpr ^ HMPR_MCNTRL_MASK);
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
    // Command submitted?
    if (pWindow_ == m_pCommandEdit && nParam_ == 1)
    {
        const char* pcsz = m_pCommandEdit->GetText();

        // If no command is given, close the command bar
        if (!*pcsz)
        {
            m_pCommandEdit->Destroy();
            m_pCommandEdit = nullptr;
        }
        // Otherwise execute the command, and if successful, clear the command text
        else if (Execute(pcsz))
        {
            m_pCommandEdit->SetText("");
            Refresh();
        }
    }
}


AccessType GetAccessParam(const char* pcsz_)
{
    if (!strcasecmp(pcsz_, "r"))
        return atRead;
    else if (!strcasecmp(pcsz_, "w"))
        return atWrite;
    else if (!strcasecmp(pcsz_, "rw"))
        return atReadWrite;

    return atNone;
}

bool Debugger::Execute(const char* pcszCommand_)
{
    bool fRet = true;

    char* psz = nullptr;

    char szCommand[256] = {};
    strncpy(szCommand, pcszCommand_, sizeof(szCommand) - 1);
    char* pszCommand = strtok(szCommand, " ");
    if (!pszCommand)
        return false;

    // Locate any parameter, stripping leading spaces
    char* pszParam = pszCommand + strlen(pszCommand) + 1;
    for (; *pszParam == ' '; pszParam++);
    bool fCommandOnly = !*pszParam;

    // Evaluate the parameter as an expression
    char* pszExprEnd = nullptr;
    EXPR* pExpr = Expr::Compile(pszParam, &pszExprEnd);
    int nParam = Expr::Eval(pExpr);

    // nop
    if (fCommandOnly && (!strcasecmp(pszCommand, "nop")))
    {
        // Does exactly what it says on the tin
    }

    // quit  or  q
    else if (fCommandOnly && (!strcasecmp(pszCommand, "q") || !strcasecmp(pszCommand, "quit")))
    {
        Debug::Stop();
        return false;
    }

    // di
    else if (fCommandOnly && !strcasecmp(pszCommand, "di"))
    {
        IFF1 = IFF2 = 0;
    }

    // ei
    else if (fCommandOnly && !strcasecmp(pszCommand, "ei"))
    {
        IFF1 = IFF2 = 1;
    }

    // im 0|1|2
    else if (!strcasecmp(pszCommand, "im"))
    {
        if (nParam >= 0 && nParam <= 2 && !*pszExprEnd)
            IM = nParam;
        else
            fRet = false;
    }

    // reset
    else if (fCommandOnly && !strcasecmp(pszCommand, "reset"))
    {
        CPU::Reset(true);
        CPU::Reset(false);

        nLastFrames = 0;
        dwLastCycle = g_dwCycleCounter;

        SetAddress(PC);
    }

    // nmi
    else if (fCommandOnly && !strcasecmp(pszCommand, "nmi"))
    {
        CPU::NMI();
        SetAddress(PC);
    }

    // zap
    else if (fCommandOnly && !strcasecmp(pszCommand, "zap"))
    {
        // Disassemble the current instruction
        uint8_t ab[MAX_Z80_INSTR_LEN] = { read_byte(PC), read_byte(PC + 1), read_byte(PC + 2), read_byte(PC + 3) };
        auto uLen = Disassemble(ab, PC);

        // Replace instruction with the appropriate number of NOPs
        for (unsigned int u = 0; u < uLen; u++)
            write_byte(PC + u, OP_NOP);
    }

    // call addr
    else if (!strcasecmp(pszCommand, "call") && nParam != -1 && !*pszExprEnd)
    {
        SP -= 2;
        write_word(SP, PC);
        SetAddress(PC = nParam);
    }

    // ret
    else if (fCommandOnly && !strcasecmp(pszCommand, "ret"))
    {
        SetAddress(PC = read_word(SP));
        SP += 2;
    }

    // push value
    else if (!strcasecmp(pszCommand, "push") && nParam != -1 && !*pszExprEnd)
    {
        SP -= 2;
        write_word(SP, nParam);
    }

    // pop [register]
    else if (!strcasecmp(pszCommand, "pop"))
    {
        // Re-parse the first argument as a register
        Expr::Release(pExpr);
        pExpr = Expr::Compile(pszParam, &pszExprEnd, Expr::regOnly);

        if (fCommandOnly)
            SP += 2;
        else if (pExpr && pExpr->nType == T_REGISTER && !pExpr->pNext && !*pszExprEnd)
        {
            Expr::SetReg(pExpr->nValue, read_word(SP));
            SP += 2;
        }
        else
            fRet = false;
    }

    // break
    else if (fCommandOnly && !strcasecmp(pszCommand, "break"))
    {
        // EI, IM 1, force NMI (super-break)
        IFF1 = 1;
        IM = 1;
        PC = NMI_INTERRUPT_HANDLER;

        // Set up SAM BASIC paging
        IO::Out(LMPR_PORT, 0x1f);
        IO::Out(HMPR_PORT, 0x01);

        Debug::Stop();
        return false;
    }
    /*
        // step [count]
        else if (!strcasecmp(pszCommand, "s") || !strcasecmp(pszCommand, "step"))
        {
            if (fCommandOnly)
                nParam = 1;
            else if (nParam == -1 || *pszExprEnd)
                fRet = false;

            if (fRet)
            {
                Expr::nCount = nParam;
                Break.pExpr = &Expr::Counter;
                Stop();
            }
        }
    */
    // x [count|until cond]
    else if (!strcasecmp(pszCommand, "x"))
    {
        psz = strtok(pszExprEnd, " ");

        // x 123  (instruction count)
        if (nParam != -1 && !*pszExprEnd)
        {
            Expr::nCount = nParam;
            Breakpoint::AddTemp(nullptr, &Expr::Counter);
            Debug::Stop();
            return false;
        }
        // x until cond
        else if (psz && !strcasecmp(psz, "until"))
        {
            psz += strlen(psz) + 1;
            EXPR* pExpr2 = Expr::Compile(psz);

            // If we have an expression set a temporary breakpoint using it
            if (pExpr2)
                Breakpoint::AddTemp(nullptr, pExpr2);
            else
                fRet = false;
        }
        // Otherwise fail if not unconditional execution
        else if (!fCommandOnly)
            fRet = false;

        if (fRet)
        {
            Debug::Stop();
            return false;
        }
    }

    // until expr
    else if (!strcasecmp(pszCommand, "u") || !strcasecmp(pszCommand, "until"))
    {
        if (nParam != -1 && !*pszExprEnd)
        {
            Breakpoint::AddTemp(nullptr, Expr::Compile(pszParam));
            Debug::Stop();
            return false;
        }
        else
            fRet = false;
    }

    // bpu cond
    else if (!strcasecmp(pszCommand, "bpu") && nParam != -1 && !*pszExprEnd)
    {
        Breakpoint::AddUntil(Expr::Compile(pszParam));
    }

    // bpx addr [if cond]
    else if (!strcasecmp(pszCommand, "bpx") && nParam != -1)
    {
        EXPR* pExpr = nullptr;
        void* pPhysAddr = nullptr;

        // Physical address?
        if (*pszExprEnd == ':')
        {
            int nPage = nParam;
            int nOffset;

            if (nPage < NUM_INTERNAL_PAGES && Expr::Eval(pszExprEnd + 1, &nOffset, &pszExprEnd) && nOffset < MEM_PAGE_SIZE)
                pPhysAddr = PageReadPtr(nPage) + nOffset;
            else
                fRet = false;
        }
        else
        {
            pPhysAddr = AddrReadPtr(nParam);
        }

        // Extract a token from after the location expression
        psz = strtok(pszExprEnd, " ");

        if (psz)
        {
            // If condition?
            if (!strcasecmp(psz, "if"))
            {
                // Compile the expression following it
                psz += strlen(psz) + 1;
                pExpr = Expr::Compile(psz);
                fRet &= pExpr != nullptr;
            }
            else
                fRet = false;
        }

        if (fRet)
            Breakpoint::AddExec(pPhysAddr, pExpr);
    }

    // bpm addr [rw|r|w] [if cond]
    else if (!strcasecmp(pszCommand, "bpm") && nParam != -1)
    {
        EXPR* pExpr = nullptr;
        void* pPhysAddr = nullptr;

        // Default is read/write
        AccessType nAccess = atReadWrite;

        // Physical address?
        if (*pszExprEnd == ':')
        {
            int nPage = nParam;
            int nOffset;

            if (nPage < NUM_INTERNAL_PAGES && Expr::Eval(pszExprEnd + 1, &nOffset, &pszExprEnd) && nOffset < MEM_PAGE_SIZE)
                pPhysAddr = PageReadPtr(nPage) + nOffset;
            else
                fRet = false;
        }
        else
        {
            pPhysAddr = AddrReadPtr(nParam);
        }

        // Extract a token from after the location expression
        psz = strtok(pszExprEnd, " ");

        if (psz)
        {
            // Check for access parameter
            AccessType t = GetAccessParam(psz);

            // If supplied, use it and skip to the next token
            if (t != atNone)
            {
                nAccess = t;
                psz = strtok(nullptr, " ");
            }
        }

        if (psz)
        {
            // If condition?
            if (!strcasecmp(psz, "if"))
            {
                // Compile the expression following it
                psz += strlen(psz) + 1;
                pExpr = Expr::Compile(psz);
                fRet = pExpr != nullptr;
            }
            else
                fRet = false;
        }

        if (fRet)
            Breakpoint::AddMemory(pPhysAddr, nAccess, pExpr);
    }

    // bpmr addrfrom addrto [rw|r|w] [if cond]
    else if (!strcasecmp(pszCommand, "bpmr") && nParam != -1)
    {
        EXPR* pExpr = nullptr;
        void* pPhysAddr = nullptr;

        // Default is read/write
        AccessType nAccess = atReadWrite;

        // Physical address?
        if (*pszExprEnd == ':')
        {
            int nPage = nParam;
            int nOffset;

            if (nPage < NUM_INTERNAL_PAGES && Expr::Eval(pszExprEnd + 1, &nOffset, &pszExprEnd) && nOffset < MEM_PAGE_SIZE)
                pPhysAddr = PageReadPtr(nPage) + nOffset;
            else
                fRet = false;
        }
        else
        {
            pPhysAddr = AddrReadPtr(nParam);
        }

        // Parse a length expression
        EXPR* pExpr2 = Expr::Compile(pszExprEnd, &pszExprEnd);
        int nLength = Expr::Eval(pExpr2);
        Expr::Release(pExpr2);

        // Length must be valid and non-zero
        if (nLength <= 0)
            fRet = false;

        // Extract a token from after the length expression
        psz = strtok(pszExprEnd, " ");

        if (psz)
        {
            // Check for access parameter
            AccessType t = GetAccessParam(psz);

            // If supplied, use it and skip to the next token
            if (t != atNone)
            {
                nAccess = t;
                psz = strtok(nullptr, " ");
            }
        }

        if (psz)
        {
            // If condition?
            if (!strcasecmp(psz, "if"))
            {
                // Compile the expression following it
                psz += strlen(psz) + 1;
                pExpr = Expr::Compile(psz);
                fRet = pExpr != nullptr;
            }
            else
                fRet = false;
        }

        if (fRet)
            Breakpoint::AddMemory(pPhysAddr, nAccess, pExpr, nLength);
    }

    // bpio port [rw|r|w] [if cond]
    else if (!strcasecmp(pszCommand, "bpio") && nParam != -1)
    {
        // Default is read/write and no expression
        AccessType nAccess = atReadWrite;
        EXPR* pExpr = nullptr;

        // Extract a token from after the port expression
        psz = strtok(pszExprEnd, " ");

        if (psz)
        {
            // Check for access parameter
            AccessType t = GetAccessParam(psz);

            // If supplied, use it and skip to the next token
            if (t != atNone)
            {
                nAccess = t;
                psz = strtok(nullptr, " ");
            }
        }

        if (psz)
        {
            // If condition?
            if (!strcasecmp(psz, "if"))
            {
                // Compile the expression following it
                psz += strlen(psz) + 1;
                pExpr = Expr::Compile(psz);
                fRet = pExpr != nullptr;
            }
            else
                fRet = false;
        }

        Breakpoint::AddPort(nParam, nAccess, pExpr);
    }

    // bpint frame|line|midi
    else if (!strcasecmp(pszCommand, "bpint"))
    {
        uint8_t bMask = 0x00;
        EXPR* pExpr = nullptr;

        if (fCommandOnly)
            bMask = 0x1f;
        else
        {
            for (psz = strtok(pszParam, " ,"); fRet && psz; psz = strtok(nullptr, " ,"))
            {
                if (!strcasecmp(psz, "frame") || !strcasecmp(psz, "f"))
                    bMask |= STATUS_INT_FRAME;
                else if (!strcasecmp(psz, "line") || !strcasecmp(psz, "l"))
                    bMask |= STATUS_INT_LINE;
                else if (!strcasecmp(psz, "midi") || !strcasecmp(psz, "m"))
                    bMask |= STATUS_INT_MIDIIN | STATUS_INT_MIDIOUT;
                else if (!strcasecmp(psz, "midiin") || !strcasecmp(psz, "mi"))
                    bMask |= STATUS_INT_MIDIIN;
                else if (!strcasecmp(psz, "midiout") || !strcasecmp(psz, "mo"))
                    bMask |= STATUS_INT_MIDIOUT;
                else if (!strcasecmp(psz, "if") && !pExpr)
                {
                    // Compile the expression following it
                    psz += strlen(psz) + 1;
                    pExpr = Expr::Compile(psz);
                    fRet = pExpr != nullptr;
                    break;
                }
                else
                    fRet = false;
            }
        }

        if (fRet)
            Breakpoint::AddInterrupt(bMask, pExpr);
    }

    // flag [+|-][sz5h3vnc]
    else if (!fCommandOnly && (!strcasecmp(pszCommand, "f") || !strcasecmp(pszCommand, "flag")))
    {
        bool fSet = true;
        uint8_t bNewF = F;

        while (fRet && *pszParam)
        {
            uint8_t bFlag = 0;

            switch (*pszParam++)
            {
            case '+': fSet = true;  continue;
            case '-': fSet = false; continue;

            case 's': case 'S': bFlag = FLAG_S; break;
            case 'z': case 'Z': bFlag = FLAG_Z; break;
            case '5':           bFlag = FLAG_5; break;
            case 'h': case 'H': bFlag = FLAG_H; break;
            case '3':           bFlag = FLAG_3; break;
            case 'v': case 'V': bFlag = FLAG_V; break;
            case 'n': case 'N': bFlag = FLAG_N; break;
            case 'c': case 'C': bFlag = FLAG_C; break;

            default: fRet = false; break;
            }

            // Set or reset the flag, as appropriate
            if (fSet)
                bNewF |= bFlag;
            else
                bNewF &= ~bFlag;
        }

        // If successful, set the modified flags
        if (fRet)
            F = bNewF;
    }

    // bc n  or  bc *
    else if (!strcasecmp(pszCommand, "bc"))
    {
        if (nParam != -1 && !*pszExprEnd)
            fRet = Breakpoint::RemoveAt(nParam);
        else if (!strcmp(pszParam, "*"))
            Breakpoint::RemoveAll();
        else
            fRet = false;
    }

    // bd n  or  bd *  or  be n  or  be *
    else if (!strcasecmp(pszCommand, "bd") || !strcasecmp(pszCommand, "be"))
    {
        bool fNewState = !strcasecmp(pszCommand, "be");

        if (nParam != -1 && !*pszExprEnd)
        {
            BREAKPT* pBreak = Breakpoint::GetAt(nParam);
            if (pBreak)
                pBreak->fEnabled = fNewState;
            else
                fRet = false;
        }
        else if (!strcmp(pszParam, "*"))
        {
            BREAKPT* pBreak = nullptr;
            for (int i = 0; (pBreak = Breakpoint::GetAt(i)); i++)
                pBreak->fEnabled = fNewState;
        }
        else
            fRet = false;
    }

    // exx
    else if (fCommandOnly && !strcasecmp(pszCommand, "exx"))
    {
        // EXX
        std::swap(BC, BC_);
        std::swap(DE, DE_);
        std::swap(HL, HL_);
    }

    // ex reg,reg2
    else if (!strcasecmp(pszCommand, "ex"))
    {
        EXPR* pExpr2 = nullptr;

        // Re-parse the first argument as a register
        Expr::Release(pExpr);
        pExpr = Expr::Compile(pszParam, &pszExprEnd, Expr::regOnly);

        // Locate and extract the second register parameter
        if ((psz = strtok(pszExprEnd, ",")))
            pExpr2 = Expr::Compile(psz, nullptr, Expr::regOnly);

        // Accept if both parameters are registers
        if (pExpr && pExpr->nType == T_REGISTER && !pExpr->pNext &&
            pExpr2 && pExpr2->nType == T_REGISTER && !pExpr2->pNext)
        {
            int nReg = Expr::GetReg(pExpr->nValue);
            int nReg2 = Expr::GetReg(pExpr2->nValue);
            Expr::SetReg(pExpr->nValue, nReg2);
            Expr::SetReg(pExpr2->nValue, nReg);
        }
        else
            fRet = false;

        Expr::Release(pExpr2);
    }

    // ld reg,value  or  r reg=value  or  r reg value
    else if (!strcasecmp(pszCommand, "r") || !strcasecmp(pszCommand, "ld"))
    {
        int nValue;

        // Re-parse the first argument as a register
        Expr::Release(pExpr);
        pExpr = Expr::Compile(pszParam, &pszExprEnd, Expr::regOnly);

        // Locate second parameter (either  ld hl,123  or r hl=123  syntax)
        psz = strtok(pszExprEnd, ",=");

        // The first parameter must be a register, the second can be any value
        if (pExpr && pExpr->nType == T_REGISTER && !pExpr->pNext && psz && Expr::Eval(psz, &nValue))
        {
            // If the view address matches PC, and PC is being set, update the view
            if (m_pView->GetAddress() == PC && pExpr->nValue == REG_PC)
                m_pView->SetAddress(nValue, true);

            // Set the register value
            Expr::SetReg(pExpr->nValue, nValue);
        }
        else
            fRet = false;
    }

    // out port,value
    else if (!strcasecmp(pszCommand, "out") && nParam != -1)
    {
        int nValue;

        // Locate value to output
        psz = strtok(pszExprEnd, ",");

        // Evaluate and output it
        if (psz && Expr::Eval(psz, &nValue))
            IO::Out(nParam, nValue);
        else
            fRet = false;
    }

    // poke addr,val[,val,...]
    else if (!strcasecmp(pszCommand, "poke") && nParam != -1)
    {
        uint8_t ab[128];
        int nBytes = 0;

        for (psz = strtok(pszExprEnd, ","); fRet && psz; psz = strtok(nullptr, ","))
        {
            int nVal;

            if (Expr::Eval(psz, &nVal, &pszExprEnd) && !*pszExprEnd)
            {
                ab[nBytes++] = nVal;

                if (nVal > 256)
                    ab[nBytes++] = nVal >> 8;
            }
            else
                fRet = false;
        }

        if (fRet && nBytes)
        {
            for (int i = 0; i < nBytes; i++)
                write_byte(nParam + i, ab[i]);
        }
        else
            fRet = false;
    }
    else
        fRet = false;

    Expr::Release(pExpr);
    return fRet;
}


////////////////////////////////////////////////////////////////////////////////
// Disassembler

#define MAX_LABEL_LEN  19
#define BAR_CHAR_LEN   54

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
    pDebugger->SetStatus(m_pcszDataTarget ? m_pcszDataTarget : "", false, sFixedFont);

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
            std::string sName = Symbol::LookupAddr(wAddr_, MAX_LABEL_LEN);

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

        uint8_t bColour = 'W';

        if (s_wAddrs[u] == PC)
        {
            // The location bar is green for a change in code flow or yellow otherwise, with black text
            uint8_t bBarColour = (m_uCodeTarget != INVALID_TARGET) ? GREEN_7 : YELLOW_7;
            fb.FillRect(nX - 1, nY - 1, BAR_CHAR_LEN * CHR_WIDTH + 1, ROW_HEIGHT - 3, bBarColour);
            bColour = 'k';

            // Add a direction arrow if we have a code target
            if (m_uCodeTarget != INVALID_TARGET)
                fb.DrawString(nX + CHR_WIDTH * (BAR_CHAR_LEN - 1), nY, (m_uCodeTarget <= PC) ? "\x80" : "\x81", BLACK);
        }

        // Check for a breakpoint at the current address.
        // Show conditional breakpoints in magenta, unconditional in red.
        auto pPhysAddr = AddrReadPtr(s_wAddrs[u]);
        int nIndex = Breakpoint::GetExecIndex(pPhysAddr);
        if (nIndex != -1)
            bColour = Breakpoint::GetAt(nIndex)->pExpr ? 'M' : 'R';

        // Show the current entry normally if it's not the current code target, otherwise show all
        // in black text with an arrow instead of the address, indicating it's the code target.
        if (m_uCodeTarget == INVALID_TARGET || s_wAddrs[u] != m_uCodeTarget)
            fb.Printf(nX, nY, "\a%c\a%c%s", bColour, (bColour != 'W') ? '0' : bColour, psz);
        else
            fb.Printf(nX, nY, "\a%c===>\a%c%s", (bColour == 'k') ? 'k' : 'G', (bColour == 'k') ? '0' : bColour, psz + 4);
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
        fb.Printf(nX+dx, nY+dy, "\ag%-3s\a%c%02X\a%c%02X", name, \
                    (regs.reg.b.h != sLastRegs.reg.b.h)?CHG_COL:'X', regs.reg.b.h,  \
                    (regs.reg.b.l != sLastRegs.reg.b.l)?CHG_COL:'X', regs.reg.b.l); \
    }

#define SingleReg(dx,dy,name,reg) \
    { \
        fb.Printf(nX+dx, nY+dy, "\ag%-2s\a%c%02X", name, \
                    (regs.reg != sLastRegs.reg)?CHG_COL:'X', regs.reg); \
    }

    DoubleReg(0, 0, "AF", af);    DoubleReg(54, 0, "AF'", af_);
    DoubleReg(0, 12, "BC", bc);    DoubleReg(54, 12, "BC'", bc_);
    DoubleReg(0, 24, "DE", de);    DoubleReg(54, 24, "DE'", de_);
    DoubleReg(0, 36, "HL", hl);    DoubleReg(54, 36, "HL'", hl_);

    DoubleReg(0, 52, "IX", ix);    DoubleReg(54, 52, "IY", iy);
    DoubleReg(0, 64, "PC", pc);    DoubleReg(54, 64, "SP", sp);

    SingleReg(0, 80, "I", i);      SingleReg(36, 80, "R", r);

    fb.DrawString(nX + 80, nY + 74, "\aK\x81\x81");

    for (i = 0; i < 4; i++)
        fb.Printf(nX + 72, nY + 84 + i * 12, "%04X", read_word(SP + i * 2));

    fb.Printf(nX, nY + 96, "\agIM \a%c%u", (IM != sLastRegs.im) ? CHG_COL : 'X', IM);
    fb.Printf(nX + 18, nY + 96, "  \a%c%cI", (IFF1 != sLastRegs.iff1) ? CHG_COL : 'X', IFF1 ? 'E' : 'D');

    char bIntDiff = status_reg ^ bLastStatus;
    fb.Printf(nX, nY + 108, "\agStat \a%c%c\a%c%c\a%c%c\a%c%c\a%c%c",
        (bIntDiff & 0x10) ? CHG_COL : (status_reg & 0x10) ? 'K' : 'X', (status_reg & 0x10) ? '-' : 'O',
        (bIntDiff & 0x08) ? CHG_COL : (status_reg & 0x08) ? 'K' : 'X', (status_reg & 0x08) ? '-' : 'F',
        (bIntDiff & 0x04) ? CHG_COL : (status_reg & 0x04) ? 'K' : 'X', (status_reg & 0x04) ? '-' : 'I',
        (bIntDiff & 0x02) ? CHG_COL : (status_reg & 0x02) ? 'K' : 'X', (status_reg & 0x02) ? '-' : 'M',
        (bIntDiff & 0x01) ? CHG_COL : (status_reg & 0x01) ? 'K' : 'X', (status_reg & 0x01) ? '-' : 'L');

    char bFlagDiff = F ^ sLastRegs.af.b.l;
    fb.Printf(nX, nY + 132, "\agFlag \a%c%c\a%c%c\a%c%c\a%c%c\a%c%c\a%c%c\a%c%c\a%c%c",
        (bFlagDiff & FLAG_S) ? CHG_COL : (F & FLAG_S) ? 'X' : 'K', (F & FLAG_S) ? 'S' : '-',
        (bFlagDiff & FLAG_Z) ? CHG_COL : (F & FLAG_Z) ? 'X' : 'K', (F & FLAG_Z) ? 'Z' : '-',
        (bFlagDiff & FLAG_5) ? CHG_COL : (F & FLAG_5) ? 'X' : 'K', (F & FLAG_5) ? '5' : '-',
        (bFlagDiff & FLAG_H) ? CHG_COL : (F & FLAG_H) ? 'X' : 'K', (F & FLAG_H) ? 'H' : '-',
        (bFlagDiff & FLAG_3) ? CHG_COL : (F & FLAG_3) ? 'X' : 'K', (F & FLAG_3) ? '3' : '-',
        (bFlagDiff & FLAG_V) ? CHG_COL : (F & FLAG_V) ? 'X' : 'K', (F & FLAG_V) ? 'V' : '-',
        (bFlagDiff & FLAG_N) ? CHG_COL : (F & FLAG_N) ? 'X' : 'K', (F & FLAG_N) ? 'N' : '-',
        (bFlagDiff & FLAG_C) ? CHG_COL : (F & FLAG_C) ? 'X' : 'K', (F & FLAG_C) ? 'C' : '-');


    int nLine = (g_dwCycleCounter < CPU_CYCLES_PER_SIDE_BORDER) ? GFX_HEIGHT_LINES - 1 : (g_dwCycleCounter - CPU_CYCLES_PER_SIDE_BORDER) / CPU_CYCLES_PER_LINE;
    int nLineCycle = (g_dwCycleCounter + CPU_CYCLES_PER_LINE - CPU_CYCLES_PER_SIDE_BORDER) % CPU_CYCLES_PER_LINE;

    fb.Printf(nX, nY + 148, "\agScan\aX %03d:%03d", nLine, nLineCycle);
    fb.Printf(nX, nY + 160, "\agT\aX %u", g_dwCycleCounter);

    uint32_t dwCycleDiff = ((nLastFrames * CPU_CYCLES_PER_FRAME) + g_dwCycleCounter) - dwLastCycle;
    if (dwCycleDiff)
        fb.Printf(nX + 12, nY + 172, "+%u", dwCycleDiff);

    fb.Printf(nX, nY + 188, "\agA \a%c%s", ReadOnlyAddr(0x0000) ? 'c' : 'X', Memory::PageDesc(GetSectionPage(SECTION_A)));
    fb.Printf(nX, nY + 200, "\agB \a%c%s", ReadOnlyAddr(0x4000) ? 'c' : 'X', Memory::PageDesc(GetSectionPage(SECTION_B)));
    fb.Printf(nX, nY + 212, "\agC \a%c%s", ReadOnlyAddr(0x8000) ? 'c' : 'X', Memory::PageDesc(GetSectionPage(SECTION_C)));
    fb.Printf(nX, nY + 224, "\agD \a%c%s", ReadOnlyAddr(0xc000) ? 'c' : 'X', Memory::PageDesc(GetSectionPage(SECTION_D)));

    fb.Printf(nX + 66, nY + 188, "\agL\aX %02X", lmpr);
    fb.Printf(nX + 66, nY + 200, "\agH\aX %02X", hmpr);
    fb.Printf(nX + 66, nY + 212, "\agV\aX %02X", vmpr);
    fb.Printf(nX + 66, nY + 224, "\agM\aX %X", ((vmpr & VMPR_MODE_MASK) >> 5) + 1);

    fb.DrawString(nX, nY + 240, "\agEvents");

    CPU_EVENT* pEvent = psNextEvent;
    for (i = 0; i < 3 && pEvent; i++, pEvent = pEvent->pNext)
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
            i--; continue;
        }

        fb.Printf(nX, nY + 252 + i * 12, "%-4s \a%c%6u\aXT", pcszEvent, CHG_COL, pEvent->due_time - g_dwCycleCounter);
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
            int nIndex = Breakpoint::GetExecIndex(pPhysAddr);

            // If there's no breakpoint, add a new one
            if (nIndex == -1)
                Breakpoint::AddExec(pPhysAddr, nullptr);
            else
                Breakpoint::RemoveAt(nIndex);
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
            wAddr = PC;
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
            PC = wAddr = GetPrevInstruction(PC);
        break;

    case HK_DOWN:
        if (!fCtrl)
            wAddr = s_wAddrs[1];
        else
        {
            uint8_t ab[MAX_Z80_INSTR_LEN];
            for (unsigned int u = 0; u < sizeof(ab); u++)
                ab[u] = read_byte(PC + u);

            wAddr = (PC += Disassemble(ab));
        }
        break;

    case HK_LEFT:
        if (!fCtrl)
            wAddr = s_wAddrs[0] - 1;
        else
            wAddr = --PC;
        break;

    case HK_RIGHT:
        if (!fCtrl)
            wAddr = s_wAddrs[0] + 1;
        else
            wAddr = ++PC;
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
    auto wPC = PC;
    auto bOpcode = read_byte(wPC);
    auto bOperand = read_byte(wPC + 1);
    uint8_t bFlags = F, bCond = 0xff;

    // Work out the possible next instruction addresses, which depend on the instruction found
    auto wJpTarget = read_word(wPC + 1);
    uint16_t wJrTarget = wPC + 2 + static_cast<signed char>(read_byte(wPC + 1));
    auto wRetTarget = read_word(SP);
    uint16_t wRstTarget = bOpcode & 0x38;

    // No instruction target or conditional jump helper string yet
    m_uCodeTarget = INVALID_TARGET;

    // Examine the current opcode to check for flow changing instructions
    switch (bOpcode)
    {
    case OP_DJNZ:
        // Set a pretend zero flag if B is 1 and would be decremented to zero
        bFlags = (B == 1) ? FLAG_Z : 0;
        bCond = 0;
        // Fall through...

    case OP_JR:     m_uCodeTarget = wJrTarget;  break;
    case OP_RET:    m_uCodeTarget = wRetTarget; break;
    case OP_JP:
    case OP_CALL:   m_uCodeTarget = wJpTarget;  break;
    case OP_JPHL:   m_uCodeTarget = HL;  break;

    case ED_PREFIX:
    {
        // RETN or RETI?
        if ((bOperand & 0xc7) == 0x45)
            m_uCodeTarget = wRetTarget;

        break;
    }

    case IX_PREFIX: if (bOperand == OP_JPHL) m_uCodeTarget = IX;  break;  // JP (IX)
    case IY_PREFIX: if (bOperand == OP_JPHL) m_uCodeTarget = IY;  break;  // JP (IY)

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
        static const uint8_t abFlags[] = { FLAG_Z, FLAG_C, FLAG_P, FLAG_S };

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
    m_pcszDataTarget = nullptr;

    // Extract potential instruction bytes
    auto wPC = PC;
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
    uint16_t wHLIXIYd = !fIndex ? HL : (((bOp0 == 0xdd) ? IX : IY) + bOp2);


    // 000r0010 = LD (BC/DE),A
    // 000r1010 = LD A,(BC/DE)
    if ((bOpcode & 0xe7) == 0x02)
        m_uDataTarget = (bOpcode & 0x10) ? DE : BC;

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
        m_uDataTarget = SP;
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
        m_uDataTarget = SP;
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
        m_uDataTarget = HL;
    }

    // ED 101000xx = LDI/CPI/INI/OUTI
    // ED 101010xx = LDD/CPD/IND/OUTD
    // ED 101100xx = LDIR/CPIR/INIR/OTIR
    // ED 101110xx = LDDR/CPDR/INDR/OTDR
    else if (bOpcode == ED_PREFIX && (bOp1 & 0xe4) == 0xa0)
    {
        m_uDataTarget = HL;
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
            m_uDataTarget = HL;
    }

    // Do we have something to display?
    if (m_uDataTarget != INVALID_TARGET)
    {
        static char sz[128];
        m_pcszDataTarget = sz;

        if (f16Bit)
        {
            snprintf(sz, std::size(sz), "%04X  \aK%04X %04X %04X\aX %04X \aK%04X %04X %04X",
                m_uDataTarget,
                read_word(m_uDataTarget - 6), read_word(m_uDataTarget - 4), read_word(m_uDataTarget - 2),
                read_word(m_uDataTarget),
                read_word(m_uDataTarget + 2), read_word(m_uDataTarget + 4), read_word(m_uDataTarget + 6));
        }
        else
        {
            snprintf(sz, std::size(sz), "%04X  \aK%02X %02X %02X %02X %02X\aX %02X \aK%02X %02X %02X %02X %02X",
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
            if ((AddrReadPtr(wAddr_) == pbMemRead1 || AddrReadPtr(wAddr_) == pbMemRead2) ||
                (AddrWritePtr(wAddr_) == pbMemWrite1 || AddrReadPtr(wAddr_) == pbMemWrite2))
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
        bool fRead = AddrReadPtr(wAddr) == pbMemRead1 || AddrReadPtr(wAddr) == pbMemRead2;
        bool fWrite = AddrWritePtr(wAddr) == pbMemWrite1 || AddrWritePtr(wAddr) == pbMemWrite2;
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

        fb.DrawString(nX, nY, psz, WHITE);
    }

    if (m_fEditing && GetAddrPosition(m_wEditAddr, nX, nY))
    {
        auto b = read_byte(m_wEditAddr);
        char ch = (b >= ' ' && b <= 0x7f) ? b : '.';

        fb.FillRect(nX - 1, nY - 1, CHR_WIDTH + 1, ROW_HEIGHT - 3, YELLOW_8);
        fb.Printf(nX, nY, "\ak%c", ch);

        pDebugger->SetStatusByte(m_wEditAddr);
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
        wEditAddr = wAddr = fCtrl ? 0 : (fShift && m_fEditing) ? wEditAddr : PC;
        break;

    case HK_END:
        wAddr = fCtrl ? (0 - m_nRows * TXT_COLUMNS) : PC;
        wEditAddr = (fCtrl ? 0 : PC + m_nRows * TXT_COLUMNS) - 1;
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
            if ((AddrReadPtr(wAddr_) == pbMemRead1 || AddrReadPtr(wAddr_) == pbMemRead2) ||
                (AddrWritePtr(wAddr_) == pbMemWrite1 || AddrReadPtr(wAddr_) == pbMemWrite2))
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
        bool fRead = AddrReadPtr(wAddr) == pbMemRead1 || AddrReadPtr(wAddr) == pbMemRead2;
        bool fWrite = AddrWritePtr(wAddr) == pbMemWrite1 || AddrWritePtr(wAddr) == pbMemWrite2;
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

        fb.DrawString(nX, nY, psz, WHITE);
    }

    if (m_fEditing && GetAddrPosition(m_wEditAddr, nX, nY, nTextX))
    {
        char sz[3];
        auto b = read_byte(m_wEditAddr);
        snprintf(sz, 3, "%02X", b);

        if (m_fRightNibble)
            nY += CHR_WIDTH;

        fb.FillRect(nX - 1, nY - 1, CHR_WIDTH + 1, ROW_HEIGHT - 3, YELLOW_8);
        fb.Printf(nX, nY, "\ak%c", sz[m_fRightNibble]);

        char ch = (b >= ' ' && b <= 0x7f) ? b : '.';
        fb.FillRect(nTextX - 1, nY - 1, CHR_WIDTH + 1, ROW_HEIGHT - 3, GREY_6);
        fb.Printf(nTextX, nY, "\ak%c", ch);
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
        wEditAddr = wAddr = fCtrl ? 0 : (fShift && m_fEditing) ? wEditAddr : PC;
        break;

    case HK_END:
        wAddr = fCtrl ? (0 - m_nRows * HEX_COLUMNS) : PC;
        wEditAddr = (fCtrl ? 0 : PC + m_nRows * HEX_COLUMNS) - 1;
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

    unsigned int uLen = 16384, uBlock = uLen >> 8;
    for (int i = 0 ; i < 256 ; i++)
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
        unsigned int uLen = (m_nHeight - uGap) * ((uint8_t*)szDisassem)[u] / 100;
        fb.DrawLine(m_nX+u, m_nY+m_nHeight-uGap-uLen, 0, uLen, (u & 16) ? WHITE : GREY_7);
    }

    fb.DrawString(m_nX, m_nY+m_nHeight-10, "Page 0: 16K in 1K units", WHITE);

    fb.SetFont(sGUIFont);
}
*/

////////////////////////////////////////////////////////////////////////////////
// Graphics View

static const int STRIP_GAP = 8;
unsigned int GfxView::s_uMode = 4, GfxView::s_uWidth = 8, GfxView::s_uZoom = 1;

GfxView::GfxView(Window* pParent_)
    : View(pParent_)
{
    SetText("Graphics");
    SetFont(sFixedFont);

    // Allocate enough space for a double-width window, at 1 byte per pixel
    m_pbData = new uint8_t[m_nWidth * m_nHeight * 2];

    // Start with the current video mode
    s_uMode = ((vmpr & VMPR_MODE_MASK) >> 5) + 1;
}

void GfxView::SetAddress(uint16_t wAddr_, bool /*fForceTop_*/)
{
    static const unsigned int auPPB[] = { 8, 8, 2, 2 };   // Pixels Per Byte in each mode

    View::SetAddress(wAddr_);

    m_uStripWidth = s_uWidth * s_uZoom * auPPB[s_uMode - 1];
    m_uStripLines = m_nHeight / s_uZoom;
    m_uStrips = (m_nWidth + STRIP_GAP + m_uStripWidth + STRIP_GAP - 1) / (m_uStripWidth + STRIP_GAP);

    auto pb = m_pbData;
    for (unsigned int u = 0; u < ((m_uStrips + 1) * m_uStripLines); u++)
    {
        switch (s_uMode)
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
                memset(pb, mode3clut[(b & 0x30) >> 4], s_uZoom); pb += s_uZoom;
                memset(pb, mode3clut[(b & 0x03)], s_uZoom); pb += s_uZoom;
            }
            break;
        }

        case 4:
        {
            for (unsigned int v = 0; v < s_uWidth; v++)
            {
                auto b = read_byte(wAddr_++);

                memset(pb, clut[b >> 4], s_uZoom); pb += s_uZoom;
                memset(pb, clut[b & 0xf], s_uZoom); pb += s_uZoom;
            }
            break;
        }
        }
    }

    char sz[128] = {};
    snprintf(sz, sizeof(sz) - 1, "%04X  Mode %u  Width %u  Zoom %ux", GetAddress(), s_uMode, s_uWidth, s_uZoom);
    pDebugger->SetStatus(sz, false, sFixedFont);

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
        s_uMode = nKey_ - '0';
        if (s_uMode < 3 && s_uWidth > 32U) s_uWidth = 32U;  // Clip width in modes 1+2
        break;

        // Toggle grid view in modes 1+2
    case 'g': case 'G':
        m_fGrid = !m_fGrid;
        break;

    case HK_HOME:
        wAddr = fCtrl ? 0 : PC;
        break;

    case HK_END:
        wAddr = fCtrl ? (static_cast<uint16_t>(0) - m_uStrips * m_uStripLines * s_uWidth) : PC;
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
        else if (s_uWidth < ((s_uMode < 3) ? 32U : 128U))   // Restrict byte width to mode limit
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

    if (!Breakpoint::IsSet())
        psz += sprintf(psz, "No breakpoints") + 1;
    else
    {
        int i = 0;

        m_nLines = 0;
        m_nActive = -1;

        for (BREAKPT* p = nullptr; (p = Breakpoint::GetAt(i)); i++, m_nLines++)
        {
            psz += sprintf(psz, "%2d: %s", i, Breakpoint::GetDesc(p)) + 1;

            // Check if we're on an execution breakpoint (ignoring condition)
            auto pPhys = AddrReadPtr(PC);
            if (p->nType == btExecute && p->Exec.pPhysAddr == pPhys)
                m_nActive = i;
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

        BREAKPT* pBreak = Breakpoint::GetAt(m_nTopLine + i);
        uint8_t bColour = (m_nTopLine + i == m_nActive) ? CYAN_7 : (pBreak && !pBreak->fEnabled) ? GREY_4 : WHITE;
        fb.DrawString(nX, nY, psz, bColour);
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
            BREAKPT* pBreak = Breakpoint::GetAt(nIndex);
            pBreak->fEnabled = !pBreak->fEnabled;
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
        fb.DrawString(nX_, nY_, "No instruction trace", WHITE);
    else
    {
        char szDis[32], sz[128], * psz = sz;

        int nPos = (nNumTraces - GetLines() + 1 + nLine_ + TRACE_SLOTS) % TRACE_SLOTS;
        TRACEDATA* pTD = &aTrace[nPos];

        Disassemble(pTD->abInstr, pTD->wPC, szDis, sizeof(szDis));
        psz += sprintf(psz, "%04X  %-18s", pTD->wPC, szDis);

        if (nLine_ != GetLines() - 1)
        {
            int nPos0 = (nPos + 0 + TRACE_SLOTS) % TRACE_SLOTS;
            int nPos1 = (nPos + 1 + TRACE_SLOTS) % TRACE_SLOTS;
            TRACEDATA* p0 = &aTrace[nPos0];
            TRACEDATA* p1 = &aTrace[nPos1];

            // Macro-tastic!
#define CHG_S(r)    (p1->regs.r != p0->regs.r)
#define CHG_H(r,h)  (p1->regs.r.b.h != p0->regs.r.b.h)
#define CHG_D(r)    (p1->regs.r.w != p0->regs.r.w)

#define CHK_S(r,RL)     if (CHG_S(r)) PRINT_S(RL,r);
#define CHK(rr,RH,RL)   if (CHG_H(rr,h) && !CHG_H(rr,l)) PRINT_H(RH,rr,h);      \
                        else if (!CHG_H(rr,h) && CHG_H(rr,l)) PRINT_H(RL,rr,l); \
                        else if (CHG_D(rr)) PRINT_D(RH RL,rr)

#define PRINT_H(N,r,h)  psz += sprintf(psz, "\ag" N "\aX %02X->%02X  ", p0->regs.r.b.h, p1->regs.r.b.h)
#define PRINT_S(N,r)    psz += sprintf(psz, "\ag" N "\aX %02X->%02X  ", p0->regs.r, p1->regs.r)
#define PRINT_D(N,r)    psz += sprintf(psz, "\ag" N "\aX %04X->%04X  ", p0->regs.r.w, p1->regs.r.w)

            // Special check for EXX, as we don't have room for all changes
            if ((CHG_D(bc) && CHG_D(bc_)) ||
                (CHG_D(de) && CHG_D(de_)) ||
                (CHG_D(hl) && CHG_D(hl_)))
            {
                psz += sprintf(psz, "BC/DE/HL <=> BC'/DE'/HL'");
            }
            // Same for BC+DE+HL changing in block instructions
            else if (CHG_D(bc) && CHG_D(de) && CHG_D(hl))
            {
                psz += sprintf(psz, "\agBC\aX->%04X \agDE\aX->%04X \agHL\aX->%04X", BC, DE, HL);
            }
            else
            {
                if (CHG_D(sp))
                {
                    if (CHG_D(af)) PRINT_D("AF", af);
                    if (CHG_D(bc)) PRINT_D("BC", bc);
                    if (CHG_D(de)) PRINT_D("DE", de);
                    if (CHG_D(hl)) PRINT_D("HL", hl);
                }
                else
                {
                    if (m_fFullMode)
                    {
                        if (CHG_D(af)) PRINT_D("AF", af);
                        if (CHG_D(bc)) PRINT_D("BC", bc);
                        if (CHG_D(de)) PRINT_D("DE", de);
                        if (CHG_D(hl)) PRINT_D("HL", hl);
                    }
                    else
                    {
                        if (CHG_H(af, h)) PRINT_H("A", af, h);
                        CHK(bc, "B", "C");
                        CHK(de, "D", "E");
                        CHK(hl, "H", "L");
                    }
                }

                if (CHG_D(ix)) PRINT_D("IX", ix);
                if (CHG_D(iy)) PRINT_D("IY", iy);
                if (CHG_D(sp)) PRINT_D("SP", sp);

                CHK_S(i, "I");
                CHK_S(im, "IM");
            }
        }

        uint8_t bColour = WHITE;

        if (nLine_ == GetLines() - 1)
        {
            fb.FillRect(nX_ - 1, nY_ - 1, m_nWidth - 112, ROW_HEIGHT - 3, YELLOW_7);
            bColour = BLACK;
        }

        fb.DrawString(nX_, nY_, sz, bColour);
    }
}

bool TrcView::cmdNavigate(int nKey_, int nMods_)
{
    if (nKey_ == HK_SPACE)
        m_fFullMode = !m_fFullMode;

    return TextView::cmdNavigate(nKey_, nMods_);
}

void TrcView::OnDblClick(int nLine_)
{
    int nPos = (nNumTraces - GetLines() + 1 + GetTopLine() + nLine_ + TRACE_SLOTS) % TRACE_SLOTS;
    TRACEDATA* pTD = &aTrace[nPos];

    View::SetAddress(pTD->wPC, true);
    pDebugger->SetView(vtDis);
}

void TrcView::OnDelete()
{
    nNumTraces = 0;
    SetLines(0);
}
