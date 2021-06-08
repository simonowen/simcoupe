// Part of SimCoupe - A SAM Coupe emulator
//
// Util.cpp: Debug tracing, and other utility tasks
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

// Changed 1999-2001 by Simon Owen
//  - Added more comprehensive tracing function with variable arguments

// ToDo:
//  - make better use of writing to a log for error conditions to help
//    troubleshoot any problems in release versions.  We currently rely
//    too much on debug tracing in debug-only versions.

#include "SimCoupe.h"
#include "Util.h"

#include "CPU.h"
#include "Memory.h"
#include "Main.h"
#include "Options.h"
#include "UI.h"

namespace Util
{

bool Init()
{
    return true;
}

void Exit()
{
}


fs::path UniqueOutputPath(const std::string& ext)
{
    auto output_path = OSD::MakeFilePath(PathType::Output);

    for (;;)
    {
        auto path = output_path / fmt::format("simc{:04}.{}", GetOption(nextfile), ext);
        SetOption(nextfile, GetOption(nextfile) + 1);

        if (!fs::exists(path))
        {
            return path;
        }
    }
}

} // namespace Util

//////////////////////////////////////////////////////////////////////////////

void Message(MsgType type, const std::string& message)
{
    TRACE("{}\n", message);
    UI::ShowMessage(type, message);

    if (type == MsgType::Fatal)
    {
        Main::Exit();
        exit(1);
    }
}


uint8_t GetSizeCode(unsigned int uSize_)
{
    uint8_t bCode = 0;
    for (bCode = 0; uSize_ > 128; bCode++, uSize_ >>= 1);
    return bCode;
}


std::string AbbreviateSize(uint64_t ullSize_)
{
    static std::string units = "KMGTPE";

    // Work up from Kilobytes
    auto nUnits = 0;
    ullSize_ /= 1000;

    // Loop while there are more than 1000 and we have another unit to move up to
    while (ullSize_ >= 1000)
    {
        // Determine the percentage error/loss in the next scaling
        auto uClipPercent = static_cast<unsigned int>((ullSize_ % 1000) * 100 / (ullSize_ - (ullSize_ % 1000)));

        // Stop if it's at least 20%
        if (uClipPercent >= 20)
            break;

        // Next unit, rounding to nearest
        nUnits++;
        ullSize_ = (ullSize_ + 500) / 1000;
    }

    return fmt::format("{}{}B", static_cast<unsigned int>(ullSize_), units[nUnits]);
}


// CRC-CCITT for id/data checksums, with bit and byte order swapped
uint16_t CrcBlock(const void* pcv_, size_t uLen_, uint16_t wCRC_/*=0xffff*/)
{
    static uint16_t awCRC[256];

    // Build the table if not already built
    if (!awCRC[1])
    {
        for (int i = 0; i < 256; i++)
        {
            uint16_t w = i << 8;

            // 8 shifts, for each bit in the update byte
            for (int j = 0; j < 8; j++)
                w = (w << 1) ^ ((w & 0x8000) ? 0x1021 : 0);

            awCRC[i] = w;
        }
    }

    // Update the CRC with each byte in the block
    auto pb = reinterpret_cast<const uint8_t*>(pcv_);
    while (uLen_--)
        wCRC_ = (wCRC_ << 8) ^ awCRC[((wCRC_ >> 8) ^ *pb++) & 0xff];

    // Return the updated CRC
    return wCRC_;
}


void PatchBlock(uint8_t* pb_, uint8_t* pbPatch_)
{
    for (;;)
    {
        // Flag+length in big-endian format
        uint16_t wLen = (pbPatch_[0] << 8) | pbPatch_[1];
        pbPatch_ += 2;

        // End marker is zero
        if (!wLen)
            break;

        // Top bit clear for skip
        else if (!(wLen & 0x8000))
            pb_ += wLen;

        // Remaining 15 bits for copy length
        else
        {
            wLen &= 0x7fff;
            memcpy(pb_, pbPatch_, wLen);
            pb_ += wLen;
            pbPatch_ += wLen;
        }
    }
}


// SAM ROM triple-peek used for stored addresses
unsigned int TPeek(const uint8_t* pb_)
{
    unsigned int u = ((pb_[0] & 0x1f) << 14) | ((pb_[2] & 0x3f) << 8) | pb_[1];

    // Clip to 512K
    return (u & ((1U << 19) - 1));
}


void AdjustBrightness(uint8_t& r_, uint8_t& g_, uint8_t& b_, int nAdjust_)
{
    int nOffset = (nAdjust_ <= 0) ? 0 : nAdjust_;
    int nMult = 100 - ((nAdjust_ <= 0) ? -nAdjust_ : nAdjust_);

    r_ = nOffset + (r_ * nMult / 100);
    g_ = nOffset + (g_ * nMult / 100);
    b_ = nOffset + (b_ * nMult / 100);
}

uint32_t RGB2Native(uint8_t r_, uint8_t g_, uint8_t b_, uint32_t dwRMask_, uint32_t dwGMask_, uint32_t dwBMask_)
{
    return RGB2Native(r_, g_, b_, 0, dwRMask_, dwGMask_, dwBMask_, 0);
}

uint32_t RGB2Native(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_, uint32_t dwRMask_, uint32_t dwGMask_, uint32_t dwBMask_, uint32_t dwAMask_)
{
    uint32_t dwRed = static_cast<uint32_t>(((static_cast<uint64_t>(dwRMask_)* (r_ + 1)) >> 8)& dwRMask_);
    uint32_t dwGreen = static_cast<uint32_t>(((static_cast<uint64_t>(dwGMask_)* (g_ + 1)) >> 8)& dwGMask_);
    uint32_t dwBlue = static_cast<uint32_t>(((static_cast<uint64_t>(dwBMask_)* (b_ + 1)) >> 8)& dwBMask_);
    uint32_t dwAlpha = static_cast<uint32_t>(((static_cast<uint64_t>(dwAMask_)* (a_ + 1)) >> 8)& dwAMask_);

    return dwRed | dwGreen | dwBlue | dwAlpha;
}


std::string tolower(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(),
        [](uint8_t ch) { return std::tolower(ch); });

    return str;
}

std::vector<std::string> split(const std::string& str, char sep)
{
    std::vector<std::string> strings;
    std::istringstream ss(str);
    std::string s;
    while (getline(ss, s, sep))
    {
        strings.push_back(std::move(s));
    }
    return strings;
}

////////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG

std::string TimeString()
{
    using namespace std::chrono;

    static std::optional<high_resolution_clock::time_point> start_time;
    auto now = high_resolution_clock::now();
    if (!start_time)
        start_time = now;
    auto elapsed = duration_cast<milliseconds>(now - *start_time).count();

    auto ms = elapsed % 1000;
    auto secs = (elapsed /= 1000) % 60;
    auto mins = (elapsed /= 60) % 100;

    auto screen_cycles = (CPU::frame_cycles + CPU_CYCLES_PER_FRAME - CPU_CYCLES_PER_SIDE_BORDER) % CPU_CYCLES_PER_FRAME;
    auto line = screen_cycles / CPU_CYCLES_PER_LINE;
    auto line_cycle = screen_cycles % CPU_CYCLES_PER_LINE;

    return fmt::format("{:02}:{:02}.{:03} {:03}:{:03}", mins, secs, ms, line, line_cycle);
}

void TraceOutputString(const std::string& str)
{
    OSD::DebugTrace(fmt::format("{} {}", TimeString(), str));
}
#endif  // _DEBUG
