/*
SAA1099.cpp: Philips SAA 1099 sound chip emulation

Copyright (c) 1998-2004, Dave Hooper <dave@rebuzz.org>  
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

- Neither the name Dave Hooper nor the names of its contributors may
  be used to endorse or promote products derived from this software without
  specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Changes from original SAASound 3.1.3 source [Simon Owen]:
//
// - combined original source into single CPP/H files
// - frequency table is now built at runtime, rather than included
// - removed export wrapper to expose implementation class
// - removed parameter config, leaving 16-bit stereo samples only
// - caller-supplied output frequency, rather than fixed 44.1KHz

#include "SimCoupe.h"

#include "SAA1099.h"

//////////////////////////////////////////////////////////////////////
// CSAAAmp: tone and noise mixing, envelope application and amplification

CSAAAmp::CSAAAmp(CSAAFreq * const ToneGenerator, const CSAANoise * const NoiseGenerator, const CSAAEnv * const EnvGenerator)
:
m_pcConnectedToneGenerator(ToneGenerator),
m_pcConnectedNoiseGenerator(NoiseGenerator),
m_pcConnectedEnvGenerator(EnvGenerator),
m_bUseEnvelope(EnvGenerator != nullptr)
{
	leftleveltimes32=leftleveltimes16=leftlevela0x0e=leftlevela0x0etimes2=0;
	rightleveltimes32=rightleveltimes16=rightlevela0x0e=rightlevela0x0etimes2=0;
	m_nMixMode = 0;
	m_bMute=true;
	m_nOutputIntermediate=0;
	last_level_byte=0;
	level_unchanged=false;
	last_leftlevel=0;
	last_rightlevel=0;
	leftlevel_unchanged=false;
	rightlevel_unchanged=false;
	cached_last_leftoutput=0;
	cached_last_rightoutput=0;
	SetAmpLevel(0x00);
}

void CSAAAmp::SetAmpLevel(BYTE level_byte)
{
	// if level unchanged since last call then do nothing
	if (level_byte != last_level_byte)
	{
		last_level_byte = level_byte;
		leftlevela0x0e = level_byte&0x0e;
		leftlevela0x0etimes2 = leftlevela0x0e<<1;
		leftleveltimes16 = (level_byte&0x0f) << 4;
		leftleveltimes32 = leftleveltimes16 << 1;

		rightlevela0x0e = (level_byte>>4) & 0x0e;
		rightlevela0x0etimes2 = rightlevela0x0e<<1;
		rightleveltimes16 = level_byte & 0xf0;
		rightleveltimes32 = rightleveltimes16 << 1;
	}

}

// output is between 0 and 480 per channel

unsigned short CSAAAmp::LeftOutput() const
{
	if (m_bMute)
		return 0;

	if (m_bUseEnvelope && m_pcConnectedEnvGenerator->IsActive())
	{
		switch (m_nOutputIntermediate)
		{
		case 0:
			return m_pcConnectedEnvGenerator->LeftLevel() * leftlevela0x0etimes2;
		case 1:
			return m_pcConnectedEnvGenerator->LeftLevel() * leftlevela0x0e;
		case 2:
		default:
			return 0;
		}
	}
	else
	{
		switch (m_nOutputIntermediate)
		{
		case 0:
		default:
			return 0;
		case 1:
			return leftleveltimes16;
		case 2:
			return leftleveltimes32;
		}
	}
}

unsigned short CSAAAmp::RightOutput() const
{
	if (m_bMute)
		return 0;

	if (m_bUseEnvelope && m_pcConnectedEnvGenerator->IsActive())
	{
		switch (m_nOutputIntermediate)
		{
		case 0:
			return m_pcConnectedEnvGenerator->RightLevel() * rightlevela0x0etimes2;
		case 1:
			return m_pcConnectedEnvGenerator->RightLevel() * rightlevela0x0e;
		case 2:
		default:
			return 0;
		}
	}
	else
	{
		switch (m_nOutputIntermediate)
		{
		case 0:
		default:
			return 0;
		case 1:
			return rightleveltimes16;
		case 2:
			return rightleveltimes32;
		}
	}
}

void CSAAAmp::SetToneMixer(BYTE bEnabled)
{
	if (bEnabled == 0)
		m_nMixMode &= ~(0x01);
	else
		m_nMixMode |= 0x01;
}

void CSAAAmp::SetNoiseMixer(BYTE bEnabled)
{
	if (bEnabled == 0)
		m_nMixMode &= ~(0x02);
	else
		m_nMixMode |= 0x02;
}


void CSAAAmp::Mute(bool bMute)
{
	// m_bMute refers to the GLOBAL mute setting (sound 28,0)
	// NOT the per-channel mixer settings !!
	m_bMute = bMute;
}


void CSAAAmp::Tick()
{
	switch (m_nMixMode)
	{
	case 0:
		// no tone or noise for this channel
		m_pcConnectedToneGenerator->Tick();
		m_nOutputIntermediate=0;
		break;
	case 1:
		// tone only for this channel
		m_nOutputIntermediate=(m_pcConnectedToneGenerator->Tick());
		// NOTE: ConnectedToneGenerator returns either 0 or 2
		break;
	case 2:
		// noise only for this channel
		m_pcConnectedToneGenerator->Tick();
		m_nOutputIntermediate= m_pcConnectedNoiseGenerator->LevelTimesTwo();
		// NOTE: ConnectedNoiseFunction returns either 0 or 1 using ->Level()
		// and either 0 or 2 when using ->LevelTimesTwo();
		break;
	case 3:
		// tone+noise for this channel ... mixing algorithm :
		m_nOutputIntermediate = m_pcConnectedToneGenerator->Tick();
		if ( m_nOutputIntermediate==2 && (m_pcConnectedNoiseGenerator->Level())==1 )
			m_nOutputIntermediate=1;
		break;
	}
	// intermediate is between 0 and 2
}

CSAAAmp::stereolevel CSAAAmp::TickAndOutputStereo()
{
	static stereolevel retval;
	static const stereolevel zeroval = { {0,0} };

	// first, do the Tick:
	Tick();

	// now calculate the returned amplitude for this sample:

	if (m_bMute)
		return zeroval;

	if (m_bUseEnvelope && m_pcConnectedEnvGenerator->IsActive())
	{
		switch (m_nOutputIntermediate)
		{
		case 0:
			retval.sep.Left=m_pcConnectedEnvGenerator->LeftLevel() * leftlevela0x0etimes2;
			retval.sep.Right=m_pcConnectedEnvGenerator->RightLevel() * rightlevela0x0etimes2;
			return retval;
		case 1:
			retval.sep.Left=m_pcConnectedEnvGenerator->LeftLevel() * leftlevela0x0e;
			retval.sep.Right=m_pcConnectedEnvGenerator->RightLevel() * rightlevela0x0e;
			return retval;
		case 2:
		default:
			return zeroval;
		}
	}
	else
	{
		switch (m_nOutputIntermediate)
		{
		case 0:
		default:
			return zeroval;
		case 1:
			retval.sep.Left=leftleveltimes16;
			retval.sep.Right=rightleveltimes16;
			return retval;
		case 2:
			retval.sep.Left=leftleveltimes32;
			retval.sep.Right=rightleveltimes32;
			return retval;
		}
	}
}


//////////////////////////////////////////////////////////////////////
// CSAAEnv: envelope processing

const CSAAEnv::ENVDATA CSAAEnv::cs_EnvData[8] =
{
	{1,false,	{	{{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},					{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
					{{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},					{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}}},
	{1,true,	{	{{15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},{15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15}},
					{{14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14},{14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14}}}},
	{1,false,	{	{{15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0},			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
					{{14,14,12,12,10,10,8,8,6,6,4,4,2,2,0,0},			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}}},
	{1,true,	{	{{15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0},			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
					{{14,14,12,12,10,10,8,8,6,6,4,4,2,2,0,0},			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}}},
	{2,false,	{	{{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},			{15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0}},
					{{0,0,2,2,4,4,6,6,8,8,10,10,12,12,14,14},			{14,14,12,12,10,10,8,8,6,6,4,4,2,2,0,0}}}},
	{2,true,	{	{{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},			{15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0}},
					{{0,0,2,2,4,4,6,6,8,8,10,10,12,12,14,14},			{14,14,12,12,10,10,8,8,6,6,4,4,2,2,0,0}}}},
	{1,false,	{	{{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
					{{0,0,2,2,4,4,6,6,8,8,10,10,12,12,14,14},			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}}},
	{1,true,	{	{{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}},
					{{0,0,2,2,4,4,6,6,8,8,10,10,12,12,14,14},			{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}}}}
};


CSAAEnv::CSAAEnv()
{
	// initialise itself with the value 'zero'
	SetNewEnvData(0);
}

CSAAEnv::~CSAAEnv()
{
	// Nothing to do
}

void CSAAEnv::InternalClock()
{
	// will only do something if envelope clock mode is set to internal
	// and the env control is enabled
	if (m_bEnabled && (!m_bClockExternally)) Tick();
}

void CSAAEnv::ExternalClock()
{
	// will only do something if envelope clock mode is set to external
	// and the env control is enabled
	if (m_bClockExternally && m_bEnabled) Tick();
}

void CSAAEnv::SetEnvControl(int nData)
{
	// process immediate stuff first:
	m_nResolution = ((nData & 0x10)==0x10) ? 2 : 1;
	m_bEnabled = ((nData & 0x80)==0x80);
	if (!m_bEnabled)
	{
		// env control was enabled, and now disabled, so reset
		// pointers to start of envelope waveform
		m_nPhase = 0;
		m_nPhasePosition = 0;
		m_bEnvelopeEnded = true;
		m_bOkForNewData = true;
		// store current new data, and set the newdata flag:
		m_bNewData = true;
		m_nNextData = nData;
		// IS THIS REALLY CORRECT THOUGH?? Should disabling
		// it REALLY RESET THESE THINGS?

		SetLevels();

		return;
	}

	// else if (m_bEnabled)

	// now buffered stuff: but only if it's ok to, and only if the
	// envgenerator is not disabled. otherwise it just stays buffered until
	// the Tick() function sets okfornewdata to true and realises there is
	// already some new data waiting
	if (m_bOkForNewData)
	{
		SetNewEnvData(nData); // also does the SetLevels() call for us.
		m_bNewData=false;
		m_bOkForNewData=false; // is this right?
	}
	else
	{
		// since the 'next resolution' changes arrive unbuffered, we
		// may need to change the current level because of this:
		SetLevels();

		// store current new data, and set the newdata flag:
		m_bNewData = true;
		m_nNextData = nData;
	}

}

unsigned short CSAAEnv::LeftLevel() const
{
	return m_nLeftLevel;
}

unsigned short CSAAEnv::RightLevel() const
{
	return m_nRightLevel;
}

inline void CSAAEnv::Tick()
{
	// if disabled, do nothing
	if (!m_bEnabled) // m_bEnabled is set directly, not buffered, so this is ok
	{
		// for sanity, reset stuff:
		m_bEnvelopeEnded = true;
		m_nPhase = 0;
		m_nPhasePosition = 0;
		m_bOkForNewData = true;
		return;
	}

	// else : m_bEnabled

	if (m_bEnvelopeEnded)
	{
		// do nothing
		// (specifically, don't change the values of m_bEnvelopeEnded,
		//  m_nPhase and m_nPhasePosition, as these will still be needed
		//  by SetLevels() should it be called again)

		return;
	}

	// else : !m_bEnvelopeEnded
	// Continue playing the same envelope ...
	// increments the phaseposition within an envelope.
	// also handles looping and resolution appropriately.
	// Changes the level of the envelope accordingly
	// through calling SetLevels() . This must be called after making
	// any changes that will affect the output levels of the env controller!!
	// SetLevels also handles left-right channel inverting

	// increment phase position
	m_nPhasePosition += m_nResolution;

	// if this means we've gone past 16 (the end of a phase)
	// then change phase, and if necessary, loop
	if (m_nPhasePosition >= 16)
	{
		m_nPhase++;
		m_nPhasePosition-=16;

		// if we should loop, then do so - and we've reached position (4)
		// otherwise, if we shouldn't loop,
		// then we've reached position (3) and so we say that
		// we're ok for new data.
		if (m_nPhase == m_nNumberOfPhases)
		{
			// at position (3) or (4)
			m_bOkForNewData = true;

			if (!m_bLooping)
			{
				// position (3) only
				m_bEnvelopeEnded = true;
				// keep pointer at end of envelope for sustain
				m_nPhase = m_nNumberOfPhases-1;
				m_nPhasePosition = 15;
				m_bOkForNewData = true;
			}
			else
			{
				// position (4) only
				m_bEnvelopeEnded = false;
				// set phase pointer to start of envelope for loop
				m_nPhase=0;
			}
		}
		else // (m_nPhase < m_nNumberOfPhases)
		{
			// not at position (3) or (4) ...
			// (i.e., we're in the middle of an envelope with
			//  more than one phase. Specifically, we're in
			//  the middle of envelope 4 or 5 - the
			//  triangle envelopes - but that's not important)

			// any commands sent to this envelope controller
			// will be buffered. Set the flag to indicate this.
			m_bOkForNewData = false;
		}
	}
	else // (m_nPhasePosition < 16)
	{
		// still within the same phase;
		// but, importantly, we are no longer at the start of the phase ...
		// so new data cannot be acted on immediately, and must
		// be buffered
		m_bOkForNewData = false;
		// Phase and PhasePosition have already been updated.
		// SetLevels() will need to be called to actually calculate
		// the output 'level' of this envelope controller
	}

	// if we have new (buffered) data, now is the time to act on it
	if (m_bNewData && m_bOkForNewData)
	{
		m_bNewData = false;
		m_bOkForNewData=false; // is this right?
		// do we need to reset OkForNewData?
		// if we do, then we can't overwrite env data just prior to
		// a new envelope starting - but what's correct? Who knows?
		SetNewEnvData(m_nNextData);
	}
	else
	{
		// ok, we didn't have any new buffered date to act on,
		// so we just call SetLevels() to calculate the output level
		// for whatever the current envelope is
		SetLevels();
	}

}

inline void CSAAEnv::SetLevels()
{
	// sets m_nLeftLevel
	// Also sets m_nRightLevel in terms of m_nLeftLevel
	//								   and m_bInvertRightChannel

	// m_nResolution: 1 means 4-bit resolution; 2 means 3-bit resolution. Resolution of envelope waveform.

	switch (m_nResolution)
	{
	case 1: // 4 bit res waveforms
	default:
		{
			m_nLeftLevel = m_pEnvData->nLevels[0][m_nPhase][m_nPhasePosition];
			if (m_bInvertRightChannel)
				m_nRightLevel = 15-m_nLeftLevel;
			else
				m_nRightLevel = m_nLeftLevel;
			break;
		}
	case 2: // 3 bit res waveforms
		{
			m_nLeftLevel = m_pEnvData->nLevels[1][m_nPhase][m_nPhasePosition];
			if (m_bInvertRightChannel)
				m_nRightLevel = 14-m_nLeftLevel;
			else
				m_nRightLevel = m_nLeftLevel;
			break;
		}
	}
}

inline void CSAAEnv::SetNewEnvData(int nData)
{
	// loads envgenerator's registers according to the bits set
	// in nData

	m_nPhase = 0;
	m_nPhasePosition = 0;
	m_pEnvData = &(cs_EnvData[(nData >> 1) & 0x07]);
	m_bInvertRightChannel = ((nData & 0x01) == 0x01);
	m_bClockExternally = ((nData & 0x20) == 0x20);
	m_nNumberOfPhases = m_pEnvData->nNumberOfPhases;
	m_bLooping = m_pEnvData->bLooping;
	m_nResolution = (((nData & 0x10)==0x10) ? 2 : 1);
	m_bEnabled = ((nData & 0x80) == 0x80);
	if (m_bEnabled)
	{
		m_bEnvelopeEnded = false;
	}
	else
	{
		// DISABLED - so set stuff accordingly
		m_bEnvelopeEnded = true;
		m_nPhase = 0;
		m_nPhasePosition = 0;
		m_bOkForNewData = true;
	}

	SetLevels();

}

bool CSAAEnv::IsActive () const
{
	return m_bEnabled;
}


//////////////////////////////////////////////////////////////////////
// CSAAFreq: frequency generator
//
// Currently only 7-bit fractional accuracy on oscillator periods

// frequency lookup table, built in constructor below
unsigned long CSAAFreq::m_FreqTable[8][256];


CSAAFreq::CSAAFreq(CSAANoise * const NoiseGenerator, CSAAEnv * const EnvGenerator)
:
m_pcConnectedNoiseGenerator(NoiseGenerator),
m_pcConnectedEnvGenerator(EnvGenerator),
m_nConnectedMode((NoiseGenerator == nullptr) ? ((EnvGenerator == nullptr) ? 0 : 1) : 2)
{
	// Build the frequency lookup table for all possible notes, if not already set
	if (!m_FreqTable[0][0])
	{
		for (int oct = 0 ; oct < 8 ; oct++)
		{
			for (int note = 0 ; note < 256 ; note++)
			{
				// Multiply by 8192 to preserve accuracy, and round the divide
				unsigned long long num = 15625ULL << (oct+13);
				int den = 511 - note;
				unsigned long freq8k = static_cast<unsigned long>((num + den/2) / den);

				m_FreqTable[oct][note] = freq8k;
			}
		}
	}

	SetAdd(); // current octave, current offset
}

void CSAAFreq::SetFreqOffset(BYTE nOffset)
{
	// nOffset between 0 and 255

	if (!m_bSync)
	{
		m_nNextOffset = nOffset;
		m_bNewData=true;
		if (m_nNextOctave==m_nCurrentOctave)
		{
			// According to Philips, if you send the SAA-1099
			// new Octave data and then new Offset data in that
			// order, on the next half-cycle of the current frequency
			// generator, ONLY the octave data is acted upon.
			// The offset data will be acted upon next time.
			m_bIgnoreOffsetData=true;
		}
	}
	else
	{
		// updates straightaway if m_bSync
		m_bNewData=false;
		m_nCurrentOffset = nOffset;
		m_nCurrentOctave = m_nNextOctave;
		SetAdd();
	}

}

void CSAAFreq::SetFreqOctave(BYTE nOctave)
{
	// nOctave between 0 and 7

	if (!m_bSync)
	{
		m_nNextOctave = nOctave;
		m_bNewData=true;
		m_bIgnoreOffsetData = false;
	}
	else
	{
		// updates straightaway if m_bSync
		m_bNewData=false;
		m_nCurrentOctave = nOctave;
		m_nCurrentOffset = m_nNextOffset;
		SetAdd();
	}
}

void CSAAFreq::UpdateOctaveOffsetData()
{
	// loads the buffered new octave and new offset data into the current registers
	// and sets up the new frequency for this frequency generator (i.e. sets up m_nAdd)
	// - called during Sync, and called when waveform half-cycle completes

	// How the SAA-1099 really treats new data:
	// if only new octave data is present,
	// then set new period based on just the octave data
	// Otherwise, if only new offset data is present,
	// then set new period based on just the offset data
	// Otherwise, if new octave data is present, and new offset data is present,
	// and the offset data was set BEFORE the octave data,
	// then set new period based on both the octave and offset data
	// Else, if the offset data came AFTER the new octave data
	// then set new period based on JUST THE OCTAVE DATA, and continue
	// signalling the offset data as 'new', so it will be acted upon
	// next half-cycle
	//
	// Weird, I know. But that's how it works. Philips even documented as much.

	m_nCurrentOctave=m_nNextOctave;
	if (!m_bIgnoreOffsetData)
	{
		m_nCurrentOffset=m_nNextOffset;
		m_bNewData=false;
	}
	m_bIgnoreOffsetData=false;

	SetAdd();
}

void CSAAFreq::SetSampleRate(int nSampleRate)
{
	m_nCounter = 0;	// don't bother adjusting existing value
	m_nSampleRateTimes4K = nSampleRate << 12;
}

unsigned short CSAAFreq::Level() const
{
	return m_nLevel;
}


unsigned short CSAAFreq::Tick()
{
	// set to the absolute level (0 or 2)
	if (!m_bSync)
	{
		m_nCounter+=m_nAdd;

		if (m_nCounter >= m_nSampleRateTimes4K)
		{
			// period elapsed for one half-cycle of
			// current frequency
			// reset counter to zero (or thereabouts, taking into account
			// the fractional part in the lower 12 bits)
			while (m_nCounter >= m_nSampleRateTimes4K)
			{
				m_nCounter-=m_nSampleRateTimes4K;
				// flip state - from 0 to -2 or vice versa
				m_nLevel=2-m_nLevel;

				// trigger any connected devices
				switch (m_nConnectedMode)
				{
				case 1:
					// env trigger
					m_pcConnectedEnvGenerator->InternalClock();
					break;

				case 2:
					// noise trigger
					m_pcConnectedNoiseGenerator->Trigger();
					break;

				default:
					// do nothing
					break;
				}
			}

			// get new frequency (set period length m_nAdd) if new data is waiting:
			if (m_bNewData)
				UpdateOctaveOffsetData();
		}
	}
	return m_nLevel;
}


void CSAAFreq::SetAdd()
{
	// nOctave between 0 and 7; nOffset between 0 and 255

	// Used to be:
	// m_nAdd = ((15625 << nOctave) * 8192) / (511 - nOffset));
	// Now just table lookup:
	m_nAdd = m_FreqTable[m_nCurrentOctave][m_nCurrentOffset];
}

void CSAAFreq::Sync(bool bSync)
{
	m_bSync = bSync;

	// update straightaway if m_bSync
	if (m_bSync)
	{
		m_nCounter = 0;
		m_nLevel=2;
		m_nCurrentOctave=m_nNextOctave;
		m_nCurrentOffset=m_nNextOffset;
		SetAdd();
	}
}


//////////////////////////////////////////////////////////////////////
// CSAANoise: noise generator

const unsigned long CSAANoise::cs_nAddBase = 31250 << 12;

/*
CSAANoise::CSAANoise()
:
m_nCounter(0),
m_nAdd(cs_nAddBase),
m_bSync(false),
m_nSampleRateTimes4K(44100<<12),
m_nSourceMode(0),
m_nRand(0x12345678)
{
}
*/
CSAANoise::CSAANoise(unsigned long seed)
:
m_nCounter(0),
m_nAdd(cs_nAddBase),
m_bSync(false),
m_nSampleRateTimes4K(44100<<12),
m_nSourceMode(0),
m_nRand(seed)
{
}

void CSAANoise::Seed(unsigned long seed)
{
	m_nRand = seed;
}

unsigned short CSAANoise::Level() const
{
	return (unsigned short)(m_nRand & 0x00000001);
}

unsigned short CSAANoise::LevelTimesTwo() const
{
	return (unsigned short)((m_nRand & 0x00000001) << 1);
}

void CSAANoise::SetSource(int nSource)
{
	m_nSourceMode = nSource;
	m_nAdd = cs_nAddBase >> m_nSourceMode;
}

void CSAANoise::Trigger()
{
	// Trigger only does anything useful when we're
	// clocking from the frequency generator - i.e
	// if bUseFreqGen = true (i.e. SourceMode = 3)

	// So if we're clocking from the noise generator
	// clock (ie, SourceMode = 0, 1 or 2) then do nothing

//	No point actually checking m_bSync here ... because if sync is true,
//	then frequency generators won't actually be generating Trigger pulses
//	so we wouldn't even get here!
//	if ( (!m_bSync) && m_bUseFreqGen)
	if (m_nSourceMode == 3)
		ChangeLevel();
}

unsigned short CSAANoise::Tick()
{
	// Tick only does anything useful when we're
	// clocking from the noise generator clock
	// (ie, SourceMode = 0, 1 or 2)

	// So, if SourceMode = 3 (ie, we're clocking from a
	// frequency generator ==> bUseFreqGen = true)
	// then do nothing
	if ( (!m_bSync) && (m_nSourceMode!=3) )
	{
		m_nCounter+=m_nAdd;
		if (m_nCounter >= m_nSampleRateTimes4K)
		{
			while (m_nCounter >= m_nSampleRateTimes4K)
			{
				m_nCounter-=m_nSampleRateTimes4K;
				ChangeLevel();
			}
		}
	}

	return (unsigned short)(m_nRand & 0x00000001);
}

void CSAANoise::Sync(bool bSync)
{
	if (bSync)
		m_nCounter = 0;

	m_bSync = bSync;
}

void CSAANoise::SetSampleRate(int nSampleRate)
{
	m_nCounter = 0;	// don't bother adjusting existing value
	m_nSampleRateTimes4K = nSampleRate << 12;
}

inline void CSAANoise::ChangeLevel()
{
	// new routine (thanks to MASS)
	if ( (m_nRand & 0x40000004) && ((m_nRand & 0x40000004) != 0x40000004) )
		m_nRand = (m_nRand<<1)+1;
	else
		m_nRand<<=1;
}


//////////////////////////////////////////////////////////////////////
// CSAASound: the bones of the 'virtual SAA-1099' emulation
//
// the actual sound generation is carried out in the other classes;
// this class provides the output stage and the external interface only

CSAASound::CSAASound(int nSampleRate)
:
m_nCurrentSaaReg(0),
m_bOutputEnabled(false),
m_bSync(false)
{
	Noise[0] = new CSAANoise(0x14af5209); // Create and seed a noise generator
	Noise[1] = new CSAANoise(0x76a9b11e); // Create and seed a noise generator
	Env[0] = new CSAAEnv;
	Env[1] = new CSAAEnv;

	// Create oscillators (tone generators) and link to noise generators and
	// envelope controllers
	Osc[0] = new CSAAFreq(Noise[0], nullptr);
	Osc[1] = new CSAAFreq(nullptr, Env[0]);
	Osc[2] = new CSAAFreq(nullptr, nullptr);
	Osc[3] = new CSAAFreq(Noise[1], nullptr);
	Osc[4] = new CSAAFreq(nullptr, Env[1]);
	Osc[5] = new CSAAFreq(nullptr, nullptr);

	// Create amplification/mixing stages and link to appropriate oscillators,
	// noise generators and envelope controlloers
    Amp[0] = new CSAAAmp(Osc[0], Noise[0], nullptr);
    Amp[1] = new CSAAAmp(Osc[1], Noise[0], nullptr);
    Amp[2] = new CSAAAmp(Osc[2], Noise[0], Env[0]);
    Amp[3] = new CSAAAmp(Osc[3], Noise[1], nullptr);
    Amp[4] = new CSAAAmp(Osc[4], Noise[1], nullptr);
	Amp[5] = new CSAAAmp(Osc[5], Noise[1], Env[1]);

	// Set the output frequency
	Osc[0]->SetSampleRate(nSampleRate);
	Osc[1]->SetSampleRate(nSampleRate);
	Osc[2]->SetSampleRate(nSampleRate);
	Osc[3]->SetSampleRate(nSampleRate);
	Osc[4]->SetSampleRate(nSampleRate);
	Osc[5]->SetSampleRate(nSampleRate);
	Noise[0]->SetSampleRate(nSampleRate);
	Noise[1]->SetSampleRate(nSampleRate);

	// reset the virtual SAA
	Clear();
}

CSAASound::~CSAASound()
{
	if (Noise[0]) delete Noise[0];
	if (Noise[1]) delete Noise[1];
	if (Env[0]) delete Env[0];
	if (Env[1]) delete Env[1];

	for (int i=5; i>=0; i--)
	{
		if (Osc[i]) delete Osc[i];
		if (Amp[i]) delete Amp[i];
	}
}


void CSAASound::Clear()
{
	// reinitialises virtual SAA:
	// sets reg 28 to 0x02; - sync and disabled
	// sets regs 00-31 (except 28) to 0x00;
	// sets reg 28 to 0x00;
	// sets current reg to 0

	WriteAddressData(28,2);

	for (int i=31; i>=0; i--) {
		if (i!=28) WriteAddressData(i,0);
	}
	WriteAddressData(28,0);
	WriteAddress(0);
}


void CSAASound::WriteData(BYTE nData)
{
	// originated from an OUT 255,d call

	// route nData to the appropriate place
	switch (m_nCurrentSaaReg)
	{
	case 0:
		// Amplitude data (==> Amp)
		Amp[0]->SetAmpLevel(nData);
		break;
	case 1:
		Amp[1]->SetAmpLevel(nData);
		break;
	case 2:
		Amp[2]->SetAmpLevel(nData);
		break;
	case 3:
		Amp[3]->SetAmpLevel(nData);
		break;
	case 4:
		Amp[4]->SetAmpLevel(nData);
		break;
	case 5:
		Amp[5]->SetAmpLevel(nData);
		break;

	case 8:
		// Freq data (==> Osc)
		Osc[0]->SetFreqOffset(nData);
		break;
	case 9:
		Osc[1]->SetFreqOffset(nData);
		break;
	case 10:
		Osc[2]->SetFreqOffset(nData);
		break;
	case 11:
		Osc[3]->SetFreqOffset(nData);
		break;
	case 12:
		Osc[4]->SetFreqOffset(nData);
		break;
	case 13:
		Osc[5]->SetFreqOffset(nData);
		break;

	case 16:
		// Freq octave data (==> Osc) for channels 0,1
		Osc[0]->SetFreqOctave(nData & 0x07);
		Osc[1]->SetFreqOctave((nData >> 4) & 0x07);
		break;

	case 17:
		// Freq octave data (==> Osc) for channels 2,3
		Osc[2]->SetFreqOctave(nData & 0x07);
		Osc[3]->SetFreqOctave((nData >> 4) & 0x07);
		break;

	case 18:
		// Freq octave data (==> Osc) for channels 4,5
		Osc[4]->SetFreqOctave(nData & 0x07);
		Osc[5]->SetFreqOctave((nData >> 4) & 0x07);
		break;

	case 20:
		// Tone mixer control (==> Amp)
		Amp[0]->SetToneMixer(nData & 0x01);
		Amp[1]->SetToneMixer(nData & 0x02);
		Amp[2]->SetToneMixer(nData & 0x04);
		Amp[3]->SetToneMixer(nData & 0x08);
		Amp[4]->SetToneMixer(nData & 0x10);
		Amp[5]->SetToneMixer(nData & 0x20);
		break;

	case 21:
		// Noise mixer control (==> Amp)
		Amp[0]->SetNoiseMixer(nData & 0x01);
		Amp[1]->SetNoiseMixer(nData & 0x02);
		Amp[2]->SetNoiseMixer(nData & 0x04);
		Amp[3]->SetNoiseMixer(nData & 0x08);
		Amp[4]->SetNoiseMixer(nData & 0x10);
		Amp[5]->SetNoiseMixer(nData & 0x20);
		break;

	case 22:
		// Noise frequency/source control (==> Noise)
		Noise[0]->SetSource(nData & 0x03);
		Noise[1]->SetSource((nData >> 4) & 0x03);
		break;

	case 24:
		// Envelope control data (==> Env) for envelope controller #0
		Env[0]->SetEnvControl(nData);
		break;

	case 25:
		// Envelope control data (==> Env) for envelope controller #1
		Env[1]->SetEnvControl(nData);
		break;

	case 28:
		if (nData & 0x02)
		{
			// Sync all devices
			// This amounts to telling them all to reset to a known state
			Osc[0]->Sync(true);
			Osc[1]->Sync(true);
			Osc[2]->Sync(true);
			Osc[3]->Sync(true);
			Osc[4]->Sync(true);
			Osc[5]->Sync(true);
			Noise[0]->Sync(true);
			Noise[1]->Sync(true);
			m_bSync = true;
		}
		else 
		{
			// Unsync all devices
			Osc[0]->Sync(false);
			Osc[1]->Sync(false);
			Osc[2]->Sync(false);
			Osc[3]->Sync(false);
			Osc[4]->Sync(false);
			Osc[5]->Sync(false);
			Noise[0]->Sync(false);
			Noise[1]->Sync(false);
			m_bSync = false;
		}

		if (nData & 0x01)
		{
			// unmute all amps - sound 'enabled'
			Amp[0]->Mute(false);
			Amp[1]->Mute(false);
			Amp[2]->Mute(false);
			Amp[3]->Mute(false);
			Amp[4]->Mute(false);
			Amp[5]->Mute(false);
			m_bOutputEnabled = true;
		}
		else
		{
			// mute all amps
			Amp[0]->Mute(true);
			Amp[1]->Mute(true);
			Amp[2]->Mute(true);
			Amp[3]->Mute(true);
			Amp[4]->Mute(true);
			Amp[5]->Mute(true);
			m_bOutputEnabled = false;
		}

		break;

	default:
		// anything else means data is being written to a register that is
		// not used within the SAA-1099 architecture, hence we ignore it.
		;
	}
}

void CSAASound::WriteAddress(BYTE nReg)
{
	// originated from an OUT 511,r call
	m_nCurrentSaaReg = nReg & 31;

	if (m_nCurrentSaaReg==24)
		Env[0]->ExternalClock();
	else if (m_nCurrentSaaReg==25)
		Env[1]->ExternalClock();
}

void CSAASound::WriteAddressData(BYTE nReg, BYTE nData)
{
	// performs WriteAddress(nReg) followed by WriteData(nData)
	WriteAddress(nReg);
	WriteData(nData);
}

BYTE CSAASound::ReadAddress()
{
	// can't remember if this is actually supported by the real
	// SAA-1099 hardware - but hey, sometimes it's useful, right?
	return m_nCurrentSaaReg;
}

void CSAASound::GenerateMany(BYTE * pBuffer, int nSamples)
{
	CSAAAmp::stereolevel stereoval;

	while (nSamples-- > 0)
	{
		Noise[0]->Tick();
		Noise[1]->Tick();

		stereoval.dword=(Amp[0]->TickAndOutputStereo()).dword;
		stereoval.dword+=(Amp[1]->TickAndOutputStereo()).dword;
		stereoval.dword+=(Amp[2]->TickAndOutputStereo()).dword;
		stereoval.dword+=(Amp[3]->TickAndOutputStereo()).dword;
		stereoval.dword+=(Amp[4]->TickAndOutputStereo()).dword;
		stereoval.dword+=(Amp[5]->TickAndOutputStereo()).dword;

		// force output into the range 0<=x<=65535
		// (strictly, the following gives us 0<=x<=63360)
		stereoval.sep.Left *= 10;
		stereoval.sep.Right *= 10;
		*pBuffer++ = stereoval.sep.Left & 0x00ff;
		*pBuffer++ = stereoval.sep.Left >> 8;
		*pBuffer++ = stereoval.sep.Right & 0x00ff;
		*pBuffer++ = stereoval.sep.Right >> 8;
	}
}
