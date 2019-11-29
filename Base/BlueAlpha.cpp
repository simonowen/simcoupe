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

// Only a subset of the 8255 PPI functionality has been emulated, as needed
// for documented sampler use.  The 0xc1 initialisation control byte sets the
// following:
//
// - Group A = mode 2 (strobed bi-directional bus)
// - Group B = mode 0
// - PortA = in/out (sample data)
// - PortB = out (b1=ADC enable, b0=DAC enable)
// - PortC = in (b7-3=status/handshaking, b2-1 unused, b0=clock)
//
// Note: This module supports only a subset of the 8255 PPI chip, as used
// for normal sampler operation.  Use outside that mode is currently undefined.

#include "SimCoupe.h"
#include "BlueAlpha.h"

#include "CPU.h"
#include "Options.h"
#include "Sound.h"

#define PORTA_CLOCK             0x01
#define PORTB_DAC_ENABLE        0x01
#define PORTB_ADC_ENABLE        0x02


CBlueAlphaDevice::CBlueAlphaDevice()
{
    Reset();
}

void CBlueAlphaDevice::Reset()
{
    m_bPortA = 0x00;    // data
    m_bPortB = 0xff;    // no active features
    m_bPortC = 0x00;    // no clock
    m_bControl = 0x18;  // control (initialised to BlueAlpha signature?)
}

bool CBlueAlphaDevice::Clock()
{
    // Toggle clock bit every half period
    m_bPortC ^= PORTA_CLOCK;

    // If DAC or ADC are enabled, return true to keep the clock running
    return !!(~m_bPortB & (PORTB_DAC_ENABLE | PORTB_ADC_ENABLE));
}

int CBlueAlphaDevice::GetClockFreq()
{
    int freq = GetOption(samplerfreq);

    // Limit to sensible frequencies
    if (freq < 8000)
        freq = 8000;
    else if (freq > 48000)
        freq = 48000;

    return freq;
}

BYTE CBlueAlphaDevice::In(WORD wPort_)
{
    switch (wPort_ & 3)
    {
    case 0:
        /*
                    // TODO: If the ADC is active, read a sample
                    if (!(m_bPortB & 0x02))
                        m_bPortA = ...;
        */
        return m_bPortA;

    case 2:
        return m_bPortC;
    }

    return 0x00;
}

void CBlueAlphaDevice::Out(WORD wPort_, BYTE bVal_)
{
    switch (wPort_ & 3)
    {
    case 0:
        m_bPortA = bVal_;

        // If the DAC is active, output the sample
//          if (!(m_bPortB & PORTB_DAC_ENABLE)) // TODO: fix output condition for MOD Player
        pDAC->Output(bVal_);

        break;

    case 1:
        // If DAC/ADC were disabled but one is now enabled, start the clock
        if (!(~m_bPortB & (PORTB_DAC_ENABLE | PORTB_ADC_ENABLE)) &&
            (~bVal_ & (PORTB_DAC_ENABLE | PORTB_ADC_ENABLE)))
            AddCpuEvent(evtBlueAlphaClock, g_dwCycleCounter + BLUE_ALPHA_CLOCK_TIME);

        m_bPortB = bVal_;
        break;

    case 3:
        m_bControl = bVal_;

        // If mode 2 is set, set the handshaking lines to show we're ready
        if ((bVal_ & 0xc0) == 0xc0)
            m_bPortC = 0xa0;
        break;
    }
}
