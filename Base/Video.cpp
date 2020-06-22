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

static std::unique_ptr<IVideoRenderer> pVideo;


bool Init(bool fFirstInit_)
{
    Exit(true);

    pVideo = UI::CreateVideo();
    return pVideo && pVideo->Init();
}

void Exit(bool fReInit_/*=false*/)
{
    pVideo.reset();
}


bool CheckCaps(int nCaps_)
{
    return pVideo && ((~pVideo->GetCaps() & nCaps_) == 0);
}

void UpdatePalette()
{
    if (pVideo)
        pVideo->UpdatePalette();
}

void Update(const FrameBuffer& fb)
{
    if (pVideo)
        pVideo->Update(fb);
}

void UpdateSize()
{
    if (pVideo)
        pVideo->UpdateSize();
}

void DisplayToSamSize(int* pnX_, int* pnY_)
{
    if (pVideo)
        pVideo->DisplayToSamSize(pnX_, pnY_);
}

void DisplayToSamPoint(int* pnX_, int* pnY_)
{
    if (pVideo)
        pVideo->DisplayToSamPoint(pnX_, pnY_);
}

} // namespace Video
