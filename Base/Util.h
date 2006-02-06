// Part of SimCoupe - A SAM Coupe emulator
//
// Util.h: Debug tracing, and other utility tasks
//
//  Copyright (c) 1999-2006  Simon Owen
//  Copyright (c) 1996-2001  Allan Skillman
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

#ifndef UTIL_H
#define UTIL_H

class Util
{
    public:
        static bool Init ();
        static void Exit ();

    public:
        static int GetUniqueFile (const char* pcszTemplate_, int nNext_, char* psz_, int cb_);
        static UINT HCF (UINT x_, UINT y_);
};


enum eMsgType { msgInfo, msgWarning, msgError, msgFatal };
void Message (eMsgType eType_, const char* pcszFormat_, ...);

BYTE GetSizeCode (UINT uSize_);

void AdjustBrightness (BYTE &r_, BYTE &g_, BYTE &b_, int nAdjust_);
void RGB2YUV (BYTE r_, BYTE g_, BYTE b_, BYTE *py_, BYTE *pu_, BYTE *pv_);
void YUV2RGB (BYTE y_, BYTE u_, BYTE v_, BYTE *pr_, BYTE *pg_, BYTE *pb_);
DWORD RGB2Native (BYTE r_, BYTE g_, BYTE b_, DWORD dwRMask_, DWORD dwGMask_, DWORD dwBMask_);
DWORD RGB2Native (BYTE r_, BYTE g_, BYTE b_, BYTE a_, DWORD dwRMask_, DWORD dwGMask_, DWORD dwBMask_, DWORD dwAMask_);

void TraceOutputString (const char *pcszFormat, ...);
void TraceOutputString (const BYTE *pcb_, size_t uLen_=0);

#ifdef _DEBUG
#define TRACE ::TraceOutputString
#else
void TraceOutputString (const char *, ...);
#define TRACE 1 ? (void)0 : ::TraceOutputString
#endif

#ifndef ULONGLONG
#define ULONGLONG	DWORD
#endif

#endif
