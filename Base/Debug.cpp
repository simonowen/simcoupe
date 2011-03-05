// Part of SimCoupe - A SAM Coupe emulator
//
// Debug.cpp: Integrated Z80 debugger
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

#include "SimCoupe.h"

#include "CPU.h"
#include "Debug.h"
#include "Disassem.h"
#include "Frame.h"
#include "Keyboard.h"
#include "Options.h"
#include "Util.h"


// Helper macro to decide on item colour - bright cyan for changed or white for unchanged
#define RegCol(a,b) ((a) != (b) ? RED_8 : WHITE)

// Breakpoint item, which may form part of a list if more than one is set

static bool fDebugRefresh;
const BYTE* pStepOutStack;

static CDebugger* pDebugger;

// Special breakpoint for step-in/step-over and one-off conditional execution
static BREAKPT Break;

// Linked lists of active breakpoints
static BREAKPT *pExecBreaks, *pReadBreaks, *pWriteBreaks, *pInBreaks, *pOutBreaks;

// Last position of debugger window and last register values
int nDebugX, nDebugY;
Z80Regs sLastRegs;
BYTE bLastStatus;
DWORD dwLastCycle;
int nLastFrames;

CAddr GetPrevInstruction (CAddr addr_);
bool IsExecBreakpoint (CAddr addr_);
void ToggleBreakpoint (CAddr addr_);

void cmdStep (int nCount_=1);
void cmdStepOver ();
void cmdStepOut ();


// Activate the debug GUI, if not already active
bool Debug::Start (BREAKPT* pBreak_)
{
    if (!pDebugger)
        GUI::Start(pDebugger = new CDebugger(pBreak_));

    return true;
}

void Debug::Stop ()
{
    if (pDebugger)
        pDebugger->Destroy();
}

void Debug::FrameEnd ()
{
    nLastFrames++;
}


// Called on every RETurn, for step-out implementation
void Debug::OnRet ()
{
    // Step-out in progress?
    if (pStepOutStack)
    {
        // Get the physical location of the return address
        const BYTE* pSP = phys_read_addr(SP-2);

        // If the stack is at or just above the starting position, it should mean we've returned
        // Allow some generous slack for data that may have been on the stack above the address
        if (pSP >= pStepOutStack && pSP < (pStepOutStack+127))
            Debug::Start();
    }
}

// Force a display refresh and test any breakpoints in case of external changes
void Debug::Refresh ()
{
    fDebugRefresh = true;
    BreakpointHit();
}

// Return whether the debug GUI is active
bool Debug::IsActive ()
{
    return pDebugger != NULL;
}

// Return whether any breakpoints are active
bool Debug::IsBreakpointSet ()
{
    return pReadBreaks || pWriteBreaks || pInBreaks || pOutBreaks ||
           pExecBreaks || Break.pAddr  || Break.pExpr;
}

// Return whether any of the active breakpoints have been hit
bool Debug::BreakpointHit ()
{
    BREAKPT* p;

    // Fetch the 'physical' address of PC
    BYTE* pPC = phys_read_addr(PC);

    // Special breakpoint used for stepping, where either condition is enough to trigger the breakpoint
    if (Break.pAddr == pPC || (Break.pExpr && Expr::Eval(Break.pExpr)))
        return Debug::Start(&Break);

    // Check execution breakpoints
    for (p = pExecBreaks ; p ; p = p->pNext)
        if (p->pAddr == pPC && (!p->pExpr || Expr::Eval(p->pExpr)))
            return Debug::Start(p);

    // Check memory read breakpoints
    for (p = pReadBreaks ; p ; p = p->pNext)
        if ((p->pAddr == pbMemRead1 || p->pAddr == pbMemRead2) && (!p->pExpr || Expr::Eval(p->pExpr)))
            return Debug::Start(p);

    // Check memory write breakpoints
    for (p = pWriteBreaks ; p ; p = p->pNext)
        if ((p->pAddr == pbMemWrite1 || p->pAddr == pbMemWrite2) && (!p->pExpr || Expr::Eval(p->pExpr)))
            return Debug::Start(p);

    // Check port read breakpoints
    for (p = pInBreaks ; p ; p = p->pNext)
        if (((wPortRead & p->Port.wMask) == p->Port.wCompare) && (!p->pExpr || Expr::Eval(p->pExpr)))
            return Debug::Start(p);

    // Check port write breakpoints
    for (p = pOutBreaks ; p ; p = p->pNext)
        if (((wPortWrite & p->Port.wMask) == p->Port.wCompare) && (!p->pExpr || Expr::Eval(p->pExpr)))
            return Debug::Start(p);

    return false;
}

////////////////////////////////////////////////////////////////////////////////

CInputDialog::CInputDialog (CWindow* pParent_/*=NULL*/, const char* pcszCaption_, const char* pcszPrompt_, PFNINPUTPROC pfnNotify_)
    : CDialog(pParent_, 0,0, pcszCaption_), m_pfnNotify(pfnNotify_)
{
    // Get the length of the prompt string, so we can position the edit box correctly
    int n = CScreen::GetStringWidth(pcszPrompt_);

    // Create the prompt text control and input edit control
    new CTextControl(this, 5, 10,  pcszPrompt_, WHITE);
    m_pInput = new CEditControl(this, 5+n+5, 7, 120);

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
            Destroy();
    }
}


// Notify handler for New Address input
bool OnAddressNotify (EXPR *pExpr_)
{
    int nAddr = Expr::Eval(pExpr_);
    pDebugger->SetAddress(nAddr);
    return true;
}

// Notify handler for Execute Until expression
bool OnUntilNotify (EXPR *pExpr_)
{
    Break.pExpr = pExpr_;
    Debug::Stop();
    return false;
}

// Notify handler for Change Lmpr input
bool OnLmprNotify (EXPR *pExpr_)
{
    int nPage = Expr::Eval(pExpr_) & LMPR_PAGE_MASK;
    IO::OutLmpr((lmpr & ~LMPR_PAGE_MASK) | nPage);
    Debug::Refresh();
    return true;
}

// Notify handler for Change Hmpr input
bool OnHmprNotify (EXPR *pExpr_)
{
    int nPage = Expr::Eval(pExpr_) & HMPR_PAGE_MASK;
    IO::OutHmpr((hmpr & ~HMPR_PAGE_MASK) | nPage);
    Debug::Refresh();
    return true;
}

// Notify handler for Change Vmpr input
bool OnVmprNotify (EXPR *pExpr_)
{
    int nPage = Expr::Eval(pExpr_) & VMPR_PAGE_MASK;
    IO::OutVmpr((vmpr & ~VMPR_PAGE_MASK) | nPage);
    Debug::Refresh();
    return true;
}

// Notify handler for Change Mode input
bool OnModeNotify (EXPR *pExpr_)
{
    int nMode = Expr::Eval(pExpr_);
    if (nMode < 1 || nMode > 4)
        return false;

    IO::OutVmpr((vmpr & ~VMPR_PAGE_MASK) | ((nMode-1) << 5));
    Debug::Refresh();
    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CDebugger::s_fTransparent = false;

CDebugger::CDebugger (BREAKPT* pBreak_/*=NULL*/)
    : CDialog(NULL, 376, 268, "SimICE", false)
{
    // Move to the last display position, if any
    if (nDebugX | nDebugY)
        Move(nDebugX, nDebugY);

    m_pRegPanel = new CRegisterPanel(this, 267, 8, 0, 0);
    m_pView = new CCodeView(this);
    m_pView->SetAddress(PC);

    // Force a break from the main CPU loop, and refresh the debugger display
    g_fBreak = fDebugRefresh = true;

    // Clear the temporary breakpoint and the step-out stack watch
    Break.pAddr = pStepOutStack = NULL;
    Break.pExpr = NULL;
    pStepOutStack = NULL;

    // Form the R register value from the working parts we maintain
    R = (R7 & 0x80) | (R & 0x7f);
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

void CDebugger::SetAddress (CAddr addr_)
{
    m_pView->SetAddress(addr_, true);
}

void CDebugger::Refresh ()
{
    m_pView->SetAddress(PC);
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
    CDialog::Draw(pScreen_);

    if (fDebugRefresh)
    {
        Refresh();
        fDebugRefresh = false;
    }
}

bool CDebugger::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    bool fRet = CDialog::OnMessage(nMessage_, nParam1_, nParam2_);

    if (!fRet && nMessage_ == GM_CHAR)
    {
        fRet = true;

        // Force lower-case
        if (nParam1_ >= 'A' && nParam1_ <= 'Z')
            nParam1_ ^= ('a' ^ 'A');

        bool fCtrl = !!(nParam2_ & HM_CTRL);

        switch (nParam1_)
        {
            case 'a':
                if (fCtrl)
                    swap(AF, AF_);
                else
                    new CInputDialog(this, "New location", "Address:", OnAddressNotify);
                break;

            case 'd':
            {
                if (fCtrl)
                    swap(DE, HL);
                else
                {
                    CAddr Addr = m_pView->GetAddress();
                    m_pView->Destroy();
                    (m_pView = new CCodeView(this))->SetAddress(Addr, true);
                }
                break;
            }

            case 'i':
                if (fCtrl)
                    IFF1 = !IFF1;
                break;

            case 't':
            {
                if (nParam2_ & HM_CTRL)
                    s_fTransparent = !s_fTransparent;
                else if (!nParam2_)
                {
                    CAddr Addr = m_pView->GetAddress();
                    m_pView->Destroy();
                    (m_pView = new CTextView(this))->SetAddress(Addr, true);
                }
                break;
            }

            case 'n':
            {
                CAddr Addr = m_pView->GetAddress();
                m_pView->Destroy();
                (m_pView = new CNumView(this))->SetAddress(Addr, true);
                break;
            }
/*
            case 'm':
            {
                CAddr Addr = m_pView->GetAddress();
                m_pView->Destroy();
                (m_pView = new CMemView(this))->SetAddress(Addr, true);
                break;
            }
*/
            case 'g':
            {
                CAddr Addr = m_pView->GetAddress();
                m_pView->Destroy();
                (m_pView = new CGraphicsView(this))->SetAddress(Addr, true);
                break;
            }

            case 'l':
                new CInputDialog(this, "Change LMPR", "Page (0-31):", OnLmprNotify);
                break;

            case 'h':
                new CInputDialog(this, "Change HMPR", "Page (0-31):", OnHmprNotify);
                break;

            case 'v':
                new CInputDialog(this, "Change VMPR", "Page (0-31):", OnVmprNotify);
                break;

            case 'm':
                new CInputDialog(this, "Change Mode", "Mode (1-4):", OnModeNotify);
                break;

            case 'x':
                if (fCtrl)
                {
                    swap(BC, BC_);
                    swap(DE, DE_);
                    swap(HL, HL_);
                }
                break;

            case 'u':
                new CInputDialog(this, "Execute until", "Expression:", OnUntilNotify);
                break;


            case HK_KP0:
                IO::OutLmpr(lmpr ^ LMPR_ROM0_OFF);
                Debug::Refresh();
                break;

            case HK_KP1:
                IO::OutLmpr(lmpr ^ LMPR_ROM1);
                Debug::Refresh();
                break;

            case HK_KP2:
                IO::OutLmpr(lmpr ^ LMPR_WPROT);
                Debug::Refresh();
                break;

            default:
                fRet = false;
                break;
        }
    }

    return fRet;

}

////////////////////////////////////////////////////////////////////////////////
// Disassembler

static const UINT ROW_GAP = 2;
CAddr CCodeView::s_aAddrs[64];

CCodeView::CCodeView (CWindow* pParent_)
    : CView(pParent_), m_uTarget(0), m_pcszTarget(NULL)
{
    // Calculate the number of rows and columns in the view
    m_uRows = m_nHeight / (ROW_GAP+sOldFont.wHeight+ROW_GAP);
    m_uColumns = m_nWidth / sOldFont.wWidth;

    // Allocate enough for a full screen of characters
    m_pszData = new char[m_uRows * m_uColumns + 1];
}

void CCodeView::SetAddress (CAddr addr_, bool fForceTop_)
{
    m_addr = addr_;

    // Update the control flow hints from the current PC value
    SetFlowTarget();

    if (!fForceTop_)
    {
        // If the address is on-screen, but not first or last row, don't refresh
        for (UINT u = 1 ; u < m_uRows-1 ; u++)
        {
            if (s_aAddrs[u] == addr_)
            {
                addr_ = s_aAddrs[0];
                break;
            }
        }
    }

    char* psz = (char*)m_pszData;
    for (UINT u = 0 ; u < m_uRows ; u++)
    {
        s_aAddrs[u] = addr_;

        memset(psz, ' ', m_uColumns);

        // Display the address in the appropriate format
        psz += addr_.sprint(psz);
        *psz++ = ' ';
        psz++;

        // Disassemble the instruction, using an appropriate PC value
        BYTE ab[4] = { addr_[0], addr_[1], addr_[2], addr_[3] };
        UINT uLen = Disassemble(ab, addr_.GetPC(), psz+13, 32);

        // Show the instruction bytes between the address and the disassembly
        for (UINT v = 0 ; v < uLen ; v++)
            psz += sprintf(psz, "%02X ", addr_[v]);
        *psz = ' ';

        // Advance to the next line/instruction
        psz += 1+strlen(psz);
        addr_ += uLen;
    }

    // Terminate the line list
    *psz = '\0';
}


void CCodeView::Draw (CScreen* pScreen_)
{
    pScreen_->SetFont(&sOldFont, true);

    UINT u = 0;
    for (char* psz = (char*)m_pszData ; *psz ; psz += strlen(psz)+1, u++)
    {
        int nHeight = ROW_GAP+sOldFont.wHeight+ROW_GAP, nX = m_nX, nY = m_nY+(nHeight*u);

        BYTE bColour = WHITE;

        if (s_aAddrs[u] == PC)
        {
            pScreen_->FillRect(nX, nY+1, m_nWidth, nHeight-3, YELLOW_7);
            bColour = BLACK;

            if (m_pcszTarget)
                pScreen_->DrawString(nX+204, nY+ROW_GAP, m_pcszTarget, bColour);
        }

        if (IsExecBreakpoint(s_aAddrs[u]))
            bColour = RED_4;

        if (!m_uTarget || s_aAddrs[u] != m_uTarget)
            pScreen_->DrawString(nX, nY+ROW_GAP, psz, bColour);
        else
        {
            pScreen_->DrawString(nX+30, nY+2, psz+5, bColour);
            pScreen_->DrawString(nX, nY+2, "===>", RED_6);
        }

    }

    pScreen_->SetFont(&sGUIFont);
}

bool CCodeView::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    bool fRet = false;

    switch (nMessage_)
    {
        case GM_BUTTONDBLCLK:
        {
            UINT uRow = (nParam2_ - m_nY) / (ROW_GAP+sOldFont.wHeight+ROW_GAP);

            if (uRow < m_uRows)
                ToggleBreakpoint(s_aAddrs[uRow]);
            break;
        }

        case GM_CHAR:
        {
            fRet = true;

            switch (nParam1_)
            {
                case HK_KP7:  cmdStep();        break;
                case HK_KP8:  cmdStepOver();    break;
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
                    cmdNavigate(nParam1_, nParam2_);
                    break;

                case 'd': case 'D':
                    fRet = true;
                    break;

                default:     fRet = false; break;
            }
            break;
        }

        case GM_MOUSEWHEEL:
            cmdNavigate((nParam1_ < 0) ? HK_UP : HK_DOWN, 0);
            break;
    }

    return fRet;

}

void CCodeView::cmdNavigate (int nKey_, int nMods_)
{
    CAddr addr = m_addr;

    switch (nKey_)
    {
        case HK_UP:
            if (!nMods_)
                addr = GetPrevInstruction(s_aAddrs[0]);
            else
                addr = (PC = GetPrevInstruction(CAddr(PC)).GetPC());
            break;

        case HK_DOWN:
            if (!nMods_)
                addr = s_aAddrs[1];
            else
            {
                BYTE ab[4];
                for (UINT u = 0 ; u < sizeof ab ; u++)
                    ab[u] = read_byte(PC+u);

                addr = (PC += Disassemble(ab));
            }
            break;

        case HK_LEFT:
            if (!nMods_)
                addr = s_aAddrs[0]-1;
            else
                addr = --PC;
            break;

        case HK_RIGHT:
            if (!nMods_)
                addr = s_aAddrs[0]+1;
            else
                addr = ++PC;
            break;

        case HK_PGDN:
        {
            addr = s_aAddrs[m_uRows-1];
            BYTE ab[] = { addr[0], addr[1], addr[2], addr[3] };
            addr += Disassemble(ab);
            break;
        }

        case HK_PGUP:
        {
            // Aim to have the current top instruction at the bottom
            CAddr a = s_aAddrs[0];

            // Start looking a screenful of single-byte instructions back
            for (addr = a - m_uRows ; ; addr--)
            {
                CAddr b = addr;

                // Disassemble a screenful of instructions
                for (UINT u = 0 ; u < m_uRows-1 ; u++)
                {
                    BYTE ab[] = { b[0], b[1], b[2], b[3] };
                    b += Disassemble(ab);
                }

                // Check for a suitable ending position
                if (b == (a-1) || b == (a-2) || b == (a-3) || b == (a-4))
                    break;
            }
            break;
        }

        default:  return;
    }

    SetAddress(addr, !nMods_);
}

// Determine the target address
void CCodeView::SetFlowTarget ()
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
    m_uTarget = 0;
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
            if (m_uTarget && (bOpcode & 0xc7) != 0xc7)
                bCond = (bOpcode >> 3) & 0x07;

            break;
    }

    // Have we got a condition to test?
    if (bCond <= 0x07)
    {
        static const BYTE abFlags[] = { FLAG_Z, FLAG_C, FLAG_P, FLAG_N };

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
            m_uTarget = 0;
    }
}

////////////////////////////////////////////////////////////////////////////////

CAddr CTextView::s_aAddrs[64];

CTextView::CTextView (CWindow* pParent_)
    : CView(pParent_)
{
    m_uRows = m_nHeight / (ROW_GAP+sOldFont.wHeight+ROW_GAP);
    m_uColumns = m_nWidth / sOldFont.wWidth;

    // Allocate enough for a full screen of characters
    m_pszData = new char[m_uRows * m_uColumns + 1];
}

void CTextView::SetAddress (CAddr addr_, bool fForceTop_)
{
    m_addr = addr_;

    char* psz = m_pszData;
    for (UINT u = 0 ; u < m_uRows ; u++)
    {
        s_aAddrs[u] = addr_;

        memset(psz, ' ', m_uColumns);
        psz += addr_.sprint(psz);
        *psz++ = ' ';
        *psz++ = ' ';
        *psz++ = ' ';

        for (UINT v = 0 ; v < 32 ; v++)
        {
            BYTE b = *addr_++;
            *psz++ = (b >= ' ' && b <= 0x7f) ? b : '.';
        }

        *psz++ = '\0';
    }

    *psz = '\0';
}


void CTextView::Draw (CScreen* pScreen_)
{
    pScreen_->SetFont(&sOldFont, true);

    UINT u = 0;
    for (char* psz = m_pszData ; *psz ; psz += strlen(psz)+1, u++)
    {
        int nHeight = 2+sOldFont.wHeight+2, nX = m_nX, nY = m_nY+(nHeight*u);
        pScreen_->DrawString(nX, nY+2, psz, WHITE);
    }

    pScreen_->SetFont(&sGUIFont);
}

bool CTextView::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    return (nMessage_ == GM_CHAR) && cmdNavigate(nParam1_, nParam2_);
}

bool CTextView::cmdNavigate (int nKey_, int nMods_)
{
    CAddr addr = m_addr;

    switch (nKey_)
    {
        // Eat requests to select the same view
        case 't': case 'T':
            return true;

        case HK_UP:         addr -= 32; break;
        case HK_DOWN:       addr += 32; break;
        case HK_LEFT:       addr--;     break;
        case HK_RIGHT:      addr++;     break;
        case HK_PGUP:     addr -= m_uRows*32; break;
        case HK_PGDN:   addr += m_uRows*32; break;

        default:
            return false;
    }

    SetAddress(addr);
    return true;
}

////////////////////////////////////////////////////////////////////////////////

CAddr CNumView::s_aAddrs[64];

CNumView::CNumView (CWindow* pParent_)
    : CView(pParent_)
{
    m_uRows = m_nHeight / (2+sOldFont.wHeight+2);
    m_uColumns = m_nWidth / sOldFont.wWidth;

    // Allocate enough for a full screen of characters, plus null terminators
    m_pszData = new char[m_uRows * (m_uColumns+1) + 2];

}

void CNumView::SetAddress (CAddr addr_, bool fForceTop_)
{
    m_addr = addr_;

    char* psz = m_pszData;
    for (UINT u = 0 ; u < m_uRows ; u++)
    {
        s_aAddrs[u] = addr_;

        memset(psz, ' ', m_uColumns);
        psz += addr_.sprint(psz);
        *psz++ = ' ';
        *psz++ = ' ';
        *psz++ = ' ';

        for (UINT v = 0 ; v < 11 ; v++)
        {
            BYTE b = *addr_++;
            sprintf(psz, "%02X ", b);
            psz[3] = ' ';
            psz += 3;
        }

        // Replace the final space with a NULL
        psz[-1] = '\0';
    }

    *psz = '\0';
}


void CNumView::Draw (CScreen* pScreen_)
{
    pScreen_->SetFont(&sOldFont, true);

    UINT u = 0;
    for (char* psz = m_pszData ; *psz ; psz += strlen(psz)+1, u++)
    {
        int nHeight = sOldFont.wHeight+4, nX = m_nX, nY = m_nY+(nHeight*u);
        pScreen_->DrawString(nX, nY+2, psz, WHITE);
    }

    pScreen_->SetFont(&sGUIFont);
}

bool CNumView::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    return (nMessage_ == GM_CHAR) && cmdNavigate(nParam1_, nParam2_);
}

bool CNumView::cmdNavigate (int nKey_, int nMods_)
{
    CAddr addr = m_addr;

    switch (nKey_)
    {
        // Eat requests to select the same view
        case 'n': case 'N':
            return true;

        case HK_UP:         addr -= 11; break;
        case HK_DOWN:       addr += 11; break;
        case HK_LEFT:       addr--;     break;
        case HK_RIGHT:      addr++;     break;
        case HK_PGUP:       addr -= m_uRows*11; break;
        case HK_PGDN:       addr += m_uRows*11; break;

        default:
            return false;
    }

    SetAddress(addr, true);
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
    pScreen_->SetFont(&sOldFont, true);

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

static const UINT STRIP_GAP = 8;
UINT CGraphicsView::s_uMode = 4, CGraphicsView::s_uWidth = 8, CGraphicsView::s_uZoom = 1;

CGraphicsView::CGraphicsView (CWindow* pParent_)
    : CView(pParent_)
{
    // Allocate enough space for a double-width window, at 1 byte per pixel
    m_pbData = new BYTE[m_nWidth*m_nHeight*2];
}

void CGraphicsView::SetAddress (CAddr addr_, bool fForceTop_)
{
    static UINT auPPB[] = { 8, 8, 2, 2 };   // Pixels Per Byte in each mode

    m_addr = addr_;

    m_uStripWidth = s_uWidth * s_uZoom * auPPB[s_uMode-1];
    m_uStripLines = m_nHeight / s_uZoom;
    m_uStrips = (m_nWidth+STRIP_GAP + m_uStripWidth+STRIP_GAP-1) / (m_uStripWidth + STRIP_GAP);

    BYTE* pb = m_pbData;
    for (UINT u = 0 ; u < (m_uStrips*m_uStripLines) ; u++)
    {
        switch (s_uMode)
        {
            case 1:
            case 2:
            {
                for (UINT v = 0 ; v < s_uWidth ; v++)
                {
                    BYTE b = *addr_++;

                    memset(pb, (b & 0x80) ? WHITE : BLACK, s_uZoom); pb += s_uZoom;
                    memset(pb, (b & 0x40) ? WHITE : BLACK, s_uZoom); pb += s_uZoom;
                    memset(pb, (b & 0x20) ? WHITE : BLACK, s_uZoom); pb += s_uZoom;
                    memset(pb, (b & 0x10) ? WHITE : BLACK, s_uZoom); pb += s_uZoom;
                    memset(pb, (b & 0x08) ? WHITE : BLACK, s_uZoom); pb += s_uZoom;
                    memset(pb, (b & 0x04) ? WHITE : BLACK, s_uZoom); pb += s_uZoom;
                    memset(pb, (b & 0x02) ? WHITE : BLACK, s_uZoom); pb += s_uZoom;
                    memset(pb, (b & 0x01) ? WHITE : BLACK, s_uZoom); pb += s_uZoom;
                }
                break;
            }

            case 3:
            {
                for (UINT v = 0 ; v < s_uWidth ; v++)
                {
                    BYTE b = *addr_++;

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
                    BYTE b = *addr_++;

                    memset(pb, clut[b >> 4],  s_uZoom); pb += s_uZoom;
                    memset(pb, clut[b & 0xf], s_uZoom); pb += s_uZoom;
                }
                break;
            }
        }
    }
}

void CGraphicsView::Draw (CScreen* pScreen_)
{
    // Clip to the client area to prevent partial strips escaping
    pScreen_->SetClip(m_nX, m_nY, m_nWidth, m_nHeight);
    pScreen_->SetFont(&sOldFont, true);

    BYTE* pb = m_pbData;

    for (UINT u = 0 ; u < m_uStrips ; u++)
    {
        int nX = m_nX + u*(m_uStripWidth+STRIP_GAP), nY = m_nY;

        for (UINT v = 0 ; v < m_uStripLines ; v++, pb += m_uStripWidth)
        {
            for (UINT w = 0 ; w < s_uZoom ; w++, nY++)
            {
                pScreen_->Poke(nX, nY, pb, m_uStripWidth);
            }
        }
    }

    pScreen_->SetFont(&sGUIFont);
    pScreen_->SetClip();
}

bool CGraphicsView::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    return (nMessage_ == GM_CHAR) && cmdNavigate(nParam1_, nParam2_);
}

bool CGraphicsView::cmdNavigate (int nKey_, int nMods_)
{
    CAddr addr = m_addr;

    switch (nKey_)
    {
        // Eat requests to select the same view
        case 'g': case 'G':
            return true;

        // Keys 1 to 4 select the screen mode
        case '1': case '2': case '3': case '4':
            s_uMode = nKey_-'0';
            break;

        case HK_UP:
            if (!nMods_)
                addr -= s_uWidth;
            else if (s_uZoom < 16)
                s_uZoom++;
            break;

        case HK_DOWN:
            if (!nMods_)
                addr += s_uWidth;
            else if (s_uZoom > 1)
                s_uZoom--;
            break;

        case HK_LEFT:
            if (!nMods_)
                addr--;
            else if (s_uWidth > 1)
                s_uWidth--;
            break;

        case HK_RIGHT:
            if (!nMods_)
                addr++;
            else if (s_uWidth < ((s_uMode < 3) ? 32U : 128U))   // Restrict width to mode limit
                s_uWidth++;
            break;

        case HK_PGUP:
            if (!nMods_)
                addr -= m_uStripLines * s_uWidth;
            else
                addr -= m_uStrips * m_uStripLines * s_uWidth;
            break;

        case HK_PGDN:
            if (!nMods_)
                addr += m_uStripLines * s_uWidth;
            else
                addr += m_uStrips * m_uStripLines * s_uWidth;
            break;

        default:
            return false;
    }

    SetAddress(addr, true);
    return true;
}

////////////////////////////////////////////////////////////////////////////////

CRegisterPanel::CRegisterPanel (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
    : CWindow(pParent_, nX_, nY_, nWidth_, nHeight_)
{
}

void CRegisterPanel::Draw (CScreen* pScreen_)
{
    char sz[32];

    pScreen_->SetFont(&sOldFont, true);

    pScreen_->DrawString(m_nX, m_nY+00,  "AF       AF'", GREEN_8);
    pScreen_->DrawString(m_nX, m_nY+12,  "BC       BC'", GREEN_8);
    pScreen_->DrawString(m_nX, m_nY+24,  "DE       DE'", GREEN_8);
    pScreen_->DrawString(m_nX, m_nY+36,  "HL       HL'", GREEN_8);
    pScreen_->DrawString(m_nX, m_nY+52,  "IX       IY",  GREEN_8);
    pScreen_->DrawString(m_nX, m_nY+64,  "PC       SP",  GREEN_8);
    pScreen_->DrawString(m_nX, m_nY+80,  "I        R",   GREEN_8);
    pScreen_->DrawString(m_nX, m_nY+92,  "IM",   GREEN_8);

#define ShowReg(buf,fmt,dx,dy,reg)  \
{   \
    sprintf(buf, fmt, regs.reg); \
    pScreen_->DrawString(m_nX+dx, m_nY+dy, buf, (regs.reg != sLastRegs.reg) ? RED_8 : WHITE);  \
}

    ShowReg(sz, "%02X", 18,  0, af.b.h); ShowReg(sz, "%02X", 30,  0, af.b.l);
    ShowReg(sz, "%02X", 18, 12, bc.b.h); ShowReg(sz, "%02X", 30, 12, bc.b.l);
    ShowReg(sz, "%02X", 18, 24, de.b.h); ShowReg(sz, "%02X", 30, 24, de.b.l);
    ShowReg(sz, "%02X", 18, 36, hl.b.h); ShowReg(sz, "%02X", 30, 36, hl.b.l);
    ShowReg(sz, "%02X", 18, 52, ix.b.h); ShowReg(sz, "%02X", 30, 52, ix.b.l);
    ShowReg(sz, "%02X", 18, 64, pc.b.h); ShowReg(sz, "%02X", 30, 64, pc.b.l);
    ShowReg(sz, "%02X", 18, 80, i);      ShowReg(sz, "%02X", 72, 80, r);

    ShowReg(sz, "%02X", 72,  0, af_.b.h); ShowReg(sz, "%02X", 84,  0, af_.b.l);
    ShowReg(sz, "%02X", 72, 12, bc_.b.h); ShowReg(sz, "%02X", 84, 12, bc_.b.l);
    ShowReg(sz, "%02X", 72, 24, de_.b.h); ShowReg(sz, "%02X", 84, 24, de_.b.l);
    ShowReg(sz, "%02X", 72, 36, hl_.b.h); ShowReg(sz, "%02X", 84, 36, hl_.b.l);
    ShowReg(sz, "%02X", 72, 52, iy.b.h);  ShowReg(sz, "%02X", 84, 52, iy.b.l);
    ShowReg(sz, "%02X", 72, 64, sp.b.h);  ShowReg(sz, "%02X", 84, 64, sp.b.l);

    sprintf(sz, "%d", IM);                 pScreen_->DrawString(m_nX+18, m_nY+92, sz, RegCol(IM, sLastRegs.im));
    sprintf(sz, "%s", IFF1 ? "EI" : "DI"); pScreen_->DrawString(m_nX+36, m_nY+92, sz, RegCol(IFF1, sLastRegs.iff1));

    static char szSet2[] = "-----", szReset2[] = "OFIML";
    char szFlags2[] = "        \0        ", bDiff2 = status_reg ^ bLastStatus;
    for (int j = 0 ; j < 5 ; j++)
    {
        BYTE bBit = 1 << (4-j);
        szFlags2[j + ((bDiff2 & bBit) ? 9 : 0)] = szReset2[j] ^ ((status_reg & bBit) ? (szReset2[j] ^ szSet2[j]) : 0);
    }
    pScreen_->DrawString(m_nX+60, m_nY+92, szFlags2, WHITE);
    pScreen_->DrawString(m_nX+60, m_nY+92, szFlags2+9, RegCol(1,0));


    pScreen_->DrawString(m_nX, m_nY+108, "Flags:", GREEN_8);
    static char szSet[] = "SZ5H3VNC", szReset[] = "--------";
    char szFlags[] = "        \0        ", bDiff = F ^ sLastRegs.af.b.l;
    for (int i = 0 ; i < 8 ; i++)
    {
        BYTE bBit = 1 << (7-i);
        szFlags[i + ((bDiff & bBit) ? 9 : 0)] = szReset[i] ^ ((F & bBit) ? (szReset[i] ^ szSet[i]) : 0);
    }
    pScreen_->DrawString(m_nX+45, m_nY+108, szFlags, WHITE);
    pScreen_->DrawString(m_nX+45, m_nY+108, szFlags+9, RegCol(1,0));

    pScreen_->DrawString(m_nX, m_nY+136, "ROM0", (lmpr&LMPR_ROM0_OFF)?GREY_3:WHITE);
    pScreen_->DrawString(m_nX, m_nY+136, "      ROM1", (lmpr&LMPR_ROM1)?WHITE:GREY_3);
    pScreen_->DrawString(m_nX, m_nY+136, "            WPROT", (lmpr&LMPR_WPROT)?WHITE:GREY_3);

    pScreen_->DrawString(m_nX, m_nY+148, "L:    H:    V:", GREEN_8);
    sprintf(sz, "  %02d    %02d    %02d", lmpr&0x1f, hmpr&0x1f, vmpr&0x1f);
    pScreen_->DrawString(m_nX, m_nY+148, sz, WHITE);

    pScreen_->DrawString(m_nX, m_nY+160, "LE:   HE:   M:", GREEN_8);
    sprintf(sz, "   %02d    %02d   %01d", lepr&0x1f, hepr&0x1f, ((vmpr&VMPR_MODE_MASK)>>5)+1);
    pScreen_->DrawString(m_nX, m_nY+160, sz, WHITE);

    int nLine = (g_dwCycleCounter < BORDER_PIXELS) ? HEIGHT_LINES-1 : (g_dwCycleCounter-BORDER_PIXELS) / TSTATES_PER_LINE;
    int nLineCycle = (g_dwCycleCounter + TSTATES_PER_LINE - BORDER_PIXELS) % TSTATES_PER_LINE;

    pScreen_->DrawString(m_nX, m_nY+176, "Scan:", GREEN_8);
    sprintf(sz, "%03d:%03d", nLine, nLineCycle);
    pScreen_->DrawString(m_nX+36, m_nY+176, sz, RegCol(1,0));

    DWORD dwCycleDiff = ((nLastFrames*TSTATES_PER_FRAME)+g_dwCycleCounter)-dwLastCycle;
    pScreen_->DrawString(m_nX, m_nY+188, "T-diff:", GREEN_8);
    sprintf(sz, "%u", dwCycleDiff);
    pScreen_->DrawString(m_nX+44, m_nY+188, sz, WHITE);

    pScreen_->DrawString(m_nX, m_nY+200, "T-states:", GREEN_8);     // Change back to T-diff!
    sprintf(sz, "%u", g_dwCycleCounter);
    pScreen_->DrawString(m_nX+58, m_nY+200, sz, WHITE);

    pScreen_->SetFont(&sGUIFont);
}

////////////////////////////////////////////////////////////////////////////////
/*
void CCommandLine::Execute (const char* pcszCommand_)
{
    char szCommand[128], *pszCommand = strtok(strcpy(szCommand, pcszCommand_), " "), *psz;
    EXPR* pReg = NULL;

    if (!pszCommand)
        return;

    if (!strcasecmp(pszCommand, "x"))
    {
        pszCommand += 1+strlen(pszCommand);

        if (*pszCommand && (pReg = Expr::Compile(pszCommand)))
        {
            Expr::nCount = Expr::Eval(pReg);
            Break.pExpr = &Expr::Counter;
            Debug::Stop();
        }
        else if (!(psz = strtok(NULL, " ")))
            Debug::Stop();
        else if (!strcasecmp(psz, "until"))
        {
            if ((Break.pExpr = Expr::Compile(psz+1+strlen(psz))))
                Debug::Stop();
        }
    }
    else if (!strcasecmp(pszCommand, "g"))
    {
        if ((pReg = Expr::Compile(pszCommand+1+strlen(pszCommand))))
        {
            Break.pAddr = phys_read_addr(Expr::Eval(pReg));
            Debug::Stop();
        }
    }
    else if (!strcasecmp(pszCommand, "im"))
    {
        if ((pReg = Expr::Compile(pszCommand+1+strlen(pszCommand))))
        {
            WORD wMode = Expr::Eval(pReg);

            if (wMode <= 2)
                IM = wMode & 0xff;
        }
    }
    else if (!strcasecmp(pszCommand, "di") && !(psz = strtok(NULL, " ")))
        IFF1 = 0;
    else if (!strcasecmp(pszCommand, "ei") && !(psz = strtok(NULL, " ")))
        IFF1 = 1;
    else if (!strcasecmp(pszCommand, "exit"))
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
    else if (!strcasecmp(pszCommand, "exx"))
    {
        // EXX
        swap(BC, BC_);
        swap(DE, DE_);
        swap(HL, HL_);
    }
    else if (!strcasecmp(pszCommand, "ex"))
    {
        // EX AF,AF' ?
        if ((psz = strtok(NULL, " ,")) && !strcasecmp(psz, "af") &&
            (psz = strtok(NULL, " '"))  && !strcasecmp(psz, "af") &&
            !(psz = strtok(NULL, " ")))
        {
            swap(AF, AF_);
        }
    }
    else if (!strcasecmp(pszCommand, "u"))
    {
        if ((psz = strtok(NULL, " ,=")))
        {
            if ((pReg = Expr::Compile(psz, &psz)))
            {
                WORD wAddr = Expr::Eval(pReg);
                m_pView->SetAddress(wAddr, true);
            }
        }
    }
    else if (!strcasecmp(pszCommand, "r") || !strcasecmp(pszCommand, "ld"))
    {
        if ((psz = strtok(NULL, " ,=")))
        {
            EXPR* pReg = Expr::Compile(psz);

            if (pReg && pReg->nType == T_REGISTER && !pReg->pNext)
            {
                int nReg = pReg->nValue;
                Expr::Release(pReg);

                if ((pReg = Expr::Compile(psz+1+strlen(psz))))
                {
                    WORD w = Expr::Eval(pReg);
                    BYTE b = w & 0xff;

                    switch (nReg)
                    {
                        case REG_A:      A = b; break;
                        case REG_F:      F = b; break;
                        case REG_B:      B = b; break;
                        case REG_C:      C = b; break;
                        case REG_D:      D = b; break;
                        case REG_E:      E = b; break;
                        case REG_H:      H = b; break;
                        case REG_L:      L = b; break;
                        case REG_ALT_A:  A_ = b; break;
                        case REG_ALT_F:  F_ = b; break;
                        case REG_ALT_B:  B_ = b; break;
                        case REG_ALT_C:  C_ = b; break;
                        case REG_ALT_D:  D_ = b; break;
                        case REG_ALT_E:  E_ = b; break;
                        case REG_ALT_H:  H_ = b; break;
                        case REG_ALT_L:  L_ = b; break;

                        case REG_AF:     AF  = w; break;
                        case REG_BC:     BC  = w; break;
                        case REG_DE:     DE  = w; break;
                        case REG_HL:     HL  = w; break;
                        case REG_ALT_AF: AF_ = w; break;
                        case REG_ALT_BC: BC_ = w; break;
                        case REG_ALT_DE: DE_ = w; break;
                        case REG_ALT_HL: HL_ = w; break;

                        case REG_IX:     IX = w; break;
                        case REG_IY:     IY = w; break;
                        case REG_SP:     SP = w; break;
                        case REG_PC:     PC = w; break;

                        case REG_IXH:    IXH = b; break;
                        case REG_IXL:    IXL = b; break;
                        case REG_IYH:    IYH = b; break;
                        case REG_IYL:    IYL = b; break;

                        case REG_I:      I = b; break;
                        case REG_R:      R7 = R = b; break;
                        case REG_IFF1:   IFF1 = !!b; break;
                        case REG_IFF2:   IFF2 = !!b; break;
                        case REG_IM:     if (b <= 2) IM = b; break;
                    }
                }
            }
        }
    }

    Expr::Release(pReg);
}
*/

// Find the longest instruction that ends before a given address
CAddr GetPrevInstruction (CAddr addr_)
{
    // Start 4 bytes back as that's the longest instruction length
    for (UINT u = 4 ; u ; u--)
    {
        CAddr a = addr_ - u;
        BYTE ab[] = { a[0], a[1], a[2], a[3] };

        // Check that the instruction length leads to the required address
        if (a+Disassemble(ab) == addr_)
            return a;
    }

    // No match found, so return 1 byte back instead
    return addr_-1;
}

bool IsExecBreakpoint (CAddr addr_)
{
    BYTE* pPhys = addr_.GetPhys();

    for (BREAKPT* p = pExecBreaks ; p ; p = p->pNext)
        if (p->pAddr == pPhys)
            return true;

    return false;
}

void ToggleBreakpoint (CAddr addr_)
{
    BYTE* pPhys = addr_.GetPhys();

    for (BREAKPT *p = pExecBreaks, *pPrev=NULL ; p ; pPrev=p, p=p->pNext)
    {
        // Check for an existing breakpoint with the same address
        if (p->pAddr == pPhys)
        {
            // Unlink it from the chain
            if (pPrev)
                pPrev->pNext = p->pNext;
            else
                pExecBreaks = p->pNext;

            // Delete the structure
            delete p;
            return;
        }
    }

    // Add a new execution breakpoint for the supplied address
    BREAKPT* pNew = new BREAKPT;
    pNew->pAddr = pPhys;
    pNew->pExpr = NULL;
    pNew->pNext = pExecBreaks;
    pExecBreaks = pNew;
}

////////////////////////////////////////////////////////////////////////////////

void cmdStep (int nCount_/*=1*/)
{
    BYTE bOpcode;
    WORD wPC;

    // Skip any index prefixes on the instruction to reach the real opcode or a CD/ED prefix
    for (wPC = PC ; ((bOpcode = read_byte(wPC)) == IX_PREFIX || bOpcode == IY_PREFIX) ; wPC++);

    // Stepping into a HALT (with interrupt enabled) will enter the appropriate interrupt handler
    // This is much friendlier than single-stepping NOPs up to the next interrupt!
    if (bOpcode == OP_HALT && IFF1)
    {
        // For IM 2, form the address of the handler and break there
        if (IM == 2)
            Break.pAddr = phys_read_addr(read_word((I << 8) | 0xff));

        // IM 0 and IM1 both use the handler at 0x0038
        else
            Break.pAddr = phys_read_addr(IM1_INTERRUPT_HANDLER);
    }

    // If an address hasn't been set, execute the requested number of instructions
    if (!Break.pAddr)
    {
        Expr::nCount = nCount_;
        Break.pExpr = &Expr::Counter;
    }

    Debug::Stop();
}

void cmdStepOver ()
{
    BYTE bOpcode, bOperand;
    WORD wPC;

    // Skip any index prefixes on the instruction to reach a CB/ED prefix or the real opcode
    for (wPC = PC ; ((bOpcode = read_byte(wPC)) == IX_PREFIX || bOpcode == IY_PREFIX) ; wPC++);
    bOperand = read_byte(wPC+1);

    // 1-byte HALT or RST ?
    if (bOpcode == OP_HALT || (bOpcode & 0xc7) == 0xc7)
        Break.pAddr = phys_read_addr(wPC+1);

    // 2-byte backwards DJNZ/JR cc, or (LD|CP|IN|OT)[I|D]R ?
    else if (((bOpcode == OP_DJNZ || (bOpcode & 0xe7) == 0x20) && (bOperand & 0x80))
           || (bOpcode == 0xed && (bOperand & 0xf4) == 0xb0))
        Break.pAddr = phys_read_addr(wPC+2);

    // 3-byte CALL, CALL cc or backwards JP cc?
    else if (bOpcode == OP_CALL || (bOpcode & 0xc7) == 0xc4 ||
           ((bOpcode & 0xc7) == 0xc2 && read_word(wPC+1) <= wPC))
        Break.pAddr = phys_read_addr(wPC+3);

    // Single step if no instruction-specific breakpoint is set
    if (!Break.pAddr)
        cmdStep();
    else
        Debug::Stop();
}

void cmdStepOut ()
{
    // Store the physical address of the current stack pointer, for checking on RETurn calls
    pStepOutStack = phys_read_addr(SP);
    Debug::Stop();
}
