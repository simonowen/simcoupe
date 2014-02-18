// Part of SimCoupe - A SAM Coupe emulator
//
// Debug.cpp: Integrated Z80 debugger
//
//  Copyright (c) 1999-2014 Simon Owen
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

#include "SimCoupe.h"
#include "Debug.h"

#include "CPU.h"
#include "Disassem.h"
#include "Frame.h"
#include "Keyboard.h"
#include "Memory.h"
#include "Options.h"
#include "SAMROM.h"
#include "Util.h"


// Helper macro to decide on item colour - light red for changed or white for unchanged
#define RegCol(a,b) ((a) != (b) ? RED_8 : WHITE)

static CDebugger* pDebugger;

// Stack position used to track stepping out
int nStepOutSP = -1;

// Last position of debugger window and last register values
int nDebugX, nDebugY;
Z80Regs sLastRegs, sCurrRegs;
BYTE bLastStatus;
DWORD dwLastCycle;
int nLastFrames;

// Activate the debug GUI, if not already active
bool Debug::Start (BREAKPT* pBreak_)
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
    }

    // Stop any existing debugger instance
    GUI::Stop();

    // Create the main debugger window, passing any breakpoint
    if (!GUI::Start(pDebugger = new CDebugger(pBreak_)))
        pDebugger = NULL;

    return true;
}

void Debug::Stop ()
{
    if (pDebugger)
    {
        pDebugger->Destroy();
        pDebugger = NULL;
    }
}

void Debug::FrameEnd ()
{
    nLastFrames++;
}


// Called on every RETurn, for step-out implementation
void Debug::OnRet ()
{
    // Step-out in progress?
    if (nStepOutSP != -1)
    {
        // If the stack is at or just above the starting position, it should mean we've returned
        // Allow some generous slack for data that may have been on the stack above the address
        if ((SP-nStepOutSP) >= 0 && (SP-nStepOutSP) < 64)
            Debug::Start();
    }
}

// Return whether the debug GUI is active
bool Debug::IsActive ()
{
    return pDebugger != NULL;
}

// Return whether any breakpoints are active
bool Debug::IsBreakpointSet ()
{
    return Breakpoint::IsSet();
}

// Return whether any of the active breakpoints have been hit
bool Debug::BreakpointHit ()
{
    return Breakpoint::IsHit();
}

////////////////////////////////////////////////////////////////////////////////

// Find the longest instruction that ends before a given address
WORD GetPrevInstruction (WORD wAddr_)
{
    // Start 4 bytes back as that's the longest instruction length
    for (UINT u = 4 ; u ; u--)
    {
        WORD w = wAddr_ - u;
        BYTE ab[] = { read_byte(w), read_byte(w+1), read_byte(w+2), read_byte(w+3) };

        // Check that the instruction length leads to the required address
        if (w+Disassemble(ab) == wAddr_)
            return w;
    }

    // No match found, so return 1 byte back instead
    return wAddr_-1;
}

////////////////////////////////////////////////////////////////////////////////

void cmdStep (int nCount_=1, bool fCtrl_=false)
{
    void *pPhysAddr = NULL;
    BYTE bOpcode;
    WORD wPC;

    // Skip any index prefixes on the instruction to reach the real opcode or a CD/ED prefix
    for (wPC = PC ; ((bOpcode = read_byte(wPC)) == IX_PREFIX || bOpcode == IY_PREFIX) ; wPC++);

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
        Breakpoint::AddTemp(pPhysAddr, NULL);

    // Otherwise execute the requested number of instructions
    else
    {
        Expr::nCount = nCount_;
        Breakpoint::AddTemp(NULL, &Expr::Counter);
    }

    Debug::Stop();
}

void cmdStepOver (bool fCtrl_=false)
{
    void *pPhysAddr = NULL;
    BYTE bOpcode, bOperand;
    WORD wPC;

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
    for (wPC = PC ; ((bOpcode = read_byte(wPC)) == IX_PREFIX || bOpcode == IY_PREFIX) ; wPC++);
    bOperand = read_byte(wPC+1);

    // 1-byte HALT or RST ?
    if (bOpcode == OP_HALT || (bOpcode & 0xc7) == 0xc7)
        pPhysAddr = AddrReadPtr(wPC+1);

    // 2-byte backwards DJNZ/JR cc, or (LD|CP|IN|OT)[I|D]R ?
    else if (((bOpcode == OP_DJNZ || (bOpcode & 0xe7) == 0x20) && (bOperand & 0x80))
           || (bOpcode == 0xed && (bOperand & 0xf4) == 0xb0))
        pPhysAddr = AddrReadPtr(wPC+2);

    // 3-byte CALL, CALL cc or backwards JP cc?
    else if (bOpcode == OP_CALL || (bOpcode & 0xc7) == 0xc4 ||
           ((bOpcode & 0xc7) == 0xc2 && read_word(wPC+1) <= wPC))
        pPhysAddr = AddrReadPtr(wPC+3);

    // Single step if no instruction-specific breakpoint is set
    if (!pPhysAddr)
        cmdStep();
    else
    {
        Breakpoint::AddTemp(pPhysAddr, NULL);
        Debug::Stop();
    }
}

void cmdStepOut ()
{
    // Store the current stack pointer, for checking on RETurn calls
    nStepOutSP = SP;
    Debug::Stop();
}

////////////////////////////////////////////////////////////////////////////////

CInputDialog::CInputDialog (CWindow* pParent_/*=NULL*/, const char* pcszCaption_, const char* pcszPrompt_, PFNINPUTPROC pfnNotify_)
    : CDialog(pParent_, 0,0, pcszCaption_), m_pfnNotify(pfnNotify_)
{
    // Get the length of the prompt string, so we can position the edit box correctly
    int n = GetTextWidth(pcszPrompt_);

    // Create the prompt text control and input edit control
    new CTextControl(this, 5, 10,  pcszPrompt_, WHITE);
    m_pInput = new CEditControl(this, 5+n+5, 6, 120);

    // Size the dialog to fit the prompt and edit control
    SetSize(8+n+120+8, 30);
    Centre();
}

void CInputDialog::OnNotify (CWindow* pWindow_, int nParam_)
{
    if (pWindow_ == m_pInput && nParam_)
    {
        // Fetch and compile the input expression
        const char* pcszExpr = m_pInput->GetText();
        EXPR *pExpr = Expr::Compile(pcszExpr);

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
static bool OnAddressNotify (EXPR *pExpr_)
{
    int nAddr = Expr::Eval(pExpr_);
    pDebugger->SetAddress(nAddr);
    return true;
}

// Notify handler for Execute Until expression
static bool OnUntilNotify (EXPR *pExpr_)
{
    Breakpoint::AddTemp(NULL, pExpr_);
    Debug::Stop();
    return false;
}

// Notify handler for Change Lmpr input
static bool OnLmprNotify (EXPR *pExpr_)
{
    int nPage = Expr::Eval(pExpr_) & LMPR_PAGE_MASK;
    IO::OutLmpr((lmpr & ~LMPR_PAGE_MASK) | nPage);
    return true;
}

// Notify handler for Change Hmpr input
static bool OnHmprNotify (EXPR *pExpr_)
{
    int nPage = Expr::Eval(pExpr_) & HMPR_PAGE_MASK;
    IO::OutHmpr((hmpr & ~HMPR_PAGE_MASK) | nPage);
    return true;
}

// Notify handler for Change Lmpr input
static bool OnLeprNotify (EXPR *pExpr_)
{
    IO::OutLepr(Expr::Eval(pExpr_));
    return true;
}

// Notify handler for Change Hepr input
static bool OnHeprNotify (EXPR *pExpr_)
{
    IO::OutHepr(Expr::Eval(pExpr_));
    return true;
}

// Notify handler for Change Vmpr input
static bool OnVmprNotify (EXPR *pExpr_)
{
    int nPage = Expr::Eval(pExpr_) & VMPR_PAGE_MASK;
    IO::OutVmpr(VMPR_MODE | nPage);
    return true;
}

// Notify handler for Change Mode input
static bool OnModeNotify (EXPR *pExpr_)
{
    int nMode = Expr::Eval(pExpr_);
    if (nMode < 1 || nMode > 4)
        return false;

    IO::OutVmpr(((nMode-1) << 5) | VMPR_PAGE);
    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CDebugger::s_fTransparent = false;

CDebugger::CDebugger (BREAKPT* pBreak_/*=NULL*/)
    : CDialog(NULL, 433, 260+36+2, "", false),
    m_pView(NULL), m_pCommandEdit(NULL)
{
    // Move to the last display position, if any
    if (nDebugX | nDebugY)
        Move(nDebugX, nDebugY);

    // Create the status text control
    m_pStatus = new CTextControl(this, 4, m_nHeight-sFixedFont.wHeight-4, "");

    // If a breakpoint was supplied, report that it was triggered
    if (pBreak_)
    {
        char sz[128]={};

        if (pBreak_->nType != btTemp)
            snprintf(sz, sizeof(sz)-1, "Breakpoint %d hit:  %s", Breakpoint::GetIndex(pBreak_), Breakpoint::GetDesc(pBreak_));
        else
        {
            if (pBreak_->pExpr && pBreak_->pExpr != &Expr::Counter)
                snprintf(sz, sizeof(sz)-1, "UNTIL condition met:  %s", pBreak_->pExpr->pcszExpr);
        }

        SetStatus(sz, YELLOW_6, &sPropFont);
    }

    // Remove all temporary breakpoints
    for (int i = 0 ; (pBreak_ = Breakpoint::GetAt(i)) ; i++)
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

CDebugger::~CDebugger ()
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
    pbMemRead1 = pbMemRead2 = pbMemWrite1 = pbMemWrite2 = NULL;

    // Debugger is gone
    pDebugger = NULL;
}

void CDebugger::SetSubTitle (const char *pcszSubTitle_)
{
    char szTitle[128] = "SimICE";
    if (pcszSubTitle_ && *pcszSubTitle_)
    {
        strcat(szTitle, " -- ");
        strcat(szTitle, pcszSubTitle_);
    }

    SetText(szTitle);
}

void CDebugger::SetAddress (WORD wAddr_)
{
    m_pView->SetAddress(wAddr_, true);
}

void CDebugger::SetView (ViewType nView_)
{
    CView *pNewView = NULL;

    // Create the new view
    switch (nView_)
    {
        case vtDis:
            pNewView = new CDisView(this);
            break;

        case vtTxt:
            pNewView = new CTxtView(this);
            break;

        case vtHex:
            pNewView = new CHexView(this);
            break;

        case vtGfx:
            pNewView = new CGfxView(this);
            break;

        case vtBpt:
            pNewView = new CBptView(this);
            break;
    }

    // New view created?
    if (pNewView)
    {
        SetSubTitle(pNewView->GetText());

        if (!m_pView)
            pNewView->SetAddress(PC);
        else
        {
            // Transfer the current address select, then replace the old one
            pNewView->SetAddress(m_pView->GetAddress());
            m_pView->Destroy();
        }

        m_pView = pNewView;
        m_nView = nView_;
    }
}

void CDebugger::SetStatus (const char *pcsz_, BYTE bColour_/*=WHITE*/, const GUIFONT *pFont_)
{
    if (m_pStatus)
    {
        if (pFont_)
            m_pStatus->SetFont(pFont_);

        m_pStatus->SetText(pcsz_, bColour_);
    }
}

void CDebugger::SetStatusByte (WORD wAddr_)
{
    size_t i;
    char szKeyword[32] = {};
    char szBinary[9]={};

    // Read byte at status location
    BYTE b = read_byte(wAddr_);

    // Change unprintable characters to a space
    char ch = (b >= ' ' && b <= 0x7f) ? b : ' ';

    // Keyword range?
    if (b >= 60)
    {
        // Keyword table in (unmodified) ROM1
        const BYTE *pcbKeywords = abSAMROM + MEM_PAGE_SIZE + 0xf8c9-0xc000;

        // Step over the required number of tokens
        for (i = b-60 ; i > 0 ; i--)
        {
            // Skip until end of token marker (bit 7 set)
            while (*pcbKeywords++ < 0x80) { }
        }

        // Copy keyword to local buffer
        for (i = 0 ; i < sizeof(szKeyword) ; i++)
        {
            // Keep only lower 7 bits
            szKeyword[i] = pcbKeywords[i] & 0x7f;

            // Stop if we've found the end of token
            if (pcbKeywords[i] >= 0x80)
                break;
        }
    }

    // Generate binary representation
    for (i = 128 ; i > 0 ; i >>= 1)
        strcat(szBinary, (b&i)?"1":"0");

    // Form full status line, and set it
    char sz[128]={};
    snprintf(sz, sizeof(sz)-1, "%04X  %02X  %03d  %s  %c  %s", wAddr_, b, b, szBinary, ch, szKeyword);
    SetStatus(sz, WHITE, &sFixedFont);
}

// Refresh the current debugger view
void CDebugger::Refresh ()
{
    // Re-set the view to the same address, to force a refresh
    m_pView->SetAddress(m_pView->GetAddress());
}


// Dialog override for background painting
void CDebugger::EraseBackground (CScreen* pScreen_)
{
    // If we're not in transparent mode, call the base to draw the normal dialog background
    if (!s_fTransparent)
        CDialog::EraseBackground(pScreen_);
}

// Dialog override for dialog drawing
void CDebugger::Draw (CScreen* pScreen_)
{
    // First draw?
    if (!m_pView)
    {
        // Form the R register from its working components, and save the current register state
        R = (R7 & 0x80) | (R & 0x7f);
        sCurrRegs = regs;

        // Set the disassembly view
        SetView(vtDis);
    }

    CDialog::Draw(pScreen_);
}

bool CDebugger::OnMessage (int nMessage_, int nParam1_, int nParam2_)
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
                    m_pCommandEdit = NULL;
                }
                else if (m_nView != vtDis)
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
                    m_pCommandEdit = new CEditControl(this, -1, m_nHeight-16, m_nWidth+2);
                    m_pCommandEdit->SetFont(&sPropFont);
                }
                break;

            case 'a':
                new CInputDialog(this, "New location", "Address:", OnAddressNotify);
                break;

            case 'b':
                SetView(vtBpt);
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
                    new CInputDialog(this, sz, "New Page:", OnLeprNotify);
                }
                else
                {
                    sprintf(sz, "Change LMPR [%02X]:", lmpr&LMPR_PAGE_MASK);
                    new CInputDialog(this, sz, "New Page:", OnLmprNotify);
                }
                break;

            case 'h':
                if (fShift)
                {
                    sprintf(sz, "Change HEPR [%02X]:", hepr);
                    new CInputDialog(this, sz, "New Page:", OnHeprNotify);
                }
                else
                {
                    sprintf(sz, "Change HMPR [%02X]:", hmpr&HMPR_PAGE_MASK);
                    new CInputDialog(this, sz, "New Page:", OnHmprNotify);
                }
                break;

            case 'v':
                sprintf(sz, "Change VMPR [%02X]:", vmpr&VMPR_PAGE_MASK);
                new CInputDialog(this, sz, "New Page:", OnVmprNotify);
                break;

            case 'm':
                sprintf(sz, "Change Mode [%X]:", ((vmpr&VMPR_MODE_MASK)>>5)+1);
                new CInputDialog(this, sz, "New Mode:", OnModeNotify);
                break;

            case 'u':
                new CInputDialog(this, "Execute until", "Expression:", OnUntilNotify);
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

    if (fRet)
    {
        m_pStatus->SetText("");
        Refresh();
    }

    // If not already processed, pass onPass on to dialog base for processing, if not
    if (!fRet)
        fRet = CDialog::OnMessage(nMessage_, nParam1_, nParam2_);

    return fRet;
}


void CDebugger::OnNotify (CWindow* pWindow_, int nParam_)
{
    // Command submitted?
    if (pWindow_ == m_pCommandEdit && nParam_ == 1)
    {
        const char *pcsz = m_pCommandEdit->GetText();

        // If no command is given, close the command bar
        if (!*pcsz)
        {
            m_pCommandEdit->Destroy();
            m_pCommandEdit = NULL;
        }
        // Otherwise execute the command, and if successful, clear the command text
        else if (Execute(pcsz) && m_pCommandEdit)
        {
            m_pCommandEdit->SetText("");
            Refresh();
        }
    }
}


AccessType GetAccessParam (const char *pcsz_)
{
    if (!strcasecmp(pcsz_, "r"))
        return atRead;
    else if (!strcasecmp(pcsz_, "w"))
        return atWrite;
    else if (!strcasecmp(pcsz_, "rw"))
        return atReadWrite;

    return atNone;
}

bool CDebugger::Execute (const char* pcszCommand_)
{
    bool fRet = true;

    char *psz = NULL;

    char szCommand[256]={};
    strncpy(szCommand, pcszCommand_, sizeof(szCommand)-1);
    char *pszCommand = strtok(szCommand, " ");
    if (!pszCommand)
        return false;

    // Locate any parameter, stripping leading spaces
    char *pszParam = pszCommand+strlen(pszCommand)+1;
    for ( ; *pszParam == ' ' ; pszParam++);
    bool fCommandOnly = !*pszParam;

    // Evaluate the parameter as an expression
    char *pszExprEnd = NULL;
    EXPR *pExpr = Expr::Compile(pszParam, &pszExprEnd);
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
        BYTE ab[4] = { read_byte(PC), read_byte(PC+1), read_byte(PC+2), read_byte(PC+3) };
        UINT uLen = Disassemble(ab, PC);

        // Replace instruction with the appropriate number of NOPs
        for (UINT u = 0 ; u < uLen ; u++)
            write_byte(PC+u, OP_NOP);
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
            Debug::Stop();
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
            Breakpoint::AddTemp(NULL, &Expr::Counter);
            Debug::Stop();
        }
        // x until cond
        else if (psz && !strcasecmp(psz, "until"))
        {
            psz += strlen(psz)+1;
            EXPR *pExpr2 = Expr::Compile(psz);

            // If we have an expression set a temporary breakpoint using it
            if (pExpr2)
                Breakpoint::AddTemp(NULL, pExpr2);
            else
                fRet = false;
        }
        // Otherwise fail if not unconditional execution
        else if (!fCommandOnly)
            fRet = false;

        if (fRet)
            Debug::Stop();
    }

    // until expr
    else if (!strcasecmp(pszCommand, "u") || !strcasecmp(pszCommand, "until"))
    {
        if (nParam != -1 && !*pszExprEnd)
        {
            Breakpoint::AddTemp(NULL, Expr::Compile(pszParam));
            Debug::Stop();
        }
        else
            fRet = false;
    }

    // bpx addr [if cond]
    else if (!strcasecmp(pszCommand, "bpx") && nParam != -1)
    {
        EXPR *pExpr = NULL;
        void *pPhysAddr = NULL;

        // Physical address?
        if (*pszExprEnd == ':')
        {
            int nPage = nParam;
            int nOffset;

            if (nPage < N_PAGES_MAIN && Expr::Eval(pszExprEnd+1, &nOffset, &pszExprEnd) && nOffset < MEM_PAGE_SIZE)
                pPhysAddr = PageReadPtr(nPage)+nOffset;
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
                psz += strlen(psz)+1;
                pExpr = Expr::Compile(psz);
                fRet &= pExpr != NULL;
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
        EXPR *pExpr = NULL;
        void *pPhysAddr = NULL;

        // Default is read/write
        AccessType nAccess = atReadWrite;

        // Physical address?
        if (*pszExprEnd == ':')
        {
            int nPage = nParam;
            int nOffset;

            if (nPage < N_PAGES_MAIN && Expr::Eval(pszExprEnd+1, &nOffset, &pszExprEnd) && nOffset < MEM_PAGE_SIZE)
                pPhysAddr = PageReadPtr(nPage)+nOffset;
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
                psz = strtok(NULL, " ");
            }
        }

        if (psz)
        {
            // If condition?
            if (!strcasecmp(psz, "if"))
            {
                // Compile the expression following it
                psz += strlen(psz)+1;
                pExpr = Expr::Compile(psz);
                fRet = pExpr != NULL;
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
        EXPR *pExpr = NULL;
        void *pPhysAddr = NULL;

        // Default is read/write
        AccessType nAccess = atReadWrite;

        // Physical address?
        if (*pszExprEnd == ':')
        {
            int nPage = nParam;
            int nOffset;

            if (nPage < N_PAGES_MAIN && Expr::Eval(pszExprEnd+1, &nOffset, &pszExprEnd) && nOffset < MEM_PAGE_SIZE)
                pPhysAddr = PageReadPtr(nPage)+nOffset;
            else
                fRet = false;
        }
        else
        {
            pPhysAddr = AddrReadPtr(nParam);
        }

        // Parse a length expression
        EXPR *pExpr2 = Expr::Compile(pszExprEnd, &pszExprEnd);
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
                psz = strtok(NULL, " ");
            }
        }

        if (psz)
        {
            // If condition?
            if (!strcasecmp(psz, "if"))
            {
                // Compile the expression following it
                psz += strlen(psz)+1;
                pExpr = Expr::Compile(psz);
                fRet = pExpr != NULL;
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
        EXPR *pExpr = NULL;

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
                psz = strtok(NULL, " ");
            }
        }

        if (psz)
        {
            // If condition?
            if (!strcasecmp(psz, "if"))
            {
                // Compile the expression following it
                psz += strlen(psz)+1;
                pExpr = Expr::Compile(psz);
                fRet = pExpr != NULL;
            }
            else
                fRet = false;
        }

        Breakpoint::AddPort(nParam, nAccess, pExpr);
    }

    // bpint frame|line|midi
    else if (!strcasecmp(pszCommand, "bpint"))
    {
        BYTE bMask = 0x00;
        EXPR *pExpr = NULL;

        if (fCommandOnly)
            bMask = 0x1f;
        else
        {
            for (psz = strtok(pszParam, " ,") ; fRet && psz ; psz = strtok(NULL, " ,"))
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
                    psz += strlen(psz)+1;
                    pExpr = Expr::Compile(psz);
                    fRet = pExpr != NULL;
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
        BYTE bNewF = F;

        while (fRet && *pszParam)
        {
            BYTE bFlag = 0;

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
                case 'c': case 'C':	bFlag = FLAG_C; break;

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
            BREAKPT *pBreak = Breakpoint::GetAt(nParam);
            if (pBreak)
                pBreak->fEnabled = fNewState;
            else
                fRet = false;
        }
        else if (!strcmp(pszParam, "*"))
        {
            BREAKPT *pBreak = NULL;
            for (int i = 0 ; (pBreak = Breakpoint::GetAt(i)) ; i++)
                pBreak->fEnabled = fNewState;
        }
        else
            fRet = false;
    }

    // exx
    else if (fCommandOnly && !strcasecmp(pszCommand, "exx"))
    {
        // EXX
        swap(BC, BC_);
        swap(DE, DE_);
        swap(HL, HL_);
    }

    // ex reg,reg2
    else if (!strcasecmp(pszCommand, "ex"))
    {
        EXPR *pExpr2 = NULL;

        // Re-parse the first argument as a register
        Expr::Release(pExpr);
        pExpr = Expr::Compile(pszParam, &pszExprEnd, Expr::regOnly);

        // Locate and extract the second register parameter
        if ((psz = strtok(pszExprEnd, ",")))
            pExpr2 = Expr::Compile(psz, NULL, Expr::regOnly);

        // Accept if both parameters are registers
        if (pExpr  && pExpr->nType  == T_REGISTER && !pExpr->pNext &&
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
        BYTE ab[128];
        int nBytes = 0;

        for (psz = strtok(pszExprEnd, ",") ; fRet && psz ; psz = strtok(NULL, ","))
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
            for (int i = 0 ; i < nBytes ; i++)
                write_byte(nParam+i, ab[i]);
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

static const int ROW_GAP = 2;
WORD CDisView::s_wAddrs[64];

CDisView::CDisView (CWindow* pParent_)
    : CView(pParent_), m_uTarget(INVALID_TARGET), m_pcszTarget(NULL)
{
    SetText("Disassemble");
    SetFont(&sFixedFont);

    // Calculate the number of rows and columns in the view
    m_uRows = m_nHeight / (ROW_GAP+sFixedFont.wHeight+ROW_GAP);
    m_uColumns = m_nWidth / (sFixedFont.wWidth+CHAR_SPACING);

    // Allocate enough for a full screen of characters
    m_pszData = new char[m_uRows * m_uColumns + 1];
}

void CDisView::SetAddress (WORD wAddr_, bool fForceTop_)
{
    CView::SetAddress(wAddr_);

    // Update the control flow / data target address hints
    if (!SetFlowTarget())
        SetDataTarget();

    if (!fForceTop_)
    {
        // If the address is on-screen, but not first or last row, don't refresh
        for (UINT u = 1 ; u < m_uRows-1 ; u++)
        {
            if (s_wAddrs[u] == wAddr_)
            {
                wAddr_ = s_wAddrs[0];
                break;
            }
        }
    }

    char* psz = m_pszData;
    for (UINT u = 0 ; u < m_uRows ; u++)
    {
        s_wAddrs[u] = wAddr_;

        memset(psz, ' ', m_uColumns);

        // Display the address in the appropriate format
        psz += sprintf(psz, "%04X", wAddr_);
        *psz++ = ' ';
        psz++;

        // Disassemble the instruction, using an appropriate PC value
        BYTE ab[4] = { read_byte(wAddr_), read_byte(wAddr_+1), read_byte(wAddr_+2), read_byte(wAddr_+3) };
        UINT uLen = Disassemble(ab, wAddr_, psz+13, 32);

        // Show the instruction bytes between the address and the disassembly
        for (UINT v = 0 ; v < uLen ; v++)
            psz += sprintf(psz, "%02X ", read_byte(wAddr_+v));
        *psz = ' ';

        // Advance to the next line/instruction
        psz += 1+strlen(psz);
        wAddr_ += uLen;
    }

    // Terminate the line list
    *psz = '\0';
}


void CDisView::Draw (CScreen* pScreen_)
{
    UINT u = 0;
    for (char* psz = (char*)m_pszData ; *psz ; psz += strlen(psz)+1, u++)
    {
        int nHeight = ROW_GAP+sFixedFont.wHeight+ROW_GAP;
        int nX = m_nX;
        int nY = m_nY+(nHeight*u);

        BYTE bColour = WHITE;

        if (s_wAddrs[u] == PC)
        {
            pScreen_->FillRect(nX-1, nY-1, m_nWidth-115, nHeight-3, YELLOW_7);
            bColour = BLACK;

            if (m_pcszTarget)
                pScreen_->DrawString(nX+210, nY, m_pcszTarget, bColour);
        }

        BYTE *pPhysAddr = AddrReadPtr(s_wAddrs[u]);
        int nIndex = Breakpoint::GetExecIndex(pPhysAddr);
        if (nIndex != -1)
            bColour = Breakpoint::GetAt(nIndex)->pExpr ? MAGENTA_3 : RED_4;

        if (m_uTarget == INVALID_TARGET || s_wAddrs[u] != m_uTarget)
            pScreen_->DrawString(nX, nY, psz, bColour);
        else
        {
            pScreen_->DrawString(nX+30, nY, psz+5, bColour);
            pScreen_->DrawString(nX, nY, "===>", RED_6);
        }

    }

    DrawRegisterPanel(pScreen_, m_nX+m_nWidth-6*16, m_nY);
}

/*static*/ void CDisView::DrawRegisterPanel (CScreen* pScreen_, int nX_, int nY_)
{
    int i;
    int nX = nX_;
    int nY = nY_;
    char sz[128];

#define ShowLabel(str,dx,dy)  \
{   \
    pScreen_->DrawString(nX+dx, nY+dy, str, GREEN_8);  \
}

    ShowLabel("AF       AF'\n"
              "BC       BC'\n"
              "DE       DE'\n"
              "HL       HL'",   0, 0);

    ShowLabel("IX       IY\n"
              "PC       SP", 0, 52);

    ShowLabel("I     R", 0, 80);


#define ShowReg(buf,fmt,dx,dy,reg)  \
{   \
    sprintf(buf, fmt, regs.reg); \
    pScreen_->DrawString(nX+dx, nY+dy, buf, (regs.reg != sLastRegs.reg) ? RED_8 : WHITE);  \
}

    ShowReg(sz, "%02X", 18,  0, af.b.h); ShowReg(sz, "%02X", 30,  0, af.b.l);
    ShowReg(sz, "%02X", 18, 12, bc.b.h); ShowReg(sz, "%02X", 30, 12, bc.b.l);
    ShowReg(sz, "%02X", 18, 24, de.b.h); ShowReg(sz, "%02X", 30, 24, de.b.l);
    ShowReg(sz, "%02X", 18, 36, hl.b.h); ShowReg(sz, "%02X", 30, 36, hl.b.l);

    ShowReg(sz, "%02X", 18, 52, ix.b.h); ShowReg(sz, "%02X", 30, 52, ix.b.l);
    ShowReg(sz, "%04X", 18, 64, pc.w);

    ShowReg(sz, "%02X", 12, 80, i);

    ShowReg(sz, "%02X", 72,  0, af_.b.h); ShowReg(sz, "%02X", 84,  0, af_.b.l);
    ShowReg(sz, "%02X", 72, 12, bc_.b.h); ShowReg(sz, "%02X", 84, 12, bc_.b.l);
    ShowReg(sz, "%02X", 72, 24, de_.b.h); ShowReg(sz, "%02X", 84, 24, de_.b.l);
    ShowReg(sz, "%02X", 72, 36, hl_.b.h); ShowReg(sz, "%02X", 84, 36, hl_.b.l);

    ShowReg(sz, "%02X", 72, 52, iy.b.h); ShowReg(sz, "%02X", 84, 52, iy.b.l);
    ShowReg(sz, "%04X", 72, 64, sp.w);

    ShowReg(sz, "%02X", 48, 80, r);


    pScreen_->DrawString(nX+72, nY+74, " \x81\x81 ", GREY_4);

    for (i = 0 ; i < 4 ; i++)
    {
        sprintf(sz, "%04X", read_word(SP+i*2));
        pScreen_->DrawString(nX+72, nY+84 + i*12, sz, WHITE);
    }


    pScreen_->DrawString(nX, nY+96, "IM", GREEN_8);
    sprintf(sz, "%u", IM);
    pScreen_->DrawString(nX+18, nY+96, sz, RegCol(IM, sLastRegs.im));
    sprintf(sz, "  %cI", IFF1?'E':'D');
    pScreen_->DrawString(nX+18, nY+96, sz, RegCol(IFF1, sLastRegs.iff1));
/*
    sprintf(sz, "     %cI2", IFF2?'E':'D');
    pScreen_->DrawString(nX+18, nY+96, sz, RegCol(IFF2, sLastRegs.iff2));
*/
    static char szInts[] = "OFIML";
    char szIntInactive[] = "     ";
    char szIntActive[]   = "     ";
    char szIntChange[]   = "     ";
    char bIntDiff = status_reg ^ bLastStatus;

    for (i = 0 ; i < 5 ; i++)
    {
        BYTE bBit = 1 << (4-i);
        char chState = (~status_reg & bBit) ? szInts[i] : '-';

        if (bIntDiff & bBit)
            szIntChange[i] = chState;
        else if (~status_reg & bBit)
            szIntActive[i] = chState;
        else
            szIntInactive[i] = chState;
    }

    pScreen_->DrawString(nX, nY+108, "Stat", GREEN_8);
    pScreen_->DrawString(nX+30, nY+108, szIntInactive, GREY_4);
    pScreen_->DrawString(nX+30, nY+108, szIntActive, WHITE);
    pScreen_->DrawString(nX+30, nY+108, szIntChange, RegCol(1,0));


    static char szFlags[] = "SZ5H3VNC";
    char szFlagInactive[] = "        ";
    char szFlagActive[]   = "        ";
    char szFlagChange[]   = "        ";
    char bFlagDiff = F ^ sLastRegs.af.b.l;
    for (i = 0 ; i < 8 ; i++)
    {
        BYTE bBit = 1 << (7-i);
        char chState = (F & bBit) ? szFlags[i] : '-';

        if (bFlagDiff & bBit)
            szFlagChange[i] = chState;
        else if (F & bBit)
            szFlagActive[i] = chState;
        else
            szFlagInactive[i] = chState;
    }

    pScreen_->DrawString(nX, nY+132, "Flag", GREEN_8);
    pScreen_->DrawString(nX+30, nY+132, szFlagInactive, GREY_4);
    pScreen_->DrawString(nX+30, nY+132, szFlagActive, WHITE);
    pScreen_->DrawString(nX+30, nY+132, szFlagChange, RegCol(1,0));

    int nLine = (g_dwCycleCounter < BORDER_PIXELS) ? HEIGHT_LINES-1 : (g_dwCycleCounter-BORDER_PIXELS) / TSTATES_PER_LINE;
    int nLineCycle = (g_dwCycleCounter + TSTATES_PER_LINE - BORDER_PIXELS) % TSTATES_PER_LINE;

    sprintf(sz, "%03d:%03d", nLine, nLineCycle);
    pScreen_->DrawString(nX, nY+148, "Scan", GREEN_8);
    pScreen_->DrawString(nX+30, nY+148, sz, WHITE);

    sprintf(sz, "%u", g_dwCycleCounter);
    pScreen_->DrawString(nX, nY+160, "T", GREEN_8);
    pScreen_->DrawString(nX+12, nY+160, sz, WHITE);

    DWORD dwCycleDiff = ((nLastFrames*TSTATES_PER_FRAME)+g_dwCycleCounter)-dwLastCycle;
    pScreen_->DrawString(nX, nY+172, "", GREEN_8);
    if (dwCycleDiff)
    {
        sprintf(sz, "+%u", dwCycleDiff);
        pScreen_->DrawString(nX+12, nY+172, sz, WHITE);
    }

    pScreen_->DrawString(nX, nY+188, "A\nB\nC\nD", GREEN_8);
    pScreen_->DrawString(nX+12, nY+188, PageDesc(GetSectionPage(SECTION_A)), (AddrWritePtr(0x0000)==PageWritePtr(SCRATCH_WRITE))?CYAN_8:WHITE);
    pScreen_->DrawString(nX+12, nY+200, PageDesc(GetSectionPage(SECTION_B)), WHITE);
    pScreen_->DrawString(nX+12, nY+212, PageDesc(GetSectionPage(SECTION_C)), WHITE);
    pScreen_->DrawString(nX+12, nY+224, PageDesc(GetSectionPage(SECTION_D)), (AddrWritePtr(0xc000)==PageWritePtr(SCRATCH_WRITE))?CYAN_8:WHITE);

    pScreen_->DrawString(nX+60, nY+188, " L\n H\n V\n M", GREEN_8);
    sprintf(sz, "   %02X\n   %02X\n   %02X\n   %X",
        lmpr, hmpr, vmpr, ((vmpr&VMPR_MODE_MASK)>>5)+1);
    pScreen_->DrawString(nX+60, nY+188, sz, WHITE);


    pScreen_->DrawString(nX, nY+240, "Events", GREEN_8);

    CPU_EVENT *pEvent = psNextEvent;
    for (i = 0 ; i < 3 && pEvent ; i++, pEvent = pEvent->psNext)
    {
        const char *pcszEvent = "????";
        switch (pEvent->nEvent)
        {
            case evtStdIntEnd:       pcszEvent = "IEND"; break;
            case evtLineIntStart:    pcszEvent = "LINE"; break;
            case evtEndOfFrame:      pcszEvent = "FRAM"; break;
            case evtMidiOutIntStart: pcszEvent = "MIDI"; break;
            case evtMidiOutIntEnd:   pcszEvent = "MEND"; break;
            case evtMouseReset:      pcszEvent = "MOUS"; break;
            case evtBlueAlphaClock:  pcszEvent = "BLUE"; break;
            case evtAsicStartup:     pcszEvent = "ASIC"; break;
            case evtTapeEdge:        pcszEvent = "TAPE"; break;

            case evtInputUpdate:     i--; continue;
        }

        sprintf(sz, "%s       T", pcszEvent);
        pScreen_->DrawString(nX, nY+252+(i*12), sz, WHITE);
        sprintf(sz, "%6u", pEvent->dwTime-g_dwCycleCounter);
        pScreen_->DrawString(nX+5*6, nY+252+(i*12), sz, RegCol(0,1));
    }
}

bool CDisView::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    switch (nMessage_)
    {
        case GM_BUTTONDBLCLK:
        {
            UINT uRow = (nParam2_ - m_nY) / (ROW_GAP+sFixedFont.wHeight+ROW_GAP);

            if (IsOver() && uRow < m_uRows)
            {
                // Find any existing execution breakpoint
                BYTE *pPhysAddr = AddrReadPtr(s_wAddrs[uRow]);
                int nIndex = Breakpoint::GetExecIndex(pPhysAddr);

                // If there's no breakpoint, add a new one
                if (nIndex == -1)
                    Breakpoint::AddExec(pPhysAddr, NULL);
                else
                    Breakpoint::RemoveAt(nIndex);
            }
            break;
        }

        case GM_CHAR:
        {
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

                case HK_KP7:  cmdStep(1, nParam2_ != HM_NONE); break;
                case HK_KP8:  cmdStepOver(nParam2_ == HM_CTRL); break;
                case HK_KP9:  cmdStepOut();     break;
                case HK_KP4:  cmdStep(10);      break;
                case HK_KP5:  cmdStep(100);     break;
                case HK_KP6:  cmdStep(1000);    break;

                case HK_UP:
                case HK_DOWN:
                case HK_LEFT:
                case HK_RIGHT:
                case HK_PGUP:
                case HK_PGDN:
                case HK_HOME:
                case HK_END:
                    return cmdNavigate(nParam1_, nParam2_);

                case 'd': case 'D':
                    break;

                default:
                    return false;
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

bool CDisView::cmdNavigate (int nKey_, int nMods_)
{
    WORD wAddr = GetAddress();
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
                BYTE ab[4];
                for (UINT u = 0 ; u < sizeof(ab) ; u++)
                    ab[u] = read_byte(PC+u);

                wAddr = (PC += Disassemble(ab));
            }
            break;

        case HK_LEFT:
            if (!fCtrl)
                wAddr = s_wAddrs[0]-1;
            else
                wAddr = --PC;
            break;

        case HK_RIGHT:
            if (!fCtrl)
                wAddr = s_wAddrs[0]+1;
            else
                wAddr = ++PC;
            break;

        case HK_PGDN:
        {
            wAddr = s_wAddrs[m_uRows-1];
            BYTE ab[] = { read_byte(wAddr), read_byte(wAddr+1), read_byte(wAddr+2), read_byte(wAddr+3) };
            wAddr += Disassemble(ab);
            break;
        }

        case HK_PGUP:
        {
            // Aim to have the current top instruction at the bottom
            WORD w = s_wAddrs[0];

            // Start looking a screenful of single-byte instructions back
            for (wAddr = w - m_uRows ; ; wAddr--)
            {
                WORD w2 = wAddr;

                // Disassemble a screenful of instructions
                for (UINT u = 0 ; u < m_uRows-1 ; u++)
                {
                    BYTE ab[] = { read_byte(w2), read_byte(w2+1), read_byte(w2+2), read_byte(w2+3) };
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
            return false;
    }

    SetAddress(wAddr, !fCtrl);
    return true;
}

// Determine the target address
bool CDisView::SetFlowTarget ()
{
    // Extract the two bytes at PC, which we'll assume are single byte opcode and operand
    WORD wPC = PC;
    BYTE bOpcode = read_byte(wPC), bOperand = read_byte(wPC+1);
    BYTE bFlags = F, bCond = 0xff;

    // Work out the possible next instruction addresses, which depend on the instruction found
    WORD wJpTarget = read_word(wPC+1);
    WORD wJrTarget = wPC + 2 + static_cast<signed char>(read_byte(wPC+1));
    WORD wRetTarget = read_word(SP);
    WORD wRstTarget = bOpcode & 0x38;

    // No instruction target or conditional jump helper string yet
    m_uTarget = INVALID_TARGET;
    m_pcszTarget = NULL;

    // Examine the current opcode to check for flow changing instructions
    switch (bOpcode)
    {
        case OP_DJNZ:
            // Set a pretend zero flag if B is 1 and would be decremented to zero
            bFlags = (B == 1) ? FLAG_Z : 0;
            bCond = 0;
            // Fall thru...

        case OP_JR:     m_uTarget = wJrTarget;  break;
        case OP_RET:    m_uTarget = wRetTarget; break;
        case OP_JP:
        case OP_CALL:   m_uTarget = wJpTarget;  break;
        case OP_JPHL:   m_uTarget = HL;  break;

        case IX_PREFIX: if (bOperand == OP_JPHL) m_uTarget = IX;  break;     // JP (IX)
        case IY_PREFIX: if (bOperand == OP_JPHL) m_uTarget = IY;  break;     // JP (IY)

        default:
            // JR cc ?
            if ((bOpcode & 0xe7) == 0x20)
            {
                // Extract the 2-bit condition code and set the possible target
                bCond = (bOpcode >> 3) & 0x03;
                m_uTarget = wJrTarget;
                break;
            }

            // Mask to check for certain groups we're interested in
            switch (bOpcode & 0xc7)
            {
                case 0xc0:  m_uTarget = wRetTarget; break;    // RET cc
                case 0xc2:                                    // JP cc
                case 0xc4:  m_uTarget = wJpTarget;  break;    // CALL cc
                case 0xc7:  m_uTarget = wRstTarget; break;    // RST
            }

            // For all but RST, extract the 3-bit condition code
            if (m_uTarget != INVALID_TARGET && (bOpcode & 0xc7) != 0xc7)
                bCond = (bOpcode >> 3) & 0x07;

            break;
    }

    // Have we got a condition to test?
    if (bCond <= 0x07)
    {
        static const BYTE abFlags[] = { FLAG_Z, FLAG_C, FLAG_P, FLAG_S };

        // Invert the 'not' conditions to give a set bit for a mask
        bFlags ^= (bCond & 1) ? 0x00 : 0xff;

        // Condition met by flags?
        if (abFlags[bCond >> 1] & bFlags)
        {
            switch (bOpcode & 0xc7)
            {
                // The action hint depends on the instruction
                case 0xc0:  m_pcszTarget = "(RET)";   break;
                case 0xc4:  m_pcszTarget = "(CALL)";  break;
                case 0xc2:  m_pcszTarget = (wJpTarget <= wPC) ? "(JUMP \x80)" : "(JUMP \x81)";  break;
                default:    m_pcszTarget = (bOperand & 0x80) ? "(JUMP \x80)" : "(JUMP \x81)";  break;
            }
        }
        else
            m_uTarget = INVALID_TARGET;
    }

    // Return whether a target has been set
    return m_uTarget != INVALID_TARGET;
}

// Determine the target address
bool CDisView::SetDataTarget ()
{
    bool f16Bit = false;
    bool fAddress = true;

    // No target or helper string yet
    m_uTarget = INVALID_TARGET;
    m_pcszTarget = NULL;

    // Extract potential instruction bytes
    WORD wPC = PC;
    BYTE bOp0 = read_byte(wPC), bOp1 = read_byte(wPC+1), bOp2 = read_byte(wPC+2), bOp3 = read_byte(wPC+3);
    BYTE bOpcode = bOp0;

    // Adjust for any index prefix
    bool fIndex = bOp0 == 0xdd || bOp0 == 0xfd;
    if (fIndex) bOpcode = bOp1;

    // Calculate potential operand addresses
    WORD wAddr12 = (bOp2 << 8) | bOp1;
    WORD wAddr23 = (bOp3 << 8) | bOp2;
    WORD wAddr = fIndex ? wAddr23 : wAddr12;
    WORD wHLIXIYd = !fIndex ? HL : (((bOp0 == 0xdd) ? IX : IY) + bOp2);


    // 000r0010 = LD (BC/DE),A
    // 000r1010 = LD A,(BC/DE)
    if ((bOpcode & 0xe7) == 0x02)
        m_uTarget = (bOpcode & 0x10) ? DE : BC;

    // 00110010 = LD (nn),A
    // 00111010 = LD A,(nn)
    else if ((bOpcode & 0xf7) == 0x32)
    {
        m_uTarget = wAddr;
        fAddress = false;
    }

    // [DD/FD] 0011010x = [INC|DEC] (HL/IX+d/IY+d)
    // [DD/FD] 01110rrr = LD (HL/IX+d/IY+d),r
    else if ((bOpcode & 0xfe) == 0x34 || bOpcode == 0x36)
        m_uTarget = wHLIXIYd;

    // [DD/FD] 00110rrr = LD (HL/IX+d/IY+d),n
    else if (bOpcode != OP_HALT && (bOpcode & 0xf8) == 0x70)
        m_uTarget = wHLIXIYd;

    // [DD/FD] 01rrr110 = LD r,(HL/IX+d/IY+d)
    else if ((bOpcode & 0xc7) == 0x46)
        m_uTarget = wHLIXIYd;

    // [DD/FD] 10xxx110 = ADD|ADC|SUB|SBC|AND|XOR|OR|CP (HL/IX+d/IY+d)
    else if ((bOpcode & 0xc7) == 0x86)
        m_uTarget = wHLIXIYd;

    // (DD) E3 = EX (SP),HL/IX/IY
    else if (bOpcode == 0xe3)
    {
        m_uTarget = SP;
        f16Bit = true;
    }

    // [DD/FD] 00100010 = LD (nn),HL/IX/IY
    // [DD/FD] 00101010 = LD HL/IX/IY,(nn)
    else if ((bOpcode & 0xf7) == 0x22)
    {
        m_uTarget = wAddr;
        f16Bit = true;
        fAddress = false;
    }

    // ED 01dd1011 = LD [BC|DE|HL|SP],(nn)
    // ED 01dd0011 = LD (nn),[BC|DE|HL|SP]
    else if (bOpcode == 0xed && (bOp1 & 0xc7) == 0x43)
    {
        m_uTarget = wAddr23;
        f16Bit = true;
        fAddress = false;
    }

    // CB prefix?
    else if (bOpcode == 0xcb)
    {
        // DD/FD CB d 00xxxrrr = LD r, RLC|RRC|RL|RR|SLA|SRA|SLL|SRL (IX+d/IY+d)
        // DD/FD CB d xxbbbrrr = [_|BIT|RES|SET] b,(IX+d/IY+d)           
        // DD/FD CB d 1xbbbrrr = LD r,[RES|SET] b,(IX+d/IY+d)
        if (fIndex)
            m_uTarget = wHLIXIYd;

        // CB 00ooo110 = RLC|RRC|RL|RR|SLA|SRA|SLL|SRL (HL)
        // CB oobbbrrr = [_|BIT|RES|SET] b,(HL)
        else if ((bOp1 & 0x07) == 0x06)
            m_uTarget = HL;
    }

    // Do we have something to display?
    if (m_uTarget != INVALID_TARGET)
    {
        static char sz[32];
        m_pcszTarget = sz;

        if (f16Bit)
        {
			if (fAddress)
				snprintf(sz, _countof(sz), "[%04X=%04X]", m_uTarget, read_word(m_uTarget));
			else
				snprintf(sz, _countof(sz), "[%04X]", read_word(m_uTarget));
        }
        else if (fAddress)
            snprintf(sz, _countof(sz), "[%04X=%02X]", m_uTarget, read_byte(m_uTarget));
        else
			snprintf(sz, _countof(sz), "[%02X]", read_byte(m_uTarget));
    }

    // Return whether a target has been set
    return m_uTarget != INVALID_TARGET;
}

////////////////////////////////////////////////////////////////////////////////

static const int TXT_COLUMNS = 64;

CTxtView::CTxtView (CWindow* pParent_)
    : CView(pParent_)
{
    SetText("Text");
    SetFont(&sFixedFont);

    m_nRows = m_nHeight / (ROW_GAP+sFixedFont.wHeight+ROW_GAP);
    m_nColumns = 80;

    m_fEditing = false;

    // Allocate enough for a full screen of characters
    m_pszData = new char[m_nRows * m_nColumns + 1];
}

void CTxtView::SetAddress (WORD wAddr_, bool fForceTop_)
{
    CView::SetAddress(wAddr_);

    char* psz = m_pszData;
    for (int i = 0 ; i < m_nRows ; i++)
    {
        memset(psz, ' ', m_nColumns);
        psz += sprintf(psz, "%04X", wAddr_);
        *psz++ = ' ';
        *psz++ = ' ';

        for (int j = 0 ; j < 64 ; j++)
        {
            BYTE b = read_byte(wAddr_++);
            *psz++ = (b >= ' ' && b <= 0x7f) ? b : '.';
        }

        *psz++ = '\0';
    }

    *psz = '\0';
}


void CTxtView::Draw (CScreen* pScreen_)
{
    int nHeight = ROW_GAP+sFixedFont.wHeight+ROW_GAP;

    UINT u = 0;
    for (char* psz = m_pszData ; *psz ; psz += strlen(psz)+1, u++)
    {
        int nX = m_nX;
        int nY = m_nY+(nHeight*u);
        pScreen_->DrawString(nX, nY+ROW_GAP, psz, WHITE);
    }

    if (m_fEditing)
    {
        WORD wOffset = m_wEditAddr - GetAddress();

        BYTE b = read_byte(m_wEditAddr);
        char ch = (b >= ' ' && b <= 0x7f) ? b : '.';

        int nRow = wOffset / TXT_COLUMNS;
        int nCol = wOffset % TXT_COLUMNS;

        if (nRow < m_nRows)
        {
            int nX = m_nX + (4 + 2 + nCol) * (sFixedFont.wWidth+CHAR_SPACING);
            int nY = m_nY + nRow*nHeight + ROW_GAP;

            pScreen_->FillRect(nX-1, nY-1, (sFixedFont.wWidth+CHAR_SPACING)+1, sFixedFont.wHeight+1, YELLOW_8);
            pScreen_->DrawString(nX, nY, &ch, BLACK, false, 1);
        }

        pDebugger->SetStatusByte(m_wEditAddr);
    }
}

bool CTxtView::OnMessage (int nMessage_, int nParam1_, int nParam2_)
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

bool CTxtView::cmdNavigate (int nKey_, int nMods_)
{
    WORD wAddr = GetAddress();
    WORD wEditAddr = m_wEditAddr;

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
            pDebugger->SetStatus("");
            break;

        case HK_HOME:
            wEditAddr = wAddr = fCtrl ? 0 : (fShift && m_fEditing) ? wEditAddr : PC;
            break;

        case HK_END:
            wAddr = fCtrl ? (0 - m_nRows*TXT_COLUMNS) : PC;
            wEditAddr = (fCtrl ? 0 : PC + m_nRows*TXT_COLUMNS) - 1;
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
            wAddr -= m_nRows*TXT_COLUMNS;
            wEditAddr -= m_nRows*TXT_COLUMNS;
            break;

        case HK_PGDN:
            wAddr += m_nRows*TXT_COLUMNS;
            wEditAddr += m_nRows*TXT_COLUMNS;
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

            return false;
        }
    }

    if (m_fEditing)
    {
        if (wEditAddr != wAddr && static_cast<WORD>(wAddr-wEditAddr) <= TXT_COLUMNS)
            wAddr -= TXT_COLUMNS;
        else if (static_cast<WORD>(wEditAddr-wAddr) >= m_nRows*TXT_COLUMNS)
            wAddr += TXT_COLUMNS;

        if (m_wEditAddr != wEditAddr)
            m_wEditAddr = wEditAddr;
    }

    SetAddress(wAddr);
    return true;
}

////////////////////////////////////////////////////////////////////////////////

static const int HEX_COLUMNS = 16;

CHexView::CHexView (CWindow* pParent_)
    : CView(pParent_)
{
    SetText("Numeric");
    SetFont(&sFixedFont);

    m_nRows = m_nHeight / (ROW_GAP+sFixedFont.wHeight+ROW_GAP);
    m_nColumns = 80;

    m_fEditing = false;

    // Allocate enough for a full screen of characters, plus null terminators
    m_pszData = new char[m_nRows * (m_nColumns+1) + 2];
}

void CHexView::SetAddress (WORD wAddr_, bool fForceTop_)
{
    CView::SetAddress(wAddr_);

    char* psz = m_pszData;
    for (int i = 0 ; i < m_nRows ; i++)
    {
        memset(psz, ' ', m_nColumns);
        psz[m_nColumns-1] = '\0';

        psz += sprintf(psz, "%04X", wAddr_);

        *psz++ = ' ';
        *psz++ = ' ';

        for (int j = 0 ; j < HEX_COLUMNS ; j++)
        {
            BYTE b = read_byte(wAddr_++);
            psz[(HEX_COLUMNS-j)*3 + 1 + j] = (b >= ' ' && b <= 0x7f) ? b : '.';
            psz += sprintf(psz, "%02X ", b);
            *psz = ' ';
        }

        psz += strlen(psz) + 1;
    }

    *psz = '\0';

    if (m_fEditing)
        pDebugger->SetStatusByte(m_wEditAddr);
}


void CHexView::Draw (CScreen* pScreen_)
{
    UINT u = 0;
    for (char* psz = m_pszData ; *psz ; psz += strlen(psz)+1, u++)
    {
        int nHeight = sFixedFont.wHeight + 4;
        int nX = m_nX;
        int nY = m_nY + 2 + (nHeight*u);

        pScreen_->DrawString(nX, nY, psz, WHITE);
    }

    if (m_fEditing)
    {
        WORD wOffset = m_wEditAddr - GetAddress();

        char sz[3];
        BYTE b = read_byte(m_wEditAddr);
        snprintf(sz, 3, "%02X", b);

        int nRow = wOffset / HEX_COLUMNS;
        int nCol = wOffset % HEX_COLUMNS;

        if (nRow < m_nRows)
        {
            int nX = m_nX + (4 + 2 + nCol*3 + m_fRightNibble) * (sFixedFont.wWidth+CHAR_SPACING);
            int nY = m_nY + nRow*(sFixedFont.wHeight+4) + 2;

            pScreen_->FillRect(nX-1, nY-1, (sFixedFont.wWidth+CHAR_SPACING)+1, sFixedFont.wHeight+1, YELLOW_8);
            pScreen_->DrawString(nX, nY, sz+m_fRightNibble, BLACK, false, 1);

            nX = m_nX + (4 + 2 + HEX_COLUMNS*3 + 1 + nCol) * (sFixedFont.wWidth+CHAR_SPACING);
            char ch = (b >= ' ' && b <= 0x7f) ? b : '.';
            pScreen_->FillRect(nX-1, nY-1, (sFixedFont.wWidth+CHAR_SPACING)+1, sFixedFont.wHeight+1, GREY_6);
            pScreen_->DrawString(nX, nY, &ch, BLACK, false, 1);
        }
    }
}

bool CHexView::OnMessage (int nMessage_, int nParam1_, int nParam2_)
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

bool CHexView::cmdNavigate (int nKey_, int nMods_)
{
    WORD wAddr = GetAddress();
    WORD wEditAddr = m_wEditAddr;

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
            pDebugger->SetStatus("");
            break;

        case HK_HOME:
            wEditAddr = wAddr = fCtrl ? 0 : (fShift && m_fEditing) ? wEditAddr : PC;
            break;

        case HK_END:
            wAddr = fCtrl ? (0 - m_nRows*HEX_COLUMNS) : PC;
            wEditAddr = (fCtrl ? 0 : PC + m_nRows*HEX_COLUMNS) - 1;
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
            wAddr -= m_nRows*HEX_COLUMNS;
            wEditAddr -= m_nRows*HEX_COLUMNS;
            break;

        case HK_PGDN:
            wAddr += m_nRows*HEX_COLUMNS;
            wEditAddr += m_nRows*HEX_COLUMNS;
            break;

        default:
        {
            // In editing mode allow new hex values to be typed
            if (m_fEditing && isxdigit(nKey_))
            {
                BYTE bNibble = isdigit(nKey_) ? nKey_-'0' : 10+tolower(nKey_)-'a';

                // Modify using the new nibble
                if (m_fRightNibble)
                    write_byte(wEditAddr, (read_byte(wEditAddr)&0xf0) | bNibble);
                else
                    write_byte(wEditAddr, (read_byte(wEditAddr)&0x0f) | (bNibble << 4));

                // Change nibble
                m_fRightNibble = !m_fRightNibble;

                // Advance to next byte if we've just completed one
                if (!m_fRightNibble)
                    wEditAddr++;

                break;
            }

            return false;
        }
    }

    if (m_fEditing)
    {
        if (wEditAddr != wAddr && static_cast<WORD>(wAddr-wEditAddr) <= HEX_COLUMNS)
            wAddr -= HEX_COLUMNS;
        else if (static_cast<WORD>(wEditAddr-wAddr) >= m_nRows*HEX_COLUMNS)
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
CMemView::CMemView (CWindow* pParent_)
    : CView(pParent_)
{
}

void CMemView::SetAddress (WORD wAddr_, bool fForceTop_)
{
    BYTE* psz = (BYTE*)szDisassem;

    UINT uLen = 16384, uBlock = uLen >> 8;
    for (int i = 0 ; i < 256 ; i++)
    {
        UINT uCount = 0;
        for (UINT u = uBlock ; u-- ; uCount += !!read_byte(wAddr_++));
        *psz++ = (uCount + uBlock/50) * 100 / uBlock;
    }
}


void CMemView::Draw (CScreen* pScreen_)
{
    pScreen_->SetFont(&sFixedFont, true);

    UINT uGap = 12;

    for (UINT u = 0 ; u < 256 ; u++)
    {
        UINT uLen = (m_nHeight - uGap) * ((BYTE*)szDisassem)[u] / 100;
        pScreen_->DrawLine(m_nX+u, m_nY+m_nHeight-uGap-uLen, 0, uLen, (u & 16) ? WHITE : GREY_7);
    }

    pScreen_->DrawString(m_nX, m_nY+m_nHeight-10, "Page 0: 16K in 1K units", WHITE);

    pScreen_->SetFont(&sGUIFont);
}
*/

////////////////////////////////////////////////////////////////////////////////
// Graphics View

static const int STRIP_GAP = 8;
UINT CGfxView::s_uMode = 4, CGfxView::s_uWidth = 8, CGfxView::s_uZoom = 1;

CGfxView::CGfxView (CWindow* pParent_)
    : CView(pParent_), m_fGrid(true)
{
    SetText("Graphics");
    SetFont(&sFixedFont);

    // Allocate enough space for a double-width window, at 1 byte per pixel
    m_pbData = new BYTE[m_nWidth*m_nHeight*2];

    // Start with the current video mode
    s_uMode = ((vmpr & VMPR_MODE_MASK) >> 5) + 1;
}

void CGfxView::SetAddress (WORD wAddr_, bool fForceTop_)
{
    static const UINT auPPB[] = { 8, 8, 2, 2 };   // Pixels Per Byte in each mode

    CView::SetAddress(wAddr_);

    m_uStripWidth = s_uWidth * s_uZoom * auPPB[s_uMode-1];
    m_uStripLines = m_nHeight / s_uZoom;
    m_uStrips = (m_nWidth+STRIP_GAP + m_uStripWidth+STRIP_GAP-1) / (m_uStripWidth+STRIP_GAP);

    BYTE* pb = m_pbData;
    for (UINT u = 0 ; u < ((m_uStrips+1)*m_uStripLines) ; u++)
    {
        switch (s_uMode)
        {
            case 1:
            case 2:
            {
                for (UINT v = 0 ; v < s_uWidth ; v++)
                {
                    BYTE b = read_byte(wAddr_++);

                    BYTE bGridBg = m_fGrid ? BLUE_1 : BLACK;
                    BYTE bBg0 = (u&1)? BLACK :bGridBg;
                    BYTE bBg1 = (u&1)? bGridBg :BLACK;

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
                for (UINT v = 0 ; v < s_uWidth ; v++)
                {
                    BYTE b = read_byte(wAddr_++);

                    // To keep things simple, draw only the odd pixels
                    memset(pb, mode3clut[(b & 0x30) >> 4], s_uZoom); pb += s_uZoom;
                    memset(pb, mode3clut[(b & 0x03)     ], s_uZoom); pb += s_uZoom;
                }
                break;
            }

            case 4:
            {
                for (UINT v = 0 ; v < s_uWidth ; v++)
                {
                    BYTE b = read_byte(wAddr_++);

                    memset(pb, clut[b >> 4],  s_uZoom); pb += s_uZoom;
                    memset(pb, clut[b & 0xf], s_uZoom); pb += s_uZoom;
                }
                break;
            }
        }
    }

    char sz[128]={};
    snprintf(sz, sizeof(sz)-1, "%04X  Mode %u  Width %u  Zoom %ux", GetAddress(), s_uMode, s_uWidth, s_uZoom);
    pDebugger->SetStatus(sz, WHITE, &sFixedFont);

}

void CGfxView::Draw (CScreen* pScreen_)
{
    // Clip to the client area to prevent partial strips escaping
    pScreen_->SetClip(m_nX, m_nY, m_nWidth, m_nHeight);

    BYTE* pb = m_pbData;

    for (UINT u = 0 ; u < m_uStrips+1 ; u++)
    {
        int nX = m_nX + u*(m_uStripWidth+STRIP_GAP);
        int nY = m_nY;

        for (UINT v = 0 ; v < m_uStripLines ; v++, pb += m_uStripWidth)
        {
            for (UINT w = 0 ; w < s_uZoom ; w++, nY++)
            {
                pScreen_->Poke(nX, nY, pb, m_uStripWidth);
            }
        }
    }

    pScreen_->SetClip();
}

bool CGfxView::OnMessage (int nMessage_, int nParam1_, int nParam2_)
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

bool CGfxView::cmdNavigate (int nKey_, int nMods_)
{
    WORD wAddr = GetAddress();
    bool fCtrl = (nMods_ & HM_CTRL) != 0;

    switch (nKey_)
    {
        // Keys 1 to 4 select the screen mode
        case '1': case '2': case '3': case '4':
            s_uMode = nKey_-'0';
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
            wAddr = fCtrl ? (static_cast<WORD>(0) - m_uStrips * m_uStripLines * s_uWidth) : PC;
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
            return false;
    }

    SetAddress(wAddr, true);
    return true;
}


////////////////////////////////////////////////////////////////////////////////
// Breakpoint View

CBptView::CBptView (CWindow* pParent_)
    : CView(pParent_)
{
    SetText("Breakpoints");
    SetFont(&sFixedFont);

    m_nRows = (m_nHeight / (ROW_GAP+sFixedFont.wHeight+ROW_GAP)) - 1;

    // Allocate enough for a full screen of characters, plus null terminators
    m_pszData = new char[m_nRows*81 + 2];
    m_pszData[0] = '\0';

    m_nLines = m_nTopLine = 0;
}

void CBptView::SetAddress (WORD wAddr_, bool fForceTop_)
{
    CView::SetAddress(wAddr_);

    char *psz = m_pszData;

    if (!Breakpoint::IsSet())
        psz += sprintf(psz, "No breakpoints") + 1;
    else
    {
        int i = 0;

        m_nLines = 0;
        m_nActive = -1;

        for (BREAKPT *p = NULL ; (p = Breakpoint::GetAt(i)) ; i++, m_nLines++)
        {
            psz += sprintf(psz, "%2d: %s", i, Breakpoint::GetDesc(p)) + 1;

            // Check if we're on an execution breakpoint (ignoring condition)
            BYTE* pPhys = AddrReadPtr(PC);
            if (p->nType == btExecute && p->Exec.pPhysAddr == pPhys)
                m_nActive = i;
        }
    }

    // Double null to terminate text
    *psz++ = '\0';
}


void CBptView::Draw (CScreen* pScreen_)
{
    int i;
    char *psz = m_pszData;

    for (i = 0 ; i < m_nTopLine && *psz ; i++)
        psz += strlen(psz)+1;

    for (i = 0 ; i < m_nRows && *psz ; i++)
    {
        int nHeight = ROW_GAP+sFixedFont.wHeight+ROW_GAP;
        int nX = m_nX + 2;
        int nY = m_nY + 4 + i*nHeight;

        BREAKPT *pBreak = Breakpoint::GetAt(m_nTopLine+i);
        BYTE bColour = (i==m_nActive) ? CYAN_8 : (pBreak && !pBreak->fEnabled) ? GREY_4 : WHITE;
        pScreen_->DrawString(nX, nY, psz, bColour);
        psz += strlen(psz)+1;
    }

    CDisView::DrawRegisterPanel(pScreen_, m_nX+m_nWidth-6*16, m_nY);
}

bool CBptView::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    switch (nMessage_)
    {
        case GM_BUTTONDBLCLK:
        {
            int nHeight = ROW_GAP+sFixedFont.wHeight+ROW_GAP;
            int nIndex = (nParam2_ - m_nY) / nHeight;

            if (IsOver() && nIndex >= 0 && nIndex < m_nLines)
            {
                BREAKPT *pBreak = Breakpoint::GetAt(nIndex);
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

bool CBptView::cmdNavigate (int nKey_, int nMods_)
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
            return false;
    }

    if (m_nTopLine < 0)
        m_nTopLine = 0;
    else if (m_nTopLine > m_nLines-m_nRows)
        m_nTopLine = m_nLines-m_nRows;

    return true;
}
