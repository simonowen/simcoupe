// Part of SimCoupe - A SAM Coupe emulator
//
// EDops.h: Z80 instruction set emulation (from xz80)
//
//  Copyright (c) 1994 Ian Collier
//  Copyright (c) 1999-2003 by Dave Laundon
//  Copyright (c) 1999-2003 by Simon Owen
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
#define edinstr(m1states, opcode)   case opcode: { \
                                        g_nLineCycle += m1states - 3;

// in r,(c)
#define in_c(x)         ( \
                            PORT_ACCESS(c), \
                            x = in_byte(bc), \
                            f = cy | parity(x) \
                        )

// out (c),r
#define out_c(x)        ( \
                            PORT_ACCESS(c), \
                            out_byte(bc,x) \
                        )

// sbc hl,rr
#define sbc_hl(x)       do { \
                            g_nLineCycle += 7; \
                            WORD z = (x); \
                            DWORD y = hl - z - cy; \
                            f = (((y & 0xb800) ^ ((hl ^ z) & 0x1000)) >> 8) |       /* S, 5, H, 3 */ \
                                ((y >> 16) & 1) |                                   /* C          */ \
                                (((hl ^ z) & (hl ^ y) & 0x8000) >> 13) |            /* V          */ \
                                2;                                                  /* N          */ \
                            f |= (!(hl = y)) << 6;                                  /* Z          */ \
                        } while (0)

// adc hl,rr
#define adc_hl(x)       do { \
                            g_nLineCycle += 7; \
                            WORD z = (x); \
                            DWORD y = hl + z + cy; \
                            f = (((y & 0xb800) ^ ((hl ^ z) & 0x1000)) >> 8) |       /* S, 5, H, 3 */ \
                                (y >> 16) |                                         /* C          */ \
                                (((hl ^ ~z) & (hl ^ y) & 0x8000) >> 13);            /* V          */ \
                            f |= (!(hl = y)) << 6;                                  /* Z          */ \
                        } while (0)

// negate a
#define neg             ( \
                            a = -a, \
                            f = (a & 0xa8) |                                        /* S, 5, 3    */ \
                                (((a & 0xf) != 0) << 4) |                           /* H          */ \
                                (a != 0) |                                          /* C          */ \
                                ((a == 0x80) << 2) |                                /* V          */ \
                                2 |                                                 /* N          */ \
                                ((!a) << 6)                                         /* Z          */ \
                        )

// Load; increment; [repeat]
#define ldi(loop)       do { \
                            BYTE x = timed_read_byte(hl); \
                            timed_write_byte(de,x); \
                            g_nLineCycle += 2; \
                            hl++; \
                            de++; \
                            bc--; \
                            f = (f & 0xc1) | (x & 0x28) | ((bc != 0) << 2); \
                            if (loop) { \
                                g_nLineCycle += 5; \
                                pc -= 2; \
                            } \
                        } while (0)

// Load; decrement; [repeat]
#define ldd(loop)       do { \
                            BYTE x = timed_read_byte(hl); \
                            timed_write_byte(de,x); \
                            g_nLineCycle += 2; \
                            hl--; \
                            de--; \
                            bc--; \
                            f = (f & 0xc1) | (x & 0x28) | ((bc != 0) << 2); \
                            if (loop) { \
                                g_nLineCycle += 5; \
                                pc -= 2; \
                            } \
                        } while (0)

// Compare; increment; [repeat]
#define cpi(loop)       do { \
                            BYTE carry = cy; \
                            cp_a(timed_read_byte(hl)); \
                            g_nLineCycle += 2; \
                            hl++; \
                            bc--; \
                            f = (f & 0xfa) | carry | ((bc != 0) << 2); \
                            if (loop) { \
                                g_nLineCycle += 5; \
                                pc -= 2; \
                            } \
                        } while (0)

// Compare; decrement; [repeat]
#define cpd(loop)       do { \
                            BYTE carry = cy; \
                            cp_a(timed_read_byte(hl)); \
                            g_nLineCycle += 2; \
                            hl--; \
                            bc--; \
                            f = (f & 0xfa) | carry | ((bc != 0) << 2); \
                            if (loop) { \
                                g_nLineCycle += 5; \
                                pc -= 2; \
                            } \
                        } while (0)

// Input; increment; [repeat]
#define ini(loop)       do { \
                            PORT_ACCESS(c); \
                            BYTE t = in_byte(bc); \
                            timed_write_byte(hl,t); \
                            hl++; \
                            b--; \
                            f = F_NADD | (parity(b) ^ (c & F_PARITY)); \
                            if (loop) { \
                                g_nLineCycle += 5; \
                                pc -= 2; \
                            } \
                        } while (0)

// Input; decrement; [repeat]
#define ind(loop)       do { \
                            PORT_ACCESS(c); \
                            BYTE t = in_byte(bc); \
                            timed_write_byte(hl,t); \
                            hl--; \
                            b--; \
                            f = F_NADD | (parity(b) ^ (c & F_PARITY) ^ F_PARITY); \
                            if (loop) { \
                                g_nLineCycle += 5; \
                                pc -= 2; \
                            } \
                        } while (0)

// I can't determine the correct flags outcome for the block OUT instructions.
// Spec says that the carry flag is left unchanged and N is set to 1, but that
// doesn't seem to be the case...

// Output; increment; [repeat]
#define oti(loop)       do { \
                            BYTE x = timed_read_byte(hl); \
                            b--; \
                            PORT_ACCESS(c); \
                            out_byte(bc,x); \
                            hl++; \
                            f = cy | (b & 0xa8) | ((!b) << 6) | F_HCARRY | F_NADD; \
                            if (loop) { \
                                g_nLineCycle += 5; \
                                pc -= 2; \
                            } \
                        } while (0)

// Output; decrement; [repeat]
#define otd(loop)       do { \
                            BYTE x = timed_read_byte(hl); \
                            b--; \
                            PORT_ACCESS(c); \
                            out_byte(bc,x); \
                            hl--; \
                            f = cy | (b & 0xa8) | ((!b) << 6) | F_HCARRY | F_NADD; \
                            if (loop) { \
                                g_nLineCycle += 5; \
                                pc -= 2; \
                            } \
                        } while (0)


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


BYTE op = timed_read_code_byte(pc++);
radjust++;

switch(op)
{


edinstr(4,0100) in_c(b);                                            endinstr;   // in b,(c)
edinstr(4,0110) in_c(c);                                            endinstr;   // in c,(c)
edinstr(4,0120) in_c(d);                                            endinstr;   // in d,(c)
edinstr(4,0130) in_c(e);                                            endinstr;   // in e,(c)
edinstr(4,0140) in_c(h);                                            endinstr;   // in h,(c)
edinstr(4,0150) in_c(l);                                            endinstr;   // in l,(c)
edinstr(4,0160) BYTE x; in_c(x);                                    endinstr;   // in x,(c) [result discarded, but flags still set]
edinstr(4,0170) in_c(a);                                            endinstr;   // in a,(c)


edinstr(4,0101) out_c(b);                                           endinstr;   // out (c),b
edinstr(4,0111) out_c(c);                                           endinstr;   // out (c),c
edinstr(4,0121) out_c(d);                                           endinstr;   // out (c),d
edinstr(4,0131) out_c(e);                                           endinstr;   // out (c),e
edinstr(4,0141) out_c(h);                                           endinstr;   // out (c),h
edinstr(4,0151) out_c(l);                                           endinstr;   // out (c),l
edinstr(4,0161) out_c(0);                                           endinstr;   // out (c),0 [output zero]
edinstr(4,0171) out_c(a);                                           endinstr;   // out (c),a


edinstr(4,0102) sbc_hl(bc);                                         endinstr;   // sbc hl,bc
edinstr(4,0112) adc_hl(bc);                                         endinstr;   // adc hl,bc
edinstr(4,0122) sbc_hl(de);                                         endinstr;   // sbc hl,de
edinstr(4,0132) adc_hl(de);                                         endinstr;   // adc hl,de
edinstr(4,0142) sbc_hl(hl);                                         endinstr;   // sbc hl,hl
edinstr(4,0152) adc_hl(hl);                                         endinstr;   // adc hl,hl
edinstr(4,0162) sbc_hl(sp);                                         endinstr;   // sbc hl,sp
edinstr(4,0172) adc_hl(sp);                                         endinstr;   // adc hl,sp


edinstr(4,0103) ld_pnn_rr(bc);                                      endinstr;   // ld (nn),bc
edinstr(4,0113) ld_rr_pnn(bc);                                      endinstr;   // ld bc,(nn)
edinstr(4,0123) ld_pnn_rr(de);                                      endinstr;   // ld (nn),de
edinstr(4,0133) ld_rr_pnn(de);                                      endinstr;   // ld de,(nn)
edinstr(4,0143) ld_pnn_rr(hl);                                      endinstr;   // ld (nn),hl
edinstr(4,0153) ld_rr_pnn(hl);                                      endinstr;   // ld hl,(nn)
edinstr(4,0163) ld_pnn_rr(sp);                                      endinstr;   // ld (nn),sp
edinstr(4,0173) ld_rr_pnn(sp);                                      endinstr;   // ld sp,(nn)


edinstr(4,0104) neg;                                                endinstr;   // neg
edinstr(4,0114) neg;                                                endinstr;   // neg
edinstr(4,0124) neg;                                                endinstr;   // neg
edinstr(4,0134) neg;                                                endinstr;   // neg
edinstr(4,0144) neg;                                                endinstr;   // neg
edinstr(4,0154) neg;                                                endinstr;   // neg
edinstr(4,0164) neg;                                                endinstr;   // neg
edinstr(4,0174) neg;                                                endinstr;   // neg


edinstr(4,0105) retn;                                               endinstr;   // retn
edinstr(4,0115) retn;                                               endinstr;   // retn
edinstr(4,0125) retn;                                               endinstr;   // retn
edinstr(4,0135) retn;                                               endinstr;   // retn
edinstr(4,0145) ret(true);                                          endinstr;   // reti
edinstr(4,0155) ret(true);                                          endinstr;   // reti
edinstr(4,0165) ret(true);                                          endinstr;   // reti
edinstr(4,0175) ret(true);                                          endinstr;   // reti


edinstr(4,0106) im = 0;                                             endinstr;   // im 0
edinstr(4,0116) im = 0;                                             endinstr;   // im 0/1
edinstr(4,0126) im = 1;                                             endinstr;   // im 1
edinstr(4,0136) im = 2;                                             endinstr;   // im 2
edinstr(4,0146) im = 0;                                             endinstr;   // im 0
edinstr(4,0156) im = 0;                                             endinstr;   // im 0/1
edinstr(4,0166) im = 1;                                             endinstr;   // im 1
edinstr(4,0176) im = 2;                                             endinstr;   // im 2


edinstr(5,0107) i = a;                                              endinstr;   // ld i,a
edinstr(5,0117) radjust = r = a;                                    endinstr;   // ld r,a

// ld a,i
edinstr(5,0127)
    a = i;
    f = cy | (a & 0xa8) | ((!a) << 6) | (iff2 << 2);
endinstr;

// ld a,r
edinstr(5,0137)
    // Only the bottom 7 bits of R are advanced by memory refresh, so the top bit is preserved
    a = r = (r & 0x80) | (radjust & 0x7f);
    f = cy | (a & 0xa8) | ((!a) << 6) | (iff2 << 2);
endinstr;

// rrd
edinstr(4,0147)
    BYTE t = timed_read_byte(hl);
    BYTE u = (a << 4) | (t >> 4);
    a = (a & 0xf0) | (t & 0x0f);
    g_nLineCycle += 4;
    timed_write_byte(hl,u);
    f = cy | parity(a);
endinstr;

// rld
edinstr(4,0157)
    BYTE t = timed_read_byte(hl);
    BYTE u = (a & 0x0f) | (t << 4);
    a = (a & 0xf0) | (t >> 4);
    g_nLineCycle += 4;
    timed_write_byte(hl,u);
    f = cy | parity(a);
endinstr;


edinstr(4,0240) ldi(false);                                         endinstr;   // ldi
edinstr(4,0250) ldd(false);                                         endinstr;   // ldd
edinstr(4,0260) ldi(bc);                                            endinstr;   // ldir
edinstr(4,0270) ldd(bc);                                            endinstr;   // lddr


edinstr(4,0241) cpi(false);                                         endinstr;   // cpi
edinstr(4,0251) cpd(false);                                         endinstr;   // cpd
edinstr(4,0261) cpi((f & 0x44) == 4);                               endinstr;   // cpir
edinstr(4,0271) cpd((f & 0x44) == 4);                               endinstr;   // cpdr


edinstr(5,0242) ini(false);                                         endinstr;   // ini
edinstr(5,0252) ind(false);                                         endinstr;   // ind
edinstr(5,0262) ini(b);                                             endinstr;   // inir
edinstr(5,0272) ind(b);                                             endinstr;   // indr


edinstr(5,0243) oti(false);                                         endinstr;   // outi
edinstr(5,0253) otd(false);                                         endinstr;   // outd
edinstr(5,0263) oti(b);                                             endinstr;   // otir
edinstr(5,0273) otd(b);                                             endinstr;   // otdr


// Anything not explicitly handled is effectively a 2 byte NOP (with predictable timing)
// Only the first three T-States are already accounted for
default:
    g_nLineCycle++;
    break;
}
