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

const Expr Expr::Counter{ "(counter)", { { TokenType::Variable, Token::Count} } };
int Expr::count{};

const std::vector<std::vector<Expr::TokenEntry>> Expr::binary_op_tokens
{
    { {"||", Token::OR}, {"or", Token::OR} },
    { {"&&", Token::AND}, {"and", Token::AND} },
    { {"|", Token::BOR}, {"bor", Token::BOR} },
    { {"^", Token::BXOR}, {"bxor", Token::BXOR} },
    { {"&", Token::BAND}, {"band", Token::BAND} },
    { {"==", Token::EQ}, {"!=", Token::NE}, {"=", Token::EQ}, {"<>", Token::NE} },
    { {"<=", Token::LE}, {">=", Token::GE}, {"<", Token::LT}, {">", Token::GT} },
    { {"<<", Token::ShiftL}, {">>", Token::ShiftR} },
    { {"+", Token::ADD}, {"-", Token::SUB} },
    { {"*", Token::MUL}, {"/", Token::DIV}, {"%", Token::MOD}, {"\\", Token::MOD} },
};

const std::vector<Expr::TokenEntry> Expr::unary_op_tokens
{
    {"peek", Token::PEEK},
    {"dpeek", Token::DPEEK},
};

const std::vector<Expr::TokenEntry> Expr::reg_tokens
{
    {"a'", Token::Alt_A}, {"f'", Token::Alt_F}, {"b'", Token::Alt_B}, {"c'", Token::Alt_C},
    {"d'", Token::Alt_D}, {"e'", Token::Alt_E}, {"h'", Token::Alt_H}, {"l'", Token::Alt_L},
    {"af'", Token::Alt_AF}, {"bc'", Token::Alt_BC}, {"de'", Token::Alt_DE}, {"hl'", Token::Alt_HL},
    {"a", Token::A}, {"f", Token::F}, {"b", Token::B}, {"c", Token::C}, {"d", Token::D}, {"e", Token::E}, {"h", Token::H}, {"l", Token::L},
    {"af", Token::AF}, {"bc", Token::BC}, {"de", Token::DE}, {"hl", Token::HL},
    {"ix", Token::IX}, {"iy", Token::IY}, {"ixh", Token::IXH}, {"ixl", Token::IXL}, {"iyh", Token::IYH}, {"iyl", Token::IYL},
    {"sp", Token::SP}, {"pc", Token::PC}, {"sph", Token::SPH}, {"spl", Token::SPL}, {"pch", Token::PCH}, {"pcl", Token::PCL},
    {"i", Token::I}, {"r", Token::R}, {"iff1", Token::IFF1}, {"iff2", Token::IFF2}, {"im", Token::IM},
};

const std::vector<Expr::TokenEntry> Expr::var_tokens
{
    {"ei", Token::EI}, {"di", Token::DI},
    {"dline", Token::DLine}, {"sline", Token::SLine},
    {"rom0", Token::ROM0}, {"rom1", Token::ROM1}, {"wprot", Token::WProt}, {"inrom", Token::InROM}, {"call", Token::Call}, {"autoexec", Token::AutoExec},
    {"lepage", Token::LEPage}, {"hepage", Token::HEPage}, {"lpage", Token::LPage}, {"hpage", Token::HPage}, {"vpage", Token::VPage}, {"vmode", Token::VMode},
    {"inval", Token::InVal}, {"outval", Token::OutVal},
    {"lepr", Token::LEPR}, {"hepr", Token::HEPR}, {"lpen", Token::LPEN}, {"hpen", Token::HPEN}, {"status", Token::STATUS},
    {"lmpr", Token::LMPR}, {"hmpr", Token::HMPR}, {"vmpr", Token::VMPR}, {"midi", Token::MIDI}, {"border", Token::BORDER}, {"attr", Token::ATTR},
};

/*static*/ bool Expr::FindToken(
    std::string str_token,
    const std::vector<TokenEntry>& tokens,
    TokenEntry& token)
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

std::optional<std::variant<Expr::Token, int>> Expr::TokenValue(TokenType type) const
{
    if (nodes.size() == 1 && nodes.front().type == type)
    {
        return nodes.front().value;
    }

    return std::nullopt;
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
            if (auto value = std::get_if<int>(&node.value))
            {
                stack[sp++] = *value;
            }
            break;

        case TokenType::UnaryOp:
            if (sp < 1)
                break;

            if (auto op = std::get_if<Token>(&node.value))
            {
                auto x = stack[--sp];

                switch (*op)
                {
                case Token::UMINUS: x = -x; break;
                case Token::UPLUS:          break;
                case Token::BNOT:   x = ~x; break;
                case Token::NOT:    x = !x; break;
                case Token::DEREF:  x = read_byte(x); break;
                case Token::PEEK:   x = read_byte(x); break;
                case Token::DPEEK:  x = read_word(x); break;
                default:                    break;
                }

                stack[sp++] = x;
            }
            break;

        case TokenType::BinaryOp:
            if (sp < 2)
                break;

            if (auto op = std::get_if<Token>(&node.value))
            {
                // Pop the arguments (in reverse order)
                int b = stack[--sp];
                int a = stack[--sp];
                int r{};

                switch (*op)
                {
                case Token::OR:     r = a || b; break;
                case Token::AND:    r = a && b; break;
                case Token::BOR:    r = a | b;  break;
                case Token::BXOR:   r = a ^ b;  break;
                case Token::BAND:   r = a & b;  break;
                case Token::EQ:     r = a == b; break;
                case Token::NE:     r = a != b; break;
                case Token::LT:     r = a < b;  break;
                case Token::LE:     r = a <= b; break;
                case Token::GE:     r = a >= b; break;
                case Token::GT:     r = a > b;  break;
                case Token::ShiftL: r = a << b; break;
                case Token::ShiftR: r = a >> b; break;
                case Token::ADD:    r = a + b;  break;
                case Token::SUB:    r = a - b;  break;
                case Token::MUL:    r = a * b;  break;
                case Token::DIV:    r = b ? a / b : 0; break;   // ignore division by zero
                case Token::MOD:    r = b ? a % b : 0; break;   // ignore division by zero
                default:                        break;
                }

                stack[sp++] = r;
            }
            break;

        case TokenType::Register:
            if (auto reg = std::get_if<Token>(&node.value))
            {
                stack[sp++] = GetReg(*reg);
            }
            break;

        case TokenType::Variable:
        {
            int r{};

            auto var = std::get_if<Token>(&node.value);
            if (!var)
                break;

            switch (*var)
            {
            case Token::EI:        r = !!REG_IFF1; break;
            case Token::DI:        r = !REG_IFF1;  break;

            case Token::DLine:
            {
                auto [line, line_cycle] = Frame::GetRasterPos(g_dwCycleCounter);
                r = line;
                break;
            }

            case Token::SLine:
            {
                auto [line, line_cycle] = Frame::GetRasterPos(g_dwCycleCounter);
                if (line >= TOP_BORDER_LINES && line < (TOP_BORDER_LINES + GFX_SCREEN_LINES))
                    r = line - TOP_BORDER_LINES;
                else
                    r = -1;
                break;
            }

            case Token::ROM0:      r = !(lmpr & LMPR_ROM0_OFF);  break;
            case Token::ROM1:      r = !!(lmpr & LMPR_ROM1);     break;
            case Token::WProt:     r = !!(lmpr & LMPR_WPROT);    break;

            case Token::LEPage:    r = lepr; break;
            case Token::HEPage:    r = hepr; break;
            case Token::LPage:     r = lmpr & LMPR_PAGE_MASK;    break;
            case Token::HPage:     r = hmpr & HMPR_PAGE_MASK;    break;
            case Token::VPage:     r = vmpr & VMPR_PAGE_MASK;    break;
            case Token::VMode:     r = ((vmpr & VMPR_MODE_MASK) >> VMPR_MODE_SHIFT) + 1; break;

            case Token::InVal:     r = bPortInVal;               break;
            case Token::OutVal:    r = bPortOutVal;              break;

            case Token::LEPR:      r = LEPR_PORT;                break;    // 128
            case Token::HEPR:      r = HEPR_PORT;                break;    // 129
            case Token::LPEN:      r = LPEN_PORT;                break;    // 248
            case Token::HPEN:      r = HPEN_PORT;                break;    // 248+256
            case Token::STATUS:    r = STATUS_PORT;              break;    // 249
            case Token::LMPR:      r = LMPR_PORT;                break;    // 250
            case Token::HMPR:      r = HMPR_PORT;                break;    // 251
            case Token::VMPR:      r = VMPR_PORT;                break;    // 252
            case Token::MIDI:      r = MIDI_PORT;                break;    // 253
            case Token::BORDER:    r = BORDER_PORT;              break;    // 254
            case Token::ATTR:      r = ATTR_PORT;                break;    // 255

            case Token::InROM:     r = (!(lmpr & LMPR_ROM0_OFF) && REG_PC < 0x4000) || (lmpr & LMPR_ROM1 && REG_PC >= 0xc000); break;
            case Token::Call:      r = REG_PC == REG_HL && !(lmpr & LMPR_ROM0_OFF) && (read_word(REG_SP) == 0x180d); break;
            case Token::AutoExec:  r = REG_PC == REG_HL && !(lmpr & LMPR_ROM0_OFF) && (read_word(REG_SP) == 0x0213) && (read_word(REG_SP + 2) == 0x5f00); break;

            case Token::Count:     r = count ? !--count : 1; break;
            default:               break;
            }

            stack[sp++] = r;
            break;
        }

        default: break;
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
    bool fLast = level == (binary_op_tokens.size() - 1);

    // Recurse to the highest precedence first
    if (!(fLast ? Factor(p, flags) : Term(p, flags, level + 1)))
        return false;

    for (;;)
    {
        size_t max_len = 0;
        TokenEntry match;

        // Check for stack operator at any precedence level
        for (auto& ops : binary_op_tokens)
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
        for (auto& op : binary_op_tokens[level])
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

        nodes.push_back({ TokenType::BinaryOp, match.token });
    }
}

bool Expr::Factor(const char*& p, int flags)
{
    bool matched = false;

    for (; std::isspace(static_cast<uint8_t>(*p)); p++);

    if (!matched && isalpha(*p))
    {
        auto p2 = p;
        std::optional<int> sym_value;

        matched = true;

        for (; isalnum(*p2) || *p2 == '_'; p2++);
        if (*p2 == '\'') p2++;

        TokenEntry token{};
        auto str_token = std::string(p, p2);

        if (!(flags & noRegs) && FindToken(str_token, reg_tokens, token))
        {
            nodes.push_back({ TokenType::Register, token.token });
        }
        else if (!(flags & noVars) && FindToken(str_token, var_tokens, token))
        {
            nodes.push_back({ TokenType::Variable, token.token });
        }
        else if (!(flags & noSyms) && (sym_value = Symbol::LookupSymbol(str_token)))
        {
            nodes.push_back({ TokenType::Number, *sym_value });
        }
        else if (FindToken(std::string(p, p2), unary_op_tokens, token))
        {
            p = p2;

            if (!Factor(p, flags))
                return false;

            nodes.push_back({ TokenType::UnaryOp, token.token });
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
            nodes.push_back({ TokenType::Number, dec_value });
            p = pDecEnd + 1;
        }
        else if (tolower(*pHexEnd) == 'h')
        {
            nodes.push_back({ TokenType::Number, hex_value });
            p = pHexEnd + 1;
        }
        else if (p[0] == '0' && p[1] == 'x')
        {
            nodes.push_back({ TokenType::Number, hex_value });
            p = pHexEnd;
        }
        else if (p[0] == '0' && p[1] == 'n')
        {
            dec_value = static_cast<int>(strtoul(p + 2, (char**)&pDecEnd, 10));
            nodes.push_back({ TokenType::Number, dec_value });
            p = pDecEnd;
        }
        else if (!isalpha(*pHexEnd))
        {
            nodes.push_back({ TokenType::Number, hex_value });
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
        auto value = static_cast<int>(strtoul(p, (char**)&p, 16));
        nodes.push_back({ TokenType::Number, value });
    }
    else if (*p == '%' && (p[1] == '0' || p[1] == '1'))
    {
        int i = 0;
        for (p++; *p == '0' || *p == '1'; p++)
            (i <<= 1) |= (*p - '0');

        nodes.push_back({ TokenType::Number, i });
    }
    else if (*p == '"' || *p == '\'')
    {
        nodes.push_back({ TokenType::Number, *++p });

        // Ensure the closing quote matches the open
        if (p[-1] != p[1])
            return false;
        else
            p += 2;
    }
    else if (*p == '-' || *p == '+' || *p == '~' || *p == '!' || *p == '*' || *p == '=')
    {
        static const std::string_view unary_symbols = "-+~!*=";
        static Token unary_tokens[] = {
            Token::UMINUS, Token::UPLUS, Token::BNOT, Token::NOT, Token::DEREF, Token::EVAL
        };

        char op = *p++;
        auto node_count = nodes.size();

        if (!Factor(p, flags))
            return false;

        if (op != '=')
        {
            auto token = unary_tokens[unary_symbols.find(op)];
            nodes.push_back({ TokenType::UnaryOp, token });
        }
        else
        {
            auto n = Expr::Eval(std::vector<Node>(nodes.begin() + node_count, nodes.end()));
            nodes.resize(node_count);
            nodes.push_back({ TokenType::Number, n });
        }
    }
    else if (*p == '$' && !(flags & noRegs))
    {
        nodes.push_back({ TokenType::Register, Token::PC });
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

int Expr::GetReg(Token reg)
{
    int ret{};

    switch (reg)
    {
    case Token::A:         ret = REG_A;      break;
    case Token::F:         ret = REG_F;      break;
    case Token::B:         ret = REG_B;      break;
    case Token::C:         ret = REG_C;      break;
    case Token::D:         ret = REG_D;      break;
    case Token::E:         ret = REG_E;      break;
    case Token::H:         ret = REG_H;      break;
    case Token::L:         ret = REG_L;      break;

    case Token::Alt_A:     ret = REG_A_;     break;
    case Token::Alt_F:     ret = REG_F_;     break;
    case Token::Alt_B:     ret = REG_B_;     break;
    case Token::Alt_C:     ret = REG_C_;     break;
    case Token::Alt_D:     ret = REG_D_;     break;
    case Token::Alt_E:     ret = REG_E_;     break;
    case Token::Alt_H:     ret = REG_H_;     break;
    case Token::Alt_L:     ret = REG_L_;     break;

    case Token::AF:        ret = REG_AF;     break;
    case Token::BC:        ret = REG_BC;     break;
    case Token::DE:        ret = REG_DE;     break;
    case Token::HL:        ret = REG_HL;     break;

    case Token::Alt_AF:    ret = REG_AF_;    break;
    case Token::Alt_BC:    ret = REG_BC_;    break;
    case Token::Alt_DE:    ret = REG_DE_;    break;
    case Token::Alt_HL:    ret = REG_HL_;    break;

    case Token::IX:        ret = REG_IX;     break;
    case Token::IY:        ret = REG_IY;     break;
    case Token::SP:        ret = REG_SP;     break;
    case Token::PC:        ret = REG_PC;     break;

    case Token::IXH:       ret = REG_IXH;    break;
    case Token::IXL:       ret = REG_IXL;    break;
    case Token::IYH:       ret = REG_IYH;    break;
    case Token::IYL:       ret = REG_IYL;    break;

    case Token::SPH:       ret = REG_SPH;    break;
    case Token::SPL:       ret = REG_SPL;    break;
    case Token::PCH:       ret = REG_PCH;    break;
    case Token::PCL:       ret = REG_PCL;    break;

    case Token::I:         ret = REG_I;      break;
    case Token::R:         ret = (REG_R7 & 0x80) | (REG_R & 0x7f); break;
    case Token::IFF1:      ret = REG_IFF1;   break;
    case Token::IFF2:      ret = REG_IFF2;   break;
    case Token::IM:        ret = REG_IM;     break;

    default:
        assert(false);
        ret = 0;
        break;
    }

    return ret;
}

void Expr::SetReg(Token reg, int value)
{
    auto w = static_cast<uint16_t>(value);
    uint8_t b = w & 0xff;

    switch (reg)
    {
    case Token::A:      REG_A = b; break;
    case Token::F:      REG_F = b; break;
    case Token::B:      REG_B = b; break;
    case Token::C:      REG_C = b; break;
    case Token::D:      REG_D = b; break;
    case Token::E:      REG_E = b; break;
    case Token::H:      REG_H = b; break;
    case Token::L:      REG_L = b; break;

    case Token::Alt_A:  REG_A_ = b; break;
    case Token::Alt_F:  REG_F_ = b; break;
    case Token::Alt_B:  REG_B_ = b; break;
    case Token::Alt_C:  REG_C_ = b; break;
    case Token::Alt_D:  REG_D_ = b; break;
    case Token::Alt_E:  REG_E_ = b; break;
    case Token::Alt_H:  REG_H_ = b; break;
    case Token::Alt_L:  REG_L_ = b; break;

    case Token::AF:     REG_AF = w; break;
    case Token::BC:     REG_BC = w; break;
    case Token::DE:     REG_DE = w; break;
    case Token::HL:     REG_HL = w; break;

    case Token::Alt_AF: REG_AF_ = w; break;
    case Token::Alt_BC: REG_BC_ = w; break;
    case Token::Alt_DE: REG_DE_ = w; break;
    case Token::Alt_HL: REG_HL_ = w; break;

    case Token::IX:     REG_IX = w; break;
    case Token::IY:     REG_IY = w; break;
    case Token::SP:     REG_SP = w; break;
    case Token::PC:     REG_PC = w; break;

    case Token::IXH:    REG_IXH = b; break;
    case Token::IXL:    REG_IXL = b; break;
    case Token::IYH:    REG_IYH = b; break;
    case Token::IYL:    REG_IYL = b; break;

    case Token::SPH:    REG_SPH = b; break;
    case Token::SPL:    REG_SPL = b; break;
    case Token::PCH:    REG_PCH = b; break;
    case Token::PCL:    REG_PCL = b; break;

    case Token::I:      REG_I = b; break;
    case Token::R:      REG_R7 = REG_R = b; break;
    case Token::IFF1:   REG_IFF1 = !!b; break;
    case Token::IFF2:   REG_IFF2 = !!b; break;
    case Token::IM:     if (b <= 2) REG_IM = b; break;

    default:
        assert(false);
        break;
    }
}
