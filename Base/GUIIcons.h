// Part of SimCoupe - A SAM Coupé emulator
//
// GUIIcons.h: Handy icons for the GUI
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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef GUIICONS_H
#define GUIICONS_H


const int ICON_SIZE = 32;
const int ICON_PALETTE_SIZE = 32;

typedef struct
{
    BYTE abPalette[ICON_PALETTE_SIZE];
    BYTE abData[ICON_SIZE][ICON_SIZE];
}
GUI_ICON;


// These are the icons currently available
extern const GUI_ICON sMouseCursor;
extern const GUI_ICON sSamIcon;
extern const GUI_ICON sSoundIcon, sDisplayIcon, sKeyboardIcon, sMouseIcon, sHardwareIcon, sMidiIcon, sPortIcon;
extern const GUI_ICON sDiskIcon, sFolderIcon;

#endif
