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

fs::path UniqueOutputPath(const std::string& ext);
}

enum class PathType { Settings, Input, Output, Resource };

enum class MsgType { Info, Warning, Error, Fatal };

void Message(MsgType type, const std::string& message);
template <typename ...Args>
void Message(MsgType type, const std::string& format, Args&& ... args)
{
    Message(type, fmt::format(format, std::forward<Args>(args)...));
}

uint8_t GetSizeCode(unsigned int uSize_);
std::string AbbreviateSize(uint64_t ullSize_);
uint16_t CrcBlock(const void* pcv_, size_t uLen_, uint16_t wCRC_ = 0xffff);
void PatchBlock(uint8_t* pb_, uint8_t* pbPatch_);
unsigned int TPeek(const uint8_t* pb_);

void AdjustBrightness(uint8_t& r_, uint8_t& g_, uint8_t& b_, int nAdjust_);
uint32_t RGB2Native(uint8_t r_, uint8_t g_, uint8_t b_, uint32_t dwRMask_, uint32_t dwGMask_, uint32_t dwBMask_);
uint32_t RGB2Native(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_, uint32_t dwRMask_, uint32_t dwGMask_, uint32_t dwBMask_, uint32_t dwAMask_);
inline float RGB2sRGB(float x) { return (x < 0.0031308f) ? (x * 12.92f) : (1.055f * std::pow(x, 1 / 2.4f) - 0.055f); }

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

inline std::string trim(const std::string& str)
{
    std::string s(str.c_str());
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);
    return s;
}

template<typename T, T invalid, typename D>
struct unique_resource
{
    unique_resource() = default;
    unique_resource(T res) : _res(res) {}
    unique_resource(const unique_resource&) = delete;
    unique_resource(unique_resource&& other) noexcept { _res = other.release(); }
    unique_resource& operator=(const unique_resource&) = delete;
    unique_resource& operator=(unique_resource&& other) noexcept { reset(other.release()); return *this; }
    ~unique_resource() { reset(); }

    const T& operator=(const T res) { reset(res); return _res; }
    operator const T& () const { return _res; }
    T* operator&() { return &_res; }
    T get() const { return _res; }

    template<typename T_ = T, typename = std::enable_if_t<std::is_pointer_v<T_>>>
    T operator->() { return _res; }

    void reset() { if (_res != invalid) { D deleter; deleter(_res); _res = invalid; } }
    void reset(T res) { reset(); _res = res; }
    T release() { T res = _res; _res = invalid; return res; }

private:
    T _res{ invalid };
};

struct FILECloser { void operator()(FILE* file) { fclose(file); } };
using unique_FILE = unique_resource<FILE*, nullptr, FILECloser>;


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

struct Rect
{
    int x, y, w, h;
};
