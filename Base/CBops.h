// Part of SimCoupe - A SAM Coupé emulator
//
// CBops.h: Z80 instruction set emulation (from xz80)
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


#define rlc(x)  (x = (x << 1) | (x >> 7), rflags(x, x & 1))
#define rrc(x)  { BYTE t = x & 1;  x = (x >> 1) | (t << 7); rflags(x,t); }
#define rl(x)   { BYTE t = x >> 7; x = (x << 1) | (f & 1);  rflags(x,t); }
#define rr(x)   { BYTE t = x & 1;  x = (x >> 1) | (f << 7); rflags(x,t); }
#define sla(x)  { BYTE t = x >> 7; x <<= 1;                 rflags(x,t); }
#define sra(x)  { BYTE t = x & 1;  x = ((signed char)x) >> 1;rflags(x,t); }
#define sll(x)  { BYTE t = x >> 7; x = (x << 1) | 1;        rflags(x,t); }  // Z80 CPU bug: bit 0 always set in the result
#define srl(x)  { BYTE t = x & 1;  x >>= 1;                 rflags(x,t); }

#define bit(n,x) (f = (f & 1) | ((x & (1 << n)) ? n == 7 ? 0x90 : 0x10 : 0x54) | (x & 0x28))    // Sabre Wulf undocumented flag fix
#define set(n,x) (x |=  (1 << n))
#define res(n,x) (x &= ~(1 << n))

{
    WORD addr;
    BYTE op, reg=0, val=0;

    // Is this an undocumented indexed CB instruction?
    if (pHlIxIy != &hl)
    {
        addr = *pHlIxIy + (signed char)timed_read_byte(pc);

        // Skip the offset byte
        g_nLineCycle += 4;
        pc++;

        // Extract the register to store the result in, and modify the opcode to be a regular indexed version
        op = timed_read_byte(pc);
        reg = op & 7;
        op = (op & 0xf8) | 6;

        // radjust here?
    }

    // Instruction involving normal register or (HL)
    else
    {
        addr = hl;
        op = timed_read_byte(pc);
        radjust++;
    }


    // Skip the last instruction byte
    pc++;

    if (op < 0x40)
    {
        switch(op)
        {
            case  0: rlc(b); break;
            case  1: rlc(c); break;
            case  2: rlc(d); break;
            case  3: rlc(e); break;
            case  4: rlc(h); break;
            case  5: rlc(l); break;
            case  6: val = timed_read_byte(addr); rlc(val); timed_write_byte(addr,val); break;
            case  7: rlc(a); break;

            case  8: rrc(b); break;
            case  9: rrc(c); break;
            case 10: rrc(d); break;
            case 11: rrc(e); break;
            case 12: rrc(h); break;
            case 13: rrc(l); break;
            case 14: val = timed_read_byte(addr); rrc(val); timed_write_byte(addr,val); break;
            case 15: rrc(a); break;

            case 0x10: rl(b); break;
            case 0x11: rl(c); break;
            case 0x12: rl(d); break;
            case 0x13: rl(e); break;
            case 0x14: rl(h); break;
            case 0x15: rl(l); break;
            case 0x16: val = timed_read_byte(addr); rl(val); timed_write_byte(addr,val); break;
            case 0x17: rl(a); break;

            case 0x18: rr(b); break;
            case 0x19: rr(c); break;
            case 0x1a: rr(d); break;
            case 0x1b: rr(e); break;
            case 0x1c: rr(h); break;
            case 0x1d: rr(l); break;
            case 0x1e: val = timed_read_byte(addr); rr(val); timed_write_byte(addr,val); break;
            case 0x1f: rr(a); break;

            case 0x20: sla(b); break;
            case 0x21: sla(c); break;
            case 0x22: sla(d); break;
            case 0x23: sla(e); break;
            case 0x24: sla(h); break;
            case 0x25: sla(l); break;
            case 0x26: val = timed_read_byte(addr); sla(val); timed_write_byte(addr,val); break;
            case 0x27: sla(a); break;

            case 0x28: sra(b); break;
            case 0x29: sra(c); break;
            case 0x2a: sra(d); break;
            case 0x2b: sra(e); break;
            case 0x2c: sra(h); break;
            case 0x2d: sra(l); break;
            case 0x2e: val = timed_read_byte(addr); sra(val); timed_write_byte(addr,val); break;
            case 0x2f: sra(a); break;

            case 0x30: sll(b); break;
            case 0x31: sll(c); break;
            case 0x32: sll(d); break;
            case 0x33: sll(e); break;
            case 0x34: sll(h); break;
            case 0x35: sll(l); break;
            case 0x36: val = timed_read_byte(addr); sll(val); timed_write_byte(addr,val); break;
            case 0x37: sll(a); break;

            case 0x38: srl(b); break;
            case 0x39: srl(c); break;
            case 0x3a: srl(d); break;
            case 0x3b: srl(e); break;
            case 0x3c: srl(h); break;
            case 0x3d: srl(l); break;
            case 0x3e: val = timed_read_byte(addr); srl(val); timed_write_byte(addr,val); break;
            case 0x3f: srl(a); break;
        }
    }
    else
    {
        BYTE n = (op >> 3) & 7;
        switch(op & 0xc7)
        {
            case 0x40: bit(n,b); break;
            case 0x41: bit(n,c); break;
            case 0x42: bit(n,d); break;
            case 0x43: bit(n,e); break;
            case 0x44: bit(n,h); break;
            case 0x45: bit(n,l); break;
            case 0x46: val = timed_read_byte(addr); bit(n,val); break;
            case 0x47: bit(n,a); break;

            case 0x80: res(n,b); break;
            case 0x81: res(n,c); break;
            case 0x82: res(n,d); break;
            case 0x83: res(n,e); break;
            case 0x84: res(n,h); break;
            case 0x85: res(n,l); break;
            case 0x86: val = timed_read_byte(addr); res(n,val); timed_write_byte(addr,val); break;
            case 0x87: res(n,a); break;

            case 0xc0: set(n,b); break;
            case 0xc1: set(n,c); break;
            case 0xc2: set(n,d); break;
            case 0xc3: set(n,e); break;
            case 0xc4: set(n,h); break;
            case 0xc5: set(n,l); break;
            case 0xc6: val = timed_read_byte(addr); set(n,val); timed_write_byte(addr,val); break;
            case 0xc7: set(n,a); break;
        }
    }

    // With the undocumented DDDB/FDCB instructions, we load the result back into a register
    if (pHlIxIy != &hl)
    {
        switch (reg)
        {
            case 0: b = val; break;
            case 1: c = val; break;
            case 2: d = val; break;
            case 3: e = val; break;
            case 4: h = val; break;
            case 5: l = val; break;
            case 7: a = val; break;
        }
    }

}

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
