// Part of SimCoupe - A SAM Coupe emulator
//
// SAASound.cpp: Dummy SAASound interface
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
// Dummy SAASound implementation, for when the real thing isn't available.

// Notes:
//  This module implements all the SAASound interface functions, with enough
//  functionality to generate sample data representing silence.

#include "SimCoupe.h"

#ifndef USE_SAASOUND
#include "./SAASound.h"

////////////////////////////////////////////////////////////////////////////////

class CSAASoundImpl : public CSAASound
{
public:
    CSAASoundImpl () : m_dwParams(0) {  }

public:
    void SetSoundParameters (SAAPARAM dwParams_) { m_dwParams = dwParams_; }
    void WriteAddress (BYTE nReg) { }
    void WriteData (BYTE nData) { }
    void WriteAddressData (BYTE nReg, BYTE nData) { }
    void Clear () { }
    BYTE ReadAddress () { return 0x00; }

    SAAPARAM GetCurrentSoundParameters () { return m_dwParams; }
    unsigned long GetCurrentSampleRate () { return m_dwParams & SAAP_44100; }

    unsigned short GetCurrentBytesPerSample ()
        { return ((m_dwParams & 8) ? 2 : 1) * ((m_dwParams & 2) ? 2 : 1); }

    void GenerateMany (BYTE* pBuffer_, unsigned long dwSamples_)
        { memset(pBuffer_, (m_dwParams & 8) ? 0x00 : 0x80, dwSamples_ * GetCurrentBytesPerSample()); }

    int SendCommand (SAACMD nCommandID, long nData) { return 0; }


    static unsigned long GetSampleRate (SAAPARAM dwParams_)
        { return dwParams_ & SAAP_16BIT; }

    static unsigned short GetBytesPerSample (SAAPARAM dwParams_)
        { return ((dwParams_ & 8) ? 2 : 1) * ((dwParams_ & 2) ? 2 : 1); }

protected:
    DWORD m_dwParams;
};

////////////////////////////////////////////////////////////////////////////////

LPCSAASOUND SAAAPI CreateCSAASound ()
{
    return reinterpret_cast<LPCSAASOUND>(new CSAASoundImpl);
}

void SAAAPI DestroyCSAASound (LPCSAASOUND pSAASound_)
{
    if (pSAASound_)
        delete reinterpret_cast<CSAASoundImpl*>(pSAASound_);
}

#endif  // !USE_SAASOUND
