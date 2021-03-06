// Part of SimCoupe - A SAM Coupe emulator
//
// GUIIcons.h: Icons for the GUI
//
//  Copyright (c) 1999-2001  Simon Owen
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

const int ICON_SIZE = 32;
const int ICON_PALETTE_SIZE = 32;

struct GUI_ICON
{
    uint8_t abPalette[ICON_PALETTE_SIZE];
    uint8_t abData[ICON_SIZE][ICON_SIZE];
};


// These are the icons currently available
extern const GUI_ICON sMouseCursor;
extern const GUI_ICON sSamIcon;
extern const GUI_ICON sChipIcon, sSoundIcon, sDisplayIcon, sKeyboardIcon, sMouseIcon;
extern const GUI_ICON sHardwareIcon, sMidiIcon, sPortIcon, sFloppyDriveIcon, sHardDiskIcon;
extern const GUI_ICON sMiscIcon, sFolderIcon, sDocumentIcon, sDiskIcon, sCompressedIcon;
extern const GUI_ICON sInformationIcon, sWarningIcon, sErrorIcon;
extern const GUI_ICON sStepIntoIcon, sStepOverIcon, sStepOutIcon, sStepToCursorIcon;
