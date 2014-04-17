// Part of SimCoupe - A SAM Coupe emulator
//
// Expr.cpp: Infix expression parsing and postfix evaluation
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

#include "Expr.h"
#include "Memory.h"
#include "Options.h"
#include "Symbol.h"


static const char* p;
static EXPR *pHead, *pTail;
static int nFlags;

const int MAX_FUNC_PARAMS = 5;

EXPR Expr::Counter = { T_VARIABLE, VAR_COUNT, NULL, "(counter)" };
int Expr::nCount;

// Free all elements in an expression list
void Expr::Release (EXPR* pExpr_)
{
    // Take care not to free the built-in special expressions
    if (pExpr_ && pExpr_ != &Counter)
    {
        delete[] pExpr_->pcszExpr;
        for (EXPR* pDel ; (pDel = pExpr_) ; pExpr_ = pExpr_->pNext, delete pDel);
    }
}

// Add an expression chain to the current expression
static EXPR* AddNode (EXPR* pExpr_)
{
    if (!pHead)
        return pHead = pTail = pExpr_;
    else
        return pTail = pTail->pNext = pExpr_;
}

// Add a new node to the end of the current expression
static EXPR* AddNode (int nType_, int nValue_)
{
    EXPR *pExpr = new EXPR;
    pExpr->nType = nType_;
    pExpr->nValue = nValue_;
    pExpr->pNext = NULL;
    pExpr->pcszExpr = NULL;

    return AddNode(pExpr);
}

////////////////////////////////////////////////////////////////////////////////

typedef struct
{
    const char* pcsz;
    int nToken;
}
TOKEN;

// Binary operators, broken into precedence levels, sorted low to high
static const TOKEN asBinaryOps[][5] =
{
    { {"||",OP_OR},     {"or",OP_OR} },
    { {"&&",OP_AND},    {"and",OP_AND} },
    { {"|",OP_BOR},     {"bor",OP_BOR} },
    { {"^",OP_BXOR},    {"bxor",OP_BXOR} },
    { {"&",OP_BAND},    {"band",OP_BAND} },
    { {"==",OP_EQ},     {"!=",OP_NE},       {"=",OP_EQ},    {"<>",OP_NE} },
    { {"<=",OP_LE},     {">=",OP_GE},       {"<",OP_LT},    {">",OP_GT} },
    { {"<<",OP_SHIFTL}, {">>",OP_SHIFTR} },
    { {"+",OP_ADD},     {"-",OP_SUB} },
    { {"*",OP_MUL},     {"/",OP_DIV},       {"%",OP_MOD},   {"\\",OP_MOD} },
    { {NULL} }
};

static const TOKEN asUnaryOps[] =
{
    {"peek",OP_PEEK},
    {"dpeek",OP_DPEEK},
    {NULL}
};

static const TOKEN asRegisters[] =
{
    {"a'",REG_ALT_A}, {"f'",REG_ALT_F}, {"b'",REG_ALT_B}, {"c'",REG_ALT_C},
    {"d'",REG_ALT_D}, {"e'",REG_ALT_E}, {"h'",REG_ALT_H}, {"l'",REG_ALT_L},
    {"af'",REG_ALT_AF}, {"bc'",REG_ALT_BC}, {"de'",REG_ALT_DE}, {"hl'",REG_ALT_HL},
    {"a",REG_A}, {"f",REG_F}, {"b",REG_B}, {"c",REG_C}, {"d",REG_D}, {"e",REG_E}, {"h",REG_H}, {"l",REG_L},
    {"af",REG_AF}, {"bc",REG_BC}, {"de",REG_DE}, {"hl",REG_HL},
    {"ix",REG_IX}, {"iy",REG_IY}, {"ixh",REG_IXH}, {"ixl",REG_IXL}, {"iyh",REG_IYH}, {"iyl",REG_IYL},
    {"sp",REG_SP}, {"pc",REG_PC}, {"sph",REG_SPH}, {"spl",REG_SPL}, {"pch",REG_PCH}, {"pcl",REG_PCL},
    {"i",REG_I}, {"r",REG_R}, {"iff1",REG_IFF1}, {"iff2",REG_IFF2}, {"im",REG_IM},
    {NULL}
};

static const TOKEN asVariables[] =
{
    {"ei",VAR_EI}, {"di",VAR_DI},
    {"dline",VAR_DLINE}, {"sline",VAR_SLINE},
    {"rom0",VAR_ROM0}, {"rom1",VAR_ROM1}, {"wprot",VAR_WPROT}, {"inrom",VAR_INROM}, {"call",VAR_CALL}, {"autoexec",VAR_AUTOEXEC},
    {"lepage",VAR_LEPAGE}, {"hepage",VAR_HEPAGE}, {"lpage",VAR_LPAGE}, {"hpage",VAR_HPAGE}, {"vpage",VAR_VPAGE}, {"vmode",VAR_VMODE},
    {"inval",VAR_INVAL}, {"outval",VAR_OUTVAL},
    {"lepr",VAR_LEPR}, {"hepr",VAR_HEPR}, {"lpen",VAR_LPEN}, {"hpen",VAR_HPEN}, {"status",VAR_STATUS},
    {"lmpr",VAR_LMPR}, {"hmpr",VAR_HMPR}, {"vmpr",VAR_VMPR}, {"midi",VAR_MIDI}, {"border",VAR_BORDER}, {"attr",VAR_ATTR},
    {NULL}
};

static const TOKEN* LookupToken (const char* pcsz_, size_t uLen_, const TOKEN* pTokens_)
{
    for ( ; pTokens_->pcsz ; pTokens_++)
        if (strlen(pTokens_->pcsz) == uLen_ && !strncasecmp(pcsz_, pTokens_->pcsz, uLen_))
            return pTokens_;

    return NULL;
}

////////////////////////////////////////////////////////////////////////////////

// Compile an infix expression to an easy-to-process postfix expression list
EXPR* Expr::Compile (const char* pcsz_, char** ppszEnd_/*=NULL*/, int nFlags_/*=0*/)
{
    // Clear the expression list and set the expression string and flags
    pHead = pTail = NULL;
    p = pcsz_;
    nFlags = nFlags_;

    // Fail if the expression was bad, or there's unexpected garbage on the end
    if (!Term() || (!ppszEnd_ && *p))
    {
        Release(pHead);
        return NULL;
    }

    // Supply the end pointer if required, or NULL if nothing was left
    if (ppszEnd_)
        *ppszEnd_ = const_cast<char*>(p);

    // Keep a copy of the original expression text in the head item
    pHead->pcszExpr = strcpy(new char[strlen(pcsz_)+1], pcsz_);

    // Return the expression list
    return pHead;
}


int Expr::GetReg (int nReg_)
{
    int nRet = 0;

    switch (nReg_)
    {
        case REG_A:         nRet = A;      break;
        case REG_F:         nRet = F;      break;
        case REG_B:         nRet = B;      break;
        case REG_C:         nRet = C;      break;
        case REG_D:         nRet = D;      break;
        case REG_E:         nRet = E;      break;
        case REG_H:         nRet = H;      break;
        case REG_L:         nRet = L;      break;

        case REG_ALT_A:     nRet = A_;     break;
        case REG_ALT_F:     nRet = F_;     break;
        case REG_ALT_B:     nRet = B_;     break;
        case REG_ALT_C:     nRet = C_;     break;
        case REG_ALT_D:     nRet = D_;     break;
        case REG_ALT_E:     nRet = E_;     break;
        case REG_ALT_H:     nRet = H_;     break;
        case REG_ALT_L:     nRet = L_;     break;

        case REG_AF:        nRet = AF;     break;
        case REG_BC:        nRet = BC;     break;
        case REG_DE:        nRet = DE;     break;
        case REG_HL:        nRet = HL;     break;

        case REG_ALT_AF:    nRet = AF_;    break;
        case REG_ALT_BC:    nRet = BC_;    break;
        case REG_ALT_DE:    nRet = DE_;    break;
        case REG_ALT_HL:    nRet = HL_;    break;

        case REG_IX:        nRet = IX;     break;
        case REG_IY:        nRet = IY;     break;
        case REG_SP:        nRet = SP;     break;
        case REG_PC:        nRet = PC;     break;

        case REG_IXH:       nRet = IXH;    break;
        case REG_IXL:       nRet = IXL;    break;
        case REG_IYH:       nRet = IYH;    break;
        case REG_IYL:       nRet = IYL;    break;

        case REG_SPH:       nRet = SPH;    break;
        case REG_SPL:       nRet = SPL;    break;
        case REG_PCH:       nRet = PCH;    break;
        case REG_PCL:       nRet = PCL;    break;

        case REG_I:         nRet = I;      break;
        case REG_R:         nRet = (R7 & 0x80) | (R & 0x7f); break;
        case REG_IFF1:      nRet = IFF1;   break;
        case REG_IFF2:      nRet = IFF2;   break;
        case REG_IM:        nRet = IM;     break;
    }

    return nRet;
}

void Expr::SetReg (int nReg_, int nValue_)
{
    WORD w = static_cast<WORD>(nValue_);
    BYTE b = w & 0xff;

    switch (nReg_)
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

        case REG_SPH:    SPH = b; break;
        case REG_SPL:    SPL = b; break;
        case REG_PCH:    PCH = b; break;
        case REG_PCL:    PCL = b; break;

        case REG_I:      I = b; break;
        case REG_R:      R7 = R = b; break;
        case REG_IFF1:   IFF1 = !!b; break;
        case REG_IFF2:   IFF2 = !!b; break;
        case REG_IM:     if (b <= 2) IM = b; break;
    }
}


// Evaluate a compiled expression
int Expr::Eval (const EXPR* pExpr_)
{
    // No expression?
    if (!pExpr_)
        return -1;

    // Value stack
    int an[128], n = 0;

    // Walk the expression list
    for ( ; pExpr_ ; pExpr_ = pExpr_->pNext)
    {
        switch (pExpr_->nType)
        {
            case T_NUMBER:
                // Push value
                an[n++] = pExpr_->nValue;
                break;

            case T_UNARY_OP:
            {
                // Pop one argument
                int x = an[--n];

                switch (pExpr_->nValue)
                {
                    case OP_UMINUS: x = -x; break;
                    case OP_UPLUS:          break;
                    case OP_BNOT:   x = ~x; break;
                    case OP_NOT:    x = !x; break;
                    case OP_DEREF:  x = read_byte(x); break;
                    case OP_PEEK:   x = read_byte(x); break;
                    case OP_DPEEK:  x = read_word(x); break;
                }

                // Push the result
                an[n++] = x;
                break;
            }

            case T_BINARY_OP:
            {
                // Pop two arguments (in reverse order)
                int b = an[--n], a = an[--n], c = 0;

                switch (pExpr_->nValue)
                {
                    case OP_OR:     c = a || b; break;
                    case OP_AND:    c = a && b; break;
                    case OP_BOR:    c = a | b;  break;
                    case OP_BXOR:   c = a ^ b;  break;
                    case OP_BAND:   c = a & b;  break;
                    case OP_EQ:     c = a == b; break;
                    case OP_NE:     c = a != b; break;
                    case OP_LT:     c = a < b;  break;
                    case OP_LE:     c = a <= b; break;
                    case OP_GE:     c = a >= b; break;
                    case OP_GT:     c = a > b;  break;
                    case OP_SHIFTL: c = a << b; break;
                    case OP_SHIFTR: c = a >> b; break;
                    case OP_ADD:    c = a + b;  break;
                    case OP_SUB:    c = a - b;  break;
                    case OP_MUL:    c = a * b;  break;
                    case OP_DIV:    c = b ? a/b : 0; break; // Avoid/ignore division by zero
                    case OP_MOD:    c = b ? a%b : 0; break;
                }

                // Push the result
                an[n++] = c;
                break;
            }

            case T_REGISTER:
            {
                // Push register value
                int r = GetReg(pExpr_->nValue);
                an[n++] = r;
                break;
            }

            case T_VARIABLE:
            {
                int r = 0;

                switch (pExpr_->nValue)
                {
                    case VAR_EI:        r = !!IFF1; break;
                    case VAR_DI:        r = !IFF1;  break;

                    case VAR_DLINE:
                    {
                        int nLine;
                        GetRasterPos(&nLine);
                        r = nLine;
                        break;
                    }

                    case VAR_SLINE:
                    {
                        int nLine;
                        GetRasterPos(&nLine);
                        if (nLine >= TOP_BORDER_LINES && nLine < (TOP_BORDER_LINES+SCREEN_LINES))
                            r = nLine - TOP_BORDER_LINES;
                        else
                            r = -1;
                        break;
                    }

                    case VAR_ROM0:      r = !(lmpr & LMPR_ROM0_OFF);  break;
                    case VAR_ROM1:      r = !!(lmpr & LMPR_ROM1);     break;
                    case VAR_WPROT:     r = !!(lmpr & LMPR_WPROT);    break;

                    case VAR_LEPAGE:    r = lepr; break;
                    case VAR_HEPAGE:    r = hepr; break;
                    case VAR_LPAGE:     r = lmpr & LMPR_PAGE_MASK;    break;
                    case VAR_HPAGE:     r = hmpr & HMPR_PAGE_MASK;    break;
                    case VAR_VPAGE:     r = vmpr & VMPR_PAGE_MASK;    break;
                    case VAR_VMODE:     r = ((vmpr & VMPR_MODE_MASK) >> VMPR_MODE_SHIFT)+1; break;

                    case VAR_INVAL:     r = bPortInVal;               break;
                    case VAR_OUTVAL:    r = bPortOutVal;              break;

                    case VAR_LEPR:      r = LEPR_PORT;                break;	// 128
                    case VAR_HEPR:      r = HEPR_PORT;                break;	// 129
                    case VAR_LPEN:      r = LPEN_PORT;                break;	// 248
                    case VAR_HPEN:      r = HPEN_PORT;                break;	// 248+256
                    case VAR_STATUS:    r = STATUS_PORT;              break;	// 249
                    case VAR_LMPR:      r = LMPR_PORT;                break;	// 250
                    case VAR_HMPR:      r = HMPR_PORT;                break;	// 251
                    case VAR_VMPR:      r = VMPR_PORT;                break;	// 252
                    case VAR_MIDI:      r = MIDI_PORT;                break;	// 253
                    case VAR_BORDER:    r = BORDER_PORT;              break;	// 254
                    case VAR_ATTR:      r = ATTR_PORT;                break;	// 255

                    case VAR_INROM:     r = (!(lmpr & LMPR_ROM0_OFF) && PC < 0x4000) || (lmpr & LMPR_ROM1 && PC >= 0xc000); break;
                    case VAR_CALL:      r = PC == HL && !(lmpr & LMPR_ROM0_OFF) && (read_word(SP) == 0x180d); break;
                    case VAR_AUTOEXEC:  r = PC == HL && !(lmpr & LMPR_ROM0_OFF) && (read_word(SP) == 0x0213) && (read_word(SP+2) == 0x5f00); break;

                    case VAR_COUNT:     r = nCount ? !--nCount : 1; break;
                }

                // Push variable value
                an[n++] = r;
                break;
            }
/*
            case T_FUNCTION:
            {
                // Pop the parameter count, and adjust the stack to the start of the parameters
                int nParams = an[--n], f = 0;
                n -= nParams;

                switch (pExpr_->nValue)
                {
                    case FN_BLAH:   f = do_stuff(an[n]);    break;
                }

                // Push return value
                an[n++] = f;
                break;
            }
*/
            default:
                TRACE("Expr::Eval(): unknown type %d!\n", pExpr_->nType);
                an[n++] = 0;
                break;
        }
    }

    // Return the overall result
    return an[--n];
}

// Evaluate an expression, returning the value and whether it was valid
bool Expr::Eval (const char* pcsz_, int *pnValue_, char** ppszEnd_/*=NULL*/, int nFlags_/*=0*/)
{
    static char sz[] = "";

    // Invalidate output values
    if (pnValue_) *pnValue_ = -1;
    if (ppszEnd_) *ppszEnd_ = sz;

    // Fail obviously invalid inputs
    if (!pcsz_ || !*pcsz_ || !pnValue_)
        return false;

    // Compile the expression, failing if there's an error
    EXPR* pExpr = Compile(pcsz_, ppszEnd_, nFlags_);
    if (!pExpr)
        return false;

    // Evaluate and release the expression
    int n = Eval(pExpr);
    Release(pExpr);

    // Expression valid
    *pnValue_ = n;
    return true;
}

// Parse an expression, optionally containing binary operators of a specified precedence level (or above)
bool Expr::Term (int n_/*=0*/)
{
    bool fLast = !asBinaryOps[n_+1][0].pcsz;

    // Recurse to the highest precedence first, dropping out if we hit a problem
    if (!(fLast ? Factor() : Term(n_+1)))
        return false;

    while (1)
    {
        int i;
        size_t uLen = 0, uMaxLen = 0;

        // Check for an operator at any precedence level
        for (i = 0 ; asBinaryOps[i][0].pcsz ; i++)
        {
            for (int j = 0 ; asBinaryOps[i][j].pcsz ; j++)
            {
                uLen = strlen(asBinaryOps[i][j].pcsz);

                // Store the length of the largest matching token at this point
                if (!strncasecmp(asBinaryOps[i][j].pcsz, p, uLen) && uLen > uMaxLen)
                    uMaxLen = uLen;
            }
        };

        // Check for an operator at the current precedence level
        for (i = 0 ; asBinaryOps[n_][i].pcsz ; i++)
        {
            uLen = strlen(asBinaryOps[n_][i].pcsz);

            // Skip if it's shorter than the longest matching token
            if (uLen < uMaxLen)
                continue;

            // Found a match at current precedence?
            if (!strncasecmp(asBinaryOps[n_][i].pcsz, p, uLen))
                break;
        };

        // Fall back if not found
        if (!asBinaryOps[n_][i].pcsz)
            return true;

        // Skip the operator on the input
        p += uLen;

        // Recurse, and drop out if we hit a problem
        if (!(fLast ? Factor() : Term(n_+1)))
            return false;

        // Add the operator to the expression (after the operands)
        AddNode(T_BINARY_OP, asBinaryOps[n_][i].nToken);
    }
}

// Parse a factor (number/variable/function) along with right-associative unary operators
bool Expr::Factor ()
{
    bool fMatched = false;

    // Strip leading whitespace
    for ( ; isspace(*p) ; p++);

    // Look for registers, variables and unary operators
    if (!fMatched && isalpha(*p))
    {
        const char *p2 = p;
        const TOKEN *pToken;
        int nSymValue;

        // Assume we'll match the input
        fMatched = true;

        // Scan for an identifier, allowing an optional trailing single-quote
        for ( ; isalnum(*p2) || *p2 == '_' ; p2++);
        if (*p2 == '\'') p2++;

        // Register?
        if (!(nFlags & noRegs) && (pToken = LookupToken(p, p2-p, asRegisters)))
            AddNode(T_REGISTER, pToken->nToken);

        // Variable?
        else if (!(nFlags & noVars) && (pToken = LookupToken(p, p2-p, asVariables)))
            AddNode(T_VARIABLE, pToken->nToken);

        // Symbol?
        else if (!(nFlags & noSyms) && (nSymValue = Symbol::LookupSymbol(std::string(p, p2))) >= 0)
            AddNode(T_NUMBER, nSymValue);

        // Unary operator (word)?
        else if ((pToken = LookupToken(p, p2-p, asUnaryOps)))
        {
            // Advance the input pointer ahead of the recursive call
            p = p2;

            // Look for a factor for the unary operator
            if (!Factor())
                return false;

            AddNode(T_UNARY_OP, pToken->nToken);
            return true;
        }
        else
            fMatched = false;

        // Advanced the input if we matched
        if (fMatched)
            p = p2;
    }

    // Check for literal values next
    if (!fMatched && !(nFlags & noVals) && isxdigit(*p))
    {
        // Assume we'll match the input
        fMatched = true;

        // Parse as decimal and hex
        const char *pDecEnd, *pHexEnd;
        int nDecValue = static_cast<int>(strtoul(p, (char**)&pDecEnd, 10));
        int nHexValue = static_cast<int>(strtoul(p, (char**)&pHexEnd, 16));

        // Accept decimal values with a '.' suffix
        if (*pDecEnd == '.')
        {
            AddNode(T_NUMBER, nDecValue);
            p = pDecEnd+1;
        }

        // Accept hex values with an 'h' suffix
        else if (tolower(*pHexEnd) == 'h')
        {
            AddNode(T_NUMBER, nHexValue);
            p = pHexEnd+1;
        }

        // Accept values using a C-style "0x" prefix
        else if (p[0] == '0' && tolower(p[1]) == 'x')
        {
            AddNode(T_NUMBER, nHexValue);
            p = pHexEnd;
        }

        // Anything not followed by an alphabetic is taken as hex
        else if (!isalpha(*pHexEnd))
        {
            AddNode(T_NUMBER, nHexValue);
            p = pHexEnd;
        }
        else
            fMatched = false;
    }

    if (fMatched)
    {
        // Nothing more to do
    }

    // Hex value with explicit prefix?
    else if ((*p == '$' || *p == '&' || *p == '#') && isxdigit(p[1]))
    {
        p++;
        AddNode(T_NUMBER, static_cast<int>(strtoul(p, (char**)&p, 16)));
    }

    // Binary value?
    else if (*p == '%' && (p[1] == '0' || p[1] == '1'))
    {
        unsigned u = 0;
        for (p++ ; *p == '0' || *p == '1' ; p++)
            (u <<= 1) |= (*p-'0');

        AddNode(T_NUMBER, u);
    }

    // Quoted character?
    else if (*p == '"' || *p == '\'')
    {
        AddNode(T_NUMBER, *++p);

        // Ensure the closing quote matches the open
        if (p[-1] != p[1])
            return false;
        else
            p += 2;
    }

    // Unary operator symbol?
    else if (*p == '-' || *p == '+' || *p == '~' || *p == '!' || *p == '*' || *p == '=')
    {
        static const char* pcszUnary = "-+~!*=";
        static int anUnary[] = { OP_UMINUS, OP_UPLUS, OP_BNOT, OP_NOT, OP_DEREF, OP_EVAL };

        // Remember the operator, and the node connected to the operand
        char op = *p++;
        EXPR *pOldTail = pTail;

        // Look for a factor for the unary operator
        if (!Factor())
            return false;

        // If it's not an eval operator, add the new node
        if (op != '=')
            AddNode(T_UNARY_OP, anUnary[strchr(pcszUnary,op)-pcszUnary]);

        // Otherwise we need to evaluate the expression now and insert the current value
        else
        {
            int n;

            if (pOldTail)
            {
                // Step back to the old tail position for the operand
                pTail = pOldTail;
                n = Expr::Eval(pTail->pNext);
                Release(pTail->pNext);
                pTail->pNext = NULL;
            }
            else
            {
                // Use the full stored expression
                n = Expr::Eval(pTail);
                Release(pTail);
                pHead = pTail = NULL;
            }

            AddNode(T_NUMBER, n);
        }
    }

    // Program Counter '$' symbol?
    else if (*p == '$' && !(nFlags & noRegs))
    {
        AddNode(T_REGISTER, REG_PC);
        p++;
    }

    // Expression in parentheses?
    else if (*p == '(')
    {
        p++;
        if (!Term() || *p++ != ')')
            return false;
    }

    // Input not recognised
    else
        return false;

    // Strip trailing whitespace
    for ( ; isspace(*p) ; p++);

    return true;
}
