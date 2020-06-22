// Part of SimCoupe - A SAM Coupe emulator
//
// Main.cpp: Main entry point
//
//  Copyright (c) 1999-2014 Simon Owen
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
#include "Main.h"

#include "CPU.h"
#include "Frame.h"
#include "GUI.h"
#include "Input.h"
#include "Options.h"
#include "OSD.h"
#include "Sound.h"
#include "UI.h"
#include "Util.h"
#include "Video.h"


extern "C" int main(int argc_, char* argv_[])
{
    if (Main::Init(argc_, argv_))
        CPU::Run();

    Main::Exit();

    return 0;
}

namespace Main
{

bool Init(int argc_, char* argv_[])
{
    // Load settings and check command-line options
    if (!Util::Init() || !Options::Load(argc_, argv_))
        return 0;

    // Initialise all modules
    return OSD::Init(true) && Frame::Init() && CPU::Init(true) && UI::Init(true) && Sound::Init(true) && Input::Init(true) && Video::Init(true);
}

void Exit()
{
    GUI::Stop();

    Video::Exit();
    Input::Exit();
    Sound::Exit();
    UI::Exit();
    CPU::Exit();
    Frame::Exit();
    OSD::Exit();

    Options::Save();

    Util::Exit();
}

} // namespace Main
