// Part of SAASound copyright 1998-2004 Dave Hooper <dave@rebuzz.org>
//
// SAASound.h: interface for the CSAASound class.
//
// Version 3.2 (8th August 2004)
// (c) 1998-2004 dave @ spc       <dave@rebuzz.org>
//
//////////////////////////////////////////////////////////////////////

#ifndef SAASOUND_H_INCLUDED
#define SAASOUND_H_INCLUDED

// Parameters for use with SetSoundParameters, for example,
// SetSoundParameters(SAAP_NOFILTER | SAAP_44100 | SAA_16BIT | SAA_STEREO);
#define SAAP_FILTER     0x00000300
#define SAAP_NOFILTER   0x00000100
#define SAAP_44100      0x00000030
#define SAAP_22050      0x00000020
#define SAAP_11025      0x00000010
#define SAAP_16BIT      0x0000000c
#define SAAP_8BIT       0x00000004
#define SAAP_STEREO     0x00000003
#define SAAP_MONO       0x00000001

// Bitmasks for use with GetCurrentSoundParameters, for example,
// unsigned long CurrentSampleRateParameter = GetCurrentSoundParameters()
#define SAAP_MASK_FILTER        0x00000300
#define SAAP_MASK_SAMPLERATE    0x00000030
#define SAAP_MASK_BITDEPTH      0x0000000c
#define SAAP_MASK_CHANNELS      0x00000003

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

#if defined(WIN32) || defined(_WIN32_WCE)
#define SAAAPI __stdcall
#else
#define SAAAPI
#endif


#ifdef __cplusplus

class CSAASound
{
public:
    virtual ~CSAASound() { }

    virtual void SetSoundParameters (SAAPARAM uParam) = 0;
    virtual void WriteAddress (BYTE nReg) = 0;
    virtual void WriteData (BYTE nData) = 0;
    virtual void WriteAddressData (BYTE nReg, BYTE nData) = 0;
    virtual void Clear () = 0;
    virtual BYTE ReadAddress () = 0;

    virtual SAAPARAM GetCurrentSoundParameters () = 0;
    virtual unsigned long GetCurrentSampleRate () = 0;
    static unsigned long GetSampleRate (SAAPARAM uParam);
    virtual unsigned short GetCurrentBytesPerSample () = 0;
    static unsigned short GetBytesPerSample (SAAPARAM uParam);

    virtual void GenerateMany (BYTE * pBuffer, unsigned long nSamples) = 0;

    virtual int SendCommand (SAACMD nCommandID, long nData) = 0;

};

typedef class CSAASound * LPCSAASOUND;

LPCSAASOUND SAAAPI CreateCSAASound ();
void SAAAPI DestroyCSAASound (LPCSAASOUND object);

#endif  // __cplusplus


#ifdef __cplusplus
extern "C" {
#endif

typedef void * SAASND;

// "C-style" interface for the CSAASound class
SAASND SAAAPI newSAASND(void);
void SAAAPI deleteSAASND(SAASND object);

void SAAAPI SAASNDSetSoundParameters(SAASND object, SAAPARAM uParam);
void SAAAPI SAASNDWriteAddress(SAASND object, BYTE nReg);
void SAAAPI SAASNDWriteData(SAASND object, BYTE nData);
void SAAAPI SAASNDWriteAddressData(SAASND object, BYTE nReg, BYTE nData);
void SAAAPI SAASNDClear(SAASND object);
BYTE SAAAPI SAASNDReadAddress(SAASND object);

SAAPARAM SAAAPI SAASNDGetCurrentSoundParameters(SAASND object);
unsigned short SAAAPI SAASNDGetCurrentBytesPerSample(SAASND object);
unsigned short SAAAPI SAASNDGetBytesPerSample(SAAPARAM uParam);
unsigned long SAAAPI SAASNDGetCurrentSampleRate(SAASND object);
unsigned long SAAAPI SAASNDGetSampleRate(SAAPARAM uParam);

void SAAAPI SAASNDGenerateMany(SAASND object, BYTE * pBuffer, unsigned long nSamples);
int SAAAPI SAASNDSendCommand(SAACMD nCommandID, long nData);


#ifdef __cplusplus
}; // extern "C"
#endif

#endif  // SAASOUND_H_INCLUDED
