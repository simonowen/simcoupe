// Part of SimCoupe - A SAM Coupe emulator
//
// Z80ops.h: Z80 instruction set emulation
//
//  Copyright (c) 1994 Ian Collier
//  Copyright (c) 1999-2003 by Dave Laundon
//  Copyright (c) 1999-2014 by Simon Owen
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
                                    WORD addr; \
                                    if (pHlIxIy == &HL) \
                                        addr = HL; \
                                    else { \
                                        addr = *pHlIxIy + (signed char)timed_read_code_byte(PC++); \
                                        g_dwCycleCounter += 5; \
                                    }

#define cy              (F & FLAG_C)

#define xh              (((REGPAIR*)pHlIxIy)->b.h)
#define xl              (((REGPAIR*)pHlIxIy)->b.l)

// ld (nn),r
#define ld_pnn_r(x)     ( timed_write_byte(timed_read_code_word(PC), x), PC += 2 )

// ld r,(nn)
#define ld_r_pnn(x)     ( x = timed_read_byte(timed_read_code_word(PC)), PC += 2 )

// ld (nn),rr
#define ld_pnn_rr(x)    ( timed_write_word(timed_read_code_word(PC), x), PC += 2 )

// ld rr,(nn)
#define ld_rr_pnn(x)    ( x = timed_read_word(timed_read_code_word(PC)), PC += 2 )

// 8-bit increment and decrement
#ifdef USE_FLAG_TABLES
#define inc(var)        ( var++, F = cy | g_abInc[var] )
#define dec(var)        ( var--, F = cy | g_abDec[var] )
#else
#define inc(var)        ( var++, F = cy | (var & 0xa8) | ((!var) << 6) | ((!( var & 0xf)) << 4) | ((var == 0x80) << 2) )
#define dec(var)        ( var--, F = cy | (var & 0xa8) | ((!var) << 6) | ((!(~var & 0xf)) << 4) | ((var == 0x7f) << 2) | FLAG_N )
#endif

// 16-bit push and pop
#define push(val)   ( SP -= 2, timed_write_word_reversed(SP,val) )
#define pop(var)    ( var = timed_read_word(SP), SP += 2 )

// 16-bit add
#define add_hl(x)       do { \
                            g_dwCycleCounter += 7; \
                            WORD z = (x); \
                            DWORD y = *pHlIxIy + z; \
                            F = (F & 0xc4) |                                        /* S, Z, V    */ \
                                (((y & 0x3800) ^ ((*pHlIxIy ^ z) & 0x1000)) >> 8) | /* 5, H, 3    */ \
                                ((y >> 16) & 0x01);                                 /* C          */ \
                            *pHlIxIy = (y & 0xffff); \
                        } while (0)

// 8-bit add
#define add_a(x)        add_a1((x),0)
#define adc_a(x)        add_a1((x),cy)
#define add_a1(x,C)     do { \
                            BYTE z = (x); \
                            WORD y = A + z + (C); \
                            F = ((y & 0xb8) ^ ((A ^ z) & 0x10)) |                   /* S, 5, H, 3 */ \
                                (y >> 8) |                                          /* C          */ \
                                (((A ^ ~z) & (A ^ y) & 0x80) >> 5);                 /* V          */ \
                            A = (y & 0xff);                                                          \
                            F |= (!A) << 6;                                         /* Z          */ \
                        } while (0)

// 8-bit subtract
#define sub_a(x)        sub_a1((x),0)
#define sbc_a(x)        sub_a1((x),cy)
#define sub_a1(x,C)     do { \
                            BYTE z = (x); \
                            WORD y = A - z - (C); \
                            F = ((y & 0xb8) ^ ((A ^ z) & 0x10)) |                   /* S, 5, H, 3 */ \
                                ((y >> 8) & 1) |                                    /* C          */ \
                                (((A ^ z) & (A ^ y) & 0x80) >> 5) |                 /* V          */ \
                                2;                                                  /* N          */ \
                            A = (y & 0xff);                                                          \
                            F |= (!A) << 6;                                         /* Z          */ \
                        } while (0)

// 8-bit compare
// Undocumented flags added by Ian Collier
#define cp_a(x)          do { \
                            BYTE z = (x); \
                            WORD y = A - z; \
                            F = ((y & 0x90) ^ ((A ^ z) & 0x10)) |                   /* S, H       */ \
                                (z & 0x28) |                                        /* 5, 3       */ \
                                ((y >> 8) & 1) |                                    /* C          */ \
                                (((A ^ z) & (A ^ y) & 0x80) >> 5) |                 /* V          */ \
                                2 |                                                 /* N          */ \
                                ((!y) << 6);                                        /* Z          */ \
                        } while (0)

// logical and
#define and_a(x)        ( A &= (x), F = FLAG_H | parity(A) )

// logical xor
#define xor_a(x)        ( A ^= (x), F = parity(A) )

// logical or
#define or_a(x)         ( A |= (x), F = parity(A) )

// Jump relative
#define jr(cc)          do { \
                            if (cc) { \
                                int j = (signed char)timed_read_code_byte(PC++); \
                                PC += j; \
                                g_dwCycleCounter += 5; \
                            } \
                            else { \
                                MEM_ACCESS(PC); \
                                PC++; \
                            } \
                        } while (0)

// Jump absolute
#define jp(cc)          do { \
                            if (cc) \
                                PC = timed_read_code_word(PC); \
                            else { \
                                MEM_ACCESS(PC); \
                                MEM_ACCESS(PC + 1); \
                                PC += 2; \
                            } \
                        } while (0)

// Call
#define call(cc)        do { \
                            if (cc) { \
                                WORD npc = timed_read_code_word(PC); \
                                g_dwCycleCounter++; \
                                push(PC+2); \
                                PC = npc; \
                            } \
                            else { \
                                MEM_ACCESS(PC); \
                                MEM_ACCESS(PC + 1); \
                                PC += 2; \
                            } \
                        } while (0)

// Return
#define ret(cc)         do { if (cc) { Debug::OnRet(); pop(PC); } } while (0)
#define retn            do { IFF1 = IFF2; ret(true); } while (0)


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


instr(4,0000)                                                       endinstr;   // nop
instr(4,0010)   swap(AF,AF_);                                       endinstr;   // ex af,af'
instr(5,0020)   --B; jr(B);                                         endinstr;   // djnz e
instr(4,0030)   jr(true);                                           endinstr;   // jr e
instr(4,0040)   jr(!(F & FLAG_Z));                                  endinstr;   // jr nz,e
instr(4,0050)   jr(F & FLAG_Z);                                     endinstr;   // jr z,e
instr(4,0060)   jr(!cy);                                            endinstr;   // jr nc,e
instr(4,0070)   jr(cy);                                             endinstr;   // jr c,e


instr(4,0001)   BC = timed_read_code_word(PC); PC += 2;             endinstr;   // ld bc,nn
instr(4,0011)   add_hl(BC);                                         endinstr;   // add hl/ix/iy,bc
instr(4,0021)   DE = timed_read_code_word(PC); PC += 2;             endinstr;   // ld de,nn
instr(4,0031)   add_hl(DE);                                         endinstr;   // add hl/ix/iy,de
instr(4,0041)   *pHlIxIy = timed_read_code_word(PC); PC += 2;       endinstr;   // ld hl/ix/iy,nn
instr(4,0051)   add_hl(*pHlIxIy);                                   endinstr;   // add hl/ix/iy,hl/ix/iy
instr(4,0061)   SP = timed_read_code_word(PC); PC += 2;             endinstr;   // ld sp,nn
instr(4,0071)   add_hl(SP);                                         endinstr;   // add hl/ix/iy,sp


instr(4,0002)   timed_write_byte(BC,A);                             endinstr;   // ld (bc),a
instr(4,0012)   A = timed_read_byte(BC);                            endinstr;   // ld a,(bc)
instr(4,0022)   timed_write_byte(DE,A);                             endinstr;   // ld (de),a
instr(4,0032)   A = timed_read_byte(DE);                            endinstr;   // ld a,(de)
instr(4,0042)   ld_pnn_rr(*pHlIxIy);                                endinstr;   // ld (nn),hl/ix/iy
instr(4,0052)   ld_rr_pnn(*pHlIxIy);                                endinstr;   // ld hl/ix/iy,(nn)
instr(4,0062)   ld_pnn_r(A);                                        endinstr;   // ld (nn),a
instr(4,0072)   ld_r_pnn(A);                                        endinstr;   // ld a,(nn)


instr(6,0003)   ++BC;                                               endinstr;   // inc bc
instr(6,0013)   --BC;                                               endinstr;   // dec bc
instr(6,0023)   ++DE;                                               endinstr;   // inc de
instr(6,0033)   --DE;                                               endinstr;   // dec de
instr(6,0043)   ++*pHlIxIy;                                         endinstr;   // inc hl/ix/iy
instr(6,0053)   --*pHlIxIy;                                         endinstr;   // dec hl/ix/iy
instr(6,0063)   ++SP;                                               endinstr;   // inc sp
instr(6,0073)   --SP;                                               endinstr;   // dec sp


instr(4,0004)   inc(B);                                             endinstr;   // inc b
instr(4,0014)   inc(C);                                             endinstr;   // inc c
instr(4,0024)   inc(D);                                             endinstr;   // inc d
instr(4,0034)   inc(E);                                             endinstr;   // inc e
instr(4,0044)   inc(xh);                                            endinstr;   // inc h/ixh/iyh
instr(4,0054)   inc(xl);                                            endinstr;   // inc l/ixl/iyl
HLinstr(0064)   BYTE t = timed_read_byte(addr);
                inc(t); g_dwCycleCounter++;
                timed_write_byte(addr,t);                           endinstr;   // inc (hl/ix+d/iy+d)
instr(4,0074)   inc(A);                                             endinstr;   // inc a


instr(4,0005)   dec(B);                                             endinstr;   // dec b
instr(4,0015)   dec(C);                                             endinstr;   // dec c
instr(4,0025)   dec(D);                                             endinstr;   // dec d
instr(4,0035)   dec(E);                                             endinstr;   // dec e
instr(4,0045)   dec(xh);                                            endinstr;   // dec h/ixh/iyh
instr(4,0055)   dec(xl);                                            endinstr;   // dec l/ixl/iyl
HLinstr(0065)   BYTE t = timed_read_byte(addr);
                dec(t); g_dwCycleCounter++;
                timed_write_byte(addr,t);                           endinstr;   // dec (hl/ix+d/iy+d)
instr(4,0075)   dec(A);                                             endinstr;   // dec a


instr(4,0006)   B = timed_read_code_byte(PC++);                     endinstr;   // ld b,n
instr(4,0016)   C = timed_read_code_byte(PC++);                     endinstr;   // ld c,n
instr(4,0026)   D = timed_read_code_byte(PC++);                     endinstr;   // ld d,n
instr(4,0036)   E = timed_read_code_byte(PC++);                     endinstr;   // ld e,n
instr(4,0046)   xh = timed_read_code_byte(PC++);                    endinstr;   // ld h/ixh/iyh,n
instr(4,0056)   xl = timed_read_code_byte(PC++);                    endinstr;   // ld l/ixl/iyl,n
HLinstr(0066)   timed_write_byte(addr,timed_read_code_byte(PC++));  endinstr;   // ld (hl/ix+d/iy+d),n
instr(4,0076)   A = timed_read_code_byte(PC++);                     endinstr;   // ld a,n


// rlca
instr(4,0007)
    A = (A << 1) | (A >> 7);
    F = (F & 0xc4) | (A & 0x29);
endinstr;

// rrca
instr(4,0017)
    F = (F & 0xc4) | (A & 1);
    A = (A >> 1) | (A << 7);
    F |= A & 0x28;
endinstr;

// rla
instr(4,0027)
    int t = A >> 7;
    A = (A << 1) | cy;
    F = (F & 0xc4) | (A & 0x28) | t;
endinstr;

// rra
instr(4,0037)
    int t = A & FLAG_C;
    A = (A >> 1) | (F << 7);
    F = (F & 0xc4) | (A & 0x28) | t;
endinstr;

// daa
instr(4,0047)
    WORD acc = A;
    BYTE carry = cy, incr = 0;

    if ((F & FLAG_H) || (A & 0x0f) > 9)
        incr = 6;

    if (F & FLAG_N)
    {
        int hd = carry || A > 0x99;

        if (incr)
        {
            acc = (acc - incr) & 0xff;

            if ((A & 0x0f) > 5)
                F &= ~FLAG_H;
        }

        if (hd)
            acc -= 0x160;
    }
    else
    {
        if (incr)
        {
            F = (F & ~FLAG_H) | (((A & 0x0f) > 9) ? FLAG_H : 0);
            acc += incr;
        }

        if (carry || ((acc & 0x1f0) > 0x90))
            acc += 0x60;
    }

    A = (BYTE)acc;
    F = (A & 0xa8) | (!A << 6) | (F & 0x12) | (parity(A) & FLAG_P) | carry | !!(acc & 0x100);
endinstr;

// cpl
instr(4,0057)
    A = ~A;
    F = (F & 0xc5) | (A & 0x28) | FLAG_H | FLAG_N;
endinstr;

// scf
instr(4,0067)
    F = (F & 0xec) | (A & 0x28) | FLAG_C;
endinstr;

// ccf
instr(4,0077)
    F = ((F & 0xed) | (cy << 4) | (A & 0x28)) ^ FLAG_C;
endinstr;


instr(4,0100)                                                       endinstr;   // ld b,b
instr(4,0110)   C = B;                                              endinstr;   // ld c,b
instr(4,0120)   D = B;                                              endinstr;   // ld d,b
instr(4,0130)   E = B;                                              endinstr;   // ld e,b
instr(4,0140)   xh = B;                                             endinstr;   // ld h/ixh/iyh,b
instr(4,0150)   xl = B;                                             endinstr;   // ld l/ixl/iyl,b
HLinstr(0160)   timed_write_byte(addr,B);                           endinstr;   // ld (hl/ix+d/iy+d),b
instr(4,0170)   A = B;                                              endinstr;   // ld a,b


instr(4,0101)   B = C;                                              endinstr;   // ld b,c
instr(4,0111)                                                       endinstr;   // ld c,c
instr(4,0121)   D = C;                                              endinstr;   // ld d,c
instr(4,0131)   E = C;                                              endinstr;   // ld e,c
instr(4,0141)   xh = C;                                             endinstr;   // ld h/ixh/iyh,c
instr(4,0151)   xl = C;                                             endinstr;   // ld l/ixl/iyl,c
HLinstr(0161)   timed_write_byte(addr,C);                           endinstr;   // ld (hl/ix+d/iy+d),c
instr(4,0171)   A = C;                                              endinstr;   // ld a,c


instr(4,0102)   B = D;                                              endinstr;   // ld b,d
instr(4,0112)   C = D;                                              endinstr;   // ld c,d
instr(4,0122)                                                       endinstr;   // ld d,d
instr(4,0132)   E = D;                                              endinstr;   // ld e,d
instr(4,0142)   xh = D;                                             endinstr;   // ld h/ixh/iyh,d
instr(4,0152)   xl = D;                                             endinstr;   // ld l/ixl/iyl,d
HLinstr(0162)   timed_write_byte(addr,D);                           endinstr;   // ld (hl/ix+d/iy+d),d
instr(4,0172)   A = D;                                              endinstr;   // ld a,d


instr(4,0103)   B = E;                                              endinstr;   // ld b,e
instr(4,0113)   C = E;                                              endinstr;   // ld c,e
instr(4,0123)   D = E;                                              endinstr;   // ld d,e
instr(4,0133)                                                       endinstr;   // ld e,e
instr(4,0143)   xh = E;                                             endinstr;   // ld h/ixh/iyh,e
instr(4,0153)   xl = E;                                             endinstr;   // ld l/ixl/iyl,e
HLinstr(0163)   timed_write_byte(addr,E);                           endinstr;   // ld (hl/ix+d/iy+d),e
instr(4,0173)   A = E;                                              endinstr;   // ld a,e


instr(4,0104)   B = xh;                                             endinstr;   // ld b,h/ixh/iyh
instr(4,0114)   C = xh;                                             endinstr;   // ld c,h/ixh/iyh
instr(4,0124)   D = xh;                                             endinstr;   // ld d,h/ixh/iyh
instr(4,0134)   E = xh;                                             endinstr;   // ld e,h/ixh/iyh
instr(4,0144)                                                       endinstr;   // ld h/ixh/iyh,h/ixh/iyh
instr(4,0154)   xl = xh;                                            endinstr;   // ld l/ixh/iyh,h/ixh/iyh
HLinstr(0164)   timed_write_byte(addr,H);                           endinstr;   // ld (hl/ix+d/iy+d),h
instr(4,0174)   A = xh;                                             endinstr;   // ld a,h/ixh/iyh


instr(4,0105)   B = xl;                                             endinstr;   // ld b,l/ixl/iyl
instr(4,0115)   C = xl;                                             endinstr;   // ld c,l/ixl/iyl
instr(4,0125)   D = xl;                                             endinstr;   // ld d,l/ixl/iyl
instr(4,0135)   E = xl;                                             endinstr;   // ld e,l/ixl/iyl
instr(4,0145)   xh = xl;                                            endinstr;   // ld h/ixh/iyh,l/ixl/iyl
instr(4,0155)                                                       endinstr;   // ld l/ixl/iyl,l/ixl/iyl
HLinstr(0165)   timed_write_byte(addr,L);                           endinstr;   // ld (hl/ix+d/iy+d),l
instr(4,0175)   A = xl;                                             endinstr;   // ld a,l/ixl/iyl


HLinstr(0106)   B = timed_read_byte(addr);                          endinstr;   // ld b,(hl/ix+d/iy+d)
HLinstr(0116)   C = timed_read_byte(addr);                          endinstr;   // ld c,(hl/ix+d/iy+d)
HLinstr(0126)   D = timed_read_byte(addr);                          endinstr;   // ld d,(hl/ix+d/iy+d)
HLinstr(0136)   E = timed_read_byte(addr);                          endinstr;   // ld e,(hl/ix+d/iy+d)
HLinstr(0146)   H = timed_read_byte(addr);                          endinstr;   // ld h,(hl/ix+d/iy+d)
HLinstr(0156)   L = timed_read_byte(addr);                          endinstr;   // ld l,(hl/ix+d/iy+d)

instr(4,0166)   regs.halted = 1; PC--;                              endinstr;   // halt

HLinstr(0176)   A = timed_read_byte(addr);                          endinstr;   // ld a,(hl/ix+d/iy+d)


instr(4,0107)   B = A;                                              endinstr;   // ld b,a
instr(4,0117)   C = A;                                              endinstr;   // ld c,a
instr(4,0127)   D = A;                                              endinstr;   // ld d,a
instr(4,0137)   E = A;                                              endinstr;   // ld e,a
instr(4,0147)   xh = A;                                             endinstr;   // ld h/ixh/iyh,a
instr(4,0157)   xl = A;                                             endinstr;   // ld l/ixl/iyl,a
HLinstr(0167)   timed_write_byte(addr,A);                           endinstr;   // ld (hl/ix+d/iy+d),a
instr(4,0177)                                                       endinstr;   // ld a,a


instr(4,0200)   add_a(B);                                           endinstr;   // add a,b
instr(4,0210)   adc_a(B);                                           endinstr;   // adc a,b
instr(4,0220)   sub_a(B);                                           endinstr;   // sub b
instr(4,0230)   sbc_a(B);                                           endinstr;   // sbc a,b
instr(4,0240)   and_a(B);                                           endinstr;   // and b
instr(4,0250)   xor_a(B);                                           endinstr;   // xor b
instr(4,0260)   or_a(B);                                            endinstr;   // or b
instr(4,0270)   cp_a(B);                                            endinstr;   // cp_a b


instr(4,0201)   add_a(C);                                           endinstr;   // add a,c
instr(4,0211)   adc_a(C);                                           endinstr;   // adc a,c
instr(4,0221)   sub_a(C);                                           endinstr;   // sub c
instr(4,0231)   sbc_a(C);                                           endinstr;   // sbc a,c
instr(4,0241)   and_a(C);                                           endinstr;   // and c
instr(4,0251)   xor_a(C);                                           endinstr;   // xor c
instr(4,0261)   or_a(C);                                            endinstr;   // or c
instr(4,0271)   cp_a(C);                                            endinstr;   // cp_a c


instr(4,0202)   add_a(D);                                           endinstr;   // add a,d
instr(4,0212)   adc_a(D);                                           endinstr;   // adc a,d
instr(4,0222)   sub_a(D);                                           endinstr;   // sub d
instr(4,0232)   sbc_a(D);                                           endinstr;   // sbc a,d
instr(4,0242)   and_a(D);                                           endinstr;   // and d
instr(4,0252)   xor_a(D);                                           endinstr;   // xor d
instr(4,0262)   or_a(D);                                            endinstr;   // or d
instr(4,0272)   cp_a(D);                                            endinstr;   // cp_a d


instr(4,0203)   add_a(E);                                           endinstr;   // add a,e
instr(4,0213)   adc_a(E);                                           endinstr;   // adc a,e
instr(4,0223)   sub_a(E);                                           endinstr;   // sub e
instr(4,0233)   sbc_a(E);                                           endinstr;   // sbc a,e
instr(4,0243)   and_a(E);                                           endinstr;   // and e
instr(4,0253)   xor_a(E);                                           endinstr;   // xor e
instr(4,0263)   or_a(E);                                            endinstr;   // or e
instr(4,0273)   cp_a(E);                                            endinstr;   // cp_a e


instr(4,0204)   add_a(xh);                                          endinstr;   // add a,h/ixh/iyh
instr(4,0214)   adc_a(xh);                                          endinstr;   // adc a,h/ixh/iyh
instr(4,0224)   sub_a(xh);                                          endinstr;   // sub h/ixh/iyh
instr(4,0234)   sbc_a(xh);                                          endinstr;   // sbc a,h/ixh/iyh
instr(4,0244)   and_a(xh);                                          endinstr;   // and h/ixh/iyh
instr(4,0254)   xor_a(xh);                                          endinstr;   // xor h/ixh/iyh
instr(4,0264)   or_a(xh);                                           endinstr;   // or h/ixh/iyh
instr(4,0274)   cp_a(xh);                                           endinstr;   // cp_a h/ixh/iyh


instr(4,0205)   add_a(xl);                                          endinstr;   // add a,l/ixl/iyl
instr(4,0215)   adc_a(xl);                                          endinstr;   // adc a,l/ixl/iyl
instr(4,0225)   sub_a(xl);                                          endinstr;   // sub l/ixl/iyl
instr(4,0235)   sbc_a(xl);                                          endinstr;   // sbc a,l/ixl/iyl
instr(4,0245)   and_a(xl);                                          endinstr;   // and l/ixl/iyl
instr(4,0255)   xor_a(xl);                                          endinstr;   // xor l/ixl/iyl
instr(4,0265)   or_a(xl);                                           endinstr;   // or l/ixl/iyl
instr(4,0275)   cp_a(xl);                                           endinstr;   // cp_a l/ixl/iyl


HLinstr(0206)   add_a(timed_read_byte(addr));                       endinstr;   // add a,(hl/ix+d/iy+d)
HLinstr(0216)   adc_a(timed_read_byte(addr));                       endinstr;   // adc a,(hl/ix+d/iy+d)
HLinstr(0226)   sub_a(timed_read_byte(addr));                       endinstr;   // sub (hl/ix+d/iy+d)
HLinstr(0236)   sbc_a(timed_read_byte(addr));                       endinstr;   // sbc a,(hl/ix+d/iy+d)
HLinstr(0246)   and_a(timed_read_byte(addr));                       endinstr;   // and (hl/ix+d/iy+d)
HLinstr(0256)   xor_a(timed_read_byte(addr));                       endinstr;   // xor (hl/ix+d/iy+d)
HLinstr(0266)   or_a(timed_read_byte(addr));                        endinstr;   // or (hl/ix+d/iy+d)
HLinstr(0276)   cp_a(timed_read_byte(addr));                        endinstr;   // cp (hl/ix+d/iy+d)


instr(4,0207)   add_a(A);                                           endinstr;   // add a,a
instr(4,0217)   adc_a(A);                                           endinstr;   // adc a,a
instr(4,0227)   sub_a(A);                                           endinstr;   // sub a
instr(4,0237)   sbc_a(A);                                           endinstr;   // sbc a,a
instr(4,0247)   and_a(A);                                           endinstr;   // and a
instr(4,0257)   xor_a(A);                                           endinstr;   // xor a
instr(4,0267)   or_a(A);                                            endinstr;   // or a
instr(4,0277)   cp_a(A);                                            endinstr;   // cp_a a


instr(5,0300)   ret(!(F & FLAG_Z));                                 endinstr;   // ret nz

// ret z
instr(5,0310)
    if (Tape::RetZHook() || Debug::RetZHook())
        break;

    ret(F & FLAG_Z);
endinstr;

instr(5,0320)   ret(!cy);                                           endinstr;   // ret nc
instr(5,0330)   ret(cy);                                            endinstr;   // ret c
instr(5,0340)   ret(!(F & FLAG_P));                                 endinstr;   // ret po
instr(5,0350)   ret(F & FLAG_P);                                    endinstr;   // ret pe
instr(5,0360)   ret(!(F & FLAG_S));                                 endinstr;   // ret p
instr(5,0370)   ret(F & FLAG_S);                                    endinstr;   // ret m

instr(4,0311)   ret(true);                                          endinstr;   // ret


instr(4,0302)   jp(!(F & FLAG_Z));                                  endinstr;   // jp nz,nn
instr(4,0312)   jp(F & FLAG_Z);                                     endinstr;   // jp z,nn
instr(4,0322)   jp(!cy);                                            endinstr;   // jp nc,nn
instr(4,0332)   jp(cy);                                             endinstr;   // jp c,nn
instr(4,0342)   jp(!(F & FLAG_P));                                  endinstr;   // jp po,nn
instr(4,0352)   jp(F & FLAG_P);                                     endinstr;   // jp pe,nn
instr(4,0362)   jp(!(F & FLAG_S));                                  endinstr;   // jp p,nn
instr(4,0372)   jp(F & FLAG_S);                                     endinstr;   // jp m,nn

instr(4,0303)   jp(true);                                           endinstr;   // jp nn


instr(4,0304)   call(!(F & FLAG_Z));                                endinstr;   // call nz,pq
instr(4,0314)   call(F & FLAG_Z);                                   endinstr;   // call z,nn
instr(4,0324)   call(!cy);                                          endinstr;   // call nc,nn
instr(4,0334)   call(cy);                                           endinstr;   // call c,nn
instr(4,0344)   call(!(F & FLAG_P));                                endinstr;   // call po
instr(4,0354)   call(F & FLAG_P);                                   endinstr;   // call pe,nn
instr(4,0364)   call(!(F & FLAG_S));                                endinstr;   // call p,nn
instr(4,0374)   call(F & FLAG_S);                                   endinstr;   // call m,nn

instr(4,0315)   call(true);                                         endinstr;   // call nn


instr(4,0306)   add_a(timed_read_code_byte(PC++));                  endinstr;   // add a,n
instr(4,0316)   adc_a(timed_read_code_byte(PC++));                  endinstr;   // adc a,n
instr(4,0326)   sub_a(timed_read_code_byte(PC++));                  endinstr;   // sub n
instr(4,0336)   sbc_a(timed_read_code_byte(PC++));                  endinstr;   // sbc a,n
instr(4,0346)   and_a(timed_read_code_byte(PC++));                  endinstr;   // and n
instr(4,0356)   xor_a(timed_read_code_byte(PC++));                  endinstr;   // xor n
instr(4,0366)   or_a(timed_read_code_byte(PC++));                   endinstr;   // or n
instr(4,0376)   cp_a(timed_read_code_byte(PC++));                   endinstr;   // cp n


instr(4,0301)   pop(BC);                                            endinstr;   // pop bc
instr(4,0321)   pop(DE);                                            endinstr;   // pop de
instr(4,0341)   pop(*pHlIxIy);                                      endinstr;   // pop hl/ix/iy
instr(4,0361)   pop(AF);                                            endinstr;   // pop af

instr(4,0351)   PC = *pHlIxIy;                                      endinstr;   // jp (hl/ix/iy)
instr(6,0371)   SP = *pHlIxIy;                                      endinstr;   // ld sp,hl/ix/iy

instr(4,0331)   swap(BC,BC_); swap(DE,DE_); swap(HL,HL_);           endinstr;   // exx


instr(5,0305)   push(BC);                                           endinstr;   // push bc
instr(5,0325)   push(DE);                                           endinstr;   // push de
instr(5,0345)   push(*pHlIxIy);                                     endinstr;   // push hl
instr(5,0365)   push(AF);                                           endinstr;   // push af

instr(4,0335)   pNewHlIxIy = &IX;                                   endinstr;   // [ix prefix]
instr(4,0375)   pNewHlIxIy = &IY;                                   endinstr;   // [iy prefix]

// [ed prefix]
instr(4,0355)
#include "EDops.h"
endinstr;


// [cb prefix]
instr(4,0313)
#include "CBops.h"
endinstr;

// out (n),a
instr(4,0323)
    BYTE bPortLow = timed_read_code_byte(PC++);
    PORT_ACCESS(bPortLow);
    out_byte((A << 8) | bPortLow, A);
endinstr;

// in a,(n)
instr(4,0333)
    BYTE bPortLow = timed_read_code_byte(PC++);
    PORT_ACCESS(bPortLow);
    A = in_byte((A << 8) | bPortLow);
endinstr;

// ex (sp),hl
instr(4,0343)
    WORD t = timed_read_word(SP);
    g_dwCycleCounter++;
    timed_write_word_reversed(SP,*pHlIxIy);
    *pHlIxIy = t;
    g_dwCycleCounter += 2;
endinstr;

instr(4,0363)   IFF1 = IFF2 = 0;                                    endinstr;   // di
instr(4,0373)   if (IO::EiHook()) break; IFF1 = IFF2 = 1; g_nTurbo &= ~TURBO_BOOT; endinstr;   // ei

instr(4,0353)   swap(DE,HL);                                        endinstr;   // ex de,hl


instr(5,0307)   push(PC); PC = 000;                                 endinstr;   // rst 0

instr(5,0317)   if (IO::Rst8Hook()) break; push(PC); PC = 010;      endinstr;   // rst 8
instr(5,0327)   push(PC); PC = 020;                                 endinstr;   // rst 16
instr(5,0337)   push(PC); PC = 030;                                 endinstr;   // rst 24
instr(5,0347)   push(PC); PC = 040;                                 endinstr;   // rst 32
instr(5,0357)   push(PC); PC = 050;                                 endinstr;   // rst 40
instr(5,0367)   if (IO::Rst48Hook()) break; push(PC); PC = 060;     endinstr;   // rst 48
instr(5,0377)   push(PC); PC = 070;                                 endinstr;   // rst 56

#ifdef NODEFAULT
    default: NODEFAULT;
#endif

#undef instr
#undef endinstr
#undef HLinstr
#undef inc
#undef dec
#undef edinstr
