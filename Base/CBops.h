// Part of SimCoupe - A SAM Coupe emulator
//
// CBops.h: Z80 instruction set emulation (from xz80)
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


#define rlc(x)  (x = (x << 1) | (x >> 7), rflags(x, x & 1))
#define rrc(x)  { uint8_t t = x & 1;  x = (x >> 1) | (t << 7); rflags(x,t); }
#define rl(x)   { uint8_t t = x >> 7; x = (x << 1) | (REG_F & FLAG_C);  rflags(x,t); }
#define rr(x)   { uint8_t t = x & 1;  x = (x >> 1) | (REG_F << 7); rflags(x,t); }
#define sla(x)  { uint8_t t = x >> 7; x <<= 1;                 rflags(x,t); }
#define sra(x)  { uint8_t t = x & 1;  x = ((signed char)x) >> 1;rflags(x,t); }
#define sll(x)  { uint8_t t = x >> 7; x = (x << 1) | 1;        rflags(x,t); }  // Z80 CPU bug: bit 0 always set in the result
#define srl(x)  { uint8_t t = x & 1;  x >>= 1;                 rflags(x,t); }

#define bit(n,x) (REG_F = (REG_F & 1) | ((x & (1 << n)) ? n == 7 ? 0x90 : 0x10 : 0x54) | (((op & 7) == 6) ? 0 : (x & 0x28)))

#define set(n,x) (x |=  (1 << n))
#define res(n,x) (x &= ~(1 << n))

#define HLbitop \
    val = timed_read_byte(addr); g_dwCycleCounter++

{
uint16_t addr;
uint8_t op, reg = 0, val = 0;

// Is this an undocumented indexed CB instruction?
if (pHlIxIy != &REG_HL)
{
    // Get the offset
    addr = *pHlIxIy + (signed char)timed_read_code_byte(REG_PC++);
    g_dwCycleCounter += 5;

    // Extract the register to store the result in, and modify the opcode to be a regular indexed version
    op = timed_read_code_byte(REG_PC++);
    g_dwCycleCounter++;

    reg = op & 7;
    op = (op & 0xf8) | 6;

    // radjust here?
}

// Instruction involving normal register or (HL)
else
{
    addr = REG_HL;

    op = timed_read_code_byte(REG_PC++);
    g_dwCycleCounter++;

    REG_R++;
}

if (op < 0x40)
{
    switch (op)
    {
    case 0x00: rlc(REG_B); break;
    case 0x01: rlc(REG_C); break;
    case 0x02: rlc(REG_D); break;
    case 0x03: rlc(REG_E); break;
    case 0x04: rlc(REG_H); break;
    case 0x05: rlc(REG_L); break;
    case 0x06: HLbitop; rlc(val); timed_write_byte(addr, val); break;
    case 0x07: rlc(REG_A); break;

    case 0x08: rrc(REG_B); break;
    case 0x09: rrc(REG_C); break;
    case 0x0a: rrc(REG_D); break;
    case 0x0b: rrc(REG_E); break;
    case 0x0c: rrc(REG_H); break;
    case 0x0d: rrc(REG_L); break;
    case 0x0e: HLbitop; rrc(val); timed_write_byte(addr, val); break;
    case 0x0f: rrc(REG_A); break;

    case 0x10: rl(REG_B); break;
    case 0x11: rl(REG_C); break;
    case 0x12: rl(REG_D); break;
    case 0x13: rl(REG_E); break;
    case 0x14: rl(REG_H); break;
    case 0x15: rl(REG_L); break;
    case 0x16: HLbitop; rl(val); timed_write_byte(addr, val); break;
    case 0x17: rl(REG_A); break;

    case 0x18: rr(REG_B); break;
    case 0x19: rr(REG_C); break;
    case 0x1a: rr(REG_D); break;
    case 0x1b: rr(REG_E); break;
    case 0x1c: rr(REG_H); break;
    case 0x1d: rr(REG_L); break;
    case 0x1e: HLbitop; rr(val); timed_write_byte(addr, val); break;
    case 0x1f: rr(REG_A); break;

    case 0x20: sla(REG_B); break;
    case 0x21: sla(REG_C); break;
    case 0x22: sla(REG_D); break;
    case 0x23: sla(REG_E); break;
    case 0x24: sla(REG_H); break;
    case 0x25: sla(REG_L); break;
    case 0x26: HLbitop; sla(val); timed_write_byte(addr, val); break;
    case 0x27: sla(REG_A); break;

    case 0x28: sra(REG_B); break;
    case 0x29: sra(REG_C); break;
    case 0x2a: sra(REG_D); break;
    case 0x2b: sra(REG_E); break;
    case 0x2c: sra(REG_H); break;
    case 0x2d: sra(REG_L); break;
    case 0x2e: HLbitop; sra(val); timed_write_byte(addr, val); break;
    case 0x2f: sra(REG_A); break;

    case 0x30: sll(REG_B); break;
    case 0x31: sll(REG_C); break;
    case 0x32: sll(REG_D); break;
    case 0x33: sll(REG_E); break;
    case 0x34: sll(REG_H); break;
    case 0x35: sll(REG_L); break;
    case 0x36: HLbitop; sll(val); timed_write_byte(addr, val); break;
    case 0x37: sll(REG_A); break;

    case 0x38: srl(REG_B); break;
    case 0x39: srl(REG_C); break;
    case 0x3a: srl(REG_D); break;
    case 0x3b: srl(REG_E); break;
    case 0x3c: srl(REG_H); break;
    case 0x3d: srl(REG_L); break;
    case 0x3e: HLbitop; srl(val); timed_write_byte(addr, val); break;
    case 0x3f: srl(REG_A); break;
    }
}
else
{
    uint8_t n = (op >> 3) & 7;
    switch (op & 0xc7)
    {
    case 0x40: bit(n, REG_B); break;
    case 0x41: bit(n, REG_C); break;
    case 0x42: bit(n, REG_D); break;
    case 0x43: bit(n, REG_E); break;
    case 0x44: bit(n, REG_H); break;
    case 0x45: bit(n, REG_L); break;
    case 0x46: HLbitop; bit(n, val); break;
    case 0x47: bit(n, REG_A); break;

    case 0x80: res(n, REG_B); break;
    case 0x81: res(n, REG_C); break;
    case 0x82: res(n, REG_D); break;
    case 0x83: res(n, REG_E); break;
    case 0x84: res(n, REG_H); break;
    case 0x85: res(n, REG_L); break;
    case 0x86: HLbitop; res(n, val); timed_write_byte(addr, val); break;
    case 0x87: res(n, REG_A); break;

    case 0xc0: set(n, REG_B); break;
    case 0xc1: set(n, REG_C); break;
    case 0xc2: set(n, REG_D); break;
    case 0xc3: set(n, REG_E); break;
    case 0xc4: set(n, REG_H); break;
    case 0xc5: set(n, REG_L); break;
    case 0xc6: HLbitop; set(n, val); timed_write_byte(addr, val); break;
    case 0xc7: set(n, REG_A); break;
    }
}

// With the undocumented DDDB/FDCB instructions, we load the result back into a register
if (pHlIxIy != &REG_HL)
{
    switch (reg)
    {
    case 0: REG_B = val; break;
    case 1: REG_C = val; break;
    case 2: REG_D = val; break;
    case 3: REG_E = val; break;
    case 4: REG_H = val; break;
    case 5: REG_L = val; break;
    case 6:          break;     // This is the ordinary documented case
    case 7: REG_A = val; break;
    }
}

}

#undef HLbitop

#undef rlc
#undef rrc
#undef rl
#undef rr
#undef sla
#undef sra
#undef sll
#undef srl

#undef bit
#undef set
#undef res
