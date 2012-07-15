// Part of SimCoupe - A SAM Coupe emulator
//
// Action.h: Actions bound to functions keys, etc.
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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef ACTION_H
#define ACTION_H

enum eActions
{
    actNewDisk1, actInsertFloppy1, actEjectFloppy1, actSaveFloppy1, actNewDisk2, actInsertFloppy2, actEjectFloppy2, actSaveFloppy2,
    actExitApplication, actOptions, actDebugger, actImportData, actExportData, actSaveScreenshot, actChangeProfiler,
    actResetButton, actNmiButton, actPause, actFrameStep, actToggleTurbo, actTempTurbo,
    actToggleSync, actToggleFullscreen, actChangeWindowSize, actChangeBorders, actToggle5_4,
    actChangeFrameSkip_REMOVED, actToggleScanlines, actToggleGreyscale, actToggleMute, actReleaseMouse,
    actPrinterOnline, actFlushPrinter, actAbout, actMinimise, actRecordGif, actRecordGifLoop, actRecordGifStop,
    actRecordWav,actRecordWavSegment, actRecordWavStop, actRecordAvi, actRecordAviHalf, actRecordAviStop,
    actSpeedFaster, actSpeedSlower, actSpeedNormal, actPaste, actPasteFile, MAX_ACTION
};

class Action
{
    public:
        static bool Do (int nAction_, bool fPressed_=true);
        static void Key (int nFnKey_, bool fPressed_, bool fCtrl_, bool fAlt_, bool fShift_);

        static const char* aszActions[MAX_ACTION];
};

#endif  // ACTION_H
