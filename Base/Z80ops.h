// Part of SimCoupe - A SAM Coupé emulator
//
// Z80ops.h: Z80 instruction set emulation
//
//  Copyright (c) 1994 Ian Collier
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
#define instr(opcode, m1states) case opcode: { \
                                    g_nLineCycle += m1states - 3;
#define endinstr                }; break

// Indirect HL instructions affected by IX/IY prefixes
#define HLinstr(opcode)         instr(opcode, 4) \
                                    WORD addr; \
                                    if (pHlIxIy == &hl) \
                                        addr = hl; \
                                    else { \
                                        addr = *pHlIxIy + (signed char)timed_read_byte(pc); \
                                        pc++; \
                                        g_nLineCycle += 5; \
                                    }


#define cy      (f & F_CARRY)

#define xh      (((REGPAIR*)pHlIxIy)->B.h_)
#define xl      (((REGPAIR*)pHlIxIy)->B.l_)


// 8-bit increment and decrement
#ifdef USE_FLAG_TABLES
#define inc(var)    ( var++, f = (f & F_CARRY) | g_abInc[var] )
#define dec(var)    ( var--, f = (f & F_CARRY) | g_abDec[var] )
#else
#define inc(var)    ( var++, f = (f & F_CARRY) | (var & 0xa8) | ((!var) << 6) | ((!( var & 0xf)) << 4) | ((var == 0x80) << 2) )
#define dec(var)    ( var--, f = (f & F_CARRY) | (var & 0xa8) | ((!var) << 6) | ((!(~var & 0xf)) << 4) | ((var == 0x7f) << 2) | F_NADD )
#endif

// add hl/ix/iy,dd
#define addhl(hi,lo) \
            do \
            { \
                g_nLineCycle += 7; \
                if(pHlIxIy == &hl) \
                { \
                    WORD t; \
                    l = t = l + (lo); \
                    f = (f & 0xc4) | (((t >>= 8) + (h & 0x0f) + ((hi) & 0x0f) > 15) << 4); \
                    h = (t += h + (hi)); \
                    f |= (h & 0x28) | (t >> 8); \
                } \
                else \
                { \
                    DWORD t = *pHlIxIy; \
                    f = (f & 0xc4) | (((t & 0xfff) + (((hi) << 8) | (lo)) > 0xfff) << 4); \
                    t += ((hi) << 8) | (lo); \
                    *pHlIxIy = t; \
                    f |= ((t >> 8) & 0x28) | (t >> 16); \
                } \
            } \
            while(0)


#define adda1(x,c) /* 8-bit add */ \
            { \
                WORD y; \
                BYTE z = (x); \
                y = a + z + (c); \
                f = (y&0xa8) | (y >> 8) | (((a&0x0f) + (z&0x0f) + (c) > 15) << 4) | (((~a^z) & 0x80 & (y^a)) >> 5); \
                f |= (!(a = y)) << 6; \
            }

#define adda(x)     adda1((x),0)
#define adca(x)     adda1((x),cy)

#define suba1(x,c) /* 8-bit subtract */ do{WORD y;\
                      BYTE z=(x);\
                      y=(a-z-(c))&0x1ff;\
                      f=(y&0xa8)|(y>>8)|(((a&0x0f)<(z&0x0f)+(c))<<4)|\
                        (((a^z)&0x80&(y^a))>>5)|2;\
                      f|=(!(a=y))<<6;\
                   } while(0)

#define suba(x)     suba1((x),0)
#define sbca(x)     suba1((x),cy)

// Undocumented flags added by Ian Collier
#define cpa(x) /* 8-bit compare */ do{WORD y;\
                      BYTE z=(x);\
                      y=(a-z)&0x1ff;\
                      f=(y&0x80)|(z&0x28)|(y>>8)|(((a&0x0f)<(z&0x0f))<<4)|\
                        (((a^z)&0x80&(y^a))>>5)|2|((!y)<<6);\
                   } while(0)

#define anda(x) /* logical and */ do{\
                      a&=(x);\
                      f=(a&0xa8)|((!a)<<6)|0x10|parity(a);\
                   } while(0)

#define xora(x) /* logical xor */ do{\
                      a^=(x);\
                      f=(a&0xa8)|((!a)<<6)|parity(a);\
                   } while(0)
#define ora(x) /* logical or */ do{\
                      a|=(x);\
                      f=(a&0xa8)|((!a)<<6)|parity(a);\
                   } while(0)

// Jump relative if condition is true
#define jr(cc)  do { \
                    if (cc) { \
                        int j = (signed char)timed_read_byte(pc); \
                        pc += j+1; \
                        g_nLineCycle += 5; \
                    } \
                    else { \
                        MEM_ACCESS(pc); \
                        pc++; \
                    } \
                } while(0)

// Jump absolute if condition is true
#define jp(cc)  do { \
                    if (cc) \
                        pc = timed_read_word(pc); \
                    else { \
                        MEM_ACCESS(pc); \
                        MEM_ACCESS(pc + 1); \
                        pc += 2; \
                    } \
                } while(0)

// Call if condition is true
#define call(cc)    do { \
                        if (cc) { \
                            WORD npc = timed_read_word(pc); \
                            g_nLineCycle++; \
                            push(pc+2); \
                            pc = npc; \
                        } \
                        else { \
                            MEM_ACCESS(pc); \
                            MEM_ACCESS(pc + 1); \
                            pc += 2; \
                        } \
                    } while(0)

// Return if condition is true
#define ret(cc) do { if (cc) pop(pc); } while(0)

#define retn    do { iff1=iff2; ret(true); } while (0)


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// nop
instr(0,4)
endinstr;

// ld bc,nn
instr(1,4)
    bc = timed_read_word(pc);
    pc += 2;
endinstr;

// ld (bc),a
instr(2,4)
    timed_write_byte(bc,a);
endinstr;

// inc bc
instr(3,6)
    bc++;
endinstr;


// inc b
instr(4,4)
    inc(b);
endinstr;

// dec b
instr(5,4)
    dec(b);
endinstr;

// ld b,n
instr(6,4)
    b = timed_read_byte(pc);
    pc++;
endinstr;

// rlc a
instr(7,4)
    a=(a<<1)|(a>>7);
    f=(f&0xc4)|(a&0x29);
endinstr;


// ex af,af'
instr(8,4)
    swap(af,alt_af);
endinstr;

// add hl/ix/iy,bc
instr(9,4)
    addhl(b,c);
endinstr;

// ld a,(bc)
instr(10,4)
    a = timed_read_byte(bc);
endinstr;

// dec bc
instr(11,6)
    bc--;
endinstr;


// inc c
instr(12,4)
    inc(c);
endinstr;

// dec c
instr(13,4)
    dec(c);
endinstr;

// ld c,n
instr(14,4)
    c = timed_read_byte(pc);
    pc++;
endinstr;

// rrc a
instr(15,4)
    f = (f & 0xc4) | (a & 1);
    a = (a >> 1) | (a << 7);
    f |= a & 0x28;
endinstr;


// djnz e
instr(16,5)
    --b;
    jr(b);
endinstr;

// ld de,nn
instr(17,4)
    de = timed_read_word(pc);
    pc += 2;
endinstr;

// ld (de),a
instr(18,4)
    timed_write_byte(de,a);
endinstr;

// inc de
instr(19,6)
    de++;
endinstr;


// inc d
instr(20,4)
    inc(d);
endinstr;

// dec d
instr(21,4)
    dec(d);
endinstr;

// ld d,n
instr(22,4)
    d = timed_read_byte(pc);
    pc++;
endinstr;

// rla
instr(23,4)
    int t = a >> 7;
    a = (a << 1) | (f & F_CARRY);
    f = (f & 0xc4) | (a & 0x28) | t;
endinstr;


// jr e
instr(24,4)
    jr(true);
endinstr;

// add hl/ix/iy,de
instr(25,4)
    addhl(d,e);
endinstr;

// ld a,(de)
instr(26,4)
    a = timed_read_byte(de);
endinstr;

// dec de
instr(27,6)
    de--;
endinstr;


// inc e
instr(28,4)
    inc(e);
endinstr;

// dec e
instr(29,4)
    dec(e);
endinstr;

// ld e,n
instr(30,4)
    e = timed_read_byte(pc);
    pc++;
endinstr;

// rra
instr(31,4)
    int t = a & F_CARRY;
    a = (a >> 1) | (f << 7);
    f = (f & 0xc4) | (a & 0x28) | t;
endinstr;


// jr nz,e
instr(32,4)
    jr(!(f & F_ZERO));
endinstr;

// ld hl,nn
instr(33,4)
    *pHlIxIy = timed_read_word(pc);
    pc += 2;
endinstr;

// ld (nn),hl
instr(34,4)
    WORD addr = timed_read_word(pc);
    pc += 2;
    timed_write_word(addr,*pHlIxIy);
endinstr;

// inc hl
instr(35,6)
    (*pHlIxIy)++;
endinstr;

// inc h
instr(36,4)
    inc(xh);
endinstr;

// dec h
instr(37,4)
    dec(xh);
endinstr;

// ld h,n
instr(38,4)
    xh = timed_read_byte(pc);
    pc++;
endinstr;

// daa
instr(39,4)
    BYTE incr=0, carry=cy;

    if ((f & 0x10) || (a & 0x0f) > 9)
        incr=6;

    if ((f & 1) || (a >> 4) > 9)
        incr |= 0x60;

    if (f & 2)
        suba(incr);
    else
    {
        if (a > 0x90 && (a & 15) > 9)
            incr|=0x60;

        adda(incr);
    }

    f = ((f | carry) & 0xfb) | parity(a);
endinstr;

// jr z,e
instr(40,4)
    jr(f & F_ZERO);
endinstr;

// add hl,hl
instr(41,4)
    addhl(xh,xl);
endinstr;

// ld hl,(nn)
instr(42,4)
    WORD addr = timed_read_word(pc);
    *pHlIxIy = timed_read_word(addr);
    pc += 2;
endinstr;

// dec hl
instr(43,6)
    (*pHlIxIy)--;
endinstr;


// inc l
instr(44,4)
    inc(xl);
endinstr;

// dec l
instr(45,4)
    dec(xl);
endinstr;

// ld l,n
instr(46,4)
    xl = timed_read_byte(pc);
    pc++;
endinstr;

// cpl
instr(47,4)
    a = ~a;
    f = (f & 0xc5) | (a & 0x28) | 0x12;
endinstr;


// jr nc,e
instr(48,4)
    jr(!(f & F_CARRY));
endinstr;

// ld sp,nn
instr(49,4)
    sp = timed_read_word(pc);
    pc += 2;
endinstr;

// ld (nn),a
instr(50,4)
    WORD addr = timed_read_word(pc);
    pc += 2;
    timed_write_byte(addr,a);
endinstr;

// inc sp
instr(51,6)
    sp++;
endinstr;


// inc (hl), inc (ix+d), inc (iy+d)
HLinstr(52)
    BYTE t=timed_read_byte(addr);
    inc(t);
    g_nLineCycle++;
    timed_write_byte(addr,t);
endinstr;

// dec (hl), dec (ix+d), dec (iy+d)
HLinstr(53)
    BYTE t=timed_read_byte(addr);
    dec(t);
    g_nLineCycle++;
    timed_write_byte(addr,t);
endinstr;

// ld (hl),n
HLinstr(54)
    BYTE t=timed_read_byte(pc);
    timed_write_byte(addr,t);
    pc++;
endinstr;

// scf
instr(55,4)
    f = (f & 0xc4) | 1 | (a & 0x28);
endinstr;


// jr c,e
instr(56,4)
    jr(f & F_CARRY);
endinstr;

// add hl,sp
instr(57,4)
    addhl(sp_h,sp_l);
endinstr;

// ld a,(nn)
instr(58,4)
    WORD addr=timed_read_word(pc);
    pc += 2;
    a=timed_read_byte(addr);
endinstr;

// dec sp
instr(59,6)
    sp--;
endinstr;


// inc a
instr(60,4)
    inc(a);
endinstr;

// dec a
instr(61,4)
    dec(a);
endinstr;

// ld a,n
instr(62,4)
    a=timed_read_byte(pc);
    pc++;
endinstr;

// ccf
instr(63,4)
    f=(f&0xc4)|(cy^F_CARRY)|(cy<<4)|(a&0x28);
endinstr;


instr(0x40,4)   {      } endinstr;                          // ld b,b
instr(0x41,4)   { b=c; } endinstr;                          // ld b,c
instr(0x42,4)   { b=d; } endinstr;                          // ld b,d
instr(0x43,4)   { b=e; } endinstr;                          // ld b,e
instr(0x44,4)   { b=xh; } endinstr;                         // ld b,h/ixh/iyh
instr(0x45,4)   { b=xl; } endinstr;                         // ld b,l/ixl/iyl
HLinstr(0x46)   { b = timed_read_byte(addr); }  endinstr;   // ld b,(hl)
instr(0x47,4)   { b=a; } endinstr;                          // ld b,a

instr(0x48,4)   { c=b;  } endinstr;                         // ld c,b
instr(0x49,4)   {      } endinstr;                          // ld c,c
instr(0x4a,4)   { c=d; } endinstr;                          // ld c,d
instr(0x4b,4)   { c=e; } endinstr;                          // ld c,e
instr(0x4c,4)   { c=xh; } endinstr;                         // ld c,h/ixh/iyh
instr(0x4d,4)   { c=xl; } endinstr;                         // ld c,l/ixl/iyl
HLinstr(0x4e)   { c = timed_read_byte(addr); }  endinstr;   // ld c,(hl)
instr(0x4f,4)   { c=a; } endinstr;                          // ld c,a


instr(0x50,4)   { d=b; } endinstr;                          // ld d,b
instr(0x51,4)   { d=c; } endinstr;                          // ld d,c
instr(0x52,4)   {      } endinstr;                          // ld d,d
instr(0x53,4)   { d=e; } endinstr;                          // ld d,e
instr(0x54,4)   { d=xh; } endinstr;                         // ld d,h/ixh/iyh
instr(0x55,4)   { d=xl; } endinstr;                         // ld d,l/ixl/iyl
HLinstr(0x56)   { d = timed_read_byte(addr); }  endinstr;   // ld d,(hl)
instr(0x57,4)   { d=a; } endinstr;                          // ld d,a

instr(0x58,4)   { e=b; } endinstr;                          // ld e,b
instr(0x59,4)   { e=c; } endinstr;                          // ld e,c
instr(0x5a,4)   { e=d; } endinstr;                          // ld e,d
instr(0x5b,4)   {      } endinstr;                          // ld e,e
instr(0x5c,4)   { e=xh; } endinstr;                         // ld e,h/ixh/iyh
instr(0x5d,4)   { e=xl; } endinstr;                         // ld e,l/ixl/iyl
HLinstr(0x5e)   { e = timed_read_byte(addr); }  endinstr;   // ld e,(hl)
instr(0x5f,4)   { e=a; } endinstr;                          // ld e,a


instr(0x60,4)   { xh=b; } endinstr;                         // ld h,b (or ixh/iyh ...)
instr(0x61,4)   { xh=c; } endinstr;                         // ld h,c
instr(0x62,4)   { xh=d; } endinstr;                         // ld h,d
instr(0x63,4)   { xh=e; } endinstr;                         // ld h,e
instr(0x64,4)   {       } endinstr;                         // ld h,h
instr(0x65,4)   { xh=xl; } endinstr;                        // ld h,l
HLinstr(0x66)   { h = timed_read_byte(addr); }  endinstr;   // ld h,(hl) - always into h
instr(0x67,4)   { xh=a; } endinstr;                         // ld h,a

instr(0x68,4)   { xl=b; } endinstr;                         // ld l,b (or ixl/iyl ...)
instr(0x69,4)   { xl=c; } endinstr;                         // ld l,c
instr(0x6a,4)   { xl=d; } endinstr;                         // ld l,d
instr(0x6b,4)   { xl=e; } endinstr;                         // ld l,e
instr(0x6c,4)   { xl=xh; } endinstr;                        // ld l,h
instr(0x6d,4)   {       } endinstr;                         // ld l,l
HLinstr(0x6e)   { l = timed_read_byte(addr); }  endinstr;   // ld l,(hl) - always into l
instr(0x6f,4)   { xl=a; } endinstr;                         // ld l,a


HLinstr(0x70)   { timed_write_byte(addr,b); } endinstr;     // ld (hl),b (or (ix+d)/(iy+d) ...)
HLinstr(0x71)   { timed_write_byte(addr,c); } endinstr;     // ld (hl),c
HLinstr(0x72)   { timed_write_byte(addr,d); } endinstr;     // ld (hl),d
HLinstr(0x73)   { timed_write_byte(addr,e); } endinstr;     // ld (hl),e
HLinstr(0x74)   { timed_write_byte(addr,h); } endinstr;     // ld (hl),h
HLinstr(0x75)   { timed_write_byte(addr,l); } endinstr;     // ld (hl),l
instr(0x76,4)   { pc--; } endinstr;                         // halt
HLinstr(0x77)   { timed_write_byte(addr,a); } endinstr;     // ld (hl),a

instr(0x78,4)   { a=b; } endinstr;                          // ld a,b
instr(0x79,4)   { a=c; } endinstr;                          // ld a,c
instr(0x7a,4)   { a=d; } endinstr;                          // ld a,d
instr(0x7b,4)   { a=e; } endinstr;                          // ld a,e
instr(0x7c,4)   { a=xh; } endinstr;                         // ld a,h/ixh/iyh
instr(0x7d,4)   { a=xl; } endinstr;                         // ld a,l/ixl/iyl
HLinstr(0x7e)   { a = timed_read_byte(addr); }  endinstr;   // ld a,(hl)
instr(0x7f,4)   {       } endinstr;                         // ld a,a



instr(0x80,4)   { adda(b); } endinstr;                      // add a,b
instr(0x81,4)   { adda(c); } endinstr;                      // add a,c
instr(0x82,4)   { adda(d); } endinstr;                      // add a,d
instr(0x83,4)   { adda(e); } endinstr;                      // add a,e
instr(0x84,4)   { adda(xh); } endinstr;                     // add a,h/ixh/iyh
instr(0x85,4)   { adda(xl); } endinstr;                     // add a,l/ixl/iyl
HLinstr(0x86)   { adda(timed_read_byte(addr)); } endinstr;  // add a,(hl)
instr(0x87,4)   { adda(a); } endinstr;                      // add a,a

instr(0x88,4)   { adca(b); } endinstr;                      // adc a,b
instr(0x89,4)   { adca(c); } endinstr;                      // adc a,c
instr(0x8a,4)   { adca(d); } endinstr;                      // adc a,d
instr(0x8b,4)   { adca(e); } endinstr;                      // adc a,e
instr(0x8c,4)   { adca(xh); } endinstr;                     // adc a,h/ixh/iyh
instr(0x8d,4)   { adca(xl); } endinstr;                     // adc a,l/ixl/iyl
HLinstr(0x8e)   { adca(timed_read_byte(addr)); } endinstr;  // adc a,(hl)
instr(0x8f,4)   { adca(a); } endinstr;                      // adc a,a


instr(0x90,4)   { suba(b); } endinstr;                      // sub b
instr(0x91,4)   { suba(c); } endinstr;                      // sub c
instr(0x92,4)   { suba(d); } endinstr;                      // sub d
instr(0x93,4)   { suba(e); } endinstr;                      // sub e
instr(0x94,4)   { suba(xh); } endinstr;                     // sub h/ixh/iyh
instr(0x95,4)   { suba(xl); } endinstr;                     // sub l/ixl/iyl
HLinstr(0x96)   { suba(timed_read_byte(addr)); } endinstr;  // sub (hl)
instr(0x97,4)   { suba(a); } endinstr;                      // sub a

instr(0x98,4)   { sbca(b); } endinstr;                      // sbc a,b
instr(0x99,4)   { sbca(c); } endinstr;                      // sbc a,c
instr(0x9a,4)   { sbca(d); } endinstr;                      // sbc a,d
instr(0x9b,4)   { sbca(e); } endinstr;                      // sbc a,e
instr(0x9c,4)   { sbca(xh); } endinstr;                     // sbc a,h/ixh/iyh
instr(0x9d,4)   { sbca(xl); } endinstr;                     // sbc a,l/ixl/iyl
HLinstr(0x9e)   { sbca(timed_read_byte(addr)); } endinstr;  // sbc a,(hl)
instr(0x9f,4)   { sbca(a); } endinstr;                      // sbc a,a


instr(0xa0,4)   { anda(b); } endinstr;                      // and b
instr(0xa1,4)   { anda(c); } endinstr;                      // and c
instr(0xa2,4)   { anda(d); } endinstr;                      // and d
instr(0xa3,4)   { anda(e); } endinstr;                      // and e
instr(0xa4,4)   { anda(xh); } endinstr;                     // and h/ixh/iyh
instr(0xa5,4)   { anda(xl); } endinstr;                     // and l/ixl/iyl
HLinstr(0xa6)   { anda(timed_read_byte(addr)); } endinstr;  // and (hl)
instr(0xa7,4)   { anda(a); } endinstr;                      // and a

instr(0xa8,4)   { xora(b); } endinstr;                      // xor b
instr(0xa9,4)   { xora(c); } endinstr;                      // xor c
instr(0xaa,4)   { xora(d); } endinstr;                      // xor d
instr(0xab,4)   { xora(e); } endinstr;                      // xor e
instr(0xac,4)   { xora(xh); } endinstr;                     // xor h/ixh/iyh
instr(0xad,4)   { xora(xl); } endinstr;                     // xor l/ixl/iyl
HLinstr(0xae)   { xora(timed_read_byte(addr)); } endinstr;  // xor (hl)
instr(0xaf,4)   { xora(a); } endinstr;                      // xor a


instr(0xb0,4)   { ora(b); } endinstr;                       // or b
instr(0xb1,4)   { ora(c); } endinstr;                       // or c
instr(0xb2,4)   { ora(d); } endinstr;                       // or d
instr(0xb3,4)   { ora(e); } endinstr;                       // or e
instr(0xb4,4)   { ora(xh); } endinstr;                      // or h/ixh/iyh
instr(0xb5,4)   { ora(xl); } endinstr;                      // or l/ixl/iyl
HLinstr(0xb6)   { ora(timed_read_byte(addr)); } endinstr;   // or (hl)
instr(0xb7,4)   { ora(a); } endinstr;                       // or a

instr(0xb8,4)   { cpa(b); } endinstr;                       // cp b
instr(0xb9,4)   { cpa(c); } endinstr;                       // cp c
instr(0xba,4)   { cpa(d); } endinstr;                       // cp d
instr(0xbb,4)   { cpa(e); } endinstr;                       // cp e
instr(0xbc,4)   { cpa(xh); } endinstr;                      // cp h/ixh/iyh
instr(0xbd,4)   { cpa(xl); } endinstr;                      // cp l/ixl/iyl
HLinstr(0xbe)   { cpa(timed_read_byte(addr)); } endinstr;   // cp (hl)
instr(0xbf,4)   { cpa(a); } endinstr;                       // cp a


// ret nz
instr(0xc0,5)
    ret(!(f & F_ZERO));
endinstr;

// pop bc
instr(0xc1,4)
    pop(bc);
endinstr;

//jp nz,nn
instr(0xc2,4)
    jp(!(f&F_ZERO));
endinstr;

//jp nn
instr(0xc3,4)
    jp(true);
endinstr;

// call nz,pq
instr(0xc4,4)
    call(!(f & F_ZERO));
endinstr;

// push bc
instr(0xc5,5)
    push(bc);
endinstr;

// add a,n
instr(0xc6,4)
    adda(timed_read_byte(pc));
    pc++;
endinstr;

// rst 0
instr(0xc7,5)
    push(pc);
    pc=0;
endinstr;

// ret z
instr(0xc8,5)
    ret(f & F_ZERO);
endinstr;

// ret
instr(0xc9,4)
    ret(true);
endinstr;

// jp z
instr(0xca,4)
    jp(f & F_ZERO);
endinstr;

// [cb prefix]
instr(0xcb,4)
#include "CBops.h"
endinstr;

// call z,nn
instr(0xcc,4)
    call(f & F_ZERO);
endinstr;

// call nn
instr(0xcd,4)
    call(true);
endinstr;

// adc a,n
instr(0xce,4)
    adca(timed_read_byte(pc));
    pc++;
endinstr;

// rst 8
instr(0xcf,5)
    push(pc);
    pc = 8;
endinstr;

// ret nc
instr(0xd0,5)
    ret(!cy);
endinstr;

// pop de
instr(0xd1,4)
    pop(de);
endinstr;

// jp nc,nn
instr(0xd2,4)
    jp(!cy);
endinstr;

// out (n), a
instr(0xd3,4)
    BYTE bPortLow = timed_read_byte(pc);
    PORT_ACCESS(bPortLow);
    out_byte((a<<8) + bPortLow,a);
    pc++;
endinstr;

// call nc,nn
instr(0xd4,4)
    call(!cy);
endinstr;

// push de
instr(0xd5,5)
    push(de);
endinstr;

// sub n
instr(0xd6,4)
    suba(timed_read_byte(pc));
    pc++;
endinstr;

// rst 16
instr(0xd7,5)
    push(pc);
    pc = 16;
endinstr;

// ret c
instr(0xd8,5)
    ret(cy);
endinstr;

// exx
instr(0xd9,4)
    swap(bc,alt_bc);
    swap(de,alt_de);
    swap(hl,alt_hl);
endinstr;

// jp c,nn
instr(0xda,4)
    jp(cy);
endinstr;

// in a,(n)
instr(0xdb,4)
    BYTE bPortLow = timed_read_byte(pc);
    PORT_ACCESS(bPortLow);
    a = in_byte((a<<8)+bPortLow);
    pc++;
endinstr;

// call c,nn
instr(0xdc,4)
    call(cy);
endinstr;

// [ix prefix]
instr(0xdd,4)
    pNewHlIxIy = &ix;
endinstr;

// sbc a,n
instr(0xde,4)
    sbca(timed_read_byte(pc));
    pc++;
endinstr;

// rst 24
instr(0xdf,5)
    push(pc);
    pc = 24;
endinstr;

// ret po
instr(0xe0,5)
    ret(!(f & F_PARITY));
endinstr;

// pop hl/ix/iy
instr(0xe1,4)
    pop(*pHlIxIy);
endinstr;

// jp po,nn
instr(0xe2,4)
    jp(!(f&F_PARITY));
endinstr;

// ex (sp),hl
instr(0xe3,4)
    WORD t = timed_read_word(sp);
    g_nLineCycle++;
    timed_write_word_reversed(sp,*pHlIxIy);
    *pHlIxIy = t;
    g_nLineCycle += 2;
endinstr;

// call po
instr(0xe4,4)
    call(!(f & F_PARITY));
endinstr;

// push hl
instr(0xe5,5)
    push(*pHlIxIy);
endinstr;

// and n
instr(0xe6,4)
    anda(timed_read_byte(pc));
    pc++;
endinstr;

// rst 32
instr(0xe7,5)
    push(pc);
    pc = 32;
endinstr;

// ret pe
instr(0xe8,5)
    ret(f&4);
endinstr;


// jp (hl), jp (ix), jp (iy)
instr(0xe9,4)
    pc = *pHlIxIy;
endinstr;

// jp pe,nn
instr(0xea,4)
    jp(f & F_PARITY);
endinstr;

// ex de,hl
instr(0xeb,4)
    swap(de,hl);
endinstr;

// call pe,nn
instr(0xec,4)
    call(f & F_PARITY);
endinstr;



// [ed prefix]
instr(0xed,4)
#include "EDops.h"
endinstr;

// xor n
instr(0xee,4)
    xora(timed_read_byte(pc));
    pc++;
endinstr;

// rst 40
instr(0xef,5)
    push(pc);
    pc = 40;
endinstr;

// ret p
instr(0xf0,5)
    ret(!(f & F_NEG));
endinstr;


// pop af
instr(0xf1,4)
    pop(af);
endinstr;

// jp p,nn
instr(0xf2,4)
    jp(!(f & F_NEG));
endinstr;

// di
instr(0xf3,4)
    iff1 = iff2 = 0;
endinstr;

// call p,nn
instr(0xf4,4)
    call(!(f & F_NEG));
endinstr;

// push af
instr(0xf5,5)
    push(af);
endinstr;

// or n
instr(0xf6,4)
    ora(timed_read_byte(pc));
    pc++;
endinstr;

// rst 48
instr(0xf7,5)
    push(pc);
    pc = 48;
endinstr;



// ret m
instr(0xf8,5)
    ret(f & F_NEG);
endinstr;

// ld sp,hl
instr(0xf9,6)
    sp = *pHlIxIy;
endinstr;

// jp m,nn
instr(0xfa,4)
    jp(f & F_NEG);
endinstr;

// ei
instr(0xfb,4)
    // According to Z80 specs, interrupts are not enabled until AFTER THE NEXT instruction
    // In CPU.cpp we'll disallow interrupts if the previous instruction was EI or DI
    iff1 = iff2 = 1;
endinstr;


// call m,nn
instr(0xfc,4)
    call(f & F_NEG);
endinstr;

// [iy prefix]
instr(0xfd,4)
    pNewHlIxIy = &iy;
endinstr;

// cp n
instr(0xfe,4)
    cpa(timed_read_byte(pc));
    pc++;
endinstr;

// rst 56
instr(0xff,5)
    push(pc);
    pc = 56;
endinstr;

#ifdef _DEBUG
// Nothing should reach here!
default:
    DebugBreak();
#endif
