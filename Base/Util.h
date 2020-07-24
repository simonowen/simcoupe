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

enum class MsgType { Info, Warning, Error, Fatal };

void Message(MsgType type, const std::string& message);
template <typename ...Args>
void Message(MsgType type, const std::string& format, Args&& ... args)
{
    Message(type, fmt::format(format, std::forward<Args>(args)...));
}

uint8_t GetSizeCode(unsigned int uSize_);
const char* AbbreviateSize(uint64_t ullSize_);
uint16_t CrcBlock(const void* pcv_, size_t uLen_, uint16_t wCRC_ = 0xffff);
void PatchBlock(uint8_t* pb_, uint8_t* pbPatch_);
unsigned int TPeek(const uint8_t* pb_);

void AdjustBrightness(uint8_t& r_, uint8_t& g_, uint8_t& b_, int nAdjust_);
uint32_t RGB2Native(uint8_t r_, uint8_t g_, uint8_t b_, uint32_t dwRMask_, uint32_t dwGMask_, uint32_t dwBMask_);
uint32_t RGB2Native(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_, uint32_t dwRMask_, uint32_t dwGMask_, uint32_t dwBMask_, uint32_t dwAMask_);

std::string tolower(std::string str);
std::vector<std::string> split(const std::string& str, char sep);

template <typename T>
std::set<typename T::value_type> to_set(T&& items)
{
    std::set<typename T::value_type> set;
    for (auto&& item : items)
        set.insert(std::move(item));
    return set;
}

#ifdef _DEBUG
void TraceOutputString(const std::string& str);
template <typename ...Args>
void TraceOutputString(const std::string& format, Args&& ... args)
{
    TraceOutputString(fmt::format(format, std::forward<Args>(args)...));
}
#define TRACE ::TraceOutputString
#else
inline void TraceOutputString(const std::string& str) { }
template <typename ...Args>
void TraceOutputString(const std::string& format, Args&& ... args) { }
#define TRACE 1 ? (void)0 : ::TraceOutputString
#endif

#ifndef MAX_PATH
#define MAX_PATH            260
#endif
