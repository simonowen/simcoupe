// Part of SimCoupe - A SAM Coupé emulator
//
// Debug.cpp: Integrated Z80 debugger
//
//  Copyright (c) 1999-2001  Simon Owen
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

// Notes:
//  The plan is to have a cross between TurboMON and Soft-ICE, for a
//  powerful debugger supporting multiple (conditional) breakpoints,
//  single stepping, et al.
//
//  All that's been done so far is a simple TurboMON-style disassembly
//  and register view!

// ToDo:
//  - er, the actual implementation? ;-)

#include "SimCoupe.h"

#include "Util.h"
#include "CPU.h"
#include "Debug.h"
#include "Disassem.h"
#include "Frame.h"
#include "Memory.h"
#include "Options.h"

namespace Debug
{

bool Init (bool fFirstInit_/*=false*/)
{
    return true;
}


void Exit (bool fReInit_/*=false*/)
{
}


void DrawString (CScreen* pScreen_, int nX_, int nY_, const char* pcsz_)
{
    // Drawing is always relative to SAM's main screen area
    const AREA* pArea = Frame::GetViewArea();

    nX_ = (nX_ + ((BORDER_BLOCKS - pArea->left) << 1)) << 3;
    (nY_ <<= 3) += TOP_BORDER_LINES - pArea->top;

    if (pScreen_)
        pScreen_->DrawOpaqueString(nX_, nY_, pcsz_);
}

void Display (CScreen* pScreen_)
{
    char sz[256];

    WORD w = regs.PC.W;
    int nY = 2;

    for (int i = 0 ; i < 20 ; i++)
    {
        int nLen = Disassemble(w, sz, sizeof sz);
        DrawString(pScreen_, 23, nY, sz);

//      TRACE("%05d %s\n", w, sz);

        sprintf(sz, "%05d", w);
        DrawString(pScreen_, 0, nY, sz);

        for (int j = 0 ; j < nLen ; j++)
        {
            sprintf(sz, "%03d", read_byte(w + j));
            DrawString(pScreen_, 6 + (j << 2), nY, sz);
        }

        w += nLen;
        nY++;
    }

//  DrawString(pScreen_, 0, 0, "TurboMON 1.0");
    DrawString(pScreen_, 0, 0, "[Debugger test page - updated at the end of every frame!]");

    sprintf(sz, "BC %05d  BC' %05d", regs.BC.W, regs.BC_.W);
    DrawString(pScreen_, 45, 2, sz);
    sprintf(sz, "DE %05d  DE' %05d", regs.DE.W, regs.DE_.W);
    DrawString(pScreen_, 45, 3, sz);
    sprintf(sz, "HL %05d  HL' %05d", regs.HL.W, regs.HL_.W);
    DrawString(pScreen_, 45, 4, sz);

    sprintf(sz, "(BC) (DE) (HL) (HL')");
    DrawString(pScreen_, 45, 6, sz);
    sprintf(sz, "%03d  %03d  %03d  %03d", read_byte(regs.BC.W), read_byte(regs.DE.W), read_byte(regs.HL.W), read_byte(regs.HL_.W));
    DrawString(pScreen_, 45, 7, sz);

    sprintf(sz, "AF %05d", regs.AF.W);
    DrawString(pScreen_, 45, 9, sz);
    sprintf(sz, "AF'%05d", regs.AF_.W);
    DrawString(pScreen_, 45, 10, sz);

    sprintf(sz, "IX %05d   IY %05d", regs.IX.W, regs.IY.W);
    DrawString(pScreen_, 45, 12, sz);
    sprintf(sz, "PC %05d   SP %05d", regs.PC.W, regs.SP.W);
    DrawString(pScreen_, 45, 13, sz);

    sprintf(sz, "A %03d", regs.AF.B.h_);
    DrawString(pScreen_, 45, 16, sz);
    sprintf(sz, "B %03d  C %03d", regs.BC.B.h_, regs.BC.B.l_);
    DrawString(pScreen_, 45, 17, sz);
    sprintf(sz, "D %03d  E %03d", regs.DE.B.h_, regs.DE.B.l_);
    DrawString(pScreen_, 45, 18, sz);
    sprintf(sz, "H %03d  L %03d", regs.HL.B.h_, regs.HL.B.l_);
    DrawString(pScreen_, 45, 19, sz);

    sprintf(sz, "I %03d  R %03d", regs.I, regs.R);
    DrawString(pScreen_, 45, 21, sz);
    sprintf(sz, "IM  %d  %s    IFF2 %d", regs.IM, regs.IFF1 ? "EI" : "DI", regs.IFF2);
    DrawString(pScreen_, 45, 22, sz);

    DrawString(pScreen_, 52, 15, "Stack:");
    for (int y = 15 ; y < 21 ; y++)
    {
        sprintf(sz, "%05d", read_word(regs.SP.W + (y << 1)));
        DrawString(pScreen_, 59, y, sz);
    }
}

};
