// Part of SimCoupe - A SAM Coupé emulator
//
// EDops.h: Z80 instruction set emulation (from xz80)
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

// Changes 2000-2001 by Dave Laundon
//  - replaced all instruction timings with raw memory and I/O timings

// Changes 1999-2001 by Simon Owen
//  - Fixed INI/IND so the zero flag is set when B becomes zero


// Basic instruction header, specifying opcode and nominal T-States of the first M-Cycle (AFTER the ED code)
// The first three T-States of the first M-Cycle are already accounted for
#define edinstr(opcode, m1states)   case opcode: { \
                                        g_nLineCycle += m1states - 3;

#define input(reg)              {                                                                               \
                                    reg = in_byte(bc);                                                          \
                                    f = (f & 1) | (reg & 0xa8) | ((!reg) << 6) | parity(reg);                   \
                                }

#define in_out_c_instr(opcode)  edinstr(opcode,4)                                                               \
                                    PORT_ACCESS(c);


#define sbchl(x) {    g_nLineCycle += 7;\
                      WORD z=(x);\
                      unsigned long t=(hl-z-cy)&0x1ffff;\
                      f=((t>>8)&0xa8)|(t>>16)|2|\
                        (((hl&0xfff)<(z&0xfff)+cy)<<4)|\
                        (((hl^z)&(hl^t)&0x8000)>>13)|\
                        ((!(t&0xffff))<<6)|2;\
                      l=t;\
                      h=t>>8;\
                   }

#define adchl(x) {    g_nLineCycle += 7;\
                      WORD z=(x);\
                      unsigned long t=hl+z+cy;\
                      f=((t>>8)&0xa8)|(t>>16)|\
                        (((hl&0xfff)+(z&0xfff)+cy>0xfff)<<4)|\
                        (((~hl^z)&(hl^t)&0x8000)>>13)|\
                        ((!(t&0xffff))<<6)|2;\
                      l=t;\
                      h=t>>8;\
                 }

#define neg (a=-a,\
            f=(a&0xa8)|((!a)<<6)|(((a&15)>0)<<4)|((a==128)<<2)|2|(a>0))

// Load; increment; [repeat]
#define ldi(loop)   do { \
    BYTE x = timed_read_byte(hl); \
    timed_write_byte(de,x); \
    g_nLineCycle += 2; \
    hl++; \
    de++; \
    bc--; \
    f = (f & 0xc1) | (x & 0x28) | (((b | c) > 0) << 2); \
    if (loop) { \
        g_nLineCycle += 5; \
        pc -= 2; \
    } \
} while (0)

// Load; decrement; [repeat]
#define ldd(loop)   do { \
    BYTE x = timed_read_byte(hl); \
    timed_write_byte(de,x); \
    g_nLineCycle += 2; \
    hl--; \
    de--; \
    bc--; \
    f = (f & 0xc1) | (x & 0x28) | (((b | c) > 0) << 2); \
    if (loop) { \
        g_nLineCycle += 5; \
        pc -= 2; \
    } \
} while (0)

// Compare; increment; [repeat]
#define cpi(loop)   do { \
    BYTE carry = cy; \
    cpa(timed_read_byte(hl)); \
    g_nLineCycle += 2; \
    hl++; \
    bc--; \
    f = (f & 0xfa) | carry | (((b | c) > 0) << 2); \
    if (loop) { \
        g_nLineCycle += 5; \
        pc -= 2; \
    } \
} while (0)

// Compare; decrement; [repeat]
#define cpd(loop)   do { \
    BYTE carry = cy; \
    cpa(timed_read_byte(hl)); \
    g_nLineCycle += 2; \
    hl--; \
    bc--; \
    f = (f & 0xfa) | carry | (((b | c) > 0) << 2); \
    if (loop) { \
        g_nLineCycle += 5; \
        pc -= 2; \
    } \
} while (0)

// Input; increment; [repeat]
#define ini(loop)   do { \
    PORT_ACCESS(c); \
    BYTE t = in_byte(bc); \
    timed_write_byte(hl,t); \
    hl++; \
    b--; \
    f = (b & 0xa8) | ((!b) << 6) | 2 | ((parity(b) ^ c) & 4); \
    if (loop) { \
        g_nLineCycle += 5; \
        pc -= 2; \
    } \
} while (0)

// Input; decrement; [repeat]
#define ind(loop)   do { \
    PORT_ACCESS(c); \
    BYTE t = in_byte(bc); \
    timed_write_byte(hl,t); \
    hl--; \
    b--; \
    f = (b & 0xa8) | ((!b) << 6) | 2 | ((parity(b) ^ c ^ 4) & 4); \
    if (loop) { \
        g_nLineCycle += 5; \
        pc -= 2; \
    } \
} while (0)

// I can't determine the correct flags outcome for the block OUT instructions.
// Spec says that the carry flag is left unchanged and N is set to 1, but that
// doesn't seem to be the case...

// Output; increment; [repeat]
#define oti(loop)  do { \
    BYTE x = timed_read_byte(hl); \
    b--; \
    PORT_ACCESS(c); \
    out_byte(bc,x); \
    hl++; \
    f = (f & 1) | 0x12 | (b & 0xa8) | ((!b) << 6); \
    if (loop) { \
        g_nLineCycle += 5; \
        pc -= 2; \
    } \
} while (0)

// Output; decrement; [repeat]
#define otd(loop)   do { \
    BYTE x = timed_read_byte(hl); \
    b--; \
    PORT_ACCESS(c); \
    out_byte(bc,x); \
    hl--; \
    f = (f & 1) | 0x12 | (b & 0xa8) | ((!b) << 6); \
    if (loop) { \
        g_nLineCycle += 5; \
        pc -= 2; \
    } \
} while (0)


{
    BYTE op=timed_read_byte(pc);
    pc++;
    radjust++;

    switch(op)
    {

// in b,(c)
in_out_c_instr(0x40)
    input(b);
endinstr;

// out (c),b
in_out_c_instr(0x41)
    out_byte(bc,b);
endinstr;

// sbc hl, bc
edinstr(0x42,4)
    sbchl(bc);
endinstr;

// ld (nn), bc
edinstr(0x43,4)
    WORD addr=timed_read_word(pc);
    pc += 2;
    timed_write_word(addr,bc);
endinstr;

// neg
edinstr(0x44,4)
    neg;
endinstr;

// retn
edinstr(0x45,4)
    retn;
endinstr;

// im 0
edinstr(0x46,4)
    im = 0;
endinstr;

// ld i,a
edinstr(0x47,5)
    i = a;
endinstr;

// in c,(c)
in_out_c_instr(0x48)
    input(c);
endinstr;

// out (c),c
in_out_c_instr(0x49)
    out_byte(bc,c);
endinstr;

// adc hl,bc
edinstr(0x4a,4)
    adchl(bc);
endinstr;

// ld bc,(nn)
edinstr(0x4b,4)
    WORD addr = timed_read_word(pc);
    pc += 2;
    bc = timed_read_word(addr);
endinstr;


// neg
edinstr(0x4c,4)
    neg;
endinstr;

// retn
edinstr(0x4d,4)
    retn;
endinstr;

// im 0/1
edinstr(0x4e,4)
    im = 0;
endinstr;

// ld r,a
edinstr(0x4f,5)
    radjust = r = a;
endinstr;


// in d,(c)
in_out_c_instr(0x50)
    input(d);
endinstr;

// out (c),d
in_out_c_instr(0x51)
    out_byte(bc,d);
endinstr;

// sbc hl,de
edinstr(0x52,4)
    sbchl(de);
endinstr;

// ld (nn),de
edinstr(0x53,4)
    WORD addr=timed_read_word(pc);
    pc += 2;
    timed_write_word(addr,de);
endinstr;


// neg
edinstr(0x54,4)
    neg;
endinstr;

// retn
edinstr(0x55,4)
    retn;
endinstr;

// im 1
edinstr(0x56,4)
    im = 1;
endinstr;

// ld a,i
edinstr(0x57,5)
    a = i;
    f = (f & 1) | (a & 0xa8) | ((!a) << 6) | (iff2 << 2);
endinstr;

// in e,(c)
in_out_c_instr(0x58)
    input(e);
endinstr;

// out (c),e
in_out_c_instr(0x59)
    out_byte(bc,e);
endinstr;

// adc hl,de
edinstr(0x5a,4)
    adchl(de);
endinstr;

// ld de,(nn)
edinstr(0x5b,4)
    WORD addr = timed_read_word(pc);
    pc += 2;
    de = timed_read_word(addr);
endinstr;

// neg
edinstr(0x5c,4)
    neg;
endinstr;

// retn
edinstr(0x5d,4)
    retn;
endinstr;

// im 2
edinstr(0x5e,4)
    im = 2;
endinstr;

// ld a,r
edinstr(0x5f,5)
    // Only the bottom 7 bits of R are advanced by memory refresh, so the top bit is preserved
    r = (r & 0x80) | (radjust & 0x7f);
    a = r;
    f = (f & 1) | (a & 0xa8) | ((!a) << 6) | (iff2 << 2);
endinstr;


// in h,(c)
in_out_c_instr(0x60)
    input(h);
endinstr;

// out (c),h
in_out_c_instr(0x61)
    out_byte(bc,h);
endinstr;

// sbc hl,hl
edinstr(0x62,4)
    sbchl(hl);
endinstr;

// ld (nn), hl
edinstr(0x63,4)
    WORD addr=timed_read_word(pc);
    pc += 2;
    timed_write_word(addr,hl);
endinstr;

// neg
edinstr(0x64,4)
    neg;
endinstr;

// reti
edinstr(0x65,4)
    ret(true);
endinstr;

// im 0
edinstr(0x66,4)
    im = 0;
endinstr;

// rrd
edinstr(0x67,4)
    BYTE t=timed_read_byte(hl);
    BYTE u=(a<<4)|(t>>4);
    a=(a&0xf0)|(t&0x0f);
    g_nLineCycle += 4;
    timed_write_byte(hl,u);
    f=(f&1)|(a&0xa8)|((!a)<<6)|parity(a);
endinstr;


// in l,(c)
in_out_c_instr(0x68)
    input(l);
endinstr;

// out (c),l
in_out_c_instr(0x69)
    out_byte(bc,l);
endinstr;

// adc hl,hl
edinstr(0x6a,4)
    adchl(hl);
endinstr;

// ld hl,(nn)
edinstr(0x6b,4)
    WORD addr = timed_read_word(pc);
    pc += 2;
    hl = timed_read_word(addr);
endinstr;


// neg
edinstr(0x6c,4)
    neg;
endinstr;

// reti
edinstr(0x6d,4)
    ret(true);
endinstr;

// im 0/1
edinstr(0x6e,4)
    im = 0;
endinstr;

// rld
edinstr(0x6f,4)
    BYTE t = timed_read_byte(hl);
    BYTE u = (a & 0x0f) | (t << 4);
    a = (a & 0xf0) | (t >> 4);
    g_nLineCycle += 4;
    timed_write_byte(hl,u);
    f = (f & 1) | (a & 0xa8) | ((!a) << 6) | parity(a);
endinstr;


// in x,(c)   [result discarded, but flags still set]
in_out_c_instr(0x70)
    BYTE x;
    input(x);
endinstr;

// out (c),0    [output zero]
in_out_c_instr(0x71)
    out_byte(bc,0);
endinstr;

// sbc hl,sp
edinstr(0x72,4)
    sbchl(sp);
endinstr;

// ld (nn),sp
edinstr(0x73,4)
    WORD addr=timed_read_word(pc);
    pc += 2;
    timed_write_word(addr,sp);
endinstr;


// neg
edinstr(0x74,4)
    neg;
endinstr;

// reti
edinstr(0x75,4)
    ret(true);
endinstr;

// im 1
edinstr(0x76,4)
    im = 1;
endinstr;


// nop for 0x77


// in a,(c)
in_out_c_instr(0x78)
    input(a);
endinstr;

// out (c),a
in_out_c_instr(0x79)
    out_byte(bc,a);
endinstr;

// adc hl,sp
edinstr(0x7a,4)
    adchl(sp);
endinstr;

// ld sp,(nn)
edinstr(0x7b,4)
    WORD addr = timed_read_word(pc);
    pc += 2;
    sp = timed_read_word(addr);
endinstr;

// neg
edinstr(0x7c,4)
    neg;
endinstr;

// reti
edinstr(0x7d,4)
    ret(true);
endinstr;

// im 2
edinstr(0x7e,4)
    im = 2;
endinstr;

// nop for 0x7f to 0x9f

// ldi
edinstr(0xa0,4)
    ldi(false);
endinstr;

// cpi
edinstr(0xa1,4)
    cpi(false);
endinstr;

// ini
edinstr(0xa2,5)
    ini(false);
endinstr;

// outi
edinstr(0xa3,5)
    oti(false);
endinstr;

// nop for 0xa4 -> 0xa7

// ldd
edinstr(0xa8,4)
    ldd(false);
endinstr;

// cpd
edinstr(0xa9,4)
    cpd(false);
endinstr;

// ind
edinstr(0xaa,5)
    ind(false);
endinstr;

// outd
edinstr(0xab,5)
    otd(false);
endinstr;

// nop for 0xac -> 0xaf



// Note: the Z80 implements "*R" as "*" followed by JR -2.  No reason to change this...

// ldir
edinstr(0xb0,4)
    ldi(b|c);
endinstr;

// cpir
edinstr(0xb1,4)
    cpi((f & 0x44) == 4);
endinstr;

// inir
edinstr(0xb2,5)
    ini(b);
endinstr;

// otir
edinstr(0xb3,5)
    oti(b);
endinstr;

// nop for 0xb4 -> 0xb7


// lddr
edinstr(0xb8,4)
    ldd(b|c);
endinstr;

// cpdr
edinstr(0xb9,4)
    cpd((f & 0x44) == 4);
endinstr;

// indr
edinstr(0xba,5)
    ind(b);
endinstr;

// otdr
edinstr(0xbb,5)
    otd(b);
endinstr;

// nop for 0xbc -> 0xff

// Anything not explicitly handled is effectively a 2 byte NOP (with predictable timing)
// Only the first three T-States are already accounted for
default:
    g_nLineCycle++;
    break;
}
}
