// Part of SimCoupe - A SAM Coupe emulator
//
// Expr.h: Infix expression parsing and postfix evaluation
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

#ifndef EXPR_H
#define EXPR_H

typedef struct tagEXPR EXPR;

class Expr
{
    public:
        // Flags to limit expression scope
        enum { none=0x00, noRegs=0x01, noVars=0x02, noFuncs=0x04, noVals=0x08,
               valOnly=noRegs|noVars|noFuncs, regOnly=noVars|noFuncs|noVals, simple=valOnly };

    public:
        static EXPR* Compile (const char* pcsz_, char** ppszEnd_=NULL, int nFlags_=none);
        static void Release (EXPR* pExpr_);
        static int Eval (const EXPR* pExpr_);
        static bool Eval (const char* pcsz_, int *pnValue_, char** ppszEnd_=NULL, int nFlags_=none);

    public:
        static int GetReg (int nReg_);
        static void SetReg (int nReg_, int nValue_);

    public:
        static EXPR Counter;
        static int nCount;

    protected:
        static bool Term (int n_=0);
        static bool Factor ();
};


typedef struct tagEXPR
{
    int nType, nValue;      // Item type and type-specific value
    struct tagEXPR* pNext;  // Link to next item in expression
    const char *pcszExpr;   // Original expression text (head item only)

private:
    ~tagEXPR () { }         // Use Expr::Release() to delete Expr chains
    friend class Expr;
}
EXPR;


// Leave the enums public to allow some poking around the tokenised expressions by calling code

// Token types
enum { T_NUMBER, T_UNARY_OP, T_BINARY_OP, T_REGISTER, T_VARIABLE, T_FUNCTION };

// Unary operators
enum { OP_UMINUS, OP_UPLUS, OP_BNOT, OP_NOT, OP_DEREF, OP_PEEK, OP_DPEEK, OP_EVAL };

// Binary operators
enum { OP_AND, OP_OR, OP_BOR, OP_BXOR, OP_BAND, OP_EQ, OP_NE, OP_LT, OP_LE, OP_GE,
       OP_GT, OP_SHIFTL, OP_SHIFTR, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD };

// Registers and (read-only) variables
enum { REG_A, REG_F, REG_B, REG_C, REG_D, REG_E, REG_H, REG_L,
       REG_ALT_A, REG_ALT_F, REG_ALT_B, REG_ALT_C, REG_ALT_D, REG_ALT_E, REG_ALT_H, REG_ALT_L,
       REG_AF, REG_BC, REG_DE, REG_HL, REG_ALT_AF, REG_ALT_BC, REG_ALT_DE, REG_ALT_HL,
       REG_IX, REG_IY, REG_IXH, REG_IXL, REG_IYH, REG_IYL,
       REG_SP, REG_PC, REG_SPH, REG_SPL, REG_PCH, REG_PCL,
       REG_I, REG_R, REG_IFF1, REG_IFF2, REG_IM,
       VAR_EI, VAR_DI, VAR_DLINE, VAR_SLINE, VAR_COUNT,
       VAR_ROM0, VAR_ROM1, VAR_WPROT, VAR_INROM, VAR_CALL, VAR_AUTOEXEC,
       VAR_LEPAGE, VAR_HEPAGE, VAR_LPAGE, VAR_HPAGE, VAR_VPAGE, VAR_VMODE,
       VAR_INVAL, VAR_OUTVAL,
       VAR_LEPR, VAR_HEPR, VAR_LPEN, VAR_HPEN, VAR_STATUS, VAR_LMPR, VAR_HMPR, VAR_VMPR, VAR_MIDI, VAR_BORDER, VAR_ATTR
 };

#endif
