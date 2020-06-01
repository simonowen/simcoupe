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
    NewDisk1, InsertFloppy1, EjectFloppy1, SaveFloppy1, NewDisk2, InsertFloppy2,
    EjectFloppy2, SaveFloppy2, ExitApplication, Options, Debugger, ImportData,
    ExportData, SavePNG, ChangeProfiler_REMOVED, ResetButton, NmiButton,
    Pause, FrameStep, ToggleTurbo, TempTurbo, ToggleScanHiRes_REMOVED, ToggleFullscreen,
    ChangeWindowSize_REMOVED, ChangeBorders_REMOVED, Toggle5_4, ToggleFilter,
    ToggleScanlines_REMOVED, ToggleGreyscale, ToggleMute, ReleaseMouse, PrinterOnline,
    FlushPrinter, About, Minimise, RecordGif, RecordGifLoop, RecordGifStop,
    RecordWav, RecordWavSegment, RecordWavStop, RecordAvi, RecordAviHalf,
    RecordAviStop, SpeedFaster, SpeedSlower, SpeedNormal, Paste, TapeInsert,
    TapeEject, TapeBrowser, SaveSSX
};

namespace Actions
{
bool Do(Action action, bool pressed = true);
void Key(int fn_key, bool pressed, bool ctrl, bool alt, bool shift);
std::string to_string(Action action);
}
