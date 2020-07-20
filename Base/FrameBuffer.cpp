// Part of SimCoupe - A SAM Coupe emulator
//
// FrameBuffer.cpp: SAM screen handling, including on-screen display text
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
#include "FrameBuffer.h"

#include "Font.h"

FrameBuffer::FrameBuffer(int width, int height) :
    m_width(width), m_height(height), m_clip_width(width), m_clip_height(height), m_pFont{ sGUIFont }
{
    assert((width & 0xf) == 0);
    m_framebuffer.resize(width * height);
}

void FrameBuffer::ClipTo(int x, int y, int width, int height)
{
    m_clip_x = std::max(0, x);
    m_clip_y = std::max(0, y);
    m_clip_width = std::min(m_width - m_clip_x, width - (m_clip_x - x));
    m_clip_height = std::min(m_height - m_clip_y, height - (m_clip_y - y));
}

void FrameBuffer::ClipNone()
{
    m_clip_x = 0;
    m_clip_y = 0;
    m_clip_width = m_width;
    m_clip_height = m_height;
}

bool FrameBuffer::Clip(int& x, int& y, int& width, int& height)
{
    auto orig_x = x;
    auto orig_y = y;

    x = std::max(m_clip_x, x);
    y = std::max(m_clip_y, y);
    width = std::min(m_clip_width - (x - m_clip_x), width - (x - orig_x));
    height = std::min(m_clip_height - (y - m_clip_y), height - (y - orig_y));

    return width > 0 && height > 0;
}

////////////////////////////////////////////////////////////////////////////////

void FrameBuffer::Plot(int x, int y, uint8_t colour)
{
    auto width = 1;
    auto height = 1;

    if (Clip(x, y, width, height))
        GetLine(y)[x] = colour;
}

void FrameBuffer::DrawLine(int x, int y, int width, int height, uint8_t colour)
{
    if (width > 0)
    {
        height = 1;
        if (Clip(x, y, width, height))
            memset(GetLine(y++) + x, colour, width);
    }
    else if (height > 0)
    {
        width = 1;
        if (Clip(x, y, width, height))
        {
            while (height--)
                GetLine(y++)[x] = colour;
        }
    }
}

void FrameBuffer::FillRect(int x, int y, int width, int height, uint8_t colour)
{
    if (Clip(x, y, width, height))
    {
        while (height--)
            memset(GetLine(y++) + x, colour, width);
    }
}

void FrameBuffer::FrameRect(int x, int y, int width, int height, uint8_t colour, bool round)
{
    if (width == 1)
    {
        DrawLine(x, y, 0, height, colour);
    }
    else if (height == 1)
    {
        DrawLine(x, y, width, 0, colour);
    }
    else
    {
        int r = round ? 1 : 0;
        DrawLine(x + r, y, width - r * 2, 0, colour);
        DrawLine(x, y + r, 0, height - r * 2, colour);
        DrawLine(x + width - 1, y + r, 0, height - r * 2, colour);
        DrawLine(x + r, y + height - 1, width - r * 2, 0, colour);
    }
}

void FrameBuffer::DrawImage(int x, int y, int width, int height, const uint8_t* img_data, const uint8_t* img_palette)
{
    int orig_x = x;
    int orig_y = y;
    int orig_width = width;

    if (!Clip(x, y, width, height))
        return;

    for (int yy = y; yy < (y + height); yy++)
    {
        auto pImage = img_data + (yy - orig_y) * orig_width;
        auto pLine = GetLine(yy);

        for (int xx = x; xx < (x + width); xx++)
        {
            auto colour = pImage[xx - orig_x];
            if (colour)
                pLine[xx] = img_palette[colour];
        }
    }
}

void FrameBuffer::Poke(int x, int y, const uint8_t* data, int len)
{
    auto orig_x = x;
    auto width = static_cast<int>(len);
    auto height = 1;

    if (Clip(x, y, width, height))
        memcpy(GetLine(y++) + x, data + x - orig_x, width);
}

int FrameBuffer::StringWidth(const char* pcsz_, int max_chars) const
{
    return m_pFont->StringWidth(pcsz_, max_chars);
}

void FrameBuffer::DrawString(int x, int y, const std::string& str, uint8_t default_colour)
{
    auto in_colour = true;
    auto expect_colour = false;
    auto colour = default_colour;
    int left = x;

    for (uint8_t ch : str)
    {
        if (ch == '\n')
        {
        x = left;
        y += m_pFont->height + Font::LINE_SPACING;
        continue;
        }
        else if (ch == '\a')
        {
            expect_colour = true;
            continue;
        }
        else if (expect_colour)
        {
            expect_colour = false;

            if (!in_colour)
                continue;

            switch (ch)
            {
            case 'k': colour = BLACK;       break;
            case 'b': colour = BLUE_8;      break;
            case 'r': colour = RED_8;       break;
            case 'm': colour = MAGENTA_8;   break;
            case 'g': colour = GREEN_8;     break;
            case 'c': colour = CYAN_8;      break;
            case 'y': colour = YELLOW_8;    break;
            case 'w': colour = GREY_6;      break;

            case 'K': colour = GREY_5;      break;
            case 'B': colour = BLUE_5;      break;
            case 'R': colour = RED_5;       break;
            case 'M': colour = MAGENTA_5;   break;
            case 'G': colour = GREEN_5;     break;
            case 'C': colour = CYAN_5;      break;
            case 'Y': colour = YELLOW_5;    break;
            case 'W': colour = WHITE;       break;

            case '0':
                in_colour = false;
                default_colour = colour;
                break;
            case '1':
                in_colour = true;
                break;

            case 'X':
                colour = default_colour;
                break;
            }

            continue;
        }

        if (ch < m_pFont->first_chr || ch > m_pFont->last_chr)
            ch = Font::DEFAULT_CHR;

        auto data_offset = (ch - m_pFont->first_chr) * m_pFont->bytes_per_chr;
        auto width = (m_pFont->data[data_offset++] & 0x0f) + m_pFont->width;

        if (m_pFont->fixed_width)
        {
            auto shifts = m_pFont->data[data_offset - 1] >> 4;
            x += shifts;
            width = m_pFont->width - shifts;
        }

        int y_from = std::max(m_clip_y, y);
        int y_to = y + m_pFont->height;
        y_to = std::min(m_clip_y + m_clip_height - 1, y_to);

        if (ch != ' ' && (x >= m_clip_x) && (x + width <= m_clip_x + m_clip_width))
        {
            data_offset += (y_from - y);

            for (int i = y_from; i < y_to; ++i)
            {
                auto pLine = GetLine(i) + x;
                auto data = m_pFont->data[data_offset++];

                if (data & 0x80) pLine[0] = colour;
                if (data & 0x40) pLine[1] = colour;
                if (data & 0x20) pLine[2] = colour;
                if (data & 0x10) pLine[3] = colour;
                if (data & 0x08) pLine[4] = colour;
                if (data & 0x04) pLine[5] = colour;
                if (data & 0x02) pLine[6] = colour;
                if (data & 0x01) pLine[7] = colour;
            }
        }

        x += width + Font::CHAR_SPACING;
    }
}

void FrameBuffer::SetFont(std::shared_ptr<Font> font)
{
    m_pFont = font;
}
