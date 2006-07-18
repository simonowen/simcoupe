// Part of SimCoupe - A SAM Coupe emulator
//
// CBops.h: Z80 instruction set emulation (from xz80)
//
//  Copyright (c) 1994 Ian Collier
//  Copyright (c) 1999-2003 by Dave Laundon
//  Copyright (c) 1999-2006 by Simon Owen
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
#define rl(x)   { BYTE t = x >> 7; x = (x << 1) | (f & F_CARRY);  rflags(x,t); }
#define rr(x)   { BYTE t = x & 1;  x = (x >> 1) | (f << 7); rflags(x,t); }
#define sla(x)  { BYTE t = x >> 7; x <<= 1;                 rflags(x,t); }
#define sra(x)  { BYTE t = x & 1;  x = ((signed char)x) >> 1;rflags(x,t); }
#define sll(x)  { BYTE t = x >> 7; x = (x << 1) | 1;        rflags(x,t); }  // Z80 CPU bug: bit 0 always set in the result
#define srl(x)  { BYTE t = x & 1;  x >>= 1;                 rflags(x,t); }

#define bit(n,x) (f = (f & 1) | ((x & (1 << n)) ? n == 7 ? 0x90 : 0x10 : 0x54) | (((op & 7) == 6) ? 0 : (x & 0x28)))

#define set(n,x) (x |=  (1 << n))
#define res(n,x) (x &= ~(1 << n))

#define HLbitop \
    val = timed_read_byte(addr); g_nLineCycle++

{
    WORD addr;
    BYTE op, reg=0, val=0;

    // Is this an undocumented indexed CB instruction?
    if (pHlIxIy != &hl)
    {
        // Get the offset
        addr = *pHlIxIy + (signed char)timed_read_code_byte(pc++);
        g_nLineCycle += 5;

        // Extract the register to store the result in, and modify the opcode to be a regular indexed version
        op = timed_read_code_byte(pc++);
        g_nLineCycle++;

        reg = op & 7;
        op = (op & 0xf8) | 6;

        // radjust here?
    }

    // Instruction involving normal register or (HL)
    else
    {
        addr = hl;

        op = timed_read_code_byte(pc++);
        g_nLineCycle++;

        radjust++;
    }

    if (op < 0x40)
    {
        switch(op)
        {
            case 0x00: rlc(b); break;
            case 0x01: rlc(c); break;
            case 0x02: rlc(d); break;
            case 0x03: rlc(e); break;
            case 0x04: rlc(h); break;
            case 0x05: rlc(l); break;
            case 0x06: HLbitop; rlc(val); timed_write_byte(addr,val); break;
            case 0x07: rlc(a); break;

            case 0x08: rrc(b); break;
            case 0x09: rrc(c); break;
            case 0x0a: rrc(d); break;
            case 0x0b: rrc(e); break;
            case 0x0c: rrc(h); break;
            case 0x0d: rrc(l); break;
            case 0x0e: HLbitop; rrc(val); timed_write_byte(addr,val); break;
            case 0x0f: rrc(a); break;

            case 0x10: rl(b); break;
            case 0x11: rl(c); break;
            case 0x12: rl(d); break;
            case 0x13: rl(e); break;
            case 0x14: rl(h); break;
            case 0x15: rl(l); break;
            case 0x16: HLbitop; rl(val); timed_write_byte(addr,val); break;
            case 0x17: rl(a); break;

            case 0x18: rr(b); break;
            case 0x19: rr(c); break;
            case 0x1a: rr(d); break;
            case 0x1b: rr(e); break;
            case 0x1c: rr(h); break;
            case 0x1d: rr(l); break;
            case 0x1e: HLbitop; rr(val); timed_write_byte(addr,val); break;
            case 0x1f: rr(a); break;

            case 0x20: sla(b); break;
            case 0x21: sla(c); break;
            case 0x22: sla(d); break;
            case 0x23: sla(e); break;
            case 0x24: sla(h); break;
            case 0x25: sla(l); break;
            case 0x26: HLbitop; sla(val); timed_write_byte(addr,val); break;
            case 0x27: sla(a); break;

            case 0x28: sra(b); break;
            case 0x29: sra(c); break;
            case 0x2a: sra(d); break;
            case 0x2b: sra(e); break;
            case 0x2c: sra(h); break;
            case 0x2d: sra(l); break;
            case 0x2e: HLbitop; sra(val); timed_write_byte(addr,val); break;
            case 0x2f: sra(a); break;

            case 0x30: sll(b); break;
            case 0x31: sll(c); break;
            case 0x32: sll(d); break;
            case 0x33: sll(e); break;
            case 0x34: sll(h); break;
            case 0x35: sll(l); break;
            case 0x36: HLbitop; sll(val); timed_write_byte(addr,val); break;
            case 0x37: sll(a); break;

            case 0x38: srl(b); break;
            case 0x39: srl(c); break;
            case 0x3a: srl(d); break;
            case 0x3b: srl(e); break;
            case 0x3c: srl(h); break;
            case 0x3d: srl(l); break;
            case 0x3e: HLbitop; srl(val); timed_write_byte(addr,val); break;
            case 0x3f: srl(a); break;

#ifdef NODEFAULT
            default: NODEFAULT;
#endif
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
            case 0x46: HLbitop; bit(n,val); break;
            case 0x47: bit(n,a); break;

            case 0x80: res(n,b); break;
            case 0x81: res(n,c); break;
            case 0x82: res(n,d); break;
            case 0x83: res(n,e); break;
            case 0x84: res(n,h); break;
            case 0x85: res(n,l); break;
            case 0x86: HLbitop; res(n,val); timed_write_byte(addr,val); break;
            case 0x87: res(n,a); break;

            case 0xc0: set(n,b); break;
            case 0xc1: set(n,c); break;
            case 0xc2: set(n,d); break;
            case 0xc3: set(n,e); break;
            case 0xc4: set(n,h); break;
            case 0xc5: set(n,l); break;
            case 0xc6: HLbitop; set(n,val); timed_write_byte(addr,val); break;
            case 0xc7: set(n,a); break;

#ifdef NODEFAULT
            default: NODEFAULT;
#endif
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
        //  case 6: break;              // This is the ordinary documented case
            case 7: a = val; break;

#ifdef NODEFAULT
            default: NODEFAULT;
#endif
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
