// Part of SimCoupe - A SAM Coupe emulator
//
// EDops.h: Z80 instruction set emulation (from xz80)
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

// Changes 2000-2001 by Dave Laundon
//  - replaced all instruction timings with raw memory and I/O timings

// Changes 1999-2001 by Simon Owen
//  - Fixed INI/IND so the zero flag is set when B becomes zero


// Basic instruction header, specifying opcode and nominal T-States of the first M-Cycle (AFTER the ED code)
// The first three T-States of the first M-Cycle are already accounted for
#define edinstr(m1states, opcode)   case opcode: { \
                                        g_dwCycleCounter += m1states - 3;

// in R,(C)
#define in_c(x)         { \
                            PORT_ACCESS(REG_C); \
                            x = IO::In(REG_BC); \
                            REG_F = cy | parity(x); \
                        }

// out (C),R
#define out_c(x)        { \
                            PORT_ACCESS(REG_C); \
                            IO::Out(REG_BC,x); \
                        }

// sbc HL,rr
#define sbc_hl(x)       do { \
                            g_dwCycleCounter += 7; \
                            uint16_t z = (x); \
                            uint32_t y = REG_HL - z - cy; \
                            REG_F = (((y & 0xb800) ^ ((REG_HL ^ z) & 0x1000)) >> 8) |       /* S, 5, H, 3 */ \
                                ((y >> 16) & 1) |                                   /* C          */ \
                                (((REG_HL ^ z) & (REG_HL ^ y) & 0x8000) >> 13) |            /* V          */ \
                                2;                                                  /* N          */ \
                            REG_HL = (y & 0xffff);                                                        \
                            REG_F |= (!REG_HL) << 6;                                        /* Z          */ \
                        } while (0)

// adc HL,rr
#define adc_hl(x)       do { \
                            g_dwCycleCounter += 7; \
                            uint16_t z = (x); \
                            uint32_t y = REG_HL + z + cy; \
                            REG_F = (((y & 0xb800) ^ ((REG_HL ^ z) & 0x1000)) >> 8) |       /* S, 5, H, 3 */ \
                                ((y >> 16) & 0x01) |                                /* C          */ \
                                (((REG_HL ^ ~z) & (REG_HL ^ y) & 0x8000) >> 13);            /* V          */ \
                            REG_HL = (y & 0xffff);                                                       \
                            REG_F |= (!REG_HL) << 6;                                        /* Z          */ \
                        } while (0)

// negate a
#define neg             ( \
                            REG_A = -REG_A, \
                            REG_F = (REG_A & 0xa8) |                                        /* S, 5, 3    */ \
                                (((REG_A & 0xf) != 0) << 4) |                           /* H          */ \
                                (REG_A != 0) |                                          /* C          */ \
                                ((REG_A == 0x80) << 2) |                                /* V          */ \
                                2 |                                                 /* N          */ \
                                ((!REG_A) << 6)                                         /* Z          */ \
                        )

// Load; increment; [repeat]
#define ldi(loop)       do { \
                            uint8_t x = timed_read_byte(REG_HL); \
                            timed_write_byte(REG_DE,x); \
                            g_dwCycleCounter += 2; \
                            REG_HL++; \
                            REG_DE++; \
                            REG_BC--; \
                            x += REG_A; \
                            REG_F = (REG_F & 0xc1) | (x & 0x08) | ((x & 0x02) << 4) | ((REG_BC != 0) << 2); \
                            if (loop) { \
                                g_dwCycleCounter += 5; \
                                REG_PC -= 2; \
                            } \
                        } while (0)

// Load; decrement; [repeat]
#define ldd(loop)       do { \
                            uint8_t x = timed_read_byte(REG_HL); \
                            timed_write_byte(REG_DE,x); \
                            g_dwCycleCounter += 2; \
                            REG_HL--; \
                            REG_DE--; \
                            REG_BC--; \
                            x += REG_A; \
                            REG_F = (REG_F & 0xc1) | (x & 0x08) | ((x & 0x02) << 4) | ((REG_BC != 0) << 2); \
                            if (loop) { \
                                g_dwCycleCounter += 5; \
                                REG_PC -= 2; \
                            } \
                        } while (0)

// Compare; increment; [repeat]
#define cpi(loop)       do { \
                            uint8_t carry = cy, x = timed_read_byte(REG_HL); \
                            uint8_t sum = REG_A - x, z = REG_A ^ x ^ sum; \
                            g_dwCycleCounter += 5; \
                            REG_HL++; \
                            REG_BC--; \
                            REG_F = (sum & 0x80) | (!sum << 6) | (((sum - ((z&0x10)>>4)) & 2) << 4) | (z & 0x10) | ((sum - ((z >> 4) & 1)) & 8) | ((REG_BC != 0) << 2) | FLAG_N | carry; \
                            if ((sum & 15) == 8 && (z & 16) != 0)   \
                                REG_F &= ~8;    \
                            if (loop) { \
                                g_dwCycleCounter += 5; \
                                REG_PC -= 2; \
                            } \
                        } while (0)

// Compare; decrement; [repeat]
#define cpd(loop)       do { \
                            uint8_t carry = cy, x = timed_read_byte(REG_HL); \
                            uint8_t sum = REG_A - x, z = REG_A ^ x ^ sum; \
                            g_dwCycleCounter += 5; \
                            REG_HL--; \
                            REG_BC--; \
                            REG_F = (sum & 0x80) | (!sum << 6) | (((sum - ((z&0x10)>>4)) & 2) << 4) | (z & 0x10) | ((sum - ((z >> 4) & 1)) & 8) | ((REG_BC != 0) << 2) | FLAG_N | carry; \
                            if ((sum & 15) == 8 && (z & 16) != 0)   \
                                REG_F &= ~8;    \
                            if (loop) { \
                                g_dwCycleCounter += 5; \
                                REG_PC -= 2; \
                            } \
                        } while (0)

// Input; increment; [repeat]
#define ini(loop)       do { \
                            PORT_ACCESS(REG_C); \
                            uint8_t t = IO::In(REG_BC); \
                            timed_write_byte(REG_HL,t); \
                            REG_HL++; \
                            REG_B--; \
                            REG_F = FLAG_N | (parity(REG_B) ^ (REG_C & FLAG_P)); \
                            if (loop) { \
                                g_dwCycleCounter += 5; \
                                REG_PC -= 2; \
                            } \
                        } while (0)

// Input; decrement; [repeat]
#define ind(loop)       do { \
                            PORT_ACCESS(REG_C); \
                            uint8_t t = IO::In(REG_BC); \
                            timed_write_byte(REG_HL,t); \
                            REG_HL--; \
                            REG_B--; \
                            REG_F = FLAG_N | (parity(REG_B) ^ (REG_C & FLAG_P) ^ FLAG_P); \
                            if (loop) { \
                                g_dwCycleCounter += 5; \
                                REG_PC -= 2; \
                            } \
                        } while (0)

// I can't determine the correct flags outcome for the block OUT instructions.
// Spec says that the carry flag is left unchanged and N is set to 1, but that
// doesn't seem to be the case...

// Output; increment; [repeat]
#define oti(loop)       do { \
                            uint8_t x = timed_read_byte(REG_HL); \
                            REG_B--; \
                            PORT_ACCESS(REG_C); \
                            IO::Out(REG_BC,x); \
                            REG_HL++; \
                            REG_F = cy | (REG_B & 0xa8) | ((!REG_B) << 6) | FLAG_H | FLAG_N; \
                            if (loop) { \
                                g_dwCycleCounter += 5; \
                                REG_PC -= 2; \
                            } \
                        } while (0)

// Output; decrement; [repeat]
#define otd(loop)       do { \
                            uint8_t x = timed_read_byte(REG_HL); \
                            REG_B--; \
                            PORT_ACCESS(REG_C); \
                            IO::Out(REG_BC,x); \
                            REG_HL--; \
                            REG_F = cy | (REG_B & 0xa8) | ((!REG_B) << 6) | FLAG_H | FLAG_N; \
                            if (loop) { \
                                g_dwCycleCounter += 5; \
                                REG_PC -= 2; \
                            } \
                        } while (0)


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


auto op = timed_read_code_byte(REG_PC++);
REG_R++;

switch (op)
{


    edinstr(4, 0100) in_c(REG_B);                                            endinstr;   // in b,(c)
    edinstr(4, 0110) in_c(REG_C);                                            endinstr;   // in c,(c)
    edinstr(4, 0120) in_c(REG_D);                                            endinstr;   // in d,(c)
    edinstr(4, 0130) in_c(REG_E);                                            endinstr;   // in e,(c)
    edinstr(4, 0140) in_c(REG_H);                                            endinstr;   // in h,(c)
    edinstr(4, 0150) in_c(REG_L);                                            endinstr;   // in l,(c)
    edinstr(4, 0160) uint8_t x; in_c(x);                                    endinstr;   // in x,(c) [result discarded, but flags still set]
    edinstr(4, 0170) in_c(REG_A);                                            endinstr;   // in a,(c)


    edinstr(4, 0101) out_c(REG_B);                                           endinstr;   // out (c),b
    edinstr(4, 0111) out_c(REG_C);                                           endinstr;   // out (c),c
    edinstr(4, 0121) out_c(REG_D);                                           endinstr;   // out (c),d
    edinstr(4, 0131) out_c(REG_E);                                           endinstr;   // out (c),e
    edinstr(4, 0141) out_c(REG_H);                                           endinstr;   // out (c),h
    edinstr(4, 0151) out_c(REG_L);                                           endinstr;   // out (c),l
    edinstr(4, 0161) out_c(GetOption(cmosz80) ? 255 : 0);                    endinstr;   // out (c),0/255
    edinstr(4, 0171) out_c(REG_A);                                           endinstr;   // out (c),a


    edinstr(4, 0102) sbc_hl(REG_BC);                                         endinstr;   // sbc hl,bc
    edinstr(4, 0112) adc_hl(REG_BC);                                         endinstr;   // adc hl,bc
    edinstr(4, 0122) sbc_hl(REG_DE);                                         endinstr;   // sbc hl,de
    edinstr(4, 0132) adc_hl(REG_DE);                                         endinstr;   // adc hl,de
    edinstr(4, 0142) sbc_hl(REG_HL);                                         endinstr;   // sbc hl,hl
    edinstr(4, 0152) adc_hl(REG_HL);                                         endinstr;   // adc hl,hl
    edinstr(4, 0162) sbc_hl(REG_SP);                                         endinstr;   // sbc hl,sp
    edinstr(4, 0172) adc_hl(REG_SP);                                         endinstr;   // adc hl,sp


    edinstr(4, 0103) ld_pnn_rr(REG_BC);                                      endinstr;   // ld (nn),bc
    edinstr(4, 0113) ld_rr_pnn(REG_BC);                                      endinstr;   // ld bc,(nn)
    edinstr(4, 0123) ld_pnn_rr(REG_DE);                                      endinstr;   // ld (nn),de
    edinstr(4, 0133) ld_rr_pnn(REG_DE);                                      endinstr;   // ld de,(nn)
    edinstr(4, 0143) ld_pnn_rr(REG_HL);                                      endinstr;   // ld (nn),hl
    edinstr(4, 0153) ld_rr_pnn(REG_HL);                                      endinstr;   // ld hl,(nn)
    edinstr(4, 0163) ld_pnn_rr(REG_SP);                                      endinstr;   // ld (nn),sp
    edinstr(4, 0173) ld_rr_pnn(REG_SP);                                      endinstr;   // ld sp,(nn)


    edinstr(4, 0104) neg;                                                endinstr;   // neg
    edinstr(4, 0114) neg;                                                endinstr;   // neg
    edinstr(4, 0124) neg;                                                endinstr;   // neg
    edinstr(4, 0134) neg;                                                endinstr;   // neg
    edinstr(4, 0144) neg;                                                endinstr;   // neg
    edinstr(4, 0154) neg;                                                endinstr;   // neg
    edinstr(4, 0164) neg;                                                endinstr;   // neg
    edinstr(4, 0174) neg;                                                endinstr;   // neg


    edinstr(4, 0105) retn;                                               endinstr;   // retn
    edinstr(4, 0115) retn;                                               endinstr;   // retn
    edinstr(4, 0125) retn;                                               endinstr;   // retn
    edinstr(4, 0135) retn;                                               endinstr;   // retn
    edinstr(4, 0145) ret(true);                                          endinstr;   // reti
    edinstr(4, 0155) ret(true);                                          endinstr;   // reti
    edinstr(4, 0165) ret(true);                                          endinstr;   // reti
    edinstr(4, 0175) ret(true);                                          endinstr;   // reti


    edinstr(4, 0106) REG_IM = 0;                                             endinstr;   // im 0
    edinstr(4, 0116) REG_IM = 0;                                             endinstr;   // im 0/1
    edinstr(4, 0126) REG_IM = 1;                                             endinstr;   // im 1
    edinstr(4, 0136) REG_IM = 2;                                             endinstr;   // im 2
    edinstr(4, 0146) REG_IM = 0;                                             endinstr;   // im 0
    edinstr(4, 0156) REG_IM = 0;                                             endinstr;   // im 0/1
    edinstr(4, 0166) REG_IM = 1;                                             endinstr;   // im 1
    edinstr(4, 0176) REG_IM = 2;                                             endinstr;   // im 2


    edinstr(5, 0107) REG_I = REG_A;                                              endinstr;   // ld i,a
    edinstr(5, 0117) REG_R7 = REG_R = REG_A;                                         endinstr;   // ld r,a

    // ld a,i
    edinstr(5, 0127)
        REG_A = REG_I;
    REG_F = cy | (REG_A & 0xa8) | ((!REG_A) << 6) | (REG_IFF2 << 2);
    endinstr;

    // ld a,r
    edinstr(5, 0137)
        // Only the bottom 7 bits of R are advanced by memory refresh, so the top bit is preserved
        REG_A = REG_R = (REG_R7 & 0x80) | (REG_R & 0x7f);
    REG_F = cy | (REG_A & 0xa8) | ((!REG_A) << 6) | (REG_IFF2 << 2);
    endinstr;

    // rrd
    edinstr(4, 0147)
        uint8_t t = timed_read_byte(REG_HL);
    uint8_t u = (REG_A << 4) | (t >> 4);
    REG_A = (REG_A & 0xf0) | (t & 0x0f);
    g_dwCycleCounter += 4;
    timed_write_byte(REG_HL, u);
    REG_F = cy | parity(REG_A);
    endinstr;

    // rld
    edinstr(4, 0157)
        uint8_t t = timed_read_byte(REG_HL);
    uint8_t u = (REG_A & 0x0f) | (t << 4);
    REG_A = (REG_A & 0xf0) | (t >> 4);
    g_dwCycleCounter += 4;
    timed_write_byte(REG_HL, u);
    REG_F = cy | parity(REG_A);
    endinstr;


    edinstr(4, 0240) ldi(false);                                         endinstr;   // ldi
    edinstr(4, 0250) ldd(false);                                         endinstr;   // ldd
    edinstr(4, 0260) ldi(REG_BC);                                            endinstr;   // ldir
    edinstr(4, 0270) ldd(REG_BC);                                            endinstr;   // lddr


    edinstr(4, 0241) cpi(false);                                         endinstr;   // cpi
    edinstr(4, 0251) cpd(false);                                         endinstr;   // cpd
    edinstr(4, 0261) cpi((REG_F & 0x44) == 4);                               endinstr;   // cpir
    edinstr(4, 0271) cpd((REG_F & 0x44) == 4);                               endinstr;   // cpdr


    edinstr(5, 0242) ini(false);                                         endinstr;   // ini
    edinstr(5, 0252) ind(false);                                         endinstr;   // ind
    edinstr(5, 0262) ini(REG_B);                                             endinstr;   // inir
    edinstr(5, 0272) ind(REG_B);                                             endinstr;   // indr


    edinstr(5, 0243) oti(false);                                         endinstr;   // outi
    edinstr(5, 0253) otd(false);                                         endinstr;   // outd
    edinstr(5, 0263) oti(REG_B);                                             endinstr;   // otir
    edinstr(5, 0273) otd(REG_B);                                             endinstr;   // otdr


    // Anything not explicitly handled is effectively a 2 byte NOP (with predictable timing)
    // Only the first three T-States are already accounted for
default:
    g_dwCycleCounter++;
    break;
}
