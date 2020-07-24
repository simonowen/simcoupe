// Part of SimCoupe - A SAM Coupe emulator
//
// UI.h: SDL user interface
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

#pragma once

#ifdef _DEBUG
#define WINDOW_CAPTION      "SimCoupe/SDL [DEBUG]"
#else
#define WINDOW_CAPTION      "SimCoupe/SDL"
#endif

// This file is included from ObjC source on macOS.
#ifdef __cplusplus

#include "Actions.h"
#include "Video.h"

class UI
{
public:
    static bool Init(bool fFirstInit_ = false);
    static void Exit(bool fReInit_ = false);

    static std::unique_ptr<IVideoRenderer> CreateVideo();
    static bool CheckEvents();

    static bool DoAction(Action action, bool pressed = true);
    static void ShowMessage(MsgType type, const std::string& str);
};

extern bool g_fActive;

#endif  // __cplusplus

// SDL_USEREVENT codes for external events (Mac OS X GUI)
#define UE_BASE                 1000
#define UE_OPENFILE             (UE_BASE+1)
#define UE_TOGGLEFULLSCREEN     (UE_BASE+2)
#define UE_TOGGLESYNC           (UE_BASE+3)
#define UE_TOGGLEGREYSCALE      (UE_BASE+4)
#define UE_RESETBUTTON          (UE_BASE+5)
#define UE_NMIBUTTON            (UE_BASE+6)
#define UE_TOGGLE54             (UE_BASE+8)
#define UE_TEMPTURBOON          (UE_BASE+9)
#define UE_TEMPTURBOOFF         (UE_BASE+10)
#define UE_DEBUGGER             (UE_BASE+11)
#define UE_SAVESCREENSHOT       (UE_BASE+12)
//#define UE_CHANGEPROFILER       (UE_BASE+13)
#define UE_PAUSE                (UE_BASE+14)
#define UE_TOGGLETURBO          (UE_BASE+15)
#define UE_TOGGLEMUTE           (UE_BASE+16)
#define UE_RELEASEMOUSE         (UE_BASE+17)
//#define UE_CHANGEWINDOWSIZE     (UE_BASE+18)
//#define UE_CHANGEBORDERS        (UE_BASE+19)
#define UE_OPTIONS              (UE_BASE+20)
#define UE_IMPORTDATA           (UE_BASE+21)
#define UE_EXPORTDATA           (UE_BASE+22)
#define UE_RECORDGIF            (UE_BASE+23)
#define UE_RECORDGIFLOOP        (UE_BASE+24)
#define UE_RECORDGIFSTOP        (UE_BASE+25)
#define UE_RECORDWAV            (UE_BASE+26)
#define UE_RECORDWAVSEGMENT     (UE_BASE+27)
#define UE_RECORDWAVSTOP        (UE_BASE+28)
#define UE_RECORDAVI            (UE_BASE+29)
#define UE_RECORDAVIHALF        (UE_BASE+30)
#define UE_RECORDAVISTOP        (UE_BASE+31)
