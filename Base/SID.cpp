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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "SimCoupe.h"
#include "SID.h"

#include "CPU.h"


CSID::CSID ()
	: m_hReSID(NULL)
{
#ifdef USE_RESID
	if (CheckLibFunction(resid, RESID_create))
	{
		m_hReSID = RESID_create();
		Reset();
	}
#endif
}

CSID::~CSID ()
{
#ifdef USE_RESID
	if (m_hReSID)
		RESID_destroy(m_hReSID), m_hReSID = NULL;
#endif
}

void CSID::Reset ()
{
#ifdef USE_RESID
	if (m_hReSID)
	{
		RESID_reset(m_hReSID);
		RESID_adjust_sampling_frequency(m_hReSID, SAMPLE_FREQ);
	}
#endif
}

void CSID::Update (bool fFrameEnd_=false)
{
#ifdef USE_RESID
    int nSamplesSoFar = fFrameEnd_ ? pDAC->GetSampleCount() : pDAC->GetSamplesSoFar();

    int nNeeded = nSamplesSoFar - m_nSamplesThisFrame;
    if (!m_hReSID || nNeeded <= 0)
        return;

    short *ps = reinterpret_cast<short*>(m_pbFrameSample + m_nSamplesThisFrame*SAMPLE_BLOCK);

    if (g_fReset)
        memset(ps, 0x00, nNeeded*SAMPLE_BLOCK); // no clock means no output
    else
    {
		int sid_clock = SID_CLOCK_PAL;

		// Generate the mono SID samples for the left channel
		RESID_clock_ds(m_hReSID, sid_clock, ps, nNeeded, 2);

		// Duplicate the left samples for the right channel
		for (int i = 0 ; i < nNeeded ; i++, ps += 2)
			ps[1] = ps[0];
	}

    m_nSamplesThisFrame = nSamplesSoFar;
#endif
}

void CSID::FrameEnd ()
{
    Update(true);
    m_nSamplesThisFrame = 0;
}

void CSID::Out (WORD wPort_, BYTE bVal_)
{
#ifdef USE_RESID
	Update();

	BYTE bReg = wPort_ >> 8;

	if (m_hReSID /*&&bReg < 0x80*/)
		RESID_write(m_hReSID, bReg & 0x1f, bVal_);
#endif
}
