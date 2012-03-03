// Part of SimCoupe - A SAM Coupe emulator
//
// Util.h: Debug tracing, and other utility tasks
//
//  Copyright (c) 1999-2012 Simon Owen
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
        static char *GetUniqueFile (const char* pcszExt_, char* pszPath_, int cbPath_);
};


enum eMsgType { msgInfo, msgWarning, msgError, msgFatal };
void Message (eMsgType eType_, const char* pcszFormat_, ...);

BYTE GetSizeCode (UINT uSize_);
void PatchBlock (BYTE *pb_, BYTE *pbPatch_);

void AdjustBrightness (BYTE &r_, BYTE &g_, BYTE &b_, int nAdjust_);
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
#define ULONGLONG   unsigned long long
#endif

#ifndef _countof
#define _countof(_Array) (sizeof(_Array)/sizeof(_Array[0]))
#endif

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX_PATH
#define MAX_PATH            260
#endif

template <class T> void swap (T& a, T& b) { T tmp=a; a=b; b=tmp; }

#endif
