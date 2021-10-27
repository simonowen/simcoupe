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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#pragma once

struct Expr
{
    enum class Token
    {
        UMINUS, UPLUS, BNOT, NOT, DEREF, PEEK, DPEEK, EVAL,

        AND, OR, BOR, BXOR, BAND, EQ, NE, LT, LE, GE,
        GT, ShiftL, ShiftR, ADD, SUB, MUL, DIV, MOD,

        A, F, B, C, D, E, H, L,
        Alt_A, Alt_F, Alt_B, Alt_C, Alt_D, Alt_E, Alt_H, Alt_L,
        AF, BC, DE, HL, Alt_AF, Alt_BC, Alt_DE, Alt_HL,
        IX, IY, IXH, IXL, IYH, IYL,
        SP, PC, SPH, SPL, PCH, PCL,
        I, R, IFF1, IFF2, IM,

        EI, DI, Halted, DLine, SLine, Count,
        ROM0, ROM1, WProt, InROM, Call, AutoExec,
        LEPage, HEPage, LPage, HPage, VPage, VMode,
        InVal, OutVal,
        LEPR, HEPR, LPEN, HPEN, STATUS, LMPR, HMPR, VMPR, MIDI, BORDER, ATTR,
    };

    enum
    {
        noFlags = 0x00, noRegs = 0x01, noVars = 0x02, noFuncs = 0x04, noVals = 0x08, noSyms = 0x10,
        valOnly = noRegs | noVars | noFuncs | noSyms, regOnly = noVars | noFuncs | noVals | noSyms, simple = valOnly
    };

    enum class TokenType { Number, UnaryOp, BinaryOp, Register, Variable };

    struct Node
    {
        TokenType type{};
        std::variant<Token, int> value;
    };

    static Expr Compile(std::string str);
    static Expr Compile(std::string str, std::string& remain, int flags = noFlags);
    static bool Eval(const std::string& str, int& value, std::string& remain, int flags = noFlags);
    static int GetReg(Token reg);
    static void SetReg(Token reg, int value);

    operator bool() const { return !nodes.empty(); }
    int Eval() const;
    std::optional<std::variant<Token, int>> TokenValue(TokenType type) const;

    static const Expr Counter;
    static int count;

    std::string str;
    std::vector<Node> nodes;

protected:
    struct TokenEntry
    {
        std::string str;
        Expr::Token token;
    };

    static const std::vector<std::vector<TokenEntry>> binary_op_tokens;
    static const std::vector<TokenEntry> unary_op_tokens;
    static const std::vector<TokenEntry> reg_tokens;
    static const std::vector<TokenEntry> var_tokens;

    static int Eval(const std::vector<Node>& nodes);
    static bool FindToken(std::string str_token, const std::vector<TokenEntry>& var_tokens, TokenEntry& token);

    bool Term(const char*& p, int flags, int level = 0);
    bool Factor(const char*& p, int flags);
};
