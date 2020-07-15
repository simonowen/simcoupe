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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "SimCoupe.h"

#include "Expr.h"
#include "Memory.h"
#include "Options.h"
#include "Symbol.h"

const Expr Expr::Counter{ "(counter)", { { TokenType::Variable, VAR_COUNT} } };
int Expr::count{};

struct Token
{
    std::string str;
    int token{};
};

static const std::vector<std::vector<Token>> binary_ops
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
};

static const std::vector<Token> unary_funcs
{
    {"peek",OP_PEEK},
    {"dpeek",OP_DPEEK},
};

static const std::vector<Token> reg_symbols
{
    {"a'",REG_ALT_A}, {"f'",REG_ALT_F}, {"b'",REG_ALT_B}, {"c'",REG_ALT_C},
    {"d'",REG_ALT_D}, {"e'",REG_ALT_E}, {"h'",REG_ALT_H}, {"l'",REG_ALT_L},
    {"af'",REG_ALT_AF}, {"bc'",REG_ALT_BC}, {"de'",REG_ALT_DE}, {"hl'",REG_ALT_HL},
    {"a",REG_A}, {"f",REG_F}, {"b",REG_B}, {"c",REG_C}, {"d",REG_D}, {"e",REG_E}, {"h",REG_H}, {"l",REG_L},
    {"af",REG_AF}, {"bc",REG_BC}, {"de",REG_DE}, {"hl",REG_HL},
    {"ix",REG_IX}, {"iy",REG_IY}, {"ixh",REG_IXH}, {"ixl",REG_IXL}, {"iyh",REG_IYH}, {"iyl",REG_IYL},
    {"sp",REG_SP}, {"pc",REG_PC}, {"sph",REG_SPH}, {"spl",REG_SPL}, {"pch",REG_PCH}, {"pcl",REG_PCL},
    {"i",REG_I}, {"r",REG_R}, {"iff1",REG_IFF1}, {"iff2",REG_IFF2}, {"im",REG_IM},
};

static const std::vector<Token> var_symbols
{
    {"ei",VAR_EI}, {"di",VAR_DI},
    {"dline",VAR_DLINE}, {"sline",VAR_SLINE},
    {"rom0",VAR_ROM0}, {"rom1",VAR_ROM1}, {"wprot",VAR_WPROT}, {"inrom",VAR_INROM}, {"call",VAR_CALL}, {"autoexec",VAR_AUTOEXEC},
    {"lepage",VAR_LEPAGE}, {"hepage",VAR_HEPAGE}, {"lpage",VAR_LPAGE}, {"hpage",VAR_HPAGE}, {"vpage",VAR_VPAGE}, {"vmode",VAR_VMODE},
    {"inval",VAR_INVAL}, {"outval",VAR_OUTVAL},
    {"lepr",VAR_LEPR}, {"hepr",VAR_HEPR}, {"lpen",VAR_LPEN}, {"hpen",VAR_HPEN}, {"status",VAR_STATUS},
    {"lmpr",VAR_LMPR}, {"hmpr",VAR_HMPR}, {"vmpr",VAR_VMPR}, {"midi",VAR_MIDI}, {"border",VAR_BORDER}, {"attr",VAR_ATTR},
};

static bool FindToken(std::string str_token, const std::vector<Token>& tokens, Token& token)
{
    str_token = tolower(str_token);

    auto it = std::find_if(tokens.begin(), tokens.end(),
        [&](const auto& token) { return token.str == str_token; });

    if (it == tokens.end())
        return false;

    token = *it;
    return true;
}

////////////////////////////////////////////////////////////////////////////////

bool Expr::IsTokenType(TokenType type) const
{
    return nodes.size() == 1 && nodes.front().type == type;
}

int Expr::TokenValue() const
{
    assert(nodes.size() == 1);
    return nodes.front().value;
}

void Expr::AddNode(Expr::TokenType type, int value)
{
    nodes.push_back({ type, value });
}

Expr Expr::Compile(std::string str)
{
    std::string remainder;

    auto expr = Compile(str, remainder, noFlags);
    if (!remainder.empty())
        expr.nodes.clear();

    return expr;
}

Expr Expr::Compile(std::string str, std::string& remain, int flags)
{
    Expr expr;
    expr.str = str;

    auto p = str.c_str();

    if (!expr.Term(p, flags))
        expr.nodes.clear();

    remain = p;
    return expr;
}

int Expr::Eval() const
{
    return Eval(nodes);
}

int Expr::Eval(const std::vector<Node>& nodes)
{
    if (nodes.empty())
        return -1;

    std::array<int, 128> stack{};
    int sp = 0;

    for (auto& node : nodes)
    {
        switch (node.type)
        {
        case TokenType::Number:
            stack[sp++] = node.value;
            break;

        case TokenType::UnaryOp:
        {
            if (sp < 1)
                break;

            auto x = stack[--sp];

            switch (node.value)
            {
            case OP_UMINUS: x = -x; break;
            case OP_UPLUS:          break;
            case OP_BNOT:   x = ~x; break;
            case OP_NOT:    x = !x; break;
            case OP_DEREF:  x = read_byte(x); break;
            case OP_PEEK:   x = read_byte(x); break;
            case OP_DPEEK:  x = read_word(x); break;
            }

            stack[sp++] = x;
            break;
        }

        case TokenType::BinaryOp:
        {
            if (sp < 2)
                break;

            // Pop the arguments (in reverse order)
            int b = stack[--sp];
            int a = stack[--sp];
            int r{};

            switch (node.value)
            {
            case OP_OR:     r = a || b; break;
            case OP_AND:    r = a && b; break;
            case OP_BOR:    r = a | b;  break;
            case OP_BXOR:   r = a ^ b;  break;
            case OP_BAND:   r = a & b;  break;
            case OP_EQ:     r = a == b; break;
            case OP_NE:     r = a != b; break;
            case OP_LT:     r = a < b;  break;
            case OP_LE:     r = a <= b; break;
            case OP_GE:     r = a >= b; break;
            case OP_GT:     r = a > b;  break;
            case OP_SHIFTL: r = a << b; break;
            case OP_SHIFTR: r = a >> b; break;
            case OP_ADD:    r = a + b;  break;
            case OP_SUB:    r = a - b;  break;
            case OP_MUL:    r = a * b;  break;
            case OP_DIV:    r = b ? a / b : 0; break;   // ignore division by zero
            case OP_MOD:    r = b ? a % b : 0; break;   // ignore division by zero
            }

            stack[sp++] = r;
            break;
        }

        case TokenType::Register:
            stack[sp++] = GetReg(node.value);
            break;

        case TokenType::Variable:
        {
            int r = 0;

            switch (node.value)
            {
            case VAR_EI:        r = !!IFF1; break;
            case VAR_DI:        r = !IFF1;  break;

            case VAR_DLINE:
            {
                auto [line, line_cycle] = Frame::GetRasterPos(g_dwCycleCounter);
                r = line;
                break;
            }

            case VAR_SLINE:
            {
                auto [line, line_cycle] = Frame::GetRasterPos(g_dwCycleCounter);
                if (line >= TOP_BORDER_LINES && line < (TOP_BORDER_LINES + GFX_SCREEN_LINES))
                    r = line - TOP_BORDER_LINES;
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
            case VAR_VMODE:     r = ((vmpr & VMPR_MODE_MASK) >> VMPR_MODE_SHIFT) + 1; break;

            case VAR_INVAL:     r = bPortInVal;               break;
            case VAR_OUTVAL:    r = bPortOutVal;              break;

            case VAR_LEPR:      r = LEPR_PORT;                break;    // 128
            case VAR_HEPR:      r = HEPR_PORT;                break;    // 129
            case VAR_LPEN:      r = LPEN_PORT;                break;    // 248
            case VAR_HPEN:      r = HPEN_PORT;                break;    // 248+256
            case VAR_STATUS:    r = STATUS_PORT;              break;    // 249
            case VAR_LMPR:      r = LMPR_PORT;                break;    // 250
            case VAR_HMPR:      r = HMPR_PORT;                break;    // 251
            case VAR_VMPR:      r = VMPR_PORT;                break;    // 252
            case VAR_MIDI:      r = MIDI_PORT;                break;    // 253
            case VAR_BORDER:    r = BORDER_PORT;              break;    // 254
            case VAR_ATTR:      r = ATTR_PORT;                break;    // 255

            case VAR_INROM:     r = (!(lmpr & LMPR_ROM0_OFF) && PC < 0x4000) || (lmpr & LMPR_ROM1 && PC >= 0xc000); break;
            case VAR_CALL:      r = PC == HL && !(lmpr & LMPR_ROM0_OFF) && (read_word(SP) == 0x180d); break;
            case VAR_AUTOEXEC:  r = PC == HL && !(lmpr & LMPR_ROM0_OFF) && (read_word(SP) == 0x0213) && (read_word(SP + 2) == 0x5f00); break;

            case VAR_COUNT:     r = count ? !--count : 1; break;
            }

            stack[sp++] = r;
            break;
        }

        case TokenType::Unknown:
            TRACE("Expr::Eval(): unknown type %d!\n", node.type);
            stack[sp++] = 0;
            break;
        }
    }

    return (sp < 1) ? 0 : stack[--sp];
}

bool Expr::Eval(const std::string& str, int& value, std::string& remainder, int flags)
{
    if (str.empty())
        return false;

    auto expr = Compile(str, remainder, flags);
    if (!expr)
        return false;

    value = expr.Eval();
    return true;
}

bool Expr::Term(const char*& p, int flags, int level)
{
    bool fLast = level == (binary_ops.size() - 1);

    // Recurse to the highest precedence first
    if (!(fLast ? Factor(p, flags) : Term(p, flags, level + 1)))
        return false;

    for (;;)
    {
        size_t max_len = 0;
        Token match;

        // Check for stack operator at any precedence level
        for (auto& ops : binary_ops)
        {
            for (auto& op : ops)
            {
                if (op.str.length() > max_len && strlen(p) >= op.str.length())
                {
                    auto str = std::string(p, op.str.length());

                    if (op.str == str)
                        max_len = op.str.length();
                }
            }
        }

        // Check for stack operator at the current precedence level
        for (auto& op : binary_ops[level])
        {
            if (op.str.length() >= max_len && strlen(p) >= op.str.length())
            {
                auto str = std::string_view(p, op.str.length());

                if (op.str == str)
                {
                    match = op;
                    break;
                }
            }
        }

        if (match.str.empty())
            return true;

        p += match.str.length();

        if (!(fLast ? Factor(p, flags) : Term(p, flags, level + 1)))
            return false;

        AddNode(TokenType::BinaryOp, match.token);
    }
}

bool Expr::Factor(const char*& p, int flags)
{
    bool matched = false;

    for (; std::isspace(static_cast<uint8_t>(*p)); p++);

    if (!matched && isalpha(*p))
    {
        auto p2 = p;
        int sym_value{};

        matched = true;

        for (; isalnum(*p2) || *p2 == '_'; p2++);
        if (*p2 == '\'') p2++;

        Token token{};
        auto str_token = std::string(p, p2);

        if (!(flags & noRegs) && FindToken(str_token, reg_symbols, token))
        {
            AddNode(TokenType::Register, token.token);
        }
        else if (!(flags & noVars) && FindToken(str_token, var_symbols, token))
        {
            AddNode(TokenType::Variable, token.token);
        }
        else if (!(flags & noSyms) && (sym_value = Symbol::LookupSymbol(str_token)) >= 0)
        {
            AddNode(TokenType::Number, sym_value);
        }
        else if (FindToken(std::string(p, p2), unary_funcs, token))
        {
            p = p2;

            if (!Factor(p, flags))
                return false;

            AddNode(TokenType::UnaryOp, token.token);
            return true;
        }
        else
            matched = false;

        if (matched)
            p = p2;
    }

    if (!matched && !(flags & noVals) && std::isxdigit(*p))
    {
        matched = true;

        char* pDecEnd{};
        char* pHexEnd{};
        int dec_value = static_cast<int>(std::strtoul(p, &pDecEnd, 10));
        int hex_value = static_cast<int>(std::strtoul(p, &pHexEnd, 16));

        if (*pDecEnd == '.')
        {
            AddNode(TokenType::Number, dec_value);
            p = pDecEnd + 1;
        }
        else if (tolower(*pHexEnd) == 'h')
        {
            AddNode(TokenType::Number, hex_value);
            p = pHexEnd + 1;
        }
        else if (p[0] == '0' && p[1] == 'x')
        {
            AddNode(TokenType::Number, hex_value);
            p = pHexEnd;
        }
        else if (p[0] == '0' && p[1] == 'n')
        {
            dec_value = static_cast<int>(strtoul(p + 2, (char**)&pDecEnd, 10));
            AddNode(TokenType::Number, dec_value);
            p = pDecEnd;
        }
        else if (!isalpha(*pHexEnd))
        {
            AddNode(TokenType::Number, hex_value);
            p = pHexEnd;
        }
        else
        {
            matched = false;
        }
    }

    if (matched)
    {
        // Nothing more to do
    }
    else if ((*p == '$' || *p == '&' || *p == '#') && isxdigit(static_cast<uint8_t>(p[1])))
    {
        p++;
        AddNode(TokenType::Number, static_cast<int>(strtoul(p, (char**)&p, 16)));
    }
    else if (*p == '%' && (p[1] == '0' || p[1] == '1'))
    {
        unsigned u = 0;
        for (p++; *p == '0' || *p == '1'; p++)
            (u <<= 1) |= (*p - '0');

        AddNode(TokenType::Number, u);
    }
    else if (*p == '"' || *p == '\'')
    {
        AddNode(TokenType::Number, *++p);

        // Ensure the closing quote matches the open
        if (p[-1] != p[1])
            return false;
        else
            p += 2;
    }
    else if (*p == '-' || *p == '+' || *p == '~' || *p == '!' || *p == '*' || *p == '=')
    {
        static const char* pcszUnary = "-+~!*=";
        static int anUnary[] = { OP_UMINUS, OP_UPLUS, OP_BNOT, OP_NOT, OP_DEREF, OP_EVAL };

        char op = *p++;
        auto node_count = nodes.size();

        if (!Factor(p, flags))
            return false;

        if (op != '=')
        {
            AddNode(TokenType::UnaryOp, anUnary[strchr(pcszUnary, op) - pcszUnary]);
        }
        else
        {
            auto n = Expr::Eval(std::vector<Node>(nodes.begin() + node_count, nodes.end()));
            nodes.resize(node_count);
            AddNode(TokenType::Number, n);
        }
    }
    else if (*p == '$' && !(flags & noRegs))
    {
        AddNode(TokenType::Register, REG_PC);
        p++;
    }
    else if (*p == '(')
    {
        p++;
        if (!Term(p, flags) || *p++ != ')')
            return false;
    }
    else
    {
        return false;
    }

    for (; std::isspace(static_cast<uint8_t>(*p)); p++);

    return true;
}

int Expr::GetReg(int reg)
{
    int ret = 0;

    switch (reg)
    {
    case REG_A:         ret = A;      break;
    case REG_F:         ret = F;      break;
    case REG_B:         ret = B;      break;
    case REG_C:         ret = C;      break;
    case REG_D:         ret = D;      break;
    case REG_E:         ret = E;      break;
    case REG_H:         ret = H;      break;
    case REG_L:         ret = L;      break;

    case REG_ALT_A:     ret = A_;     break;
    case REG_ALT_F:     ret = F_;     break;
    case REG_ALT_B:     ret = B_;     break;
    case REG_ALT_C:     ret = C_;     break;
    case REG_ALT_D:     ret = D_;     break;
    case REG_ALT_E:     ret = E_;     break;
    case REG_ALT_H:     ret = H_;     break;
    case REG_ALT_L:     ret = L_;     break;

    case REG_AF:        ret = AF;     break;
    case REG_BC:        ret = BC;     break;
    case REG_DE:        ret = DE;     break;
    case REG_HL:        ret = HL;     break;

    case REG_ALT_AF:    ret = AF_;    break;
    case REG_ALT_BC:    ret = BC_;    break;
    case REG_ALT_DE:    ret = DE_;    break;
    case REG_ALT_HL:    ret = HL_;    break;

    case REG_IX:        ret = IX;     break;
    case REG_IY:        ret = IY;     break;
    case REG_SP:        ret = SP;     break;
    case REG_PC:        ret = PC;     break;

    case REG_IXH:       ret = IXH;    break;
    case REG_IXL:       ret = IXL;    break;
    case REG_IYH:       ret = IYH;    break;
    case REG_IYL:       ret = IYL;    break;

    case REG_SPH:       ret = SPH;    break;
    case REG_SPL:       ret = SPL;    break;
    case REG_PCH:       ret = PCH;    break;
    case REG_PCL:       ret = PCL;    break;

    case REG_I:         ret = I;      break;
    case REG_R:         ret = (R7 & 0x80) | (R & 0x7f); break;
    case REG_IFF1:      ret = IFF1;   break;
    case REG_IFF2:      ret = IFF2;   break;
    case REG_IM:        ret = IM;     break;
    }

    return ret;
}

void Expr::SetReg(int reg, int value)
{
    auto w = static_cast<uint16_t>(value);
    uint8_t b = w & 0xff;

    switch (reg)
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

    case REG_AF:     AF = w; break;
    case REG_BC:     BC = w; break;
    case REG_DE:     DE = w; break;
    case REG_HL:     HL = w; break;

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
