// SAASound.h: interface for the CSAASound class.
//
//////////////////////////////////////////////////////////////////////

#ifndef SAASOUND_H_INCLUDED
#define SAASOUND_H_INCLUDED

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef SAASNDC_H_INCLUDED

// Parameters for use with SetSoundParameters, for example,
// SetSoundParameters(SAAP_NOFILTER | SAAP_44100 | SAA_16BIT | SAA_STEREO);
#define SAAP_FILTER 0x00000300
#define SAAP_NOFILTER 0x00000100
#define SAAP_44100 0x00000030
#define SAAP_22050 0x00000020
#define SAAP_11025 0x00000010
#define SAAP_16BIT 0x0000000c
#define SAAP_8BIT 0x00000004
#define SAAP_STEREO 0x00000003
#define SAAP_MONO 0x00000001

// Bitmasks for use with GetCurrentSoundParameters, for example,
// unsigned long CurrentSampleRateParameter = GetCurrentSoundParameters()
#define SAAP_MASK_FILTER 0x00000300
#define SAAP_MASK_SAMPLERATE 0x000000030
#define SAAP_MASK_BITDEPTH 0x0000000c
#define SAAP_MASK_CHANNELS 0x00000003

typedef unsigned long SAAPARAM;


// command #defines for use with SendCommand function, eg, 
// int nCurrentSampleRate = SendCommand(SAACMD_GetSampleRate,0);
// or
// int nError = SendCommand(SAACMD_SetSampleRate,44100);
typedef unsigned long SAACMD;
#define SAACMD_SetVolumeBoost   0x00000001
#define SAACMD_GetVolumeBoost   0x00000002
#define SAACMD_SetSampleRate    0x00000003
#define SAACMD_GetSampleRate    0x00000004
#define SAACMD_SetNumChannels   0x00000005
#define SAACMD_GetNumChannels   0x00000006
#define SAACMD_SetBitDepth      0x00000007
#define SAACMD_GetBitDepth      0x00000008
#define SAACMD_SetFilterMode    0x00000009
#define SAACMD_GetFilterMode    0x0000000a

// 'Special' return values.
#define SAASENDCOMMAND_UNKNOWN_INVALID_COMMAND          0x80000000
#define SAASENDCOMMAND_FEATURE_NOT_YET_IMPLEMENTED      0x80000001
#define SAASENDCOMMAND_OK                               0x80000002
#define SAASENDCOMMAND_INVALIDPARAMETERS                0x80000003



#ifndef BYTE
#define BYTE unsigned char
#endif

#ifndef WINAPI
#define WINAPI
#endif

#endif // SAASNDC_H_INCLUDED

#ifdef _WINDOWS
extern "C"
#endif
class CSAASound
{
public:
    CSAASound();
    ~CSAASound();

    void SetSoundParameters(SAAPARAM uParam);
    void WriteAddress(BYTE nReg);
    void WriteData(BYTE nData);
    void WriteAddressData(BYTE nReg, BYTE nData);
    void Clear(void);
    BYTE ReadAddress(void);

    SAAPARAM GetCurrentSoundParameters(void);
    unsigned long GetCurrentSampleRate(void);
    static unsigned long GetSampleRate(SAAPARAM uParam);
    unsigned short GetCurrentBytesPerSample(void);
    static unsigned short GetBytesPerSample(SAAPARAM uParam);

    void GenerateMany(BYTE * pBuffer, unsigned long nSamples);
    void ClickClick(int bValue);
// 'Generate' function is obsolete
//  unsigned long Generate(void);

    int SendCommand(SAACMD nCommandID, long nData);

};

typedef class CSAASound * LPCSAASOUND;

LPCSAASOUND WINAPI CreateCSAASound(void);
void WINAPI DestroyCSAASound(LPCSAASOUND object);

#endif  // SAASOUND_H_INCLUDED
