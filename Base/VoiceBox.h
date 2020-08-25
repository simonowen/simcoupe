// Part of SimCoupe - A SAM Coupe emulator
//
// BlueAlpha.cpp: Blue Alpha Sampler
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

#include "SAMIO.h"
#include "sp0256.h"
#include "Sound.h"

class VoiceBoxDevice final : public SoundDevice
{
public:
    VoiceBoxDevice();

    void Reset() override;
    uint8_t In(uint16_t port) override;
    void Out(uint16_t port, uint8_t val) override;
    void FrameEnd() override;

protected:
    void Update(bool frame_end = false);

    sp0256_device m_sp0256;
};

extern std::unique_ptr<VoiceBoxDevice> pVoiceBox;
