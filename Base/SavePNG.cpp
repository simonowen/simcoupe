// Part of SimCoupe - A SAM Coupe emulator
//
// PNG.cpp: Screenshot saving in PNG format
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

#include "SimCoupe.h"
#include "SavePNG.h"
#include "Frame.h"

#ifdef HAVE_LIBZ
#include "zlib.h"

namespace
{
constexpr std::string_view PNG_SIGNATURE{ "\x89PNG\r\n\x1A\n" };
constexpr std::string_view PNG_CN_IHDR{ "IHDR" };
constexpr std::string_view PNG_CN_PLTE{ "PLTE" };
constexpr std::string_view PNG_CN_IDAT{ "IDAT" };
constexpr std::string_view PNG_CN_IEND{ "IEND" };

constexpr uint8_t PNG_BIT_DEPTH{ 8 };
constexpr uint8_t PNG_COLOR_MASK_COLOR{ 3 };        // Indexed-colour
constexpr uint8_t PNG_COMPRESSION_TYPE_BASE{ 0 };   // Deflate method 8, 32K window
constexpr uint8_t PNG_FILTER_TYPE_DEFAULT{ 0 };     // Single row per-byte filtering
constexpr uint8_t PNG_INTERLACE_NONE{ 0 };          // Non-interlaced image

struct PNG_IHDR
{
    std::array<uint8_t, sizeof(uint32_t)> width_be;
    std::array<uint8_t, sizeof(uint32_t)> height_be;
    uint8_t bitdepth{ PNG_BIT_DEPTH };
    uint8_t colour{ PNG_COLOR_MASK_COLOR };
    uint8_t compression{ PNG_COMPRESSION_TYPE_BASE };
    uint8_t filter{ PNG_FILTER_TYPE_DEFAULT };
    uint8_t interlace{ PNG_INTERLACE_NONE };
};
static_assert(sizeof(PNG_IHDR) == 13, "Bad PNG_IHDR size");

constexpr uint32_t big_endian_value(uint32_t x)
{
#ifdef __BIG_ENDIAN__
    return x;
#else
    return byteswap(x);
#endif
}

bool write_chunk(FILE* hFile_, const std::string_view& type, const void* data=nullptr, size_t data_len=0)
{
    auto length_be = big_endian_value(static_cast<uint32_t>(data_len));
    size_t written = fwrite(&length_be, 1, sizeof(length_be), hFile_);

    written += fwrite(type.data(), 1, type.size(), hFile_);
    auto crc = crc32(0, reinterpret_cast<const Bytef*>(type.data()), static_cast<uInt>(type.size()));

    if (data)
    {
        written += fwrite(data, 1, data_len, hFile_);
        crc = crc32(crc, reinterpret_cast<const Bytef*>(data), static_cast<uInt>(data_len));
    }

    auto crc_be = big_endian_value(static_cast<uint32_t>(crc));
    written += fwrite(&crc_be, 1, sizeof(crc_be), hFile_);

    return written == ((3 * sizeof(uint32_t)) + data_len);
}

auto ihdr_block(const FrameBuffer& fb)
{
    auto width_be = big_endian_value(fb.Width());
    auto height_be = big_endian_value(fb.Height() * PAL_FIELDS_PER_FRAME);

    PNG_IHDR ihdr{
        {
            static_cast<uint8_t>(width_be & 0xff),
            static_cast<uint8_t>((width_be >> 8) & 0xff),
            static_cast<uint8_t>((width_be >> 16) & 0xff),
            static_cast<uint8_t>((width_be >> 24) & 0xff)
        },
        {
            static_cast<uint8_t>(height_be & 0xff),
            static_cast<uint8_t>((height_be >> 8) & 0xff),
            static_cast<uint8_t>((height_be >> 16) & 0xff),
            static_cast<uint8_t>((height_be >> 24) & 0xff)
        },
        PNG_BIT_DEPTH,
        PNG_COLOR_MASK_COLOR,
        PNG_COMPRESSION_TYPE_BASE,
        PNG_FILTER_TYPE_DEFAULT,
        PNG_INTERLACE_NONE
    };

    return ihdr;
}

auto plte_block(const FrameBuffer& fb)
{
    auto sam_palette = IO::Palette();

    std::vector<uint8_t> plte;
    plte.reserve(sam_palette.size() * sizeof(sam_palette[0]));

    for (const auto& c : sam_palette)
    {
        plte.push_back(c.red);
        plte.push_back(c.green);
        plte.push_back(c.blue);
    }

    return plte;
}

auto idat_block(const FrameBuffer& fb) -> std::optional<std::vector<uint8_t>>
{
    auto width = fb.Width();
    auto height = fb.Height() * PAL_FIELDS_PER_FRAME;

    std::vector<uint8_t> img_data;
    img_data.reserve(height * (1 + width));

    for (int y = 0; y < height; ++y)
    {
        auto line_ptr = fb.GetLine(y >> 1);
        img_data.push_back(PNG_FILTER_TYPE_DEFAULT);
        std::copy(line_ptr, line_ptr + width, std::back_inserter(img_data));
    }

    constexpr auto output_size_factor = 2;
    std::vector<uint8_t> zdata;
    zdata.resize(img_data.size() * output_size_factor);
    auto zlen = static_cast<uLongf>(zdata.capacity());

    if (compress(zdata.data(), &zlen,
        static_cast<const Bytef*>(img_data.data()),
        static_cast<uLong>(img_data.size())) != Z_OK)
    {
        return std::nullopt;
    }

    zdata.resize(zlen);
    return zdata;
}

bool SaveFile(FILE* file, const FrameBuffer& fb)
{
    auto ihdr = ihdr_block(fb);
    auto plte_data = plte_block(fb);
    auto idat_data = idat_block(fb);

    return idat_data.has_value() &&
        fwrite(PNG_SIGNATURE.data(), PNG_SIGNATURE.size(), 1, file) &&
        write_chunk(file, PNG_CN_IHDR, &ihdr, sizeof(ihdr)) &&
        write_chunk(file, PNG_CN_PLTE, plte_data.data(), plte_data.size()) &&
        write_chunk(file, PNG_CN_IDAT, idat_data->data(), idat_data->size()) &&
        write_chunk(file, PNG_CN_IEND);
}

} // namespace

#endif // HAVE_LIBZ


namespace PNG
{

bool Save(const FrameBuffer& fb)
{
#ifdef HAVE_LIBZ
    auto png_path = Util::UniqueOutputPath("png");
    unique_FILE file = fopen(png_path.c_str(), "wb");
    if (file && SaveFile(file, fb))
    {
        Frame::SetStatus("Saved {}", png_path);
        return true;
    }

    Frame::SetStatus("Save failed: {}", png_path);
#else
    Frame::SetStatus("Screen saving requires zlib");
#endif

    return false;
}

} // namespace PNG
