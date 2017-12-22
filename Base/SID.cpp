// Part of SimCoupe - A SAM Coupe emulator
//
// SID.cpp: SID interface implementation using reSID library
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

#include "SimCoupe.h"
#include "SID.h"

#include "CPU.h"
#include "Options.h"


CSID::CSID ()
{
#ifdef USE_RESID
    m_pSID = new RESID_NAMESPACE::SID;
    Reset();
#endif
}

CSID::~CSID ()
{
#ifdef USE_RESID
    delete m_pSID; m_pSID = nullptr;
#endif
}

void CSID::Reset ()
{
#ifdef USE_RESID
    if (m_pSID)
    {
        m_nChipType = GetOption(sid);
        m_pSID->set_chip_model((m_nChipType == 2) ? RESID_NAMESPACE::MOS8580 : RESID_NAMESPACE::MOS6581);

        m_pSID->reset();
        m_pSID->adjust_sampling_frequency(SAMPLE_FREQ);
    }
#endif
}

void CSID::Update (bool fFrameEnd_=false)
{
#ifdef USE_RESID
    int nSamplesSoFar = fFrameEnd_ ? pDAC->GetSampleCount() : pDAC->GetSamplesSoFar();

    int nNeeded = nSamplesSoFar - m_nSamplesThisFrame;
    if (!m_pSID || nNeeded <= 0)
        return;

    short *ps = reinterpret_cast<short*>(m_pbFrameSample + m_nSamplesThisFrame*SAMPLE_BLOCK);

    if (g_fReset)
        memset(ps, 0x00, nNeeded*SAMPLE_BLOCK); // no clock means no output
    else
    {
        int sid_clock = SID_CLOCK_PAL;

        // Generate the mono SID samples for the left channel
        m_pSID->clock(sid_clock, ps, nNeeded, 2);

        // Duplicate the left samples for the right channel
        for (int i = 0 ; i < nNeeded ; i++, ps += 2)
            ps[1] = ps[0];
    }

    m_nSamplesThisFrame = nSamplesSoFar;
#else
	(void)fFrameEnd_;
#endif
}

void CSID::FrameEnd ()
{
    // Check for change of chip type
    if (GetOption(sid) != m_nChipType)
        Reset();

    Update(true);
    m_nSamplesThisFrame = 0;
}

void CSID::Out (WORD wPort_, BYTE bVal_)
{
#ifdef USE_RESID
    Update();

    BYTE bReg = wPort_ >> 8;

    if (m_pSID)
        m_pSID->write(bReg & 0x1f, bVal_);
#else
    (void)wPort_; (void)bVal_;
#endif
}
