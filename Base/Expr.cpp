// Part of SimCoupe - A SAM Coupe emulator
//
// Expr.cpp: Infix expression parsing and postfix evaluation
//
//  Copyright (c) 1999-2012 Simon Owen
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


static const char* p;
static EXPR *pHead, *pTail;
static int nFlags;

const int MAX_FUNC_PARAMS = 5;


EXPR Expr::True = { T_NUMBER, 1, NULL }, Expr::False = { T_NUMBER, 0, NULL };

EXPR Expr::Counter = { T_VARIABLE, VAR_COUNT, NULL };
int Expr::nCount;

// Free all elements in an expression list
void Expr::Release (EXPR* pExpr_)
{
    // Take care not to free the built-in special expressions
    if (pExpr_ && pExpr_ != &Expr::True && pExpr_ != &False && pExpr_ != &Counter)
        for (EXPR* pDel ; (pDel = pExpr_) ; pExpr_ = pExpr_->pNext, delete pDel);
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
    {"in",OP_IN}, {"out",OP_OUT},
    {NULL}
};

static const TOKEN asRegisters[] =
{
    {"a",REG_A}, {"f",REG_F}, {"b",REG_B}, {"c",REG_C}, {"d",REG_D}, {"e",REG_E}, {"h",REG_H}, {"l",REG_L},
    {"af",REG_AF}, {"bc",REG_BC}, {"de",REG_DE}, {"hl",REG_HL},
    {"a'",REG_ALT_A}, {"f'",REG_ALT_F}, {"b'",REG_ALT_B}, {"c'",REG_ALT_C},
    {"d'",REG_ALT_D}, {"e'",REG_ALT_E}, {"h'",REG_ALT_H}, {"l'",REG_ALT_L},
    {"bc'",REG_ALT_BC}, {"de'",REG_ALT_DE}, {"hl'",REG_ALT_HL},
    {"ix",REG_IX}, {"iy",REG_IY}, {"ixh",REG_IXH}, {"ixl",REG_IXL}, {"iyh",REG_IYH}, {"iyl",REG_IYL},
    {"sp",REG_SP}, {"pc",REG_PC}, {"i",REG_I}, {"r",REG_R}, {"iff1",REG_IFF1}, {"iff2",REG_IFF2}, {"im",REG_IM}
};

static const TOKEN asVariables[] =
{
    {"ei",VAR_EI}, {"di",VAR_DI},
    {"dline",VAR_DLINE}, {"sline",VAR_SLINE}, {"lcycles",VAR_LCYCLES},
    {"rom0",VAR_ROM0}, {"rom1",VAR_ROM1}, {"wprot",VAR_WPROT},
    {"lmpr",VAR_LMPR}, {"hmpr",VAR_HMPR}, {"vmpr",VAR_VMPR}, {"mode",VAR_MODE}, {"lepr",VAR_LEPR}, {"hepr",VAR_HEPR},
    {NULL}
};

static const TOKEN* LookupToken (const char* pcsz_, size_t uLen_, const TOKEN* pTokens_)
{
    for ( ; pTokens_->pcsz ; pTokens_++)
        if (strlen(pTokens_->pcsz) == uLen_ && !memcmp(pcsz_, pTokens_->pcsz, uLen_))
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

    // Supply the end pointer if required
    if (ppszEnd_)
        *ppszEnd_ = const_cast<char*>(p);

    // Return the expression list
    return pHead;
}

// Evaluate a compiled expression
int Expr::Eval (const EXPR* pExpr_)
{
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
                    case OP_IN:     x = (wPortRead  == x) || (x <= 0xff && ((wPortRead&0xff)  == x)); break;
                    case OP_OUT:    x = (wPortWrite == x) || (x <= 0xff && ((wPortWrite&0xff) == x)); break;
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
            case T_VARIABLE:
            {
                int r = 0;

                switch (pExpr_->nValue)
                {
                    case REG_A:         r = A;      break;
                    case REG_F:         r = F;      break;
                    case REG_B:         r = B;      break;
                    case REG_C:         r = C;      break;
                    case REG_D:         r = D;      break;
                    case REG_E:         r = E;      break;
                    case REG_H:         r = H;      break;
                    case REG_L:         r = L;      break;
                    case REG_AF:        r = AF;     break;
                    case REG_BC:        r = BC;     break;
                    case REG_DE:        r = DE;     break;
                    case REG_HL:        r = HL;     break;
                    case REG_ALT_A:     r = A_;     break;
                    case REG_ALT_F:     r = F_;     break;
                    case REG_ALT_B:     r = B_;     break;
                    case REG_ALT_C:     r = C_;     break;
                    case REG_ALT_D:     r = D_;     break;
                    case REG_ALT_E:     r = E_;     break;
                    case REG_ALT_H:     r = H_;     break;
                    case REG_ALT_L:     r = L_;     break;
                    case REG_ALT_AF:    r = AF_;    break;
                    case REG_ALT_BC:    r = BC_;    break;
                    case REG_ALT_DE:    r = DE_;    break;
                    case REG_ALT_HL:    r = HL_;    break;
                    case REG_IX:        r = IX;     break;
                    case REG_IY:        r = IY;     break;
                    case REG_IXH:       r = IXH;    break;
                    case REG_IXL:       r = IXL;    break;
                    case REG_IYH:       r = IYH;    break;
                    case REG_IYL:       r = IYL;    break;
                    case REG_SP:        r = SP;     break;
                    case REG_PC:        r = PC;     break;
                    case REG_I:         r = I;      break;
                    case REG_R:         r = (R7 & 0x80) | (R & 0x7f); break;
                    case REG_IFF1:      r = IFF1;   break;
                    case REG_IFF2:      r = IFF2;   break;
                    case REG_IM:        r = IM;     break;

                    case VAR_EI:        r = !!IFF1; break;
                    case VAR_DI:        r = !IFF1;  break;
/*
                    // FIXME
                    case VAR_DLINE:     r = g_nLine;        break;
                    case VAR_SLINE:     r = g_nLine - TOP_BORDER_LINES; break;
                    case VAR_LCYCLES:   r = g_nLineCycle;   break;
*/
                    case VAR_ROM0:      r = !(lmpr & LMPR_ROM0_OFF);  break;
                    case VAR_ROM1:      r = !!(lmpr & LMPR_ROM1);     break;
                    case VAR_WPROT:     r = !!(lmpr & LMPR_WPROT);    break;
                    case VAR_LMPR:      r = LMPR_PAGE;                break;
                    case VAR_HMPR:      r = HMPR_PAGE;                break;
                    case VAR_VMPR:      r = VMPR_PAGE;                break;
                    case VAR_MODE:      r = 1+(VMPR_MODE >> 5);       break;
                    case VAR_LEPR:      r = lepr;                     break;
                    case VAR_HEPR:      r = hepr;                     break;

                    case VAR_COUNT:     r = nCount ? !--nCount : 1; break;
                }

                // Push register value
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
bool Expr::Eval (const char* pcsz_, int& nValue_, int nFlags_/*=0*/)
{
    char* pszEnd = NULL;

    // Fail obviously invalid inputs
    if (!pcsz_ || !*pcsz_)
        return false;

    // Compile the expression, failing if there's an error
    EXPR* pExpr = Compile(pcsz_, &pszEnd, nFlags_);
    if (!pExpr)
        return false;

    // Evaluate and release the expression
    int n = Eval(pExpr);
    Release(pExpr);

    // Fail if there's anything left in the input
    if (*pszEnd)
        return false;

    // Expression valid
    nValue_ = n;
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
        size_t uLen = 0;

        // Check for an operator at the current precedence level
        for (i = 0 ; asBinaryOps[n_][i].pcsz ; i++)
        {
            // Check for a matching operator
            uLen = strlen(asBinaryOps[n_][i].pcsz);
            if (!memcmp(asBinaryOps[n_][i].pcsz, p, uLen))
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

    // Starts with a valid hex digit?
    if (isxdigit(*p))
    {
        // Assume we'll match the input
        fMatched = true;

        // Parse as hex initially
        const char* p2;
        int nValue = static_cast<int>(strtoul(p, (char**)&p2, 16));

        // Accept values using a C-style "0x" prefix
        if (p[0] == '0' && tolower(p[1]) == 'x')
        {
            AddNode(T_NUMBER, nValue);
            p = p2;
        }

        // Also accept hex values with an 'h' suffix
        else if (tolower(*p2) == 'h')
        {
            AddNode(T_NUMBER, nValue);
            p = p2+1;
        }

        // Check for binary values with a 'b' suffix
        else if (*p == '0' || *p == '1')
        {
            nValue = 0;
            for (p2 = p ; *p2 == '0' || *p2 == '1' ; p2++)
                (nValue <<= 1) |= (*p2-'0');

            // If there's a 'b' suffix it's binary
            if (tolower(*p2) == 'b')
            {
                AddNode(T_NUMBER, nValue);
                p = p2+1;
            }
            else
                fMatched = false;
        }
        else
            fMatched = false;

        if (!fMatched && isdigit(*p))
        {
            // Parse as decimal (leading zeroes should not give octal!)
            AddNode(T_NUMBER, static_cast<int>(strtoul(p, (char**)&p, 10)));
            fMatched = true;
        }
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
        static const char* pcszUnary = "-+~!*";
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

    // Variable, operator or function?
    else if (isalpha(*p))
    {
        const TOKEN* pToken;

        // Scan the alphabetic part of the name
        const char* pcsz = p;
        for ( ; isalpha(*p) || *p == '\'' ; p++);

        // Unary operator?
        if ((pToken = LookupToken(pcsz, p-pcsz, asUnaryOps)))
        {
            // Look for a factor for the unary operator
            if (!Factor())
                return false;

            AddNode(T_UNARY_OP, pToken->nToken);
        }
        else
        {
            // Now include any alphanumeric tail, and ' for HL' etc.
            for ( ; isalnum(*p) || *p == '\'' ; p++);
/*
            // Function?
            if (!(nFlags & noFuncs) && (*p == '('))
            {
                int nParams = 0;

                for (p++ ; *p != ')' && Term() ; nParams++)
                {
                    // Too many parameters?
                    if (nParams > MAX_FUNC_PARAMS)
                        return false;

                    // Eat any separator
                    else if (*p == ',')
                        p++;
                }

                // Check for a closing bracket
                if (*p++ != ')')
                    return false;

                AddNode(T_NUMBER, nParams);
                AddNode(T_FUNCTION, 0);
            }
            else
*/
            if (!(nFlags & noRegs) && (pToken = LookupToken(pcsz, p-pcsz, asRegisters)))
                AddNode(T_REGISTER, pToken->nToken);
            else if (!(nFlags & noVars) && (pToken = LookupToken(pcsz, p-pcsz, asVariables)))
                AddNode(T_VARIABLE, pToken->nToken);
            else
                return false;
        }
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
