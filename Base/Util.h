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

#pragma once

namespace Util
{
bool Init();
void Exit();

char* GetUniqueFile(const char* pcszExt_, char* pszPath_, int cbPath_);
}


enum eMsgType { msgInfo, msgWarning, msgError, msgFatal };
void Message(eMsgType eType_, const char* pcszFormat_, ...);

uint8_t GetSizeCode(unsigned int uSize_);
const char* AbbreviateSize(uint64_t ullSize_);
uint16_t CrcBlock(const void* pcv_, size_t uLen_, uint16_t wCRC_ = 0xffff);
void PatchBlock(uint8_t* pb_, uint8_t* pbPatch_);
unsigned int TPeek(const uint8_t* pb_);

void AdjustBrightness(uint8_t& r_, uint8_t& g_, uint8_t& b_, int nAdjust_);
uint32_t RGB2Native(uint8_t r_, uint8_t g_, uint8_t b_, uint32_t dwRMask_, uint32_t dwGMask_, uint32_t dwBMask_);
uint32_t RGB2Native(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_, uint32_t dwRMask_, uint32_t dwGMask_, uint32_t dwBMask_, uint32_t dwAMask_);

std::string tolower(std::string str);

void TraceOutputString(const char* pcszFormat, ...);
void TraceOutputString(const uint8_t* pcb_, size_t uLen_ = 0);

#ifdef _DEBUG
#define TRACE ::TraceOutputString
#else
void TraceOutputString(const char*, ...);
#define TRACE 1 ? (void)0 : ::TraceOutputString
#endif

#ifndef MAX_PATH
#define MAX_PATH            260
#endif
