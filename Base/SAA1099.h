// Part of SimCoupe - A SAM Coupe emulator
//
// SAA1099.cpp: Philips SAA 1099 sound chip emulation
//
// Copyright (c) 1998-2004, Dave Hooper <dave@rebuzz.org>  
// All rights reserved.

#ifndef SAA1099_H
#define SAA1099_H

class CSAAEnv
{
	typedef struct
	{
		int nNumberOfPhases;
		bool bLooping;
		unsigned short nLevels[2][2][16]; // [Resolution][Phase][Withinphase]
	} ENVDATA;

protected:
	unsigned short m_nLeftLevel, m_nRightLevel;
	ENVDATA const * m_pEnvData;

	bool m_bEnabled;
	bool m_bInvertRightChannel;
	BYTE m_nPhase;
	BYTE m_nPhasePosition;
	bool m_bEnvelopeEnded;
	char m_nPhaseAdd[2];
	char m_nCurrentPhaseAdd;
	bool m_bLooping;
	char m_nNumberOfPhases;
	char m_nResolution;
	char m_nInitialLevel;
	bool m_bNewData;
	BYTE m_nNextData;
	bool m_bOkForNewData;
	bool m_bClockExternally;
	static const ENVDATA cs_EnvData[8];

	void Tick();
	void SetLevels();
	void SetNewEnvData(int nData);

public:
	CSAAEnv();
	~CSAAEnv();

	void InternalClock();
	void ExternalClock();
	void SetEnvControl(int nData); // really just a BYTE
	unsigned short LeftLevel() const;
	unsigned short RightLevel() const;
	bool IsActive() const;

};


class CSAANoise
{
protected:
	unsigned long m_nCounter;
	unsigned long m_nAdd;
	bool m_bSync; // see description of "SYNC" bit of register 28
//	BYTE m_nSampleRateMode; // 0=44100, 1=22050; 2=11025
	unsigned long m_nSampleRateTimes4K; // = (44100*4096) when RateMode=0, for example
	int m_nSourceMode;
	static const unsigned long cs_nAddBase; // nAdd for 31.25 kHz noise at 44.1 kHz samplerate

	// pseudo-random number generator
	unsigned long m_nRand;

	void ChangeLevel();

public:
//	CSAANoise();
	CSAANoise(unsigned long seed);
	~CSAANoise() { }

	void SetSource(int nSource);
	void Trigger();
	void SetSampleRate(int nSampleRate);
	void Seed(unsigned long seed);

	unsigned short Tick();
	unsigned short Level() const;
	unsigned short LevelTimesTwo() const;
	void Sync(bool bSync);

};


class CSAAFreq
{
protected:
	static unsigned long m_FreqTable[8][256];

	unsigned long m_nCounter;
	unsigned long m_nAdd;
	unsigned short m_nLevel;

	int m_nCurrentOffset;
	int m_nCurrentOctave;
	int m_nNextOffset;
	int m_nNextOctave;
	bool m_bIgnoreOffsetData;
	bool m_bNewData;
	bool m_bSync;

	int m_nSampleRateMode;
	unsigned long m_nSampleRateTimes4K;
	CSAANoise * const m_pcConnectedNoiseGenerator;
	CSAAEnv * const m_pcConnectedEnvGenerator;
	const int m_nConnectedMode; // 0 = nothing; 1 = envgenerator; 2 = noisegenerator

	void UpdateOctaveOffsetData();
	void SetAdd();

public:
	CSAAFreq(CSAANoise * const pcNoiseGenerator, CSAAEnv * const pcEnvGenerator);
	~CSAAFreq() { }
	void SetFreqOffset(BYTE nOffset);
	void SetFreqOctave(BYTE nOctave);
	void SetSampleRate(int nSampleRate);
	void Sync(bool bSync);
	unsigned short Tick();
	unsigned short Level() const;

};


class CSAAAmp
{
public:
	typedef union
	{
		struct {
			unsigned short Left;
			unsigned short Right;
		} sep;
		unsigned long dword;
	} stereolevel;

protected:
	unsigned short leftleveltimes16, leftleveltimes32, leftlevela0x0e, leftlevela0x0etimes2;
	unsigned short rightleveltimes16, rightleveltimes32, rightlevela0x0e, rightlevela0x0etimes2;
	unsigned short monoleveltimes16, monoleveltimes32;
	unsigned short m_nOutputIntermediate;
	unsigned int m_nMixMode;
	CSAAFreq * const m_pcConnectedToneGenerator; // not const because amp calls ->Tick()
	const CSAANoise * const m_pcConnectedNoiseGenerator;
	const CSAAEnv * const m_pcConnectedEnvGenerator;
	const bool m_bUseEnvelope;
	mutable bool m_bMute;
	mutable BYTE last_level_byte;
	mutable bool level_unchanged;
	mutable unsigned short last_leftlevel, last_rightlevel;
	mutable bool leftlevel_unchanged, rightlevel_unchanged;
	mutable unsigned short cached_last_leftoutput, cached_last_rightoutput;

public:
	CSAAAmp(CSAAFreq * const ToneGenerator, const CSAANoise * const NoiseGenerator, const CSAAEnv * const EnvGenerator);
	~CSAAAmp() { }

	void SetAmpLevel(BYTE level_byte); // really just a BYTE
	void SetToneMixer(BYTE bEnabled);
	void SetNoiseMixer(BYTE bEnabled);
	unsigned short LeftOutput() const;
	unsigned short RightOutput() const;
	unsigned short MonoOutput() const;
	void Mute(bool bMute);
	void Tick();
	unsigned short TickAndOutputMono();
	stereolevel TickAndOutputStereo();
};

//////////////////////////////////////////////////////////////////////

class CSAASound
{
protected:
	int m_nCurrentSaaReg;
	bool m_bOutputEnabled;
	bool m_bSync;

	CSAAFreq * Osc[6];
	CSAANoise * Noise[2];
	CSAAAmp * Amp[6];
	CSAAEnv * Env[2];

public:
	CSAASound(int nSampleRate);
	~CSAASound();

	void WriteAddress(BYTE nReg);
	void WriteData(BYTE nData);
	void WriteAddressData(BYTE nReg, BYTE nData);
	void Clear();
	BYTE ReadAddress();

	void GenerateMany(BYTE * pBuffer, int nSamples);
};

#endif // SAA1099_H
