// Part of SimCoupe - A SAM Coupe emulator
//
// Util.h: Debug tracing, and other utility tasks
//
//  Copyright (c) 1999-2015 Simon Owen
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

#ifndef UTIL_H
#define UTIL_H

namespace Util
{
bool Init();
void Exit();

char* GetUniqueFile(const char* pcszExt_, char* pszPath_, int cbPath_);
}


enum eMsgType { msgInfo, msgWarning, msgError, msgFatal };
void Message(eMsgType eType_, const char* pcszFormat_, ...);

BYTE GetSizeCode(UINT uSize_);
const char* AbbreviateSize(uint64_t ullSize_);
WORD CrcBlock(const void* pcv_, size_t uLen_, WORD wCRC_ = 0xffff);
void PatchBlock(BYTE* pb_, BYTE* pbPatch_);
UINT TPeek(const BYTE* pb_);

void AdjustBrightness(BYTE& r_, BYTE& g_, BYTE& b_, int nAdjust_);
DWORD RGB2Native(BYTE r_, BYTE g_, BYTE b_, DWORD dwRMask_, DWORD dwGMask_, DWORD dwBMask_);
DWORD RGB2Native(BYTE r_, BYTE g_, BYTE b_, BYTE a_, DWORD dwRMask_, DWORD dwGMask_, DWORD dwBMask_, DWORD dwAMask_);

void TraceOutputString(const char* pcszFormat, ...);
void TraceOutputString(const BYTE* pcb_, size_t uLen_ = 0);

#ifdef _DEBUG
#define TRACE ::TraceOutputString
#else
void TraceOutputString(const char*, ...);
#define TRACE 1 ? (void)0 : ::TraceOutputString
#endif

#ifndef _countof
#define _countof(_Array) (sizeof(_Array)/sizeof(_Array[0]))
#endif

#ifndef MAX_PATH
#define MAX_PATH            260
#endif

#endif // UTIL_H
