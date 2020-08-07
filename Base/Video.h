// Part of SimCoupe - A SAM Coupe emulator
//
// Video.h: Base video interface
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#pragma once

#include "FrameBuffer.h"

namespace Video
{
constexpr auto background_fill_percent = 10;

bool Init();
void Exit();

void NativeToSam(int& x, int& y);
void ResizeWindow(int height);
std::pair<int, int> MouseRelative();

void OptionsChanged();
void Update(const FrameBuffer& fb);
}


class IVideoBase
{
public:
    virtual ~IVideoBase() = default;

    virtual bool Init() = 0;
    virtual Rect DisplayRect() const = 0;
    virtual void ResizeWindow(int height) const = 0;
    virtual std::pair<int, int> MouseRelative() = 0;
    virtual void OptionsChanged() = 0;
    virtual void Update(const FrameBuffer& fb) = 0;
};
