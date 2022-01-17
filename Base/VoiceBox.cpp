// Part of SimCoupe - A SAM Coupe emulator
//
// VoiceBox.cpp: Blue Alpha VoiceBox
//
//  Copyright (c) 2020 Simon Owen
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
#include "VoiceBox.h"

#include "CPU.h"
#include "BlueAlpha.h"
#include "Options.h"
#include "Sound.h"
#include "Stream.h"

constexpr uint16_t SP0256_ROM_ADDR = 0x1000;

VoiceBoxDevice::VoiceBoxDevice() :
    m_sp0256(SAMPLE_FREQ)
{
    auto rom_path = OSD::MakeFilePath(PathType::Resource, "sp0256-al2.bin");
    if (auto file = Stream::Open(rom_path.c_str()))
    {
        std::vector<uint8_t> rom(file->GetSize());
        file->Read(rom.data(), rom.size());
        m_sp0256.load_rom(SP0256_ROM_ADDR, rom);
    }
    else if (GetOption(voicebox))
    {
        Message(MsgType::Warning, "Error loading SP0256 allophone data:\n\n{}", rom_path);
    }

    Reset();
}

void VoiceBoxDevice::Reset()
{
    m_sp0256.reset();
}

uint8_t VoiceBoxDevice::In(uint16_t /*port*/)
{
    return BLUEALPHA_SIGNATURE | (m_sp0256.spb640_r(0) ? 0 : 1);
}

void VoiceBoxDevice::Out(uint16_t /*port*/, uint8_t val)
{
    Update();
    m_sp0256.spb640_w(0, val);
}

void VoiceBoxDevice::Update(bool frame_end)
{
    if (!GetOption(voicebox))
        return;

    int samples_so_far = frame_end ? pDAC->GetSampleCount() : pDAC->GetSamplesSoFar();

    int samples_needed = samples_so_far - m_samples_this_frame;
    if (samples_needed <= 0)
        return;

    auto pb = m_sample_buffer.data() + m_samples_this_frame * BYTES_PER_SAMPLE;
    if (CPU::reset_asserted)
    {
        memset(pb, 0x00, samples_needed * BYTES_PER_SAMPLE);
    }
    else
    {
        m_sp0256.sound_stream_update((stream_sample_t*)pb, samples_needed);

        auto pw = reinterpret_cast<int16_t*>(pb);
        for (int i = samples_needed - 1; i >= 0; --i)
        {
            pw[i * 2] = pw[i * 2 + 1] = pw[i];
        }
    }

    m_samples_this_frame = samples_so_far;
}

void VoiceBoxDevice::FrameEnd()
{
    Update(true);
    m_samples_this_frame = 0;
}

/*
10 REM Blue Alpha VoiceBox Demo
20 IF IN 65407 BAND 254 <> 24 THEN PRINT "No VoiceBox?" : STOP
30 IF IN 65407 BAND 1 = 1 THEN GO TO 30
40 READ a : OUT 65407,a
50 IF a <> 0 THEN GO TO 30
60 DATA 46,7,45,1,42,30,16, 2, 17,31, 2, 55,55,12,16,8,31,9,20, 0
*/
