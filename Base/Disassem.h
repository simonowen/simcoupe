// Part of SimCoupe - A SAM Coupe emulator
//
// Disassem.h: Z80 disassembler
//
//  Copyright (c) 1999-2014  Simon Owen
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

#ifndef DISASSEM_H
#define DISASSEM_H

// Maximum Z80 instruction length
#define MAX_Z80_INSTR_LEN  4

UINT Disassemble(BYTE* pb_, WORD wPC_ = 0, char* psz_ = nullptr, size_t cbSize_ = 0, int nSymbolMax_ = 0);

#endif
