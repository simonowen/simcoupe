// Part of SimCoupe - A SAM Coupe emulator
//
// Z80ops.h: Z80 instruction set emulation
//
//  Copyright (c) 1994 Ian Collier
//  Copyright (c) 1999-2003 by Dave Laundon
//  Copyright (c) 1999-2010 by Simon Owen
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
                                    if (pHlIxIy == &hl) \
                                        addr = hl; \
                                    else { \
                                        addr = *pHlIxIy + (signed char)timed_read_code_byte(pc++); \
                                        g_dwCycleCounter += 5; \
                                    }

#define cy              (f & F_CARRY)

#define xh              (((REGPAIR*)pHlIxIy)->B.h_)
#define xl              (((REGPAIR*)pHlIxIy)->B.l_)

// ld (nn),r
#define ld_pnn_r(x)     ( timed_write_byte(timed_read_code_word(pc), x), pc += 2 )

// ld r,(nn)
#define ld_r_pnn(x)     ( x = timed_read_byte(timed_read_code_word(pc)), pc += 2 )

// ld (nn),rr
#define ld_pnn_rr(x)    ( timed_write_word(timed_read_code_word(pc), x), pc += 2 )

// ld rr,(nn)
#define ld_rr_pnn(x)    ( x = timed_read_word(timed_read_code_word(pc)), pc += 2 )

// 8-bit increment and decrement
#ifdef USE_FLAG_TABLES
#define inc(var)        ( var++, f = cy | g_abInc[var] )
#define dec(var)        ( var--, f = cy | g_abDec[var] )
#else
#define inc(var)        ( var++, f = cy | (var & 0xa8) | ((!var) << 6) | ((!( var & 0xf)) << 4) | ((var == 0x80) << 2) )
#define dec(var)        ( var--, f = cy | (var & 0xa8) | ((!var) << 6) | ((!(~var & 0xf)) << 4) | ((var == 0x7f) << 2) | F_NADD )
#endif

// 16-bit add
#define add_hl(x)       do { \
                            g_dwCycleCounter += 7; \
                            WORD z = (x); \
                            DWORD y = *pHlIxIy + z; \
                            f = (f & 0xc4) |                                        /* S, Z, V    */ \
                                (((y & 0x3800) ^ ((*pHlIxIy ^ z) & 0x1000)) >> 8) | /* 5, H, 3    */ \
                                (y >> 16);                                          /* C          */ \
                            *pHlIxIy = y; \
                        } while (0)

// 8-bit add
#define add_a(x)        add_a1((x),0)
#define adc_a(x)        add_a1((x),cy)
#define add_a1(x,c)     do { \
                            BYTE z = (x); \
                            WORD y = a + z + (c); \
                            f = ((y & 0xb8) ^ ((a ^ z) & 0x10)) |                   /* S, 5, H, 3 */ \
                                (y >> 8) |                                          /* C          */ \
                                (((a ^ ~z) & (a ^ y) & 0x80) >> 5);                 /* V          */ \
                            a = y;                                                                   \
                            f |= (!a) << 6;                                         /* Z          */ \
                        } while (0)

// 8-bit subtract
#define sub_a(x)        sub_a1((x),0)
#define sbc_a(x)        sub_a1((x),cy)
#define sub_a1(x,c)     do { \
                            BYTE z = (x); \
                            WORD y = a - z - (c); \
                            f = ((y & 0xb8) ^ ((a ^ z) & 0x10)) |                   /* S, 5, H, 3 */ \
                                ((y >> 8) & 1) |                                    /* C          */ \
                                (((a ^ z) & (a ^ y) & 0x80) >> 5) |                 /* V          */ \
                                2;                                                  /* N          */ \
                            a = y;                                                                   \
                            f |= (!a) << 6;                                         /* Z          */ \
                        } while (0)

// 8-bit compare
// Undocumented flags added by Ian Collier
#define cp_a(x)          do { \
                            BYTE z = (x); \
                            WORD y = a - z; \
                            f = ((y & 0x90) ^ ((a ^ z) & 0x10)) |                   /* S, H       */ \
                                (z & 0x28) |                                        /* 5, 3       */ \
                                ((y >> 8) & 1) |                                    /* C          */ \
                                (((a ^ z) & (a ^ y) & 0x80) >> 5) |                 /* V          */ \
                                2 |                                                 /* N          */ \
                                ((!y) << 6);                                        /* Z          */ \
                        } while (0)

// logical and
#define and_a(x)        ( a &= (x), f = parity(a) | F_HCARRY )

// logical xor
#define xor_a(x)        ( a ^= (x), f = parity(a) )

// logical or
#define or_a(x)         ( a |= (x), f = parity(a) )

// Jump relative
#define jr(cc)          do { \
                            if (cc) { \
                                int j = (signed char)timed_read_code_byte(pc++); \
                                pc += j; \
                                g_dwCycleCounter += 5; \
                            } \
                            else { \
                                MEM_ACCESS(pc); \
                                pc++; \
                            } \
                        } while (0)

// Jump absolute
#define jp(cc)          do { \
                            if (cc) \
                                pc = timed_read_code_word(pc); \
                            else { \
                                MEM_ACCESS(pc); \
                                MEM_ACCESS(pc + 1); \
                                pc += 2; \
                            } \
                        } while (0)

// Call
#define call(cc)        do { \
                            if (cc) { \
                                WORD npc = timed_read_code_word(pc); \
                                g_dwCycleCounter++; \
                                push(pc+2); \
                                pc = npc; \
                            } \
                            else { \
                                MEM_ACCESS(pc); \
                                MEM_ACCESS(pc + 1); \
                                pc += 2; \
                            } \
                        } while (0)

// Return
#define ret(cc)         do { if (cc) { pop(pc); Debug::OnRet(); } } while (0)
#define retn            do { iff1 = iff2; ret(true); } while (0)


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


instr(4,0000)                                                       endinstr;   // nop
instr(4,0010)   swap(af,alt_af);                                    endinstr;   // ex af,af'
instr(5,0020)   --b; jr(b);                                         endinstr;   // djnz e
instr(4,0030)   jr(true);                                           endinstr;   // jr e
instr(4,0040)   jr(!(f & F_ZERO));                                  endinstr;   // jr nz,e
instr(4,0050)   jr(f & F_ZERO);                                     endinstr;   // jr z,e
instr(4,0060)   jr(!cy);                                            endinstr;   // jr nc,e
instr(4,0070)   jr(cy);                                             endinstr;   // jr c,e


instr(4,0001)   bc = timed_read_code_word(pc); pc += 2;             endinstr;   // ld bc,nn
instr(4,0011)   add_hl(bc);                                         endinstr;   // add hl/ix/iy,bc
instr(4,0021)   de = timed_read_code_word(pc); pc += 2;             endinstr;   // ld de,nn
instr(4,0031)   add_hl(de);                                         endinstr;   // add hl/ix/iy,de
instr(4,0041)   *pHlIxIy = timed_read_code_word(pc); pc += 2;       endinstr;   // ld hl/ix/iy,nn
instr(4,0051)   add_hl(*pHlIxIy);                                   endinstr;   // add hl/ix/iy,hl/ix/iy
instr(4,0061)   sp = timed_read_code_word(pc); pc += 2;             endinstr;   // ld sp,nn
instr(4,0071)   add_hl(sp);                                         endinstr;   // add hl/ix/iy,sp


instr(4,0002)   timed_write_byte(bc,a);                             endinstr;   // ld (bc),a
instr(4,0012)   a = timed_read_byte(bc);                            endinstr;   // ld a,(bc)
instr(4,0022)   timed_write_byte(de,a);                             endinstr;   // ld (de),a
instr(4,0032)   a = timed_read_byte(de);                            endinstr;   // ld a,(de)
instr(4,0042)   ld_pnn_rr(*pHlIxIy);                                endinstr;   // ld (nn),hl/ix/iy
instr(4,0052)   ld_rr_pnn(*pHlIxIy);                                endinstr;   // ld hl/ix/iy,(nn)
instr(4,0062)   ld_pnn_r(a);                                        endinstr;   // ld (nn),a
instr(4,0072)   ld_r_pnn(a);                                        endinstr;   // ld a,(nn)


instr(6,0003)   ++bc;                                               endinstr;   // inc bc
instr(6,0013)   --bc;                                               endinstr;   // dec bc
instr(6,0023)   ++de;                                               endinstr;   // inc de
instr(6,0033)   --de;                                               endinstr;   // dec de
instr(6,0043)   ++*pHlIxIy;                                         endinstr;   // inc hl/ix/iy
instr(6,0053)   --*pHlIxIy;                                         endinstr;   // dec hl/ix/iy
instr(6,0063)   ++sp;                                               endinstr;   // inc sp
instr(6,0073)   --sp;                                               endinstr;   // dec sp


instr(4,0004)   inc(b);                                             endinstr;   // inc b
instr(4,0014)   inc(c);                                             endinstr;   // inc c
instr(4,0024)   inc(d);                                             endinstr;   // inc d
instr(4,0034)   inc(e);                                             endinstr;   // inc e
instr(4,0044)   inc(xh);                                            endinstr;   // inc h/ixh/iyh
instr(4,0054)   inc(xl);                                            endinstr;   // inc l/ixl/iyl
HLinstr(0064)   BYTE t = timed_read_byte(addr);
                inc(t); g_dwCycleCounter++;
                timed_write_byte(addr,t);                           endinstr;   // inc (hl/ix+d/iy+d)
instr(4,0074)   inc(a);                                             endinstr;   // inc a


instr(4,0005)   dec(b);                                             endinstr;   // dec b
instr(4,0015)   dec(c);                                             endinstr;   // dec c
instr(4,0025)   dec(d);                                             endinstr;   // dec d
instr(4,0035)   dec(e);                                             endinstr;   // dec e
instr(4,0045)   dec(xh);                                            endinstr;   // dec h/ixh/iyh
instr(4,0055)   dec(xl);                                            endinstr;   // dec l/ixl/iyl
HLinstr(0065)   BYTE t = timed_read_byte(addr);
                dec(t); g_dwCycleCounter++;
                timed_write_byte(addr,t);                           endinstr;   // dec (hl/ix+d/iy+d)
instr(4,0075)   dec(a);                                             endinstr;   // dec a


instr(4,0006)   b = timed_read_code_byte(pc++);                     endinstr;   // ld b,n
instr(4,0016)   c = timed_read_code_byte(pc++);                     endinstr;   // ld c,n
instr(4,0026)   d = timed_read_code_byte(pc++);                     endinstr;   // ld d,n
instr(4,0036)   e = timed_read_code_byte(pc++);                     endinstr;   // ld e,n
instr(4,0046)   xh = timed_read_code_byte(pc++);                    endinstr;   // ld h/ixh/iyh,n
instr(4,0056)   xl = timed_read_code_byte(pc++);                    endinstr;   // ld l/ixl/iyl,n
HLinstr(0066)   timed_write_byte(addr,timed_read_code_byte(pc++));  endinstr;   // ld (hl/ix+d/iy+d),n
instr(4,0076)   a = timed_read_code_byte(pc++);                     endinstr;   // ld a,n


// rlca
instr(4,0007)
    a = (a << 1) | (a >> 7);
    f = (f & 0xc4) | (a & 0x29);
endinstr;

// rrca
instr(4,0017)
    f = (f & 0xc4) | (a & 1);
    a = (a >> 1) | (a << 7);
    f |= a & 0x28;
endinstr;

// rla
instr(4,0027)
    int t = a >> 7;
    a = (a << 1) | cy;
    f = (f & 0xc4) | (a & 0x28) | t;
endinstr;

// rra
instr(4,0037)
    int t = a & F_CARRY;
    a = (a >> 1) | (f << 7);
    f = (f & 0xc4) | (a & 0x28) | t;
endinstr;

// daa
instr(4,0047)
    WORD acc = a;
    BYTE carry = cy, incr = 0;

    if ((f & F_HCARRY) || (a & 0x0f) > 9)
        incr = 6;

    if (f & F_NADD)
    {
        int hd = carry || a > 0x99;

        if (incr)
        {
            acc = (acc - incr) & 0xff;

            if ((a & 0x0f) > 5)
                f &= ~F_HCARRY;
        }

        if (hd)
            acc -= 0x160;
    }
    else
    {           
        if (incr)
        {
            f = (f & ~F_HCARRY) | (((a & 0x0f) > 9) ? F_HCARRY : 0);
            acc += incr;
        }

        if (carry || ((acc & 0x1f0) > 0x90))
            acc += 0x60;
    }

    a = acc;
    f = (a & 0xa8) | (!a << 6) | (f & 0x12) | (parity(a) & F_PARITY) | carry | !!(acc & 0x100);
endinstr;

// cpl
instr(4,0057)
    a = ~a;
    f = (f & 0xc5) | (a & 0x28) | F_HCARRY | F_NADD;
endinstr;

// scf
instr(4,0067)
    f = (f & 0xc4) | (a & 0x28) | F_CARRY;
endinstr;

// ccf
instr(4,0077)
    f = (f & 0xc4) | (cy ^ F_CARRY) | (cy << 4) | (a & 0x28);
endinstr;


instr(4,0100)                                                       endinstr;   // ld b,b
instr(4,0110)   c = b;                                              endinstr;   // ld c,b
instr(4,0120)   d = b;                                              endinstr;   // ld d,b
instr(4,0130)   e = b;                                              endinstr;   // ld e,b
instr(4,0140)   xh = b;                                             endinstr;   // ld h/ixh/iyh,b
instr(4,0150)   xl = b;                                             endinstr;   // ld l/ixl/iyl,b
HLinstr(0160)   timed_write_byte(addr,b);                           endinstr;   // ld (hl/ix+d/iy+d),b
instr(4,0170)   a = b;                                              endinstr;   // ld a,b


instr(4,0101)   b = c;                                              endinstr;   // ld b,c
instr(4,0111)                                                       endinstr;   // ld c,c
instr(4,0121)   d = c;                                              endinstr;   // ld d,c
instr(4,0131)   e = c;                                              endinstr;   // ld e,c
instr(4,0141)   xh = c;                                             endinstr;   // ld h/ixh/iyh,c
instr(4,0151)   xl = c;                                             endinstr;   // ld l/ixl/iyl,c
HLinstr(0161)   timed_write_byte(addr,c);                           endinstr;   // ld (hl/ix+d/iy+d),c
instr(4,0171)   a = c;                                              endinstr;   // ld a,c


instr(4,0102)   b = d;                                              endinstr;   // ld b,d
instr(4,0112)   c = d;                                              endinstr;   // ld c,d
instr(4,0122)                                                       endinstr;   // ld d,d
instr(4,0132)   e = d;                                              endinstr;   // ld e,d
instr(4,0142)   xh = d;                                             endinstr;   // ld h/ixh/iyh,d
instr(4,0152)   xl = d;                                             endinstr;   // ld l/ixl/iyl,d
HLinstr(0162)   timed_write_byte(addr,d);                           endinstr;   // ld (hl/ix+d/iy+d),d
instr(4,0172)   a = d;                                              endinstr;   // ld a,d


instr(4,0103)   b = e;                                              endinstr;   // ld b,e
instr(4,0113)   c = e;                                              endinstr;   // ld c,e
instr(4,0123)   d = e;                                              endinstr;   // ld d,e
instr(4,0133)                                                       endinstr;   // ld e,e
instr(4,0143)   xh = e;                                             endinstr;   // ld h/ixh/iyh,e
instr(4,0153)   xl = e;                                             endinstr;   // ld l/ixl/iyl,e
HLinstr(0163)   timed_write_byte(addr,e);                           endinstr;   // ld (hl/ix+d/iy+d),e
instr(4,0173)   a = e;                                              endinstr;   // ld a,e


instr(4,0104)   b = xh;                                             endinstr;   // ld b,h/ixh/iyh
instr(4,0114)   c = xh;                                             endinstr;   // ld c,h/ixh/iyh
instr(4,0124)   d = xh;                                             endinstr;   // ld d,h/ixh/iyh
instr(4,0134)   e = xh;                                             endinstr;   // ld e,h/ixh/iyh
instr(4,0144)                                                       endinstr;   // ld h/ixh/iyh,h/ixh/iyh
instr(4,0154)   xl = xh;                                            endinstr;   // ld l/ixh/iyh,h/ixh/iyh
HLinstr(0164)   timed_write_byte(addr,h);                           endinstr;   // ld (hl/ix+d/iy+d),h
instr(4,0174)   a = xh;                                             endinstr;   // ld a,h/ixh/iyh


instr(4,0105)   b = xl;                                             endinstr;   // ld b,l/ixl/iyl
instr(4,0115)   c = xl;                                             endinstr;   // ld c,l/ixl/iyl
instr(4,0125)   d = xl;                                             endinstr;   // ld d,l/ixl/iyl
instr(4,0135)   e = xl;                                             endinstr;   // ld e,l/ixl/iyl
instr(4,0145)   xh = xl;                                            endinstr;   // ld h/ixh/iyh,l/ixl/iyl
instr(4,0155)                                                       endinstr;   // ld l/ixl/iyl,l/ixl/iyl
HLinstr(0165)   timed_write_byte(addr,l);                           endinstr;   // ld (hl/ix+d/iy+d),l
instr(4,0175)   a = xl;                                             endinstr;   // ld a,l/ixl/iyl


HLinstr(0106)   b = timed_read_byte(addr);                          endinstr;   // ld b,(hl/ix+d/iy+d)
HLinstr(0116)   c = timed_read_byte(addr);                          endinstr;   // ld c,(hl/ix+d/iy+d)
HLinstr(0126)   d = timed_read_byte(addr);                          endinstr;   // ld d,(hl/ix+d/iy+d)
HLinstr(0136)   e = timed_read_byte(addr);                          endinstr;   // ld e,(hl/ix+d/iy+d)
HLinstr(0146)   h = timed_read_byte(addr);                          endinstr;   // ld h,(hl/ix+d/iy+d)
HLinstr(0156)   l = timed_read_byte(addr);                          endinstr;   // ld l,(hl/ix+d/iy+d)

instr(4,0166)   halted = 1; pc--;                                   endinstr;   // halt

HLinstr(0176)   a = timed_read_byte(addr);                          endinstr;   // ld a,(hl/ix+d/iy+d)


instr(4,0107)   b = a;                                              endinstr;   // ld b,a
instr(4,0117)   c = a;                                              endinstr;   // ld c,a
instr(4,0127)   d = a;                                              endinstr;   // ld d,a
instr(4,0137)   e = a;                                              endinstr;   // ld e,a
instr(4,0147)   xh = a;                                             endinstr;   // ld h/ixh/iyh,a
instr(4,0157)   xl = a;                                             endinstr;   // ld l/ixl/iyl,a
HLinstr(0167)   timed_write_byte(addr,a);                           endinstr;   // ld (hl/ix+d/iy+d),a
instr(4,0177)                                                       endinstr;   // ld a,a


instr(4,0200)   add_a(b);                                           endinstr;   // add a,b
instr(4,0210)   adc_a(b);                                           endinstr;   // adc a,b
instr(4,0220)   sub_a(b);                                           endinstr;   // sub b
instr(4,0230)   sbc_a(b);                                           endinstr;   // sbc a,b
instr(4,0240)   and_a(b);                                           endinstr;   // and b
instr(4,0250)   xor_a(b);                                           endinstr;   // xor b
instr(4,0260)   or_a(b);                                            endinstr;   // or b
instr(4,0270)   cp_a(b);                                            endinstr;   // cp b


instr(4,0201)   add_a(c);                                           endinstr;   // add a,c
instr(4,0211)   adc_a(c);                                           endinstr;   // adc a,c
instr(4,0221)   sub_a(c);                                           endinstr;   // sub c
instr(4,0231)   sbc_a(c);                                           endinstr;   // sbc a,c
instr(4,0241)   and_a(c);                                           endinstr;   // and c
instr(4,0251)   xor_a(c);                                           endinstr;   // xor c
instr(4,0261)   or_a(c);                                            endinstr;   // or c
instr(4,0271)   cp_a(c);                                            endinstr;   // cp c


instr(4,0202)   add_a(d);                                           endinstr;   // add a,d
instr(4,0212)   adc_a(d);                                           endinstr;   // adc a,d
instr(4,0222)   sub_a(d);                                           endinstr;   // sub d
instr(4,0232)   sbc_a(d);                                           endinstr;   // sbc a,d
instr(4,0242)   and_a(d);                                           endinstr;   // and d
instr(4,0252)   xor_a(d);                                           endinstr;   // xor d
instr(4,0262)   or_a(d);                                            endinstr;   // or d
instr(4,0272)   cp_a(d);                                            endinstr;   // cp d


instr(4,0203)   add_a(e);                                           endinstr;   // add a,e
instr(4,0213)   adc_a(e);                                           endinstr;   // adc a,e
instr(4,0223)   sub_a(e);                                           endinstr;   // sub e
instr(4,0233)   sbc_a(e);                                           endinstr;   // sbc a,e
instr(4,0243)   and_a(e);                                           endinstr;   // and e
instr(4,0253)   xor_a(e);                                           endinstr;   // xor e
instr(4,0263)   or_a(e);                                            endinstr;   // or e
instr(4,0273)   cp_a(e);                                            endinstr;   // cp e


instr(4,0204)   add_a(xh);                                          endinstr;   // add a,h/ixh/iyh
instr(4,0214)   adc_a(xh);                                          endinstr;   // adc a,h/ixh/iyh
instr(4,0224)   sub_a(xh);                                          endinstr;   // sub h/ixh/iyh
instr(4,0234)   sbc_a(xh);                                          endinstr;   // sbc a,h/ixh/iyh
instr(4,0244)   and_a(xh);                                          endinstr;   // and h/ixh/iyh
instr(4,0254)   xor_a(xh);                                          endinstr;   // xor h/ixh/iyh
instr(4,0264)   or_a(xh);                                           endinstr;   // or h/ixh/iyh
instr(4,0274)   cp_a(xh);                                           endinstr;   // cp h/ixh/iyh


instr(4,0205)   add_a(xl);                                          endinstr;   // add a,l/ixl/iyl
instr(4,0215)   adc_a(xl);                                          endinstr;   // adc a,l/ixl/iyl
instr(4,0225)   sub_a(xl);                                          endinstr;   // sub l/ixl/iyl
instr(4,0235)   sbc_a(xl);                                          endinstr;   // sbc a,l/ixl/iyl
instr(4,0245)   and_a(xl);                                          endinstr;   // and l/ixl/iyl
instr(4,0255)   xor_a(xl);                                          endinstr;   // xor l/ixl/iyl
instr(4,0265)   or_a(xl);                                           endinstr;   // or l/ixl/iyl
instr(4,0275)   cp_a(xl);                                           endinstr;   // cp l/ixl/iyl


HLinstr(0206)   add_a(timed_read_byte(addr));                       endinstr;   // add a,(hl/ix+d/iy+d)
HLinstr(0216)   adc_a(timed_read_byte(addr));                       endinstr;   // adc a,(hl/ix+d/iy+d)
HLinstr(0226)   sub_a(timed_read_byte(addr));                       endinstr;   // sub (hl/ix+d/iy+d)
HLinstr(0236)   sbc_a(timed_read_byte(addr));                       endinstr;   // sbc a,(hl/ix+d/iy+d)
HLinstr(0246)   and_a(timed_read_byte(addr));                       endinstr;   // and (hl/ix+d/iy+d)
HLinstr(0256)   xor_a(timed_read_byte(addr));                       endinstr;   // xor (hl/ix+d/iy+d)
HLinstr(0266)   or_a(timed_read_byte(addr));                        endinstr;   // or (hl/ix+d/iy+d)
HLinstr(0276)   cp_a(timed_read_byte(addr));                        endinstr;   // cp (hl/ix+d/iy+d)


instr(4,0207)   add_a(a);                                           endinstr;   // add a,a
instr(4,0217)   adc_a(a);                                           endinstr;   // adc a,a
instr(4,0227)   sub_a(a);                                           endinstr;   // sub a
instr(4,0237)   sbc_a(a);                                           endinstr;   // sbc a,a
instr(4,0247)   and_a(a);                                           endinstr;   // and a
instr(4,0257)   xor_a(a);                                           endinstr;   // xor a
instr(4,0267)   or_a(a);                                            endinstr;   // or a
instr(4,0277)   cp_a(a);                                            endinstr;   // cp a


instr(5,0300)   ret(!(f & F_ZERO));                                 endinstr;   // ret nz
instr(5,0310)   ret(f & F_ZERO);                                    endinstr;   // ret z
instr(5,0320)   ret(!cy);                                           endinstr;   // ret nc
instr(5,0330)   ret(cy);                                            endinstr;   // ret c
instr(5,0340)   ret(!(f & F_PARITY));                               endinstr;   // ret po
instr(5,0350)   ret(f & F_PARITY);                                  endinstr;   // ret pe
instr(5,0360)   ret(!(f & F_NEG));                                  endinstr;   // ret p
instr(5,0370)   ret(f & F_NEG);                                     endinstr;   // ret m

instr(4,0311)   ret(true);                                          endinstr;   // ret


instr(4,0302)   jp(!(f & F_ZERO));                                  endinstr;   // jp nz,nn
instr(4,0312)   jp(f & F_ZERO);                                     endinstr;   // jp z,nn
instr(4,0322)   jp(!cy);                                            endinstr;   // jp nc,nn
instr(4,0332)   jp(cy);                                             endinstr;   // jp c,nn
instr(4,0342)   jp(!(f & F_PARITY));                                endinstr;   // jp po,nn
instr(4,0352)   jp(f & F_PARITY);                                   endinstr;   // jp pe,nn
instr(4,0362)   jp(!(f & F_NEG));                                   endinstr;   // jp p,nn
instr(4,0372)   jp(f & F_NEG);                                      endinstr;   // jp m,nn

instr(4,0303)   jp(true);                                           endinstr;   // jp nn


instr(4,0304)   call(!(f & F_ZERO));                                endinstr;   // call nz,pq
instr(4,0314)   call(f & F_ZERO);                                   endinstr;   // call z,nn
instr(4,0324)   call(!cy);                                          endinstr;   // call nc,nn
instr(4,0334)   call(cy);                                           endinstr;   // call c,nn
instr(4,0344)   call(!(f & F_PARITY));                              endinstr;   // call po
instr(4,0354)   call(f & F_PARITY);                                 endinstr;   // call pe,nn
instr(4,0364)   call(!(f & F_NEG));                                 endinstr;   // call p,nn
instr(4,0374)   call(f & F_NEG);                                    endinstr;   // call m,nn

instr(4,0315)   call(true);                                         endinstr;   // call nn


instr(4,0306)   add_a(timed_read_code_byte(pc++));                  endinstr;   // add a,n
instr(4,0316)   adc_a(timed_read_code_byte(pc++));                  endinstr;   // adc a,n
instr(4,0326)   sub_a(timed_read_code_byte(pc++));                  endinstr;   // sub n
instr(4,0336)   sbc_a(timed_read_code_byte(pc++));                  endinstr;   // sbc a,n
instr(4,0346)   and_a(timed_read_code_byte(pc++));                  endinstr;   // and n
instr(4,0356)   xor_a(timed_read_code_byte(pc++));                  endinstr;   // xor n
instr(4,0366)   or_a(timed_read_code_byte(pc++));                   endinstr;   // or n
instr(4,0376)   cp_a(timed_read_code_byte(pc++));                   endinstr;   // cp n


instr(4,0301)   pop(bc);                                            endinstr;   // pop bc
instr(4,0321)   pop(de);                                            endinstr;   // pop de
instr(4,0341)   pop(*pHlIxIy);                                      endinstr;   // pop hl/ix/iy
instr(4,0361)   pop(af);                                            endinstr;   // pop af

instr(4,0351)   pc = *pHlIxIy;                                      endinstr;   // jp (hl/ix/iy)
instr(6,0371)   sp = *pHlIxIy;                                      endinstr;   // ld sp,hl/ix/iy

instr(4,0331)   swap(bc,alt_bc); swap(de,alt_de); swap(hl,alt_hl);  endinstr;   // exx


instr(5,0305)   push(bc);                                           endinstr;   // push bc
instr(5,0325)   push(de);                                           endinstr;   // push de
instr(5,0345)   push(*pHlIxIy);                                     endinstr;   // push hl
instr(5,0365)   push(af);                                           endinstr;   // push af

instr(4,0335)   pNewHlIxIy = &ix;                                   endinstr;   // [ix prefix]
instr(4,0375)   pNewHlIxIy = &iy;                                   endinstr;   // [iy prefix]

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
    BYTE bPortLow = timed_read_code_byte(pc++);
    PORT_ACCESS(bPortLow);
    out_byte((a << 8) | bPortLow, a);
endinstr;

// in a,(n)
instr(4,0333)
    BYTE bPortLow = timed_read_code_byte(pc++);
    PORT_ACCESS(bPortLow);
    a = in_byte((a << 8) | bPortLow);
endinstr;

// ex (sp),hl
instr(4,0343)
    WORD t = timed_read_word(sp);
    g_dwCycleCounter++;
    timed_write_word_reversed(sp,*pHlIxIy);
    *pHlIxIy = t;
    g_dwCycleCounter += 2;
endinstr;

instr(4,0363)   iff1 = iff2 = 0;                                    endinstr;   // di
instr(4,0373)   iff1 = iff2 = 1; g_nFastBooting = 0;                endinstr;   // ei

instr(4,0353)   swap(de,hl);                                        endinstr;   // ex de,hl


instr(5,0307)   push(pc); pc = 000;                                 endinstr;   // rst 0

instr(5,0317)   if (IO::Rst8Hook()) break; push(pc); pc = 010;      endinstr;   // rst 8
instr(5,0327)   push(pc); pc = 020;                                 endinstr;   // rst 16
instr(5,0337)   push(pc); pc = 030;                                 endinstr;   // rst 24
instr(5,0347)   push(pc); pc = 040;                                 endinstr;   // rst 32
instr(5,0357)   push(pc); pc = 050;                                 endinstr;   // rst 40
instr(5,0367)   push(pc); pc = 060;                                 endinstr;   // rst 48
instr(5,0377)   push(pc); pc = 070;                                 endinstr;   // rst 56

#ifdef NODEFAULT
    default: NODEFAULT;
#endif

#undef instr
#undef endinstr
#undef HLinstr
#undef inc
#undef dec
#undef edinstr
