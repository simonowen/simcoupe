// Part of SimCoupe - A SAM Coupe emulator
//
// SAA1099.cpp: Philips SAA 1099 sound chip emulation
//
// Copyright (c) 1998-2014, Dave Hooper <dave@rebuzz.org>  
// All rights reserved.

#pragma once

class CSAAEnv
{
    struct ENVDATA
    {
        int nNumberOfPhases;
        bool bLooping;
        unsigned short nLevels[2][2][16]; // [Resolution][Phase][Withinphase]
    };

protected:
    unsigned short m_nLeftLevel = 0, m_nRightLevel = 0;
    ENVDATA const* m_pEnvData = nullptr;

    bool m_bEnabled = false;
    bool m_bInvertRightChannel = false;
    uint8_t m_nPhase = 0;
    uint8_t m_nPhasePosition = 0;
    bool m_bEnvelopeEnded = true;
    char m_nPhaseAdd[2];
    bool m_bLooping = false;
    char m_nNumberOfPhases = 0;
    char m_nResolution = 0;
    bool m_bNewData = false;
    uint8_t m_nNextData = 0;
    bool m_bOkForNewData = false;
    bool m_bClockExternally = false;
    static const ENVDATA cs_EnvData[8];

    void Tick();
    void SetLevels();
    void SetNewEnvData(int nData);

public:
    CSAAEnv();
    CSAAEnv(const CSAAEnv&) = delete;
    void operator= (const CSAAEnv&) = delete;
    ~CSAAEnv();

    void InternalClock();
    void ExternalClock();
    void SetEnvControl(int nData); // really just a uint8_t
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
    unsigned long m_nSampleRateTimes4K; // = (44100*4096) when RateMode=0, for example
    int m_nSourceMode;
    static const unsigned long cs_nAddBase; // nAdd for 31.25 kHz noise at 44.1 kHz samplerate

    // pseudo-random number generator
    unsigned long m_nRand;

    void ChangeLevel();

public:
    CSAANoise(unsigned long seed);

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

    unsigned long m_nCounter = 0;
    unsigned long m_nAdd = 0;
    unsigned short m_nLevel = 2;

    int m_nCurrentOffset = 0;
    int m_nCurrentOctave = 0;
    int m_nNextOffset = 0;
    int m_nNextOctave = 0;
    bool m_bIgnoreOffsetData = false;
    bool m_bNewData = false;
    bool m_bSync = false;

    unsigned long m_nSampleRateTimes4K = (44100 << 12);
    CSAANoise* const m_pcConnectedNoiseGenerator = nullptr;
    CSAAEnv* const m_pcConnectedEnvGenerator = nullptr;
    const int m_nConnectedMode = 0; // 0 = nothing; 1 = envgenerator; 2 = noisegenerator

    void UpdateOctaveOffsetData();
    void SetAdd();

public:
    CSAAFreq(CSAANoise* const pcNoiseGenerator, CSAAEnv* const pcEnvGenerator);

    void SetFreqOffset(uint8_t nOffset);
    void SetFreqOctave(uint8_t nOctave);
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
    unsigned short leftleveltimes16 = 0, leftleveltimes32 = 0, leftlevela0x0e = 0, leftlevela0x0etimes2 = 0;
    unsigned short rightleveltimes16 = 0, rightleveltimes32 = 0, rightlevela0x0e = 0, rightlevela0x0etimes2 = 0;
    unsigned short m_nOutputIntermediate = 0;
    unsigned int m_nMixMode = 0;
    CSAAFreq* const m_pcConnectedToneGenerator = nullptr; // not const because amp calls ->Tick()
    const CSAANoise* const m_pcConnectedNoiseGenerator = nullptr;
    const CSAAEnv* const m_pcConnectedEnvGenerator = nullptr;
    const bool m_bUseEnvelope = false;
    mutable bool m_bMute = true;
    mutable uint8_t last_level_byte = 0;
    mutable bool level_unchanged = false;
    mutable unsigned short last_leftlevel = 0, last_rightlevel = 0;
    mutable bool leftlevel_unchanged = false, rightlevel_unchanged = false;
    mutable unsigned short cached_last_leftoutput = 0, cached_last_rightoutput = 0;

public:
    CSAAAmp(CSAAFreq* const ToneGenerator, const CSAANoise* const NoiseGenerator, const CSAAEnv* const EnvGenerator);

    void SetAmpLevel(uint8_t level_byte); // really just a uint8_t
    void SetToneMixer(uint8_t bEnabled);
    void SetNoiseMixer(uint8_t bEnabled);
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
    int m_nCurrentSaaReg = 0;
    bool m_bOutputEnabled = false;
    bool m_bSync = false;

    CSAAFreq* Osc[6];
    CSAANoise* Noise[2];
    CSAAAmp* Amp[6];
    CSAAEnv* Env[2];

public:
    CSAASound(int nSampleRate);
    CSAASound(const CSAASound&) = delete;
    void operator= (const CSAASound&) = delete;
    ~CSAASound();

    void WriteAddress(uint8_t nReg);
    void WriteData(uint8_t nData);
    void WriteAddressData(uint8_t nReg, uint8_t nData);
    void Clear();
    uint8_t ReadAddress();

    void GenerateMany(uint8_t* pBuffer, int nSamples);
};
