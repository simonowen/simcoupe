// Part of SimCoupe - A SAM Coupe emulator
//
// Actions.h: Actions bound to functions keys, etc.
//
//  Copyright (c) 2005-2012 Simon Owen
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

enum class Action
{
    None,
    NewDisk1, InsertDisk1, EjectDisk1,
    NewDisk2, InsertDisk2, EjectDisk2,
    InsertTape, EjectTape, TapeBrowser,
    Paste, ImportData, ExportData, ExportCometSymbols, SavePNG, SaveSSX,
    TogglePrinter, FlushPrinter,
    ToggleFullscreen, ToggleTV, ToggleSmoothing, ToggleMotionBlur,
    RecordAvi, RecordAviHalf, RecordAviStop,
    RecordGif, RecordGifHalf, RecordGifLoop, RecordGifLoopHalf, RecordGifStop,
    RecordWav, RecordWavSegment, RecordWavStop,
    SpeedNormal, SpeedSlower, SpeedFaster, SpeedTurbo, ToggleTurbo,
    Reset, Nmi, Pause, FrameStep,
    ReleaseMouse,
    Options, Debugger, About, Minimise, ExitApp, ToggleRasterDebug,
};

namespace Actions
{
bool Do(Action action, bool pressed = true);
void Key(int fn_key, bool pressed, bool ctrl, bool alt, bool shift);
}
