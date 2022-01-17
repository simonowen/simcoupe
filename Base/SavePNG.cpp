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

#ifdef HAVE_LIBPNG
#include <png.h>
#endif

#include "Frame.h"

namespace PNG
{
#ifdef HAVE_LIBPNG
static std::string s_error;
static std::string s_warning;

static std::vector<uint8_t> PNGData(const FrameBuffer& fb)
{
    constexpr auto HEIGHT_SCALE = 2;
    auto output_height = fb.Height() * HEIGHT_SCALE;

    std::vector<png_const_bytep> row_pointers(output_height);
    for (int y = 0; y < output_height; ++y)
    {
        row_pointers[y] = fb.GetLine(y / HEIGHT_SCALE);
    }

    auto palette = IO::Palette();
    std::vector<png_color> png_palette;
    std::transform(palette.begin(), palette.end(), std::back_inserter(png_palette),
        [](COLOUR c) { return png_color{ c.red, c.green, c.blue }; });

    auto png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr)
    {
        return {};
    }

    auto info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, nullptr);
        return {};
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return {};
    }

    std::vector<uint8_t> png_data;
    png_set_write_fn(png_ptr, &png_data,
        [](png_structp png_ptr, png_bytep data, size_t length)
        {
            auto& buffer = *reinterpret_cast<decltype(&png_data)>(png_get_io_ptr(png_ptr));
            buffer.insert(buffer.end(), data, data + length);
        },
        [](png_structp){ });

    png_set_IHDR(png_ptr, info_ptr,
        fb.Width(), output_height,
        8, PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_set_PLTE(png_ptr, info_ptr, png_palette.data(), static_cast<int>(png_palette.size()));

    png_write_info(png_ptr, info_ptr);
    png_write_image(png_ptr, const_cast<png_bytepp>(row_pointers.data()));
    png_write_end(png_ptr, NULL);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    return png_data;
}

#endif // HAVE_LIBPNG


bool Save(const FrameBuffer& fb)
{
#ifdef HAVE_LIBPNG
    auto png_path = Util::UniqueOutputPath("png");
    if (unique_FILE file = fopen(png_path.c_str(), "wb"))
    {
        if (auto png_data = PNGData(fb); !png_data.empty())
        {
            fwrite(png_data.data(), 1, png_data.size(), file);
            Frame::SetStatus("Saved {}", png_path);
            return true;
        }
    }

    Frame::SetStatus("Save failed: {}", png_path);
#else
    Frame::SetStatus("Screen saving requires libpng");
#endif

    return false;
}

} // namespace PNG
