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


#define edinstr(opcode)         case opcode: {

#define input(reg)              {                                                                               \
                                    reg = in_byte(bc);                                                          \
                                    f = (f & 1) | (reg & 0xa8) | ((!reg) << 6) | parity(reg);                   \
                                }

// Ports that catch the attention of the ASIC cause an extra delay
#define C_PORT_ACCESS           do { if (c >= 0xf8) g_nLineCycle +=4; PORT_ACCESS(c); } while (0)

#define in_out_c_instr(opcode)  case opcode:                                                                    \
                                {                                                                               \
                                    C_PORT_ACCESS;


#define sbchl(x) {    g_nLineCycle += 8;\
                      WORD z=(x);\
                      unsigned long t=(hl-z-cy)&0x1ffff;\
                      f=((t>>8)&0xa8)|(t>>16)|2|\
                        (((hl&0xfff)<(z&0xfff)+cy)<<4)|\
                        (((hl^z)&(hl^t)&0x8000)>>13)|\
                        ((!(t&0xffff))<<6)|2;\
                      l=t;\
                      h=t>>8;\
                   }

#define adchl(x) {    g_nLineCycle += 8;\
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
edinstr(0x42)
    sbchl(bc);
endinstr;

// ld (nn), bc
edinstr(0x43)
{
    WORD addr=timed_read_word(pc);
    pc += 2;
    timed_write_word(addr,bc);
}
endinstr;

// neg
edinstr(0x44)
    neg;
endinstr;

// retn
edinstr(0x45)
    retn;
endinstr;

// im 0
edinstr(0x46)
    im = 0;
endinstr;

// ld i,a
edinstr(0x47)
    i = a;
    g_nLineCycle += 4;
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
edinstr(0x4a)
    adchl(bc);
endinstr;

// ld bc,(nn)
edinstr(0x4b)
{
    WORD addr = timed_read_word(pc);
    pc += 2;
    bc = timed_read_word(addr);
}
endinstr;


// neg
edinstr(0x4c)
    neg;
endinstr;

// retn
edinstr(0x4d)
    retn;
endinstr;

// im 0/1
edinstr(0x4e)
    im = 0;
endinstr;

// ld r,a
edinstr(0x4f)
    radjust = r = a;
    g_nLineCycle += 4;
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
edinstr(0x52)
    sbchl(de);
endinstr;

// ld (nn),de
edinstr(0x53)
{
    WORD addr=timed_read_word(pc);
    pc += 2;
    timed_write_word(addr,de);
}
endinstr;


// neg
edinstr(0x54)
    neg;
endinstr;

// retn
edinstr(0x55)
    retn;
endinstr;

// im 1
edinstr(0x56)
    im = 1;
endinstr;

// ld a,i
edinstr(0x57)
    a = i;
    f = (f & 1) | (a & 0xa8) | ((!a) << 6) | (iff2 << 2);
    g_nLineCycle += 4;
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
edinstr(0x5a)
    adchl(de);
endinstr;

// ld de,(nn)
edinstr(0x5b)
{
    WORD addr = timed_read_word(pc);
    pc += 2;
    de = timed_read_word(addr);
}
endinstr;

// neg
edinstr(0x5c)
    neg;
endinstr;

// retn
edinstr(0x5d)
    retn;
endinstr;

// im 2
edinstr(0x5e)
    im = 2;
endinstr;

// ld a,r
edinstr(0x5f)
{
    // only the bottom 7 bits of R are advanced by memory refresh, so the top bit is preserved
    r = (r & 0x80) | (radjust & 0x7f);
    a = r;
    f = (f & 1) | (a & 0xa8) | ((!a) << 6) | (iff2 << 2);
    g_nLineCycle += 4;
}
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
edinstr(0x62)
    sbchl(hl);
endinstr;

// ld (nn), hl
edinstr(0x63)
{
    WORD addr=timed_read_word(pc);
    pc += 2;
    timed_write_word(addr,hl);
}
endinstr;

// neg
edinstr(0x64)
    neg;
endinstr;

// reti
edinstr(0x65)
    ret;
endinstr;

// im 0
edinstr(0x66)
    im = 0;
endinstr;

// rrd
edinstr(0x67)
{
    BYTE t=timed_read_byte(hl);
    BYTE u=(a<<4)|(t>>4);
    a=(a&0xf0)|(t&0x0f);
    timed_write_byte(hl,u);
    f=(f&1)|(a&0xa8)|((!a)<<6)|parity(a);
    g_nLineCycle += 4;
}
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
edinstr(0x6a)
    adchl(hl);
endinstr;

// ld hl,(nn)
edinstr(0x6b)
{
    WORD addr = timed_read_word(pc);
    pc += 2;
    hl = timed_read_word(addr);
}
endinstr;


// neg
edinstr(0x6c)
    neg;
endinstr;

// reti
edinstr(0x6d)
    ret;
endinstr;

// im 0/1
edinstr(0x6e)
    im = 0;
endinstr;

// rld
edinstr(0x6f)
{
    BYTE t = timed_read_byte(hl);
    BYTE u = (a & 0x0f) | (t << 4);
    a = (a & 0xf0) | (t >> 4);
    timed_write_byte(hl,u);
    f = (f & 1) | (a & 0xa8) | ((!a) << 6) | parity(a);
    g_nLineCycle += 4;
}
endinstr;


// in x,(c)   [result discarded, but flags still set]
in_out_c_instr(0x70)
{
    BYTE x;
    input(x);
}
endinstr;

// out (c),0    [output zero]
in_out_c_instr(0x71)
    out_byte(bc,0);
endinstr;

// sbc hl,sp
edinstr(0x72)
    sbchl(sp);
endinstr;

// ld (nn),sp
edinstr(0x73)
{
    WORD addr=timed_read_word(pc);
    pc += 2;
    timed_write_word(addr,sp);
}
endinstr;


// neg
edinstr(0x74)
    neg;
endinstr;

// reti
edinstr(0x75)
    ret;
endinstr;

// im 1
edinstr(0x76)
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
edinstr(0x7a)
    adchl(sp);
endinstr;

// ld sp,(nn)
edinstr(0x7b)
{
    WORD addr = timed_read_word(pc);
    pc += 2;
    sp = timed_read_word(addr);
}
endinstr;

// neg
edinstr(0x7c)
    neg;
endinstr;

// reti
edinstr(0x7d)
    ret;
endinstr;

// im 2
edinstr(0x7e)
    im = 2;
endinstr;

// nop for 0x7f to 0x9f

// ldi
edinstr(0xa0)
{
    BYTE x = timed_read_byte(hl);
    timed_write_byte(de,x);
    g_nLineCycle += 4;

    hl++;
    de++;
    bc--;
    f = (f & 0xc1) | (x & 0x28) | (((b | c) > 0) << 2);
}
endinstr;

// cpi
edinstr(0xa1)
{
    BYTE carry = cy;
    cpa(timed_read_byte(hl));
    g_nLineCycle += 4;

    hl++;
    bc--;
    f = (f & 0xfa) | carry | (((b | c) > 0) << 2);
}
endinstr;

// ini
edinstr(0xa2)
{
    g_nLineCycle += 4;
    PORT_ACCESS(c);
    BYTE t = in_byte(bc);
    timed_write_byte(hl,t);

    hl++;
    b--;
    f = (b & 0xa8) | ((!b) << 6) | 2 | ((parity(b) ^ c) & 4);
}
endinstr;

// outi
edinstr(0xa3)
{
// I can't determine the correct flags outcome for the block OUT instructions.
// Spec says that the carry flag is left unchanged and N is set to 1, but that
// doesn't seem to be the case...

    BYTE x = timed_read_byte(hl);
    b--;
    C_PORT_ACCESS;
    out_byte(bc,x);

    hl++;
    f = (f & 1) | 0x12 | (b & 0xa8) | ((!b) << 6);
}
endinstr;

// nop for 0xa4 -> 0xa7

// ldd
edinstr(0xa8)
{
    BYTE x = timed_read_byte(hl);
    timed_write_byte(de,x);
    g_nLineCycle += 4;

    hl--;
    de--;
    bc--;
    f = (f & 0xc1) | (x & 0x28) | (((b | c) > 0) << 2);
}
endinstr;

// cpd
edinstr(0xa9)
{
    BYTE carry = cy;
    cpa(timed_read_byte(hl));
    g_nLineCycle += 4;

    hl--;
    bc--;
    f = (f & 0xfa) | carry | (((b | c) > 0) << 2);
}
endinstr;

// ind
edinstr(0xaa)
{
    g_nLineCycle += 4;
    PORT_ACCESS(c);
    BYTE t = in_byte(bc);
    timed_write_byte(hl,t);

    hl--;
    b--;
    f = (b & 0xa8) | ((!b) << 6) | 2 | ((parity(b) ^ c ^ 4) & 4);
}
endinstr;

// outd
edinstr(0xab)
{
    BYTE x = timed_read_byte(hl);
    b--;
    C_PORT_ACCESS;
    out_byte(bc,x);

    hl--;
    f = (f & 1) | 0x12 | (b & 0xa8) | ((!b) << 6);
}
endinstr;

// nop for 0xac -> 0xaf



// Note: the Z80 implements "*R" as "*" followed by JR -2.  No reason to change this...

// ldir
edinstr(0xb0)
{
    BYTE x = timed_read_byte(hl);
    timed_write_byte(de,x);
    g_nLineCycle += 4;

    hl++;
    de++;
    bc--;
    f = (f & 0xc1) | (x & 0x28) | (((b | c) > 0) << 2);

    if (b|c)
    {
        MEM_ACCESS(pc - 1);
        pc -= 2;
    }
}
endinstr;

// cpir
edinstr(0xb1)
{
    BYTE carry = cy;
    cpa(timed_read_byte(hl));
    g_nLineCycle += 4;

    hl++;
    bc--;
    f = (f & 0xfa) | carry | (((b | c) > 0) << 2);

    if ((f & 0x44) == 4)
    {
        MEM_ACCESS(pc - 1);
        pc -= 2;
        g_nLineCycle += 4;
    }
}
endinstr;

// inir
edinstr(0xb2)
{
    g_nLineCycle += 4;
    PORT_ACCESS(c);
    BYTE t=in_byte(bc);
    timed_write_byte(hl,t);

    hl++;
    b--;
    f = (b & 0xa8) | ((b > 0) << 6) | 2 | ((parity(b) ^ c) & 4);

    if (b)
    {
        g_nLineCycle += 4;
        pc -= 2;
    }
}
endinstr;

// otir
edinstr(0xb3)
{
    BYTE x = timed_read_byte(hl);
    b--;
    C_PORT_ACCESS;
    out_byte(bc,x);

    hl++;
    f = (f & 1) | 0x12 | (b & 0xa8) | ((!b) << 6);

    if (b) {
        g_nLineCycle += 4;
        pc -= 2;
    }
}
endinstr;

// nop for 0xb4 -> 0xb7


// lddr
edinstr(0xb8)
{
    BYTE x=timed_read_byte(hl);
    timed_write_byte(de,x);
    g_nLineCycle += 4;

    hl--;
    de--;
    bc--;
    f = (f & 0xc1) | (x & 0x28) | (((b | c) > 0) << 2);

    if (b | c)
    {
        MEM_ACCESS(pc - 1);
        pc -= 2;
    }
}
endinstr;

// cpdr
edinstr(0xb9)
{
    BYTE carry = cy;
    cpa(timed_read_byte(hl));
    g_nLineCycle += 4;

    hl--;
    bc--;
    f = (f & 0xfa) | carry | (((b | c) > 0) <<2 );

    if ((f & 0x44) == 4)
    {
        MEM_ACCESS(pc - 1);
        pc -= 2;
        g_nLineCycle += 4;
    }
}
endinstr;

// indr
edinstr(0xba)
{
    g_nLineCycle += 4;
    PORT_ACCESS(c);
    BYTE t = in_byte(bc);
    timed_write_byte(hl,t);

    hl--;
    b--;
    f = (b & 0xa8) | ((b > 0) << 6) | 2 | ((parity(b) ^ c ^ 4) & 4);

    if (b)
    {
        g_nLineCycle += 4;
        pc -= 2;
    }
}
endinstr;

// otdr
edinstr(0xbb)
{
    BYTE x=timed_read_byte(hl);
    b--;
    C_PORT_ACCESS;
    out_byte(bc,x);

    hl--;

    f = (f & 1) | 0x12 | (b & 0xa8) | ((!b) << 6);
    if (b)
    {
        g_nLineCycle += 4;
        pc -= 2;
    }
}
endinstr;

// nop for 0xbc -> 0xff

// Anything not explicitly handled is effectively a 2 byte NOP (with predictable timing) (that's already accounted for)
default:
    break;
}
}
