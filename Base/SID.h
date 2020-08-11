// Part of SimCoupe - A SAM Coupe emulator
//
// SID.h: SID interface implementation using reSID library
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

#include "Sound.h"

#undef SID  // TODO: fix Win32 name clash using namespace

#include <resid/sid.h>
#define SID_CLOCK_PAL   985248

class SIDDevice final : public SoundDevice
{
public:
    SIDDevice();

public:
    void Reset() override;
    void Update(bool fFrameEnd_);
    void FrameEnd() override;

    void Out(uint16_t wPort_, uint8_t bVal_) override;

protected:
    std::unique_ptr<SID> m_sid;
    int m_chip_type = 0;
};

extern std::unique_ptr<SIDDevice> pSID;
