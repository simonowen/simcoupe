// Part of SimCoupe - A SAM Coupe emulator
//
// Expr.cpp: Infix expression parsing and postfix evaluation
//
//  Copyright (c) 1999-2004  Simon Owen
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


const char* p;
EXPR *pHead, *pTail;

////////////////////////////////////////////////////////////////////////////////

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

// Add a new node to the end of the current expression
EXPR* AddNode (int nType_, int nValue_)
{
    if (!pHead)
        pHead = pTail = new EXPR;
    else
        pTail = pTail->pNext = new EXPR;

    pTail->nType = nType_;
    pTail->nValue = nValue_;
    pTail->pNext = NULL;

    return pTail;
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
    { {"*",OP_MUL},     {"/",OP_DIV},       {"%",OP_MOD} },
    { {NULL} }
};

static const TOKEN asUnaryOps[] =
{
    {"peek",OP_PEEK},
    {"dpeek",OP_DPEEK},
    {"in",OP_IN},
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
    {"dline",VAR_LINE}, {"sline",VAR_SLINE}, {"lcycles",VAR_LCYCLES},
    {NULL}
};

const TOKEN* LookupToken (const char* pcsz_, size_t uLen_, const TOKEN* pTokens_)
{
    for ( ; pTokens_->pcsz ; pTokens_++)
        if (strlen(pTokens_->pcsz) == uLen_ && !memcmp(pcsz_, pTokens_->pcsz, uLen_))
            return pTokens_;

    return NULL;
}

////////////////////////////////////////////////////////////////////////////////

// Compile an infix expression to an easy-to-process postfix expression list
EXPR* Expr::Compile (const char* pcsz_, char** ppcszEnd_/*=NULL*/)
{
    // Clear the expression list and set the expression string
    pHead = pTail = NULL;
    p = pcsz_;

    // Fail if the expression was bad, or there's unexpected garbage on the end
    if (!Term() || (!ppcszEnd_ && *p))
    {
        Release(pHead);
        return NULL;
    }

    // Supply the end pointer if required
    if (ppcszEnd_)
        *ppcszEnd_ = const_cast<char*>(p);

    // Return the expression list
    return pHead;
}

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
                    case OP_IN:     x = in_byte(x); break;
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
                    case REG_A:         r = regs.AF.B.h_;   break;
                    case REG_F:         r = regs.AF.B.l_;   break;
                    case REG_B:         r = regs.BC.B.h_;   break;
                    case REG_C:         r = regs.BC.B.l_;   break;
                    case REG_D:         r = regs.DE.B.h_;   break;
                    case REG_E:         r = regs.DE.B.l_;   break;
                    case REG_H:         r = regs.HL.B.h_;   break;
                    case REG_L:         r = regs.HL.B.l_;   break;
                    case REG_AF:        r = regs.AF.W;      break;
                    case REG_BC:        r = regs.BC.W;      break;
                    case REG_DE:        r = regs.DE.W;      break;
                    case REG_HL:        r = regs.HL.W;      break;
                    case REG_ALT_A:     r = regs.AF_.B.h_;  break;
                    case REG_ALT_F:     r = regs.AF_.B.l_;  break;
                    case REG_ALT_B:     r = regs.BC_.B.h_;  break;
                    case REG_ALT_C:     r = regs.BC_.B.l_;  break;
                    case REG_ALT_D:     r = regs.DE_.B.h_;  break;
                    case REG_ALT_E:     r = regs.DE_.B.l_;  break;
                    case REG_ALT_H:     r = regs.HL_.B.h_;  break;
                    case REG_ALT_L:     r = regs.HL_.B.l_;  break;
                    case REG_ALT_AF:    r = regs.AF_.W;     break;
                    case REG_ALT_BC:    r = regs.BC_.W;     break;
                    case REG_ALT_DE:    r = regs.DE_.W;     break;
                    case REG_ALT_HL:    r = regs.HL_.W;     break;
                    case REG_IX:        r = regs.IX.W;      break;
                    case REG_IY:        r = regs.IY.W;      break;
                    case REG_IXH:       r = regs.IX.B.h_;   break;
                    case REG_IXL:       r = regs.IX.B.l_;   break;
                    case REG_IYH:       r = regs.IY.B.h_;   break;
                    case REG_IYL:       r = regs.IY.B.l_;   break;
                    case REG_SP:        r = regs.SP.W;      break;
                    case REG_PC:        r = regs.PC.W;      break;
                    case REG_I:         r = regs.I;         break;
                    case REG_R:         r = regs.R;         break;
                    case REG_IFF1:      r = regs.IFF1;      break;
                    case REG_IFF2:      r = regs.IFF2;      break;
                    case REG_IM:        r = regs.IM;        break;

                    case VAR_EI:        r = regs.IFF1;      break;
                    case VAR_DI:        r = !regs.IFF1;     break;
                    case VAR_LINE:      r = g_nLine;        break;
                    case VAR_SLINE:     r = g_nLine - TOP_BORDER_LINES; break;
                    case VAR_LCYCLES:   r = g_nLineCycle;   break;

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
    // Strip leading whitespace
    for ( ; isspace(*p) ; p++);

    // Decimal, hex or octal, using the standard C prefixes
    if (isdigit(*p))
        AddNode(T_NUMBER, strtoul(p, (char**)&p, 0));

    // Hex value with explicit prefix?
    else if ((*p == '$' || *p == '&' || *p == '#') && isxdigit(p[1]))
        AddNode(T_NUMBER, strtoul(++p, (char**)&p, 16));

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
    else if (*p == '-' || *p == '+' || *p == '~' || *p == '!' || *p == '*')
    {
        static const char* pcszUnary = "-+~!*";
        static int anUnary[] = { OP_UMINUS, OP_UPLUS, OP_BNOT, OP_NOT, OP_DEREF };

        // Remember the operator
        char op = *p++;

        // Look for a factor for the unary operator
        if (!Factor())
            return false;

        AddNode(T_UNARY_OP, anUnary[strchr(pcszUnary,op)-pcszUnary]);
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
            if (*p == '(')
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
            if ((pToken = LookupToken(pcsz, p-pcsz, asRegisters)))
                AddNode(T_REGISTER, pToken->nToken);
            else if ((pToken = LookupToken(pcsz, p-pcsz, asVariables)))
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
