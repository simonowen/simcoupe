// Part of SimCoupe - A SAM Coupe emulator
//
// Video.cpp: Base video interface
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

#include "SimCoupe.h"
#include "Video.h"

#include "Frame.h"
#include "GUI.h"
#include "Options.h"
#include "UI.h"

namespace Video
{

static std::unique_ptr<IVideoBase> s_pVideo;


bool Init()
{
    Exit();

    s_pVideo = UI::CreateVideo();
    if (!s_pVideo)
        Message(MsgType::Fatal, "Video initialisation failed");

    return s_pVideo != nullptr;
}

void Exit()
{
    s_pVideo.reset();
}

void OptionsChanged()
{
    if (s_pVideo)
    {
        s_pVideo->OptionsChanged();
    }
}

void Update(const FrameBuffer& fb)
{
    s_pVideo->Update(fb);
}

void NativeToSam(int& x, int& y)
{
    auto scale = GUI::IsActive() ? 1 : 2;
    auto rect = s_pVideo->DisplayRect();

    if (rect.w && rect.h)
    {
        x = (x - rect.x) * Frame::Width() / rect.w / scale;
        y = (y - rect.y) * Frame::Height() / rect.h / scale;
    }
}

void ResizeWindow(int height)
{
    s_pVideo->ResizeWindow(height);
}

Rect DisplayRect()
{
    return s_pVideo->DisplayRect();
}

std::pair<int, int> MouseRelative()
{
    return s_pVideo->MouseRelative();
}

} // namespace Video
