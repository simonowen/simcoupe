// Part of SimCoupe - A SAM Coupe emulator
//
// Debug.cpp: Integrated Z80 debugger
//
//  Copyright (c) 1999-2003  Simon Owen
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

#include "Util.h"
#include "CPU.h"
#include "Debug.h"
#include "Disassem.h"
#include "Expr.h"
#include "Frame.h"
#include "Memory.h"
#include "Options.h"


// Helper macro to decide on item colour - bright cyan for changed or white for unchanged
#define RegCol(a,b) ((a) != (b) ? CYAN_8 : WHITE)

// Breakpoint item, which may form part of a list if more than one is set
typedef struct tagBREAKPT
{
    union
    {
        const BYTE* pAddr;
        struct { WORD wMask, wCompare; } Port;
    };

    EXPR* pExpr;
    struct tagBREAKPT* pNext;
}
BREAKPT;


static bool fDebugRefresh, fTransparent;
const BYTE* pStepOutStack;

static CDebugger* pDebugger;

// Special breakpoint for step-in/step-over and one-off conditional execution
static BREAKPT Break;

// Linked lists of active breakpoints
static BREAKPT *pExecBreaks, *pReadBreaks, *pWriteBreaks, *pInBreaks, *pOutBreaks;

// Last position of debugger window and last register values
int nDebugX, nDebugY;
Z80Regs sLastRegs, sSafeRegs;

char szDisassem[1024];
WORD awAddrs[32];

WORD wTarget;
const char* pcszCond;


// Activate the debug GUI, if not already active
bool Debug::Start ()
{
    if (!pDebugger)
        GUI::Start(pDebugger = new CDebugger);

    return true;
}


// Called on every RETurn, for step-out implementation
void Debug::OnRet ()
{
    // Step-out in progress?
    if (pStepOutStack)
    {
        // Get the physical location of the return address
        const BYTE* pSP = phys_read_addr(regs.SP.W-2);

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
    BYTE* pPC = phys_read_addr(regs.PC.W);

    // Special breakpoint used for stepping, where either condition is enough to trigger the breakpoint
    if (Break.pAddr == pPC || (Break.pExpr && Expr::Eval(Break.pExpr)))
        return Debug::Start();

    // Check execution breakpoints
    for (p = pExecBreaks ; p ; p = p->pNext)
        if (p->pAddr == pPC && (!p->pExpr || Expr::Eval(p->pExpr)))
            return Debug::Start();

    // Check memory read breakpoints
    for (p = pReadBreaks ; p ; p = p->pNext)
        if ((p->pAddr == pbMemRead1 || p->pAddr == pbMemRead2) && (!p->pExpr || Expr::Eval(p->pExpr)))
            return Debug::Start();

    // Check memory write breakpoints
    for (p = pWriteBreaks ; p ; p = p->pNext)
        if ((p->pAddr == pbMemWrite1 || p->pAddr == pbMemWrite2) && (!p->pExpr || Expr::Eval(p->pExpr)))
            return Debug::Start();

    // Check port read breakpoints
    for (p = pInBreaks ; p ; p = p->pNext)
        if (((wPortRead & p->Port.wMask) == p->Port.wCompare) && (!p->pExpr || Expr::Eval(p->pExpr)))
            return Debug::Start();

    // Check port write breakpoints
    for (p = pOutBreaks ; p ; p = p->pNext)
        if (((wPortWrite & p->Port.wMask) == p->Port.wCompare) && (!p->pExpr || Expr::Eval(p->pExpr)))
            return Debug::Start();

    return false;
}

////////////////////////////////////////////////////////////////////////////////

CDebugger::CDebugger (CWindow* pParent_/*=NULL*/)
    : CDialog(pParent_, 376, 268, "SimICE", false)
{
    // Move to the last display position, if any
    if (nDebugX | nDebugY)
        Move(nDebugX, nDebugY);

    m_pCmdLine = new CCommandLine(this, 2, m_nHeight-19, 242);
    m_pStepInto = new CImageButton(this, 247, m_nHeight-21, 21, 19, &sStepIntoIcon, 3, 2);
    m_pStepOver = new CImageButton(this, 270, m_nHeight-21, 21, 19, &sStepOverIcon, 3, 2);
    m_pStepOut  = new CImageButton(this, 293, m_nHeight-21, 21, 19, &sStepOutIcon,  3, 2);
    m_pClose = new CTextButton(this, m_nWidth-48, m_nHeight-20, "Close", 46);
    m_pTransparent = new CButton(this, m_nWidth-5, 0, 5, 5);

    m_pDisassembly = new CDisassembly(this, 5, 5, 0, 0);
    m_pRegPanel = new CRegisterPanel(this, 267, 8, 0, 0);

    // Force a break from the main CPU loop, and refresh the debugger display
    g_fBreak = fDebugRefresh = true;

    pStepOutStack = NULL;

    // Keep a copy of the current registers, in case we need to undo changes
    regs.R = (regs.R & 0x80) | (static_cast<BYTE>(radjust) & 0x7f);
    sSafeRegs = regs;
}

CDebugger::~CDebugger ()
{
    // Remember the dialog position for next time
    nDebugX = m_nX;
    nDebugY = m_nY;

    // Remember the current register values so we know what's changed next time
    sLastRegs = regs;
    radjust = regs.R;

    // Clear any cached data that could cause an immediate retrigger
    wPortRead = wPortWrite = 0;
    pbMemRead1 = pbMemRead2 = pbMemWrite1 = pbMemWrite2 = NULL;

    // Debugger is gone
    pDebugger = NULL;
}


void CDebugger::Refresh ()
{
    m_pDisassembly->SetAddress(regs.PC.W);

    // Extract the two bytes at PC, which we'll assume are single byte opcode and operand
    WORD wPC = regs.PC.W;
    BYTE bOpcode = read_byte(wPC), bOperand = read_byte(wPC+1);
    BYTE bFlags = regs.AF.B.l_, bCond = 0xff;

    // Work out the possible next instruction addresses, which depend on the instruction found
    WORD wJrTarget = wPC + 2 + static_cast<signed char>(read_byte(wPC+1));
    WORD wJpTarget = read_word(wPC+1), wRetTarget = read_word(regs.SP.W), wRstTarget = bOpcode & 0x38;

    // No instruction target or conditional jump helper string yet
    wTarget = 0;
    pcszCond = NULL;

    // Examine the current opcode to check for flow changing instructions
    switch (bOpcode)
    {
        case OP_DJNZ:
            // Set a pretend zero flag if B is 1 and would be decremented to zero
            bFlags = (regs.BC.B.h_ == 1) ? F_ZERO : 0;
            bCond = 0;
            // Fall thru...

        case OP_JR:     wTarget = wJrTarget;  break;
        case OP_RET:    wTarget = wRetTarget; break;
        case OP_JP:
        case OP_CALL:   wTarget = wJpTarget;  break;

        default:
            // JR cc ?
            if ((bOpcode & 0xe7) == 0x20)
            {
                // Extract the 2-bit condition code and set the possible target
                bCond = (bOpcode >> 3) & 0x03;
                wTarget = wJrTarget;
                break;
            }

            // Mask to check for certain groups we're interested in
            switch (bOpcode & 0xc7)
            {
                case 0xc0:  wTarget = wRetTarget; break;    // RET cc
                case 0xc2:                                  // JP cc
                case 0xc4:  wTarget = wJpTarget;  break;    // CALL cc
                case 0xc7:  wTarget = wRstTarget; break;    // RST
            }

            // For all but RST, extract the 3-bit condition code
            if (wTarget && (bOpcode & 0xc7) != 0xc7)
                bCond = (bOpcode >> 3) & 0x07;

            break;
    }

    // Have we got a condition to test?
    if (bCond <= 0x07)
    {
        static const BYTE abFlags[] = { F_ZERO, F_CARRY, F_PARITY, F_NEG };

        // Invert the 'not' conditions to give a set bit for a mask
        bFlags ^= (bCond & 1) ? 0x00 : 0xff;

        // Condition met by flags?
        if (abFlags[bCond >> 1] & bFlags)
        {
            switch (bOpcode & 0xc7)
            {
                // The action hint depends on the instruction
                case 0xc0:  pcszCond = "(RET)";   break;
                case 0xc4:  pcszCond = "(CALL)";  break;
                case 0xc2:  pcszCond = (wJpTarget <= wPC) ? "(JUMP \x80)" : "(JUMP \x81)";  break;
                default:    pcszCond = (bOperand & 0x80) ? "(JUMP \x80)" : "(JUMP \x81)";  break;
            }
        }
        else
            wTarget = 0;
    }
}

// Dialog override for background painting
void CDebugger::EraseBackground (CScreen* pScreen_)
{
    // If we're not in transparent mode, call the base to draw the normal dialog background
    if (!fTransparent)
        CDialog::EraseBackground(pScreen_);
}

// Dialog override for dialog drawing
void CDebugger::Draw (CScreen* pScreen_)
{
    CDialog::Draw(pScreen_);

    Break.pAddr = pStepOutStack = NULL;
    Break.pExpr = NULL;

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

        switch (nParam1_)
        {
            case GK_KP7:  m_pCmdLine->Execute("t"); break;
            case GK_KP8:  m_pCmdLine->Execute("p"); break;
            case GK_KP9:  m_pCmdLine->Execute("p ret"); break;
            case GK_KP4:  m_pCmdLine->Execute("x 10");  break;
            case GK_KP5:  m_pCmdLine->Execute("x 100"); break;
            case GK_KP6:  m_pCmdLine->Execute("x 1000"); break;

            default:     fRet = false; break;
        }
    }

    return fRet;
}

void CDebugger::OnNotify (CWindow* pWindow_, int nParam_)
{
    if (pWindow_ == m_pStepOut)
        m_pCmdLine->Execute("p ret");
    else if (pWindow_ == m_pStepInto)
        m_pCmdLine->Execute("t");
    else if (pWindow_ == m_pStepOver)
        m_pCmdLine->Execute("p");
    else if (pWindow_ == m_pTransparent)
        fTransparent = !fTransparent;
    else if (pWindow_ == m_pClose)
        Destroy();
}


CDisassembly::CDisassembly (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
    : CWindow(pParent_, nX_, nY_, nWidth_, nHeight_)
{
}

void CDisassembly::SetAddress (WORD wAddr_)
{
    UINT u;

    for (u = 1 ; u < 19 ; u++)
    {
        if (awAddrs[u] == wAddr_)
            return;
    }

    char* psz = szDisassem;
    for (int i = 0 ; i < 20 ; i++)
    {
        memset(psz, ' ', 64);
        sprintf(psz, "%04X", awAddrs[i] = wAddr_);

        UINT uLen = Disassemble(wAddr_, psz+5+4*3+2, 32);

        for (UINT u = 0 ; u < uLen ; u++)
            sprintf(psz+6+(3*u), "%02X", read_byte(wAddr_ + u));

        psz[4] = psz[5] = psz[8] = psz[11] = psz[14] = psz[17] = ' ';
        psz += strlen(psz)+1;

        wAddr_ += uLen;
    }

    *psz = '\0';
}


void CDisassembly::Draw (CScreen* pScreen_)
{
    pScreen_->SetFont(&sOldFont, true);

    UINT u = 0;
    for (char* psz = szDisassem ; *psz ; psz += strlen(psz)+1, u++)
    {
        int nHeight = sOldFont.wHeight+4, nX = m_nX, nY = m_nY+(nHeight*u);

        BYTE bColour = WHITE;

        if (awAddrs[u] == regs.PC.W)
        {
            pScreen_->FillRect(nX, nY+1, 254, nHeight-3, YELLOW_7);
            bColour = BLACK;

            if (pcszCond)
                pScreen_->DrawString(nX+204, nY+2, pcszCond, bColour);
        }

        if (!wTarget || awAddrs[u] != wTarget)
            pScreen_->DrawString(nX, nY+2, psz, bColour);
        else
        {
            pScreen_->DrawString(nX+30, nY+2, psz+5, bColour);
            pScreen_->DrawString(nX, nY+2, "===>", RegCol(1,0));
        }
    }

    pScreen_->SetFont(&sGUIFont);
}


CRegisterPanel::CRegisterPanel (CWindow* pParent_, int nX_, int nY_, int nWidth_, int nHeight_)
    : CWindow(pParent_, nX_, nY_, nWidth_, nHeight_)
{
}

void CRegisterPanel::Draw (CScreen* pScreen_)
{
    char sz[32];

    pScreen_->SetFont(&sOldFont, true);

    pScreen_->DrawString(m_nX, m_nY+00,  "AF       AF'", WHITE);
    pScreen_->DrawString(m_nX, m_nY+12,  "BC       BC'", WHITE);
    pScreen_->DrawString(m_nX, m_nY+24,  "DE       DE'", WHITE);
    pScreen_->DrawString(m_nX, m_nY+36,  "HL       HL'", WHITE);
    pScreen_->DrawString(m_nX, m_nY+52,  "IX       IY",  WHITE);
    pScreen_->DrawString(m_nX, m_nY+64,  "PC       SP",  WHITE);
    pScreen_->DrawString(m_nX, m_nY+80,  "I        R",   WHITE);
    pScreen_->DrawString(m_nX, m_nY+92,  "IM",   WHITE);
//  pScreen_->DrawString(m_nX, m_nY+124, "T:",   WHITE);

    sprintf(sz, "%04X", regs.AF.W); pScreen_->DrawString(m_nX+18, m_nY+00, sz, RegCol(regs.AF.W, sLastRegs.AF.W));
    sprintf(sz, "%04X", regs.BC.W); pScreen_->DrawString(m_nX+18, m_nY+12, sz, RegCol(regs.BC.W, sLastRegs.BC.W));
    sprintf(sz, "%04X", regs.DE.W); pScreen_->DrawString(m_nX+18, m_nY+24, sz, RegCol(regs.DE.W, sLastRegs.DE.W));
    sprintf(sz, "%04X", regs.HL.W); pScreen_->DrawString(m_nX+18, m_nY+36, sz, RegCol(regs.HL.W, sLastRegs.HL.W));
    sprintf(sz, "%04X", regs.IX.W); pScreen_->DrawString(m_nX+18, m_nY+52, sz, RegCol(regs.IX.W, sLastRegs.IX.W));
    sprintf(sz, "%04X", regs.PC.W); pScreen_->DrawString(m_nX+18, m_nY+64, sz, RegCol(regs.PC.W, sLastRegs.PC.W));
    sprintf(sz, "%02X", regs.I);    pScreen_->DrawString(m_nX+18, m_nY+80, sz, RegCol(regs.I, sLastRegs.I));
    sprintf(sz, "%d", regs.IM);     pScreen_->DrawString(m_nX+18, m_nY+92, sz, RegCol(regs.IM, sLastRegs.IM));

    sprintf(sz, "%04X", regs.AF_.W); pScreen_->DrawString(m_nX+72, m_nY+00, sz, RegCol(regs.AF_.W, sLastRegs.AF_.W));
    sprintf(sz, "%04X", regs.BC_.W); pScreen_->DrawString(m_nX+72, m_nY+12, sz, RegCol(regs.BC_.W, sLastRegs.BC_.W));
    sprintf(sz, "%04X", regs.DE_.W); pScreen_->DrawString(m_nX+72, m_nY+24, sz, RegCol(regs.DE_.W, sLastRegs.DE_.W));
    sprintf(sz, "%04X", regs.HL_.W); pScreen_->DrawString(m_nX+72, m_nY+36, sz, RegCol(regs.HL_.W, sLastRegs.HL_.W));
    sprintf(sz, "%04X", regs.IY.W);  pScreen_->DrawString(m_nX+72, m_nY+52, sz, RegCol(regs.IY.W, sLastRegs.IY.W));
    sprintf(sz, "%04X", regs.SP.W);  pScreen_->DrawString(m_nX+72, m_nY+64, sz, RegCol(regs.SP.W, sLastRegs.SP.W));
    sprintf(sz, "%02X", regs.R);     pScreen_->DrawString(m_nX+72, m_nY+80, sz, RegCol(regs.R, sLastRegs.R));
    sprintf(sz, "%s", regs.IFF1 ? "EI" : "DI"); pScreen_->DrawString(m_nX+54, m_nY+92, sz, RegCol(regs.IFF1, sLastRegs.IFF1));

    static char szReset[] = "sz-h-onc", abToggle[] = { 0x20, 0x20, 0x06, 0x20, 0x06, 0x2a, 0x20, 0x20 };
    char szFlags[] = "        \0        ", bDiff = regs.AF.B.l_ ^ sLastRegs.AF.B.l_;
    for (int i = 0 ; i < 8 ; i++)
    {
        BYTE bBit = 1 << (7-i);
        szFlags[i + ((bDiff & bBit) ? 9 : 0)] = szReset[i] ^ ((regs.AF.B.l_ & bBit) ? abToggle[i] : 0);
    }
    pScreen_->DrawString(m_nX, m_nY+170, szFlags, WHITE);
    pScreen_->DrawString(m_nX, m_nY+170, szFlags+9, RegCol(1,0));

    sprintf(sz, "L:%02d  H:%02d  V:%02d", lmpr&0x1f, hmpr&0x1f, vmpr&0x1f);
    pScreen_->DrawString(m_nX, m_nY+108, sz, WHITE);

    sprintf(sz, "LE:%02d  HE:%02d  M:%01d", lepr&0x1f, hepr&0x1f, ((vmpr&VMPR_MODE_MASK)>>5)+1);
    pScreen_->DrawString(m_nX, m_nY+124, sz, WHITE);

    sprintf(sz, "%03d:%03d", g_nLine, g_nLineCycle);
    pScreen_->DrawString(m_nX+48, m_nY+140, sz, RegCol(1,0));

    sprintf(sz, "%02X", status_reg);
    pScreen_->DrawString(m_nX+48, m_nY+154, sz, RegCol(1,0));

    pScreen_->SetFont(&sGUIFont);
}

////////////////////////////////////////////////////////////////////////////////

CCommandLine::CCommandLine (CWindow* pParent_, int nX_, int nY_, int nWidth_)
    : CEditControl(pParent_, nX_, nY_, nWidth_)
{
}

bool CCommandLine::OnMessage (int nMessage_, int nParam1_, int nParam2_)
{
    if (nMessage_ == GM_CHAR && nParam1_ == '\r')
    {
        Execute(GetText());
        SetText("");
        return true;
    }

    return CEditControl::OnMessage(nMessage_, nParam1_, nParam2_);
}


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
            pDebugger->Destroy();
        }
        else if (!(psz = strtok(NULL, " ")))
            pDebugger->Destroy();
        else if (!strcasecmp(psz, "until"))
        {
            if ((Break.pExpr = Expr::Compile(psz+1+strlen(psz))))
                pDebugger->Destroy();
        }
    }
    else if (!strcasecmp(pszCommand, "g"))
    {
        if ((pReg = Expr::Compile(pszCommand+1+strlen(pszCommand))))
        {
            Break.pAddr = phys_read_addr(Expr::Eval(pReg));
            pDebugger->Destroy();
        }
    }
    else if (!strcasecmp(pszCommand, "im"))
    {
        if ((pReg = Expr::Compile(pszCommand+1+strlen(pszCommand))))
        {
            WORD wMode = Expr::Eval(pReg);

            if (wMode <= 2)
                regs.IM = wMode & 0xff;
        }
    }
    else if (!strcasecmp(pszCommand, "di") && !(psz = strtok(NULL, " ")))
        regs.IFF1 = 0;
    else if (!strcasecmp(pszCommand, "ei") && !(psz = strtok(NULL, " ")))
        regs.IFF1 = 1;
    else if (!strcasecmp(pszCommand, "undo"))
        regs = sSafeRegs;
    else if (!strcasecmp(pszCommand, "exx"))
    {
        // EXX
        swap(regs.BC.W, regs.BC_.W);
        swap(regs.DE.W, regs.DE_.W);
        swap(regs.HL.W, regs.HL_.W);
    }
    else if (!strcasecmp(pszCommand, "ex"))
    {
        // EX AF,AF' ?
        if ((psz = strtok(NULL, " ,")) && !strcasecmp(psz, "af") &&
            (psz = strtok(NULL, " '"))  && !strcasecmp(psz, "af") &&
            !(psz = strtok(NULL, " ")))
        {
            swap(regs.AF.W, regs.AF_.W);
        }
    }
    else if (!strcasecmp(pszCommand, "u"))
    {
        if ((psz = strtok(NULL, " ,=")))
        {
            if ((pReg = Expr::Compile(psz, &psz)))
            {
                WORD wAddr = Expr::Eval(pReg);
                pDebugger->m_pDisassembly->SetAddress(wAddr);
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
                        case REG_A:      regs.AF.B.h_ = b; break;
                        case REG_F:      regs.AF.B.l_ = b; break;
                        case REG_B:      regs.BC.B.h_ = b; break;
                        case REG_C:      regs.BC.B.l_ = b; break;
                        case REG_D:      regs.DE.B.h_ = b; break;
                        case REG_E:      regs.DE.B.l_ = b; break;
                        case REG_H:      regs.HL.B.h_ = b; break;
                        case REG_L:      regs.HL.B.l_ = b; break;
                        case REG_ALT_A:  regs.AF_.B.h_ = b; break;
                        case REG_ALT_F:  regs.AF_.B.l_ = b; break;
                        case REG_ALT_B:  regs.BC_.B.h_ = b; break;
                        case REG_ALT_C:  regs.BC_.B.l_ = b; break;
                        case REG_ALT_D:  regs.DE_.B.h_ = b; break;
                        case REG_ALT_E:  regs.DE_.B.l_ = b; break;
                        case REG_ALT_H:  regs.HL_.B.h_ = b; break;
                        case REG_ALT_L:  regs.HL_.B.l_ = b; break;

                        case REG_AF:     regs.AF.W  = w; break;
                        case REG_BC:     regs.BC.W  = w; break;
                        case REG_DE:     regs.DE.W  = w; break;
                        case REG_HL:     regs.HL.W  = w; break;
                        case REG_ALT_AF: regs.AF_.W = w; break;
                        case REG_ALT_BC: regs.BC_.W = w; break;
                        case REG_ALT_DE: regs.DE_.W = w; break;
                        case REG_ALT_HL: regs.HL_.W = w; break;

                        case REG_IX:     regs.IX.W = w; break;
                        case REG_IY:     regs.IY.W = w; break;
                        case REG_SP:     regs.SP.W = w; break;
                        case REG_PC:     regs.PC.W = w; break;

                        case REG_IXH:    regs.IX.B.h_ = b; break;
                        case REG_IXL:    regs.IX.B.l_ = b; break;
                        case REG_IYH:    regs.IY.B.h_ = b; break;
                        case REG_IYL:    regs.IY.B.l_ = b; break;

                        case REG_I:      regs.I = b; break;
                        case REG_R:      regs.R = b; break;
                        case REG_IFF1:   regs.IFF1 = !!b; break;
                        case REG_IFF2:   regs.IFF2 = !!b; break;
                        case REG_IM:     if (b <= 2) regs.IM = b; break;
                    }
                }
            }
        }
    }
    else if (!strcasecmp(pszCommand, "t") || !strcasecmp(pszCommand, "p"))
    {
        bool fStepOver = !strcasecmp(pszCommand, "p");
        Expr::nCount = 1;

        if (fStepOver)
        {
            if ((psz = strtok(NULL, " ")) && !strcasecmp(psz, "ret"))
            {
                // Store the physical address of the current stack pointer, for checking on RETurn calls
                pStepOutStack = phys_read_addr(regs.SP.W);
                pDebugger->Destroy();
                return;
            }
        }

        BYTE bOpcode, bOpcode2;
        WORD wPC;

        // Skip any index prefixes on the instruction to reach a CB/ED prefix or the real opcode
        for (wPC = regs.PC.W ; ((bOpcode = read_byte(wPC)) == 0xdd || bOpcode == 0xfd) ; wPC++);
        bOpcode2 = read_byte(wPC+1);

        // Stepping over the current instruction?
        if (fStepOver)
        {
            // 1-byte HALT or RST ?
            if (bOpcode == OP_HALT || (bOpcode & 0xc7) == 0xc7)
                Break.pAddr = phys_read_addr(wPC+1);

            // 2-byte backwards DJNZ/JR cc, or (LD|CP|IN|OT)[I|D]R ?
            else if (((bOpcode == OP_DJNZ || (bOpcode & 0xe7) == 0x20) && (bOpcode2 & 0x80))
                   || (bOpcode == 0xed && (bOpcode2 & 0xf4) == 0xb0))
                Break.pAddr = phys_read_addr(wPC+2);

            // 3-byte CALL, CALL cc or backwards JP cc?
            else if (bOpcode == OP_CALL || (bOpcode & 0xc7) == 0xc4
                   || (bOpcode & 0xc7) == 0xc2 && read_word(wPC+1) <= wPC)
                Break.pAddr = phys_read_addr(wPC+3);
        }

        // Stepping into a HALT (with interrupt enabled) will enter the appropriate interrupt handler
        // This is much friendlier than single-stepping NOPs up to the next interrupt!
        else if (bOpcode == OP_HALT && regs.IFF1)
        {
            // For IM 2, form the address of the handler and break there
            if (regs.IM == 2)
                Break.pAddr = phys_read_addr(read_word((regs.I << 8) | 0xff));

            // IM 0 and IM1 both use the handler at 0x0038
            else
                Break.pAddr = phys_read_addr(IM1_INTERRUPT_HANDLER);
        }

        // Single step unless there's an alternative break address set
        Break.pExpr = Break.pAddr ? NULL : &Expr::Counter;

        pDebugger->Destroy();
    }
    else if (!strcasecmp(pszCommand, "help"))
        new CMessageBox(GetParent(), "Sorry, not available yet.", "Help!", mbInformation);

    Expr::Release(pReg);
}
