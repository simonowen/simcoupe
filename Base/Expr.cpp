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

#include "CPU.h"
#include "Expr.h"
#include "Frame.h"
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

    const auto& io_state = IO::State();
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
            case Token::EI:
                r = cpu.get_iff1() ? 1 : 0;
                break;

            case Token::DI:
                r = cpu.get_iff1() ? 0 : 1;
                break;

            case Token::DLine:
            {
                auto [line, line_cycle] = Frame::GetRasterPos(CPU::frame_cycles);
                r = line;
                break;
            }

            case Token::SLine:
            {
                auto [line, line_cycle] = Frame::GetRasterPos(CPU::frame_cycles);
                if (line >= TOP_BORDER_LINES && line < (TOP_BORDER_LINES + GFX_SCREEN_LINES))
                    r = line - TOP_BORDER_LINES;
                else
                    r = -1;
                break;
            }

            case Token::ROM0:      r = !(io_state.lmpr & LMPR_ROM0_OFF);  break;
            case Token::ROM1:      r = !!(io_state.lmpr & LMPR_ROM1);     break;
            case Token::WProt:     r = !!(io_state.lmpr & LMPR_WPROT);    break;

            case Token::LEPage:    r = io_state.lepr; break;
            case Token::HEPage:    r = io_state.hepr; break;
            case Token::LPage:     r = io_state.lmpr & LMPR_PAGE_MASK;    break;
            case Token::HPage:     r = io_state.hmpr & HMPR_PAGE_MASK;    break;
            case Token::VPage:     r = io_state.vmpr & VMPR_PAGE_MASK;    break;
            case Token::VMode:     r = ((io_state.vmpr & VMPR_MODE_MASK) >> VMPR_MODE_SHIFT) + 1; break;

            case Token::InVal:     r = IO::last_in_val;          break;
            case Token::OutVal:    r = IO::last_out_val;         break;

            case Token::LEPR:      r = LEPR_PORT;                break;
            case Token::HEPR:      r = HEPR_PORT;                break;
            case Token::LPEN:      r = LPEN_PORT;                break;
            case Token::HPEN:      r = HPEN_PORT;                break;
            case Token::STATUS:    r = STATUS_PORT;              break;
            case Token::LMPR:      r = LMPR_PORT;                break;
            case Token::HMPR:      r = HMPR_PORT;                break;
            case Token::VMPR:      r = VMPR_PORT;                break;
            case Token::MIDI:      r = MIDI_PORT;                break;
            case Token::BORDER:    r = BORDER_PORT;              break;
            case Token::ATTR:      r = ATTR_PORT;                break;

            case Token::InROM:
                r = (!(io_state.lmpr & LMPR_ROM0_OFF) && cpu.get_pc() < 0x4000) ||
                    (io_state.lmpr & LMPR_ROM1 && cpu.get_pc() >= 0xc000);
                break;

            case Token::Call:
                r = cpu.get_pc() == cpu.get_hl() &&
                    !(io_state.lmpr & LMPR_ROM0_OFF) &&
                    (read_word(cpu.get_sp()) == 0x180d);
                break;

            case Token::AutoExec:
                r = cpu.get_pc() == cpu.get_hl() &&
                    !(io_state.lmpr & LMPR_ROM0_OFF) &&
                    (read_word(cpu.get_sp()) == 0x0213) &&
                    (read_word(cpu.get_sp() + 2) == 0x5f00);
                break;

            case Token::Count:
                r = count ? !--count : 1;
                break;

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
    case Token::A:         ret = cpu.get_a(); break;
    case Token::F:         ret = cpu.get_f(); break;
    case Token::B:         ret = cpu.get_b(); break;
    case Token::C:         ret = cpu.get_c(); break;
    case Token::D:         ret = cpu.get_d(); break;
    case Token::E:         ret = cpu.get_e(); break;
    case Token::H:         ret = cpu.get_h(); break;
    case Token::L:         ret = cpu.get_l(); break;

    case Token::Alt_A:     ret = z80::get_high8(cpu.get_alt_af()); break;
    case Token::Alt_F:     ret = z80::get_low8(cpu.get_alt_af()); break;
    case Token::Alt_B:     ret = z80::get_high8(cpu.get_alt_bc()); break;
    case Token::Alt_C:     ret = z80::get_low8(cpu.get_alt_bc()); break;
    case Token::Alt_D:     ret = z80::get_high8(cpu.get_alt_de()); break;
    case Token::Alt_E:     ret = z80::get_low8(cpu.get_alt_de()); break;
    case Token::Alt_H:     ret = z80::get_high8(cpu.get_alt_hl()); break;
    case Token::Alt_L:     ret = z80::get_low8(cpu.get_alt_hl()); break;

    case Token::AF:        ret = cpu.get_af(); break;
    case Token::BC:        ret = cpu.get_bc(); break;
    case Token::DE:        ret = cpu.get_de(); break;
    case Token::HL:        ret = cpu.get_hl(); break;

    case Token::Alt_AF:    ret = cpu.get_alt_af(); break;
    case Token::Alt_BC:    ret = cpu.get_alt_bc(); break;
    case Token::Alt_DE:    ret = cpu.get_alt_de(); break;
    case Token::Alt_HL:    ret = cpu.get_alt_hl(); break;

    case Token::IX:        ret = cpu.get_ix(); break;
    case Token::IY:        ret = cpu.get_iy(); break;
    case Token::SP:        ret = cpu.get_sp(); break;
    case Token::PC:        ret = cpu.get_pc(); break;

    case Token::IXH:       ret = cpu.get_ixh(); break;
    case Token::IXL:       ret = cpu.get_ixl(); break;
    case Token::IYH:       ret = cpu.get_iyh(); break;
    case Token::IYL:       ret = cpu.get_iyl(); break;

    case Token::SPH:       ret = z80::get_high8(cpu.get_sp()); break;
    case Token::SPL:       ret = z80::get_low8(cpu.get_sp()); break;
    case Token::PCH:       ret = z80::get_high8(cpu.get_pc()); break;
    case Token::PCL:       ret = z80::get_low8(cpu.get_pc()); break;

    case Token::I:         ret = cpu.get_i(); break;
    case Token::R:         ret = cpu.get_r(); break;
    case Token::IFF1:      ret = cpu.get_iff1() ? 1 : 0; break;
    case Token::IFF2:      ret = cpu.get_iff2() ? 1 : 0; break;
    case Token::IM:        ret = cpu.get_int_mode(); break;

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
    case Token::A:      cpu.set_a(b); break;
    case Token::F:      cpu.set_f(b); break;
    case Token::B:      cpu.set_b(b); break;
    case Token::C:      cpu.set_c(b); break;
    case Token::D:      cpu.set_d(b); break;
    case Token::E:      cpu.set_e(b); break;
    case Token::H:      cpu.set_h(b); break;
    case Token::L:      cpu.set_l(b); break;

    case Token::Alt_A:  cpu.set_alt_af(z80::make16(b, z80::get_low8(cpu.get_af()))); break;
    case Token::Alt_F:  cpu.set_alt_af(z80::make16(z80::get_high8(cpu.get_af()), b)); break;
    case Token::Alt_B:  cpu.set_alt_bc(z80::make16(b, z80::get_low8(cpu.get_bc()))); break;
    case Token::Alt_C:  cpu.set_alt_bc(z80::make16(z80::get_high8(cpu.get_bc()), b)); break;
    case Token::Alt_D:  cpu.set_alt_de(z80::make16(b, z80::get_low8(cpu.get_de()))); break;
    case Token::Alt_E:  cpu.set_alt_de(z80::make16(z80::get_high8(cpu.get_de()), b)); break;
    case Token::Alt_H:  cpu.set_alt_hl(z80::make16(b, z80::get_low8(cpu.get_hl()))); break;
    case Token::Alt_L:  cpu.set_alt_hl(z80::make16(z80::get_high8(cpu.get_hl()), b)); break;

    case Token::AF:     cpu.set_af(w); break;
    case Token::BC:     cpu.set_bc(w); break;
    case Token::DE:     cpu.set_de(w); break;
    case Token::HL:     cpu.set_hl(w); break;

    case Token::Alt_AF: cpu.set_alt_af(w); break;
    case Token::Alt_BC: cpu.set_alt_bc(w); break;
    case Token::Alt_DE: cpu.set_alt_de(w); break;
    case Token::Alt_HL: cpu.set_alt_hl(w); break;

    case Token::IX:     cpu.set_ix(w); break;
    case Token::IY:     cpu.set_iy(w); break;
    case Token::SP:     cpu.set_sp(w); break;
    case Token::PC:     cpu.set_pc(w); break;

    case Token::IXH:    cpu.set_ixh(b); break;
    case Token::IXL:    cpu.set_ixl(b); break;
    case Token::IYH:    cpu.set_iyh(b); break;
    case Token::IYL:    cpu.set_iyl (b); break;

    case Token::SPH:    cpu.set_sp(z80::make16(b, z80::get_low8(cpu.get_sp()))); break;
    case Token::SPL:    cpu.set_alt_af(z80::make16(z80::get_high8(cpu.get_sp()), b)); break;
    case Token::PCH:    cpu.set_pc(z80::make16(b, z80::get_low8(cpu.get_pc()))); break;
    case Token::PCL:    cpu.set_alt_af(z80::make16(z80::get_high8(cpu.get_pc()), b)); break;

    case Token::I:      cpu.set_i(b); break;
    case Token::R:      cpu.set_r(b); break;
    case Token::IFF1:   cpu.set_iff1(!!b); break;
    case Token::IFF2:   cpu.set_iff2(!!b); break;
    case Token::IM:     if (b <= 2) cpu.set_int_mode(b); break;

    default:
        assert(false);
        break;
    }
}
