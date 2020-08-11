// Part of SAASound copyright 1998-2018 Dave Hooper <dave@beermex.com>
//
// SAAImpl.cpp: implementation of the CSAASound class.
// the bones of the 'virtual SAA-1099' emulation
//
// the actual sound generation is carried out in the other classes;
// this class provides the output stage and the external interface only
//
//////////////////////////////////////////////////////////////////////

#ifdef WIN32
#include <assert.h>
#else
#define assert(exp)	((void)0)
#endif

#include "SAASound.h"

#include "types.h"
#include "SAAEnv.h"
#include "SAANoise.h"
#include "SAAFreq.h"
#include "SAAAmp.h"
#include "SAASound.h"
#include "SAAImpl.h"

//////////////////////////////////////////////////////////////////////
// Globals
//////////////////////////////////////////////////////////////////////

#ifdef DEBUGSAA
#include <stdio.h>	// for sprintf
FILE * dbgfile = NULL;
FILE * pcmfile = NULL;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSAASoundInternal::CSAASoundInternal()
:
m_nCurrentSaaReg(0),
m_bOutputEnabled(false),
m_bSync(false),
m_uParam(0),
m_uParamRate(0)
{
	prev_output_mono = 0;
	prev_output_stereo.sep.Left = prev_output_stereo.sep.Right = 0;

	#ifdef DEBUGSAA
	dbgfile = fopen("debugsaa.txt","wt");
	pcmfile = fopen("debugsaa.pcm","wb");
	#endif

	// Create and link up the objects that make up the emulator
	Noise[0] = new CSAANoise(0xffffffff); // Create and seed a noise generator
	Noise[1] = new CSAANoise(0xffffffff); // Create and seed a noise generator
	Env[0] = new CSAAEnv;
	Env[1] = new CSAAEnv;

	// (check that memory allocated succeeded this far)
	assert (Noise[0] != NULL);
	assert (Noise[1] != NULL);
	assert (Env[0] != NULL);
	assert (Env[1] != NULL);

	// Create oscillators (tone generators) and link to noise generators and
	// envelope controllers
	Osc[0] = new CSAAFreq(Noise[0], NULL);
	Osc[1] = new CSAAFreq(NULL, Env[0]);
	Osc[2] = new CSAAFreq(NULL, NULL);
	Osc[3] = new CSAAFreq(Noise[1], NULL);
	Osc[4] = new CSAAFreq(NULL, Env[1]);
	Osc[5] = new CSAAFreq(NULL, NULL);
	for (int i=5; i>=0; i--)
	{
		assert (Osc[i] != NULL);
	}

	// Create amplification/mixing stages and link to appropriate oscillators,
	// noise generators and envelope controllers
	Amp[0] = new CSAAAmp(Osc[0], Noise[0], NULL),
	Amp[1] = new CSAAAmp(Osc[1], Noise[0], NULL),
	Amp[2] = new CSAAAmp(Osc[2], Noise[0], Env[0]),
	Amp[3] = new CSAAAmp(Osc[3], Noise[1], NULL),
	Amp[4] = new CSAAAmp(Osc[4], Noise[1], NULL),
	Amp[5] = new CSAAAmp(Osc[5], Noise[1], Env[1]);
	for (int j=5; j>=0; j--)
	{
		assert (Amp[j] != NULL);
	}

	// set parameters
	SetSoundParameters(SAAP_FILTER | SAAP_11025 | SAAP_8BIT | SAAP_MONO);
	// reset the virtual SAA
	Clear();

	SetClockRate(8000000);
}

CSAASoundInternal::~CSAASoundInternal()
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
	
#ifdef DEBUGSAA
	if (dbgfile) fclose(dbgfile);
#endif
}

//////////////////////////////////////////////////////////////////////
// CSAASound members
//////////////////////////////////////////////////////////////////////

void CSAASoundInternal::SetClockRate(unsigned int nClockRate)
{
	for (int i = 0; i < 6; i++)
	{
		Osc[i]->SetClockRate(nClockRate);
	}
	Noise[0]->SetClockRate(nClockRate);
	Noise[1]->SetClockRate(nClockRate);
}

void CSAASoundInternal::Clear(void)
{
	// reinitialises virtual SAA:
	// sets reg 28 to 0x02; - sync and disabled
	// sets regs 00-31 (except 28) to 0x00;
	// sets reg 28 to 0x00;
	// sets current reg to 0
	WriteAddressData(28,2);
	for (int i=31; i>=0; i--)
	{
		if (i!=28) WriteAddressData(i,0);
	}
	WriteAddressData(28,0);
	WriteAddress(0);
}

void CSAASoundInternal::WriteData(BYTE nData)
{
	// originated from an OUT 255,d call

#ifdef DEBUGSAA
	fprintf(dbgfile, "%lu %02d:%02x\n", m_nDebugSample, m_nCurrentSaaReg, nData);
#endif

	// route nData to the appropriate place
	switch (m_nCurrentSaaReg)
	{
		// Amplitude data (==> Amp)
		case 0:
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

		// Freq data (==> Osc)
		case 8:
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

		// Freq octave data (==> Osc) for channels 0,1
		case 16:
			Osc[0]->SetFreqOctave(nData & 0x07);
			Osc[1]->SetFreqOctave((nData >> 4) & 0x07);
			break;

		// Freq octave data (==> Osc) for channels 2,3
		case 17:
			Osc[2]->SetFreqOctave(nData & 0x07);
			Osc[3]->SetFreqOctave((nData >> 4) & 0x07);
			break;

		// Freq octave data (==> Osc) for channels 4,5
		case 18:
			Osc[4]->SetFreqOctave(nData & 0x07);
			Osc[5]->SetFreqOctave((nData >> 4) & 0x07);
			break;

		// Tone mixer control (==> Amp)
		case 20:
			Amp[0]->SetToneMixer(nData & 0x01);
			Amp[1]->SetToneMixer(nData & 0x02);
			Amp[2]->SetToneMixer(nData & 0x04);
			Amp[3]->SetToneMixer(nData & 0x08);
			Amp[4]->SetToneMixer(nData & 0x10);
			Amp[5]->SetToneMixer(nData & 0x20);
			break;

		// Noise mixer control (==> Amp)
		case 21:
			Amp[0]->SetNoiseMixer(nData & 0x01);
			Amp[1]->SetNoiseMixer(nData & 0x02);
			Amp[2]->SetNoiseMixer(nData & 0x04);
			Amp[3]->SetNoiseMixer(nData & 0x08);
			Amp[4]->SetNoiseMixer(nData & 0x10);
			Amp[5]->SetNoiseMixer(nData & 0x20);
			break;

		// Noise frequency/source control (==> Noise)
		case 22:
			Noise[0]->SetSource(nData & 0x03);
			Noise[1]->SetSource((nData >> 4) & 0x03);
			break;

		// Envelope control data (==> Env) for envelope controller #0
		case 24:
			Env[0]->SetEnvControl(nData);
			break;

		// Envelope control data (==> Env) for envelope controller #1
		case 25:
			Env[1]->SetEnvControl(nData);
			break;

		// Global enable and reset (sync) controls
		case 28:
			// Reset (sync) bit
			if (nData & 0x02)
			{
				// Sync all devices
				// This amounts to telling them all to reset to a
				// known state.
				Osc[0]->Sync(true);
				Osc[1]->Sync(true);
				Osc[2]->Sync(true);
				Osc[3]->Sync(true);
				Osc[4]->Sync(true);
				Osc[5]->Sync(true);
				Noise[0]->Sync(true);
				Noise[1]->Sync(true);
				Amp[0]->Sync(true);
				Amp[1]->Sync(true);
				Amp[2]->Sync(true);
				Amp[3]->Sync(true);
				Amp[4]->Sync(true);
				Amp[5]->Sync(true);
				m_bSync = true;
			}
			else
			{
				// Unsync all devices i.e. run oscillators
				Osc[0]->Sync(false);
				Osc[1]->Sync(false);
				Osc[2]->Sync(false);
				Osc[3]->Sync(false);
				Osc[4]->Sync(false);
				Osc[5]->Sync(false);
				Noise[0]->Sync(false);
				Noise[1]->Sync(false);
				Amp[0]->Sync(false);
				Amp[1]->Sync(false);
				Amp[2]->Sync(false);
				Amp[3]->Sync(false);
				Amp[4]->Sync(false);
				Amp[5]->Sync(false);
				m_bSync = false;
			}

			// Global mute bit
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
			// anything else means data is being written to a register
			// that is not used within the SAA-1099 architecture
			// hence, we ignore it.
			{}
	}
}

void CSAASoundInternal::WriteAddress(BYTE nReg)
{
	// originated from an OUT 511,r call
#ifdef DEBUGSAA
	fprintf(dbgfile, "%lu %02d:", m_nDebugSample, nReg);
#endif
	m_nCurrentSaaReg = nReg & 31;
	if (m_nCurrentSaaReg==24)
	{
		Env[0]->ExternalClock();
#ifdef DEBUGSAA
		fprintf(dbgfile, "<!ENVO!>");
#endif
	}
	else if (m_nCurrentSaaReg==25)
	{
		Env[1]->ExternalClock();
#ifdef DEBUGSAA
		fprintf(dbgfile, "<!ENV1!>");
#endif
	}
#ifdef DEBUGSAA
	fprintf(dbgfile,"\n");
#endif
}

void CSAASoundInternal::WriteAddressData(BYTE nReg, BYTE nData)
{
	// performs WriteAddress(nReg) followed by WriteData(nData)
	WriteAddress(nReg);
	WriteData(nData);
}

BYTE CSAASoundInternal::ReadAddress(void)
{
	// Not a real hardware function of the SAA-1099, which is write-only
	return(m_nCurrentSaaReg);
}

void CSAASoundInternal::SetSoundParameters(SAAPARAM uParam)
{
	int sampleratemode = 0;

	switch (uParam & SAAP_MASK_FILTER)
	{
		case SAAP_NOFILTER: // disable filter
			m_uParam = (m_uParam & ~SAAP_MASK_FILTER) | SAAP_NOFILTER;
			break;
		case SAAP_FILTER: // enable filter
			m_uParam = (m_uParam & ~SAAP_MASK_FILTER) | SAAP_FILTER;
			break;
		case 0:// change nothing!
		default:
			break;
	}

	// always enable FILTER mode
	m_uParam = (m_uParam & ~SAAP_MASK_FILTER) | SAAP_FILTER;

	switch (uParam & SAAP_MASK_SAMPLERATE)
	{
		case SAAP_44100:
			sampleratemode = 0;
			m_uParamRate = (m_uParamRate & ~SAAP_MASK_SAMPLERATE) | SAAP_44100;
			break;
		case SAAP_22050:
			sampleratemode = 1;
			m_uParamRate = (m_uParamRate & ~SAAP_MASK_SAMPLERATE) | SAAP_22050;
			break;
		case SAAP_11025:
			sampleratemode = 2;
			m_uParamRate = (m_uParamRate & ~SAAP_MASK_SAMPLERATE) | SAAP_11025;
			break;
		case 0:// change nothing!
		default:
			break;
	}

	/* Enabling the filter automatically puts the oscillators and
	 * noise generators into an ultra-high-resolution mode of 88.2kHz */
	if ( (m_uParam & SAAP_MASK_FILTER) == SAAP_FILTER)
	{
		sampleratemode -= 1;
	}

	Osc[0]->SetSampleRateMode(sampleratemode);
	Osc[1]->SetSampleRateMode(sampleratemode);
	Osc[2]->SetSampleRateMode(sampleratemode);
	Osc[3]->SetSampleRateMode(sampleratemode);
	Osc[4]->SetSampleRateMode(sampleratemode);
	Osc[5]->SetSampleRateMode(sampleratemode);
	Noise[0]->SetSampleRateMode(sampleratemode);
	Noise[1]->SetSampleRateMode(sampleratemode);
		
	switch (uParam & SAAP_MASK_BITDEPTH)
	{
		case SAAP_8BIT: // set 8bit mode
			m_uParam = (m_uParam & ~SAAP_MASK_BITDEPTH) | SAAP_8BIT;
			break;
		case SAAP_16BIT: // set 16bit mode
			m_uParam = (m_uParam & ~SAAP_MASK_BITDEPTH) | SAAP_16BIT;
			break;
		case 0:// change nothing!
		default:
			break;
	}

	switch (uParam & SAAP_MASK_CHANNELS)
	{
		case SAAP_MONO: // set mono
			m_uParam = (m_uParam & ~SAAP_MASK_CHANNELS) | SAAP_MONO;
			break;
		case SAAP_STEREO: // set stereo
			m_uParam = (m_uParam & ~SAAP_MASK_CHANNELS) | SAAP_STEREO;
			break;
		case 0:// change nothing!
		default:
			break;
	}
}

SAAPARAM CSAASoundInternal::GetCurrentSoundParameters(void)
{
	return m_uParam | m_uParamRate;
}

unsigned short CSAASoundInternal::GetCurrentBytesPerSample(void)
{
	return CSAASound::GetBytesPerSample(m_uParam);
}

/*static*/ unsigned short CSAASound::GetBytesPerSample(SAAPARAM uParam)
{
	switch (uParam & (SAAP_MASK_CHANNELS | SAAP_MASK_BITDEPTH))
	{
		case SAAP_MONO | SAAP_8BIT:
			return 1;
		case SAAP_MONO | SAAP_16BIT:
		case SAAP_STEREO | SAAP_8BIT:
			return 2;
		case SAAP_STEREO | SAAP_16BIT:
			return 4;
		default:
			return 0;
	}
}

unsigned long CSAASoundInternal::GetCurrentSampleRate(void)
{
	return CSAASound::GetSampleRate(m_uParamRate);
}

/*static*/ unsigned long CSAASound::GetSampleRate(SAAPARAM uParam) // static member function
{
	switch (uParam & SAAP_MASK_SAMPLERATE)
	{
		case SAAP_11025:
			return 11025;
		case SAAP_22050:
			return 22050;
		case SAAP_44100:
			return 44100;
		default:
			return 0;
	}
}

void CSAASoundInternal::GenerateMany(BYTE * pBuffer, unsigned long nSamples)
{
	unsigned short mono1,mono2,mono;
	stereolevel stereoval, stereoval1, stereoval2;

	unsigned short prev_mono = prev_output_mono;
	stereolevel prev_stereo;
	prev_stereo.dword = prev_output_stereo.dword;

#ifdef DEBUGSAA
	BYTE * pBufferStart = pBuffer;
	unsigned long nTotalSamples = nSamples;
#endif

#define IGNORE_LOWPASS

	switch (m_uParam)
	{
		// NO FILTER 
		case SAAP_NOFILTER | SAAP_MONO | SAAP_8BIT:
			while (nSamples--)
			{
				Noise[0]->Tick();
				Noise[1]->Tick();

				mono = (Amp[0]->TickAndOutputMono() +
					Amp[1]->TickAndOutputMono() +
					Amp[2]->TickAndOutputMono() +
					Amp[3]->TickAndOutputMono() +
					Amp[4]->TickAndOutputMono() +
					Amp[5]->TickAndOutputMono());
				mono = mono * 5;
				mono = 0x80 + (mono >> 8);

				*pBuffer++ = (unsigned char)mono;
			}
			break;

		case SAAP_NOFILTER | SAAP_MONO | SAAP_16BIT:
			while (nSamples--)
			{
				Noise[0]->Tick();
				Noise[1]->Tick();

				mono = (Amp[0]->TickAndOutputMono() +
					Amp[1]->TickAndOutputMono() +
					Amp[2]->TickAndOutputMono() +
					Amp[3]->TickAndOutputMono() +
					Amp[4]->TickAndOutputMono() +
					Amp[5]->TickAndOutputMono());

				// force output into the range 0<=x<=65535
				// (strictly, the following gives us 0<=x<=63360)
				mono *= 5;

				*pBuffer++ = mono & 0x00ff;
				*pBuffer++ = mono >> 8;
			}
			break;

		case SAAP_NOFILTER | SAAP_STEREO | SAAP_8BIT:
			while (nSamples--)
			{
				Noise[0]->Tick();
				Noise[1]->Tick();

				stereoval.dword = (Amp[0]->TickAndOutputStereo()).dword;
				stereoval.dword += (Amp[1]->TickAndOutputStereo()).dword;
				stereoval.dword += (Amp[2]->TickAndOutputStereo()).dword;
				stereoval.dword += (Amp[3]->TickAndOutputStereo()).dword;
				stereoval.dword += (Amp[4]->TickAndOutputStereo()).dword;
				stereoval.dword += (Amp[5]->TickAndOutputStereo()).dword;

				// force output into the range 0<=x<=255
				stereoval.sep.Left = stereoval.sep.Left * 10;
				stereoval.sep.Right = stereoval.sep.Right * 10;

				*pBuffer++ = 0x80 + ((stereoval.sep.Left) >> 8);
				*pBuffer++ = 0x80 + ((stereoval.sep.Right) >> 8);
			}
			break;

		case SAAP_NOFILTER | SAAP_STEREO | SAAP_16BIT:
			while (nSamples--)
			{
				Noise[0]->Tick();
				Noise[1]->Tick();

				stereoval.dword = (Amp[0]->TickAndOutputStereo()).dword;
				stereoval.dword += (Amp[1]->TickAndOutputStereo()).dword;
				stereoval.dword += (Amp[2]->TickAndOutputStereo()).dword;
				stereoval.dword += (Amp[3]->TickAndOutputStereo()).dword;
				stereoval.dword += (Amp[4]->TickAndOutputStereo()).dword;
				stereoval.dword += (Amp[5]->TickAndOutputStereo()).dword;

				// force output into the range 0<=x<=65535
				// (strictly, the following gives us 0<=x<=63360)
				stereoval.sep.Left = stereoval.sep.Left * 10;
				stereoval.sep.Right = stereoval.sep.Right * 10;

				*pBuffer++ = stereoval.sep.Left & 0x00ff;
				*pBuffer++ = stereoval.sep.Left >> 8;
				*pBuffer++ = stereoval.sep.Right & 0x00ff;
				*pBuffer++ = stereoval.sep.Right >> 8;
			}
			break;

		// FILTER : (high-quality mode + bandpass filter)
		case SAAP_FILTER | SAAP_MONO | SAAP_8BIT:
			while (nSamples--)
			{
				// tick noise generators twice due to oversampling
				// TODO encode the oversampling ratio explicitly somewhere?
				Noise[0]->Tick();
				Noise[1]->Tick();
				mono1 = (Amp[0]->TickAndOutputMono() +
					Amp[1]->TickAndOutputMono() +
					Amp[2]->TickAndOutputMono() +
					Amp[3]->TickAndOutputMono() +
					Amp[4]->TickAndOutputMono() +
					Amp[5]->TickAndOutputMono());

				Noise[0]->Tick();
				Noise[1]->Tick();
				mono2 = (Amp[0]->TickAndOutputMono() +
					Amp[1]->TickAndOutputMono() +
					Amp[2]->TickAndOutputMono() +
					Amp[3]->TickAndOutputMono() +
					Amp[4]->TickAndOutputMono() +
					Amp[5]->TickAndOutputMono());

				// force output into the range 0<=x<=255
				mono = ((mono1 + mono2)*5) >> 1;
				mono = 0x80 + (mono >> 8);
				
#ifndef IGNORE_LOWPASS
				// lowpass filter
				mono = (prev_mono + mono) >> 1;
				prev_mono = mono;
#endif
				*pBuffer++ = (unsigned char)mono;
			}
			break;
	
		case SAAP_FILTER | SAAP_MONO | SAAP_16BIT:
			while (nSamples--)
			{
				// tick noise generators twice due to oversampling
				// TODO encode the oversampling ratio explicitly somewhere?
				Noise[0]->Tick();
				Noise[1]->Tick();
				mono1 = (Amp[0]->TickAndOutputMono() +
					Amp[1]->TickAndOutputMono() +
					Amp[2]->TickAndOutputMono() +
					Amp[3]->TickAndOutputMono() +
					Amp[4]->TickAndOutputMono() +
					Amp[5]->TickAndOutputMono());

				Noise[0]->Tick();
				Noise[1]->Tick();
				mono2 = (Amp[0]->TickAndOutputMono() +
					Amp[1]->TickAndOutputMono() +
					Amp[2]->TickAndOutputMono() +
					Amp[3]->TickAndOutputMono() +
					Amp[4]->TickAndOutputMono() +
					Amp[5]->TickAndOutputMono());

				// force output into the range 0<=x<=65535
				// (strictly, the following gives us 0<=x<=63360)
				mono1 *= 5;
				mono2 *= 5;
				mono = (mono1 + mono2) >> 1;

#ifndef IGNORE_LOWPASS
				// lowpass filter
				mono = (prev_mono + mono) >> 1;
				prev_mono = mono;
#endif

				*pBuffer++ = (unsigned char)(mono & 0x00ff);
				*pBuffer++ = (unsigned char)(mono >> 8);
			}
			break;
	
		case SAAP_FILTER | SAAP_STEREO | SAAP_8BIT:
			while (nSamples--)
			{
				// tick noise generators twice due to oversampling
				// TODO encode the oversampling ratio explicitly somewhere?
				Noise[0]->Tick();
				Noise[1]->Tick();
				stereoval1.dword=(Amp[0]->TickAndOutputStereo()).dword;
				stereoval1.dword+=(Amp[1]->TickAndOutputStereo()).dword;
				stereoval1.dword+=(Amp[2]->TickAndOutputStereo()).dword;
				stereoval1.dword+=(Amp[3]->TickAndOutputStereo()).dword;
				stereoval1.dword+=(Amp[4]->TickAndOutputStereo()).dword;
				stereoval1.dword+=(Amp[5]->TickAndOutputStereo()).dword;

				Noise[0]->Tick();
				Noise[1]->Tick();
				stereoval2.dword = (Amp[0]->TickAndOutputStereo()).dword;
				stereoval2.dword += (Amp[1]->TickAndOutputStereo()).dword;
				stereoval2.dword += (Amp[2]->TickAndOutputStereo()).dword;
				stereoval2.dword += (Amp[3]->TickAndOutputStereo()).dword;
				stereoval2.dword += (Amp[4]->TickAndOutputStereo()).dword;
				stereoval2.dword += (Amp[5]->TickAndOutputStereo()).dword;

				// force output into the range 0<=x<=255
				stereoval.sep.Left = ((stereoval1.sep.Left + stereoval2.sep.Left) * 10) >> 1;
				stereoval.sep.Right = ((stereoval1.sep.Right + stereoval2.sep.Right) * 10) >> 1;

#ifndef IGNORE_LOWPASS
				// lowpass filter
				stereoval.sep.Left = (stereoval.sep.Left + prev_stereo.sep.Left) >> 1;
				stereoval.sep.Right = (stereoval.sep.Right + prev_stereo.sep.Right) >> 1;
				prev_stereo.dword = stereoval.dword;
#endif
				*pBuffer++ = 0x80+((stereoval.sep.Left)>>8);
				*pBuffer++ = 0x80+((stereoval.sep.Right)>>8);
			}
			break;
	
		case SAAP_FILTER | SAAP_STEREO | SAAP_16BIT:
			while (nSamples--)
			{
				// tick noise generators twice due to oversampling
				// TODO encode the oversampling ratio explicitly somewhere?
				Noise[0]->Tick();
				Noise[1]->Tick();
				stereoval1.dword = (Amp[0]->TickAndOutputStereo()).dword;
				stereoval1.dword += (Amp[1]->TickAndOutputStereo()).dword;
				stereoval1.dword += (Amp[2]->TickAndOutputStereo()).dword;
				stereoval1.dword += (Amp[3]->TickAndOutputStereo()).dword;
				stereoval1.dword += (Amp[4]->TickAndOutputStereo()).dword;
				stereoval1.dword += (Amp[5]->TickAndOutputStereo()).dword;

				Noise[0]->Tick();
				Noise[1]->Tick();
				stereoval2.dword = (Amp[0]->TickAndOutputStereo()).dword;
				stereoval2.dword += (Amp[1]->TickAndOutputStereo()).dword;
				stereoval2.dword += (Amp[2]->TickAndOutputStereo()).dword;
				stereoval2.dword += (Amp[3]->TickAndOutputStereo()).dword;
				stereoval2.dword += (Amp[4]->TickAndOutputStereo()).dword;
				stereoval2.dword += (Amp[5]->TickAndOutputStereo()).dword;

				// force output into the range 0<=x<=65535
				// (strictly, the following gives us 0<=x<=63360)
				stereoval.sep.Left = ((stereoval1.sep.Left + stereoval2.sep.Left) * 10) >> 1;
				stereoval.sep.Right = ((stereoval1.sep.Right + stereoval2.sep.Right) * 10) >> 1;

#ifndef IGNORE_LOWPASS
				// lowpass filter
				stereoval.sep.Left = (stereoval.sep.Left + prev_stereo.sep.Left) >> 1;
				stereoval.sep.Right = (stereoval.sep.Right + prev_stereo.sep.Right) >> 1;
				prev_stereo.dword = stereoval.dword;
#endif
				*pBuffer++ = stereoval.sep.Left & 0x00ff;
				*pBuffer++ = stereoval.sep.Left >> 8;
				*pBuffer++ = stereoval.sep.Right & 0x00ff;
				*pBuffer++ = stereoval.sep.Right >> 8;
			}
			break;

		default: // ie - the m_uParam contains modes not implemented yet
			{
#ifdef DEBUGSAA
				char error[256];
				sprintf(error,"not implemented: uParam=%#L.8x\n",m_uParam);
	#ifdef WIN32
				OutputDebugStringA(error);
	#else
				fprintf(stderr, error);
	#endif
#endif
			}
	}

#ifdef DEBUGSAA
	fwrite(pBufferStart, GetCurrentBytesPerSample(), nTotalSamples, pcmfile);
	m_nDebugSample += nTotalSamples;
#endif

	prev_output_mono = prev_mono;
	prev_output_stereo.dword = prev_stereo.dword;


}

int CSAASoundInternal::SendCommand(SAACMD nCommandID, long nData)
{
	/********************/
	/* to be completed! */
	/********************/
	switch (nCommandID)
	{
		case SAACMD_SetSampleRate: return SAASENDCOMMAND_FEATURE_NOT_YET_IMPLEMENTED;
		case SAACMD_GetSampleRate: return SAASENDCOMMAND_FEATURE_NOT_YET_IMPLEMENTED;
		case SAACMD_SetVolumeBoost: return SAASENDCOMMAND_FEATURE_NOT_YET_IMPLEMENTED;
		case SAACMD_GetVolumeBoost: return SAASENDCOMMAND_FEATURE_NOT_YET_IMPLEMENTED;
		case SAACMD_SetFilterMode: return SAASENDCOMMAND_FEATURE_NOT_YET_IMPLEMENTED;
		case SAACMD_GetFilterMode: return SAASENDCOMMAND_FEATURE_NOT_YET_IMPLEMENTED;
		case SAACMD_SetBitDepth: return SAASENDCOMMAND_FEATURE_NOT_YET_IMPLEMENTED;
		case SAACMD_GetBitDepth: return SAASENDCOMMAND_FEATURE_NOT_YET_IMPLEMENTED;
		case SAACMD_SetNumChannels: return SAASENDCOMMAND_FEATURE_NOT_YET_IMPLEMENTED;
		case SAACMD_GetNumChannels: return SAASENDCOMMAND_FEATURE_NOT_YET_IMPLEMENTED;
	
		default: return SAASENDCOMMAND_UNKNOWN_INVALID_COMMAND;
	}
}

///////////////////////////////////////////////////////

LPCSAASOUND SAAAPI CreateCSAASound(void)
{
	return (new CSAASoundInternal);
}

void SAAAPI DestroyCSAASound(LPCSAASOUND object)
{
	delete (object);
}

///////////////////////////////////////////////////////
