// Part of SimCoupe - A SAM Coupe emulator
//
// Z80ops.h: Z80 instruction set emulation
//
//  Copyright (c) 1994 Ian Collier
//  Copyright (c) 1999-2003 by Dave Laundon
//  Copyright (c) 1999-2015 by Simon Owen
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

// Changes 1996-1998 by Allan Skillman
//  - rounded instruction timings up to multiple of 4 tstates
//  - added delayed EI
//  - added inline i386 asm optimisations

// Changes 2000-2001 by Dave Laundon
//  - replaced all instruction timings with raw memory and I/O timings

// Changes 1999-2001 by Simon Owen
//  - added pHlIxIy pointer to help with HL and IX/IY instructions
//  - removed non-portable asm optimisations


// Disable the 'conversion from x to y, possible loss of data' warning until they're corrected
#ifdef _WINDOWS
#pragma warning(disable:4244)
#endif

////////////////////////////////////////////////////////////////////////////////
//  H E L P E R   M A C R O S

// Basic instruction header, specifying opcode and nominal T-States of the first M-Cycle
// The first three T-States of the first M-Cycle are already accounted for
#define instr(m1states, opcode) case opcode: { \
                                    g_dwCycleCounter += m1states - 3;
#define endinstr                } break

// Indirect HL instructions affected by IX/IY prefixes
#define HLinstr(opcode)         instr(4, opcode) \
                                    uint16_t addr; \
                                    if (pHlIxIy == &REG_HL) \
                                        addr = REG_HL; \
                                    else { \
                                        addr = *pHlIxIy + (signed char)timed_read_code_byte(REG_PC++); \
                                        g_dwCycleCounter += 5; \
                                    }

#define cy              (REG_F & FLAG_C)

#define xh              (((REGPAIR*)pHlIxIy)->b.h)
#define xl              (((REGPAIR*)pHlIxIy)->b.l)

// ld (nn),r
#define ld_pnn_r(x)     ( timed_write_byte(timed_read_code_word(REG_PC), x), REG_PC += 2 )

// ld r,(nn)
#define ld_r_pnn(x)     ( x = timed_read_byte(timed_read_code_word(REG_PC)), REG_PC += 2 )

// ld (nn),rr
#define ld_pnn_rr(x)    ( timed_write_word(timed_read_code_word(REG_PC), x), REG_PC += 2 )

// ld rr,(nn)
#define ld_rr_pnn(x)    ( x = timed_read_word(timed_read_code_word(REG_PC)), REG_PC += 2 )

// 8-bit increment and decrement
#ifdef USE_FLAG_TABLES
#define inc(var)        ( var++, REG_F = cy | g_abInc[var] )
#define dec(var)        ( var--, REG_F = cy | g_abDec[var] )
#else
#define inc(var)        ( var++, REG_F = cy | (var & 0xa8) | ((!var) << 6) | ((!( var & 0xf)) << 4) | ((var == 0x80) << 2) )
#define dec(var)        ( var--, REG_F = cy | (var & 0xa8) | ((!var) << 6) | ((!(~var & 0xf)) << 4) | ((var == 0x7f) << 2) | FLAG_N )
#endif

// 16-bit push and pop
#define push(val)   ( REG_SP -= 2, timed_write_word_reversed(REG_SP,val) )
#define pop(var)    ( var = timed_read_word(REG_SP), REG_SP += 2 )

// 16-bit add
#define add_hl(x)       do { \
                            g_dwCycleCounter += 7; \
                            uint16_t z = (x); \
                            uint32_t y = *pHlIxIy + z; \
                            REG_F = (REG_F & 0xc4) |                                        /* S, Z, V    */ \
                                (((y & 0x3800) ^ ((*pHlIxIy ^ z) & 0x1000)) >> 8) | /* 5, H, 3    */ \
                                ((y >> 16) & 0x01);                                 /* C          */ \
                            *pHlIxIy = (y & 0xffff); \
                        } while (0)

// 8-bit add
#define add_a(x)        add_a1((x),0)
#define adc_a(x)        add_a1((x),cy)
#define add_a1(x,carry) do { \
                            uint8_t z = (x); \
                            uint16_t y = REG_A + z + (carry); \
                            REG_F = ((y & 0xb8) ^ ((REG_A ^ z) & 0x10)) |                   /* S, 5, H, 3 */ \
                                (y >> 8) |                                          /* C          */ \
                                (((REG_A ^ ~z) & (REG_A ^ y) & 0x80) >> 5);                 /* V          */ \
                            REG_A = (y & 0xff);                                                          \
                            REG_F |= (!REG_A) << 6;                                         /* Z          */ \
                        } while (0)

// 8-bit subtract
#define sub_a(x)        sub_a1((x),0)
#define sbc_a(x)        sub_a1((x),cy)
#define sub_a1(x,carry) do { \
                            uint8_t z = (x); \
                            uint16_t y = REG_A - z - (carry); \
                            REG_F = ((y & 0xb8) ^ ((REG_A ^ z) & 0x10)) |                   /* S, 5, H, 3 */ \
                                ((y >> 8) & 1) |                                    /* C          */ \
                                (((REG_A ^ z) & (REG_A ^ y) & 0x80) >> 5) |                 /* V          */ \
                                2;                                                  /* N          */ \
                            REG_A = (y & 0xff);                                                          \
                            REG_F |= (!REG_A) << 6;                                         /* Z          */ \
                        } while (0)

// 8-bit compare
// Undocumented flags added by Ian Collier
#define cp_a(x)          do { \
                            uint8_t z = (x); \
                            uint16_t y = REG_A - z; \
                            REG_F = ((y & 0x90) ^ ((REG_A ^ z) & 0x10)) |                   /* S, H       */ \
                                (z & 0x28) |                                        /* 5, 3       */ \
                                ((y >> 8) & 1) |                                    /* C          */ \
                                (((REG_A ^ z) & (REG_A ^ y) & 0x80) >> 5) |                 /* V          */ \
                                2 |                                                 /* N          */ \
                                ((!y) << 6);                                        /* Z          */ \
                        } while (0)

// logical and
#define and_a(x)        ( REG_A &= (x), REG_F = FLAG_H | parity(REG_A) )

// logical xor
#define xor_a(x)        ( REG_A ^= (x), REG_F = parity(REG_A) )

// logical or
#define or_a(x)         ( REG_A |= (x), REG_F = parity(REG_A) )

// Jump relative
#define jr(cc)          do { \
                            if (cc) { \
                                int j = (signed char)timed_read_code_byte(REG_PC++); \
                                REG_PC += j; \
                                g_dwCycleCounter += 5; \
                            } \
                            else { \
                                MEM_ACCESS(REG_PC); \
                                REG_PC++; \
                            } \
                        } while (0)

// Jump absolute
#define jp(cc)          do { \
                            if (cc) \
                                REG_PC = timed_read_code_word(REG_PC); \
                            else { \
                                MEM_ACCESS(REG_PC); \
                                MEM_ACCESS(REG_PC + 1); \
                                REG_PC += 2; \
                            } \
                        } while (0)

// Call
#define call(cc)        do { \
                            if (cc) { \
                                auto npc = timed_read_code_word(REG_PC); \
                                g_dwCycleCounter++; \
                                push(REG_PC+2); \
                                REG_PC = npc; \
                            } \
                            else { \
                                MEM_ACCESS(REG_PC); \
                                MEM_ACCESS(REG_PC + 1); \
                                REG_PC += 2; \
                            } \
                        } while (0)

// Return
#define ret(cc)         do { if (cc) { Debug::OnRet(); pop(REG_PC); } } while (0)
#define retn            do { REG_IFF1 = REG_IFF2; ret(true); } while (0)


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


instr(4, 0000)                                                       endinstr;   // nop
instr(4, 0010)   std::swap(REG_AF, REG_AF_);                         endinstr;   // ex af,af'
instr(5, 0020)   --REG_B; jr(REG_B);                                 endinstr;   // djnz e
instr(4, 0030)   jr(true);                                           endinstr;   // jr e
instr(4, 0040)   jr(!(REG_F& FLAG_Z));                               endinstr;   // jr nz,e
instr(4, 0050)   jr(REG_F& FLAG_Z);                                  endinstr;   // jr z,e
instr(4, 0060)   jr(!cy);                                            endinstr;   // jr nc,e
instr(4, 0070)   jr(cy);                                             endinstr;   // jr c,e


instr(4, 0001)   REG_BC = timed_read_code_word(REG_PC); REG_PC += 2; endinstr;   // ld bc,nn
instr(4, 0011)   add_hl(REG_BC);                                     endinstr;   // add hl/ix/iy,bc
instr(4, 0021)   REG_DE = timed_read_code_word(REG_PC); REG_PC += 2; endinstr;   // ld de,nn
instr(4, 0031)   add_hl(REG_DE);                                     endinstr;   // add hl/ix/iy,de
instr(4, 0041)* pHlIxIy = timed_read_code_word(REG_PC); REG_PC += 2; endinstr;   // ld hl/ix/iy,nn
instr(4, 0051)   add_hl(*pHlIxIy);                                   endinstr;   // add hl/ix/iy,hl/ix/iy
instr(4, 0061)   REG_SP = timed_read_code_word(REG_PC); REG_PC += 2; endinstr;   // ld sp,nn
instr(4, 0071)   add_hl(REG_SP);                                     endinstr;   // add hl/ix/iy,sp


instr(4, 0002)   timed_write_byte(REG_BC, REG_A);                    endinstr;   // ld (bc),a
instr(4, 0012)   REG_A = timed_read_byte(REG_BC);                    endinstr;   // ld a,(bc)
instr(4, 0022)   timed_write_byte(REG_DE, REG_A);                    endinstr;   // ld (de),a
instr(4, 0032)   REG_A = timed_read_byte(REG_DE);                    endinstr;   // ld a,(de)
instr(4, 0042)   ld_pnn_rr(*pHlIxIy);                                endinstr;   // ld (nn),hl/ix/iy
instr(4, 0052)   ld_rr_pnn(*pHlIxIy);                                endinstr;   // ld hl/ix/iy,(nn)
instr(4, 0062)   ld_pnn_r(REG_A);                                    endinstr;   // ld (nn),a
instr(4, 0072)   ld_r_pnn(REG_A);                                    endinstr;   // ld a,(nn)


instr(6, 0003)   ++REG_BC;                                           endinstr;   // inc bc
instr(6, 0013)   --REG_BC;                                           endinstr;   // dec bc
instr(6, 0023)   ++REG_DE;                                           endinstr;   // inc de
instr(6, 0033)   --REG_DE;                                           endinstr;   // dec de
instr(6, 0043)++* pHlIxIy;                                           endinstr;   // inc hl/ix/iy
instr(6, 0053)--* pHlIxIy;                                           endinstr;   // dec hl/ix/iy
instr(6, 0063)   ++REG_SP;                                           endinstr;   // inc sp
instr(6, 0073)   --REG_SP;                                           endinstr;   // dec sp

instr(4, 0004)   inc(REG_B);                                         endinstr;   // inc b
instr(4, 0014)   inc(REG_C);                                         endinstr;   // inc c
instr(4, 0024)   inc(REG_D);                                         endinstr;   // inc d
instr(4, 0034)   inc(REG_E);                                         endinstr;   // inc e
instr(4, 0044)   inc(xh);                                            endinstr;   // inc h/ixh/iyh
instr(4, 0054)   inc(xl);                                            endinstr;   // inc l/ixl/iyl
HLinstr(0064)
    auto t = timed_read_byte(addr);
    inc(t); g_dwCycleCounter++;
    timed_write_byte(addr, t);                                       endinstr;   // inc (hl/ix+d/iy+d)
instr(4, 0074)   inc(REG_A);                                         endinstr;   // inc a


instr(4, 0005)   dec(REG_B);                                         endinstr;   // dec b
instr(4, 0015)   dec(REG_C);                                         endinstr;   // dec c
instr(4, 0025)   dec(REG_D);                                         endinstr;   // dec d
instr(4, 0035)   dec(REG_E);                                         endinstr;   // dec e
instr(4, 0045)   dec(xh);                                            endinstr;   // dec h/ixh/iyh
instr(4, 0055)   dec(xl);                                            endinstr;   // dec l/ixl/iyl
// dec (hl/ix+d/iy+d)
HLinstr(0065)
    auto t = timed_read_byte(addr);
    dec(t);
    g_dwCycleCounter++;
    timed_write_byte(addr, t);
endinstr;
instr(4, 0075)   dec(REG_A);                                         endinstr;   // dec a


instr(4, 0006)   REG_B = timed_read_code_byte(REG_PC++);             endinstr;   // ld b,n
instr(4, 0016)   REG_C = timed_read_code_byte(REG_PC++);             endinstr;   // ld c,n
instr(4, 0026)   REG_D = timed_read_code_byte(REG_PC++);             endinstr;   // ld d,n
instr(4, 0036)   REG_E = timed_read_code_byte(REG_PC++);             endinstr;   // ld e,n
instr(4, 0046)   xh = timed_read_code_byte(REG_PC++);                endinstr;   // ld h/ixh/iyh,n
instr(4, 0056)   xl = timed_read_code_byte(REG_PC++);                endinstr;   // ld l/ixl/iyl,n
// ld (hl/ix+d/iy+d),n
HLinstr(0066)
    timed_write_byte(addr, timed_read_code_byte(REG_PC++));
endinstr;
instr(4, 0076)   REG_A = timed_read_code_byte(REG_PC++);             endinstr;   // ld a,n


// rlca
instr(4, 0007)
    REG_A = (REG_A << 1) | (REG_A >> 7);
    REG_F = (REG_F & 0xc4) | (REG_A & 0x29);
endinstr;

// rrca
instr(4, 0017)
    REG_F = (REG_F & 0xc4) | (REG_A & 1);
    REG_A = (REG_A >> 1) | (REG_A << 7);
    REG_F |= REG_A & 0x28;
endinstr;

// rla
instr(4, 0027)
    int t = REG_A >> 7;
    REG_A = (REG_A << 1) | cy;
    REG_F = (REG_F & 0xc4) | (REG_A & 0x28) | t;
endinstr;

// rra
instr(4, 0037)
    int t = REG_A & FLAG_C;
    REG_A = (REG_A >> 1) | (REG_F << 7);
    REG_F = (REG_F & 0xc4) | (REG_A & 0x28) | t;
endinstr;

// daa
instr(4, 0047)
    uint16_t acc = REG_A;
    uint8_t carry = cy, incr = 0;

    if ((REG_F & FLAG_H) || (REG_A & 0x0f) > 9)
    incr = 6;

    if (REG_F & FLAG_N)
    {
        int hd = carry || REG_A > 0x99;

        if (incr)
        {
            acc = (acc - incr) & 0xff;

            if ((REG_A & 0x0f) > 5)
                REG_F &= ~FLAG_H;
        }

        if (hd)
            acc -= 0x160;
    }
    else
    {
        if (incr)
        {
            REG_F = (REG_F & ~FLAG_H) | (((REG_A & 0x0f) > 9) ? FLAG_H : 0);
            acc += incr;
        }

        if (carry || ((acc & 0x1f0) > 0x90))
            acc += 0x60;
    }

    REG_A = (uint8_t)acc;
    REG_F = (REG_A & 0xa8) | (!REG_A << 6) | (REG_F & 0x12) | (parity(REG_A) & FLAG_P) | carry | !!(acc & 0x100);
endinstr;

// cpl
instr(4, 0057)
    REG_A = ~REG_A;
    REG_F = (REG_F & 0xc5) | (REG_A & 0x28) | FLAG_H | FLAG_N;
endinstr;

// scf
instr(4, 0067)
    REG_F = (REG_F & 0xec) | (REG_A & 0x28) | FLAG_C;
endinstr;

// ccf
instr(4, 0077)
    REG_F = ((REG_F & 0xed) | (cy << 4) | (REG_A & 0x28)) ^ FLAG_C;
endinstr;


instr(4, 0100)                                                       endinstr;   // ld b,b
instr(4, 0110)   REG_C = REG_B;                                      endinstr;   // ld c,b
instr(4, 0120)   REG_D = REG_B;                                      endinstr;   // ld d,b
instr(4, 0130)   REG_E = REG_B;                                      endinstr;   // ld e,b
instr(4, 0140)   xh = REG_B;                                         endinstr;   // ld h/ixh/iyh,b
instr(4, 0150)   xl = REG_B;                                         endinstr;   // ld l/ixl/iyl,b
HLinstr(0160)   timed_write_byte(addr, REG_B);                       endinstr;   // ld (hl/ix+d/iy+d),b
instr(4, 0170)   REG_A = REG_B;                                      endinstr;   // ld a,b


instr(4, 0101)   REG_B = REG_C;                                      endinstr;   // ld b,c
instr(4, 0111)                                                       endinstr;   // ld c,c
instr(4, 0121)   REG_D = REG_C;                                      endinstr;   // ld d,c
instr(4, 0131)   REG_E = REG_C;                                      endinstr;   // ld e,c
instr(4, 0141)   xh = REG_C;                                         endinstr;   // ld h/ixh/iyh,c
instr(4, 0151)   xl = REG_C;                                         endinstr;   // ld l/ixl/iyl,c
HLinstr(0161)   timed_write_byte(addr, REG_C);                       endinstr;   // ld (hl/ix+d/iy+d),c
instr(4, 0171)   REG_A = REG_C;                                      endinstr;   // ld a,c


instr(4, 0102)   REG_B = REG_D;                                      endinstr;   // ld b,d
instr(4, 0112)   REG_C = REG_D;                                      endinstr;   // ld c,d
instr(4, 0122)                                                       endinstr;   // ld d,d
instr(4, 0132)   REG_E = REG_D;                                      endinstr;   // ld e,d
instr(4, 0142)   xh = REG_D;                                         endinstr;   // ld h/ixh/iyh,d
instr(4, 0152)   xl = REG_D;                                         endinstr;   // ld l/ixl/iyl,d
HLinstr(0162)   timed_write_byte(addr, REG_D);                       endinstr;   // ld (hl/ix+d/iy+d),d
instr(4, 0172)   REG_A = REG_D;                                      endinstr;   // ld a,d


instr(4, 0103)   REG_B = REG_E;                                      endinstr;   // ld b,e
instr(4, 0113)   REG_C = REG_E;                                      endinstr;   // ld c,e
instr(4, 0123)   REG_D = REG_E;                                      endinstr;   // ld d,e
instr(4, 0133)                                                       endinstr;   // ld e,e
instr(4, 0143)   xh = REG_E;                                         endinstr;   // ld h/ixh/iyh,e
instr(4, 0153)   xl = REG_E;                                         endinstr;   // ld l/ixl/iyl,e
HLinstr(0163)   timed_write_byte(addr, REG_E);                       endinstr;   // ld (hl/ix+d/iy+d),e
instr(4, 0173)   REG_A = REG_E;                                      endinstr;   // ld a,e


instr(4, 0104)   REG_B = xh;                                         endinstr;   // ld b,h/ixh/iyh
instr(4, 0114)   REG_C = xh;                                         endinstr;   // ld c,h/ixh/iyh
instr(4, 0124)   REG_D = xh;                                         endinstr;   // ld d,h/ixh/iyh
instr(4, 0134)   REG_E = xh;                                         endinstr;   // ld e,h/ixh/iyh
instr(4, 0144)                                                       endinstr;   // ld h/ixh/iyh,h/ixh/iyh
instr(4, 0154)   xl = xh;                                            endinstr;   // ld l/ixh/iyh,h/ixh/iyh
HLinstr(0164)   timed_write_byte(addr, REG_H);                       endinstr;   // ld (hl/ix+d/iy+d),h
instr(4, 0174)   REG_A = xh;                                         endinstr;   // ld a,h/ixh/iyh


instr(4, 0105)   REG_B = xl;                                         endinstr;   // ld b,l/ixl/iyl
instr(4, 0115)   REG_C = xl;                                         endinstr;   // ld c,l/ixl/iyl
instr(4, 0125)   REG_D = xl;                                         endinstr;   // ld d,l/ixl/iyl
instr(4, 0135)   REG_E = xl;                                         endinstr;   // ld e,l/ixl/iyl
instr(4, 0145)   xh = xl;                                            endinstr;   // ld h/ixh/iyh,l/ixl/iyl
instr(4, 0155)                                                       endinstr;   // ld l/ixl/iyl,l/ixl/iyl
HLinstr(0165)   timed_write_byte(addr, REG_L);                       endinstr;   // ld (hl/ix+d/iy+d),l
instr(4, 0175)   REG_A = xl;                                         endinstr;   // ld a,l/ixl/iyl


HLinstr(0106)   REG_B = timed_read_byte(addr);                       endinstr;   // ld b,(hl/ix+d/iy+d)
HLinstr(0116)   REG_C = timed_read_byte(addr);                       endinstr;   // ld c,(hl/ix+d/iy+d)
HLinstr(0126)   REG_D = timed_read_byte(addr);                       endinstr;   // ld d,(hl/ix+d/iy+d)
HLinstr(0136)   REG_E = timed_read_byte(addr);                       endinstr;   // ld e,(hl/ix+d/iy+d)
HLinstr(0146)   REG_H = timed_read_byte(addr);                       endinstr;   // ld h,(hl/ix+d/iy+d)
HLinstr(0156)   REG_L = timed_read_byte(addr);                       endinstr;   // ld l,(hl/ix+d/iy+d)

instr(4, 0166)   REG_PC--;                                           endinstr;   // halt

HLinstr(0176)   REG_A = timed_read_byte(addr);                       endinstr;   // ld a,(hl/ix+d/iy+d)


instr(4, 0107)   REG_B = REG_A;                                      endinstr;   // ld b,a
instr(4, 0117)   REG_C = REG_A;                                      endinstr;   // ld c,a
instr(4, 0127)   REG_D = REG_A;                                      endinstr;   // ld d,a
instr(4, 0137)   REG_E = REG_A;                                      endinstr;   // ld e,a
instr(4, 0147)   xh = REG_A;                                         endinstr;   // ld h/ixh/iyh,a
instr(4, 0157)   xl = REG_A;                                         endinstr;   // ld l/ixl/iyl,a
HLinstr(0167)   timed_write_byte(addr, REG_A);                       endinstr;   // ld (hl/ix+d/iy+d),a
instr(4, 0177)                                                       endinstr;   // ld a,a


instr(4, 0200)   add_a(REG_B);                                       endinstr;   // add a,b
instr(4, 0210)   adc_a(REG_B);                                       endinstr;   // adc a,b
instr(4, 0220)   sub_a(REG_B);                                       endinstr;   // sub b
instr(4, 0230)   sbc_a(REG_B);                                       endinstr;   // sbc a,b
instr(4, 0240)   and_a(REG_B);                                       endinstr;   // and b
instr(4, 0250)   xor_a(REG_B);                                       endinstr;   // xor b
instr(4, 0260)   or_a(REG_B);                                        endinstr;   // or b
instr(4, 0270)   cp_a(REG_B);                                        endinstr;   // cp_a b


instr(4, 0201)   add_a(REG_C);                                       endinstr;   // add a,c
instr(4, 0211)   adc_a(REG_C);                                       endinstr;   // adc a,c
instr(4, 0221)   sub_a(REG_C);                                       endinstr;   // sub c
instr(4, 0231)   sbc_a(REG_C);                                       endinstr;   // sbc a,c
instr(4, 0241)   and_a(REG_C);                                       endinstr;   // and c
instr(4, 0251)   xor_a(REG_C);                                       endinstr;   // xor c
instr(4, 0261)   or_a(REG_C);                                        endinstr;   // or c
instr(4, 0271)   cp_a(REG_C);                                        endinstr;   // cp_a c


instr(4, 0202)   add_a(REG_D);                                       endinstr;   // add a,d
instr(4, 0212)   adc_a(REG_D);                                       endinstr;   // adc a,d
instr(4, 0222)   sub_a(REG_D);                                       endinstr;   // sub d
instr(4, 0232)   sbc_a(REG_D);                                       endinstr;   // sbc a,d
instr(4, 0242)   and_a(REG_D);                                       endinstr;   // and d
instr(4, 0252)   xor_a(REG_D);                                       endinstr;   // xor d
instr(4, 0262)   or_a(REG_D);                                        endinstr;   // or d
instr(4, 0272)   cp_a(REG_D);                                        endinstr;   // cp_a d


instr(4, 0203)   add_a(REG_E);                                       endinstr;   // add a,e
instr(4, 0213)   adc_a(REG_E);                                       endinstr;   // adc a,e
instr(4, 0223)   sub_a(REG_E);                                       endinstr;   // sub e
instr(4, 0233)   sbc_a(REG_E);                                       endinstr;   // sbc a,e
instr(4, 0243)   and_a(REG_E);                                       endinstr;   // and e
instr(4, 0253)   xor_a(REG_E);                                       endinstr;   // xor e
instr(4, 0263)   or_a(REG_E);                                        endinstr;   // or e
instr(4, 0273)   cp_a(REG_E);                                        endinstr;   // cp_a e


instr(4, 0204)   add_a(xh);                                          endinstr;   // add a,h/ixh/iyh
instr(4, 0214)   adc_a(xh);                                          endinstr;   // adc a,h/ixh/iyh
instr(4, 0224)   sub_a(xh);                                          endinstr;   // sub h/ixh/iyh
instr(4, 0234)   sbc_a(xh);                                          endinstr;   // sbc a,h/ixh/iyh
instr(4, 0244)   and_a(xh);                                          endinstr;   // and h/ixh/iyh
instr(4, 0254)   xor_a(xh);                                          endinstr;   // xor h/ixh/iyh
instr(4, 0264)   or_a(xh);                                           endinstr;   // or h/ixh/iyh
instr(4, 0274)   cp_a(xh);                                           endinstr;   // cp_a h/ixh/iyh


instr(4, 0205)   add_a(xl);                                          endinstr;   // add a,l/ixl/iyl
instr(4, 0215)   adc_a(xl);                                          endinstr;   // adc a,l/ixl/iyl
instr(4, 0225)   sub_a(xl);                                          endinstr;   // sub l/ixl/iyl
instr(4, 0235)   sbc_a(xl);                                          endinstr;   // sbc a,l/ixl/iyl
instr(4, 0245)   and_a(xl);                                          endinstr;   // and l/ixl/iyl
instr(4, 0255)   xor_a(xl);                                          endinstr;   // xor l/ixl/iyl
instr(4, 0265)   or_a(xl);                                           endinstr;   // or l/ixl/iyl
instr(4, 0275)   cp_a(xl);                                           endinstr;   // cp_a l/ixl/iyl


HLinstr(0206)   add_a(timed_read_byte(addr));                        endinstr;   // add a,(hl/ix+d/iy+d)
HLinstr(0216)   adc_a(timed_read_byte(addr));                        endinstr;   // adc a,(hl/ix+d/iy+d)
HLinstr(0226)   sub_a(timed_read_byte(addr));                        endinstr;   // sub (hl/ix+d/iy+d)
HLinstr(0236)   sbc_a(timed_read_byte(addr));                        endinstr;   // sbc a,(hl/ix+d/iy+d)
HLinstr(0246)   and_a(timed_read_byte(addr));                        endinstr;   // and (hl/ix+d/iy+d)
HLinstr(0256)   xor_a(timed_read_byte(addr));                        endinstr;   // xor (hl/ix+d/iy+d)
HLinstr(0266)   or_a(timed_read_byte(addr));                         endinstr;   // or (hl/ix+d/iy+d)
HLinstr(0276)   cp_a(timed_read_byte(addr));                         endinstr;   // cp (hl/ix+d/iy+d)


instr(4, 0207)   add_a(REG_A);                                       endinstr;   // add a,a
instr(4, 0217)   adc_a(REG_A);                                       endinstr;   // adc a,a
instr(4, 0227)   sub_a(REG_A);                                       endinstr;   // sub a
instr(4, 0237)   sbc_a(REG_A);                                       endinstr;   // sbc a,a
instr(4, 0247)   and_a(REG_A);                                       endinstr;   // and a
instr(4, 0257)   xor_a(REG_A);                                       endinstr;   // xor a
instr(4, 0267)   or_a(REG_A);                                        endinstr;   // or a
instr(4, 0277)   cp_a(REG_A);                                        endinstr;   // cp_a a


instr(5, 0300)   ret(!(REG_F & FLAG_Z));                             endinstr;   // ret nz

// ret z
instr(5, 0310)
    if (Tape::RetZHook() || Debug::RetZHook())
        break;
    ret(REG_F & FLAG_Z);
endinstr;

instr(5, 0320)   ret(!cy);                                           endinstr;   // ret nc
instr(5, 0330)   ret(cy);                                            endinstr;   // ret c
instr(5, 0340)   ret(!(REG_F & FLAG_P));                             endinstr;   // ret po
instr(5, 0350)   ret(REG_F & FLAG_P);                                endinstr;   // ret pe
instr(5, 0360)   ret(!(REG_F & FLAG_S));                             endinstr;   // ret p
instr(5, 0370)   ret(REG_F & FLAG_S);                                endinstr;   // ret m

instr(4, 0311)   ret(true);                                          endinstr;   // ret


instr(4, 0302)   jp(!(REG_F & FLAG_Z));                              endinstr;   // jp nz,nn
instr(4, 0312)   jp(REG_F & FLAG_Z);                                 endinstr;   // jp z,nn
instr(4, 0322)   jp(!cy);                                            endinstr;   // jp nc,nn
instr(4, 0332)   jp(cy);                                             endinstr;   // jp c,nn
instr(4, 0342)   jp(!(REG_F & FLAG_P));                              endinstr;   // jp po,nn
instr(4, 0352)   jp(REG_F & FLAG_P);                                 endinstr;   // jp pe,nn
instr(4, 0362)   jp(!(REG_F & FLAG_S));                              endinstr;   // jp p,nn
instr(4, 0372)   jp(REG_F & FLAG_S);                                 endinstr;   // jp m,nn

instr(4, 0303)   jp(true);                                           endinstr;   // jp nn


instr(4, 0304)   call(!(REG_F & FLAG_Z));                            endinstr;   // call nz,pq
instr(4, 0314)   call(REG_F & FLAG_Z);                               endinstr;   // call z,nn
instr(4, 0324)   call(!cy);                                          endinstr;   // call nc,nn
instr(4, 0334)   call(cy);                                           endinstr;   // call c,nn
instr(4, 0344)   call(!(REG_F & FLAG_P));                            endinstr;   // call po
instr(4, 0354)   call(REG_F & FLAG_P);                               endinstr;   // call pe,nn
instr(4, 0364)   call(!(REG_F & FLAG_S));                            endinstr;   // call p,nn
instr(4, 0374)   call(REG_F & FLAG_S);                               endinstr;   // call m,nn

instr(4, 0315)   call(true);                                         endinstr;   // call nn


instr(4, 0306)   add_a(timed_read_code_byte(REG_PC++));              endinstr;   // add a,n
instr(4, 0316)   adc_a(timed_read_code_byte(REG_PC++));              endinstr;   // adc a,n
instr(4, 0326)   sub_a(timed_read_code_byte(REG_PC++));              endinstr;   // sub n
instr(4, 0336)   sbc_a(timed_read_code_byte(REG_PC++));              endinstr;   // sbc a,n
instr(4, 0346)   and_a(timed_read_code_byte(REG_PC++));              endinstr;   // and n
instr(4, 0356)   xor_a(timed_read_code_byte(REG_PC++));              endinstr;   // xor n
instr(4, 0366)   or_a(timed_read_code_byte(REG_PC++));               endinstr;   // or n
instr(4, 0376)   cp_a(timed_read_code_byte(REG_PC++));               endinstr;   // cp n


instr(4, 0301)   pop(REG_BC);                                        endinstr;   // pop bc
instr(4, 0321)   pop(REG_DE);                                        endinstr;   // pop de
instr(4, 0341)   pop(*pHlIxIy);                                      endinstr;   // pop hl/ix/iy
instr(4, 0361)   pop(REG_AF);                                        endinstr;   // pop af

instr(4, 0351)   REG_PC = *pHlIxIy;                                  endinstr;   // jp (hl/ix/iy)
instr(6, 0371)   REG_SP = *pHlIxIy;                                  endinstr;   // ld sp,hl/ix/iy

// exx
instr(4, 0331)
    std::swap(REG_BC, REG_BC_);
    std::swap(REG_DE, REG_DE_);
    std::swap(REG_HL, REG_HL_);
endinstr;   


instr(5, 0305)   push(REG_BC);                                       endinstr;   // push bc
instr(5, 0325)   push(REG_DE);                                       endinstr;   // push de
instr(5, 0345)   push(*pHlIxIy);                                     endinstr;   // push hl
instr(5, 0365)   push(REG_AF);                                       endinstr;   // push af

instr(4, 0335)   pNewHlIxIy = &REG_IX;                               endinstr;   // [ix prefix]
instr(4, 0375)   pNewHlIxIy = &REG_IY;                               endinstr;   // [iy prefix]

// [ed prefix]
instr(4, 0355)
#include "EDops.h"
endinstr;


// [cb prefix]
instr(4, 0313)
#include "CBops.h"
endinstr;

// out (n),a
instr(4, 0323)
    auto bPortLow = timed_read_code_byte(REG_PC++);
    PORT_ACCESS(bPortLow);
    out_byte((REG_A << 8) | bPortLow, REG_A);
endinstr;

// in a,(n)
instr(4, 0333)
    auto bPortLow = timed_read_code_byte(REG_PC++);
    PORT_ACCESS(bPortLow);
    REG_A = in_byte((REG_A << 8) | bPortLow);
endinstr;

// ex (sp),hl
instr(4, 0343)
    auto t = timed_read_word(REG_SP);
    g_dwCycleCounter++;
    timed_write_word_reversed(REG_SP, *pHlIxIy);
    *pHlIxIy = t;
    g_dwCycleCounter += 2;
endinstr;

instr(4, 0363)   REG_IFF1 = REG_IFF2 = 0;                                    endinstr;   // di
instr(4, 0373)   if (IO::EiHook()) break; REG_IFF1 = REG_IFF2 = 1; g_nTurbo &= ~TURBO_BOOT; endinstr;   // ei

instr(4, 0353)   std::swap(REG_DE, REG_HL);                                   endinstr;   // ex de,hl


instr(5, 0307)   push(REG_PC); REG_PC = 000;                                 endinstr;   // rst 0

instr(5, 0317)   if (IO::Rst8Hook()) break; push(REG_PC); REG_PC = 010;      endinstr;   // rst 8
instr(5, 0327)   push(REG_PC); REG_PC = 020;                                 endinstr;   // rst 16
instr(5, 0337)   push(REG_PC); REG_PC = 030;                                 endinstr;   // rst 24
instr(5, 0347)   push(REG_PC); REG_PC = 040;                                 endinstr;   // rst 32
instr(5, 0357)   push(REG_PC); REG_PC = 050;                                 endinstr;   // rst 40
instr(5, 0367)   if (IO::Rst48Hook()) break; push(REG_PC); REG_PC = 060;     endinstr;   // rst 48
instr(5, 0377)   push(REG_PC); REG_PC = 070;                                 endinstr;   // rst 56

#undef instr
#undef endinstr
#undef HLinstr
#undef inc
#undef dec
#undef edinstr
