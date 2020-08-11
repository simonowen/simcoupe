// Part of SimCoupe - A SAM Coupe emulator
//
// Disassem.cpp: Z80 disassembler
//
//  Copyright (c) 1999-2015 Simon Owen
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

// Notes:
//  This is a Z80 to C conversion of the disassembler I did for TurboMON!
//
//  Fairly compact but an utter nightmare to debug!

#include "SimCoupe.h"
#include "Disassem.h"

#include "Memory.h"
#include "Options.h"
#include "Symbol.h"

// Bit table indicating which opcodes can have a DD/FD index prefix
uint8_t abIndexableOpcodes[] =
{
    0x08, 0x8A, 0x0A, 0x8A, 0x3E, 0xBE, 0x3E, 0x08, 0x08, 0x8B, 0x0A, 0x4A, 0x3E, 0x3E, 0x3E, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x3E, 0x3E, 0x36, 0x08, 0x00, 0x87, 0x00, 0x00, 0x3C, 0x3C, 0x3C, 0x00
};

static const char* const szUnused = "*[q* PREFIX*]";

static const char* const szNormal =
"\xb3[\x87[\xa9[\xa1[\x99[NOP|EX AF,AF']|\x99[DJNZ|JR] %e\2]|JR \x9bg,%e\2]|"
"\x99[j\xa3" "d,%a\3|ADD q,\xa3" "d]|j\xa9[\x99[o,A|A,o]|\x9b[p,q|q,p|p,A|A,p]\3]|"
"\x99" "b\xa3" "d|!\x81" "b\x9fs%h[|\5]|j%h[\x9fs,n\2]i,%m\6|"
"\xa9[R\x99[L|R]\xa1[C]A|\x9b[DAA|CPL|SCF|CCF]]]\1|%h[j%l[\x9fs|"
"\x9fr],%l[\x87s\1|\x87r]|%l[j\x9fi,\x87r|HALT\1]]\5|a\x87s%l[\1|\5]|"
"\x87[RET f|\x99[POP k|\xa3[RET|EXX|JP (q)|jSP,q]]|JP f,%a\3|"
"\x9f[JP %a\3||OUT (%p),A\2|IN A,(%p)\2|EX (SP),q|EX DE,HL|DI|EI]|CALL f,%a\3|"
"\x99[PUSH k|CALL %a\3]|a\xa9[n\2]\x9b[|||n\2]%b\2|RST %f]\1]";

static const char* const szEDprefix =
"\xb3[|\x87[!\x81[IN|OUT] [%h[\x9fr|X],](%q)\x81[|,%h[\x9fr|%o]]\2|"
"\x99[SB|AD]C q,\xa3" "d|j\x99[p,]\xa3" "d\x99[|,p]\4|NEG|RET\x99[N|I]|IM \x9b[0|0**|1|2]|"
"\xa9[j\xa1[|A,]\x99[I|R]\xa1[,A]|\xa1[R\x99[R|L]D|NOP]]]\2|"
"\xa9[|\x91[\x83[LD|CP|IN|\xa1[OUT\x99[I|D]\2|OT]]\x99[I|D]\xa1[|R]\2]]]NOP\2";

static const char* const szCBprefix =
"%l[%i[e \xb3" "c\x87r\2]\xb3[|e %c,i\3]j\x87r,e* ci\3|e \xb3" "c\x87r%i[\2]]\3";

static const char* aszStrings[] =
{
    "\x9f[ADD|ADC|SUB|SBC|AND|XOR|OR|CP] [A,|A,||A,]",  "[IN|DE]C ", "[|!!%c,]",
    "[BC|DE|q|SP]", "\xb3[\x9f[RLC|RRC|RL* |RR* |SLA|SRA|SLL|SRL]|BIT|RES|SET]",
    "\x9fg", "[NZ|Z|NC|C|PO|PE|P|M]", "%i[H\0]q*h", "(q%i[)\0]%d)", "LD ",
    "\xa3[BC|DE|q|AF]", "%i[L\0]q*l", "", "%n", "\xa1([BC|DE])", "(%a)", "%i[HL|IX|IY]",
    "[B|C|D|E|H|L|i|A]", "[B|C|D|E|h|l|i|A]"
};


static uint16_t wPC = 0;
static char szOutput[64], * pszOut;
static uint8_t bOpcode, * pbOpcode;
static uint8_t abStack[10], * pbStack;

int nType = 0;

bool fHex = true, fLowerCase = false;


// Skip the rest of the [ ] block, including any nested blocks
static void SkipBlock(const char** ppcszTable)
{
    while (1)
    {
        switch (*(*ppcszTable)++)
        {
        case ']':   return;
        case '[':   SkipBlock(ppcszTable);
        }
    }
}

// Lower-case 'a'-'n' are function numbers
static void Function(uint8_t b_, int nSymbolMax_)
{
    int i;
    uint8_t bOp0 = pbOpcode[0], bOp1 = pbOpcode[1], bOp2 = pbOpcode[2];
    bool fPositive = !(bOp1 & 0x80);
    uint8_t bDisp = fPositive ? bOp1 : (0 - bOp1);

    switch (b_)
    {
    case 'a':
    {
        uint16_t wAddr = (bOp2 << 8) | bOp1;

        std::string sName = nSymbolMax_ ? Symbol::LookupAddr(wAddr, nSymbolMax_, true) : "";

        if (!sName.length())
            pszOut += sprintf(pszOut, fHex ? "%04X" : "%d", wAddr);
        else
            pszOut += sprintf(pszOut, "\ab%s\aX", sName.c_str());

        break;
    }
    case 'b':   *pszOut++ = '%'; for (i = 0; i < 8; i++) *pszOut++ = '0' + ((bOp1 >> (7 - i)) & 1); break;
    case 'c':   *pszOut++ = '0' + ((bOp0 >> 3) & 7); break;
    case 'd':   if (bOp1) pszOut += sprintf(pszOut, fHex ? "%c%02X" : "%c%d", fPositive ? '+' : '-', bDisp); break;
    case 'e':
    {
        uint16_t wAddr = wPC + 2 + static_cast<signed char>(bOp1);

        std::string sName = nSymbolMax_ ? Symbol::LookupAddr(wAddr, nSymbolMax_, true) : "";

        if (!sName.length())
            pszOut += sprintf(pszOut, fHex ? "%04X" : "%d", wAddr);
        else
            pszOut += sprintf(pszOut, "\ab%s\aX", sName.c_str());

        break;
    }
    case 'f':   pszOut += sprintf(pszOut, fHex ? "%X" : "%d", (bOp0 & 0x38)); break;
    case 'h':   *pbStack = ((bOp0 >> 3) & 7) == 6; break;
    case 'i':   *pbStack = nType; break;
    case 'l':   *pbStack = (bOp0 & 7) == 6; break;
    case 'm':   pszOut += sprintf(pszOut, fHex ? "%02X" : "%d", pbOpcode[1 + (!nType ? 0 : 1)]); break;
    case 'n':   pszOut += sprintf(pszOut, fHex ? "%02X" : "%d", bOp1); break;
    case 'o':   pszOut += sprintf(pszOut, fHex ? "%02X" : "%d", GetOption(cmosz80) ? 255 : 0); break;
    case 'p':
    {
        uint8_t bPort = bOp1;
        bool fRead = (bOp0 == 0xdb); // IN A,(n)

        std::string sName = nSymbolMax_ ? Symbol::LookupPort(bPort, fRead).substr(0, nSymbolMax_) : "";

        if (!sName.length())
            pszOut += sprintf(pszOut, fHex ? "%02X" : "%d", bPort);
        else
            pszOut += sprintf(pszOut, "\am%s\aX", sName.c_str());

        break;
    }
    case 'q':
    {
        if (nSymbolMax_)
            pszOut += sprintf(pszOut, "\am%c\aX", 'C');
        else
            *pszOut++ = 'C';

        break;
    }
    }
}

static unsigned int ParseStr(const char* pcsz_, int nSymbolMax_)
{
    while (1)
    {
        uint8_t b = *pcsz_++;

        switch (b)
        {
        case '[':
        {
            int nPos = *pbStack++;
            while (nPos)
            {
                switch (*pcsz_++)
                {
                case '[':   SkipBlock(&pcsz_);  break;
                case ']':   nPos = 0;           break;
                case '!':
                case '|':   nPos--;             break;
                }
            }
            pbStack--;

            break;
        }

        case ']':
        case '!':   continue;
        case '|':   SkipBlock(&pcsz_);      break;
        case '*':   *pszOut++ = *pcsz_++;   break;
        case '%':   Function(*pcsz_++, nSymbolMax_); break;
        case ' ':   pszOut = szOutput + 5;    break;

        default:
            if (b >= 0x80)
                *pbStack = (pbOpcode[0] >> ((b >> 3) & 7))& (b & 7);
            else if (b >= 'a' && b < 'a' + std::size(aszStrings))
                ParseStr(aszStrings[b - 'a'], nSymbolMax_);
            else if (b < 5)
                return nType ? b + 1 : b;
            else if (b <= 6)
                return nType ? b - 2 : b - 4;
            else if (isalpha(b))
                *pszOut++ = fLowerCase ? (b | 0x20) : (b & ~0x20);
            else
                *pszOut++ = b;
        }
    }

    return 0;
}

unsigned int Disassemble(uint8_t* pb_, uint16_t wPC_/*=0*/, char* psz_/*=nullptr*/, size_t cbSize_/*=0*/, int nSymbolMax_/*=0*/)
{
    uint8_t abOpcode[MAX_Z80_INSTR_LEN];

    // Initialise our working variables
    memset(pszOut = szOutput, ' ', sizeof(szOutput));
    memcpy(pbOpcode = abOpcode, pb_, MAX_Z80_INSTR_LEN);
    *(pbStack = abStack) = 0;
    wPC = wPC_;

    // Check for and skip any index prefix
    switch (*pbOpcode)
    {
    case 0xdd:  nType = 1;  pbOpcode++;     break;
    case 0xfd:  nType = 2;  pbOpcode++;     break;
    default:    nType = 0;                  break;
    }

    // Check for the a prefix for the two extended sets
    const char* pcszTable = szNormal;
    switch (bOpcode = *pbOpcode)
    {
    case 0xed:  pcszTable = szEDprefix; pbOpcode++;     break;
    case 0xcb:  pcszTable = szCBprefix; pbOpcode++;     break;
    }

    // If we have an index prefix, make sure the opcode following it is affected by it
    if (nType && !(abIndexableOpcodes[bOpcode & 31] & (1 << ((bOpcode >> 5) & 7))))
        pcszTable = szUnused;

    // DD/FD CB instructions are a bit odd, as the main opcode byte comes after the offset
    // If we move it back before the offset it'll fit the normal processing model
    if (nType && pcszTable == szCBprefix)
    {
        pbOpcode--;
        pbOpcode[0] = pbOpcode[2];
    }

    // Parse the instruction, terminate and copy the output to the supplied buffer
    auto uLength = ParseStr(pcszTable, nSymbolMax_);
    *pszOut = '\0';

    // Copy the output to the supplied buffer (if any), taking care not to overflow it
    if (psz_)
    {
        size_t uOutput = pszOut - szOutput + 1U;
        size_t uLen = std::min(cbSize_ - 1, uOutput);
        strncpy(psz_, szOutput, uLen)[uLen] = '\0';
    }

    // Return the instruction length
    return uLength;
}
