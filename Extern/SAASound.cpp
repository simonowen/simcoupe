// Part of SimCoupe - A SAM Coupé emulator
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
#include "../Extern/SAASound.h"


#ifdef DUMMY_SAASOUND

////////////////////////////////////////////////////////////////////////////////

LPCSAASOUND WINAPI CreateCSAASound ()
{
    return reinterpret_cast<LPCSAASOUND>(new DWORD);
}

void WINAPI DestroyCSAASound (LPCSAASOUND p_)
{
    delete reinterpret_cast<DWORD*>(p_);
}

////////////////////////////////////////////////////////////////////////////////

void CSAASound::SetSoundParameters (SAAPARAM dwParams_)
{
    *reinterpret_cast<DWORD*>(this) = dwParams_;
}

void CSAASound::WriteAddress (BYTE bReg_)
{
}

void CSAASound::WriteData (BYTE bData_)
{
}

void CSAASound::WriteAddressData (BYTE bReg_, BYTE bData_)
{
}

void CSAASound::Clear ()
{
}

BYTE CSAASound::ReadAddress ()
{
    return 0x00;
}

SAAPARAM CSAASound::GetCurrentSoundParameters ()
{
    return *reinterpret_cast<DWORD*>(this);
}

unsigned long CSAASound::GetCurrentSampleRate ()
{
    return *reinterpret_cast<DWORD*>(this) & SAAP_44100;
}

/*static*/ unsigned long CSAASound::GetSampleRate (SAAPARAM dwParams_)
{
    return dwParams_ & SAAP_16BIT;
}

unsigned short CSAASound::GetCurrentBytesPerSample ()
{
    DWORD dwParams = *reinterpret_cast<DWORD*>(this);
    return ((dwParams & 8) ? 2 : 1) * ((dwParams & 2) ? 2 : 1);
}

/*static*/ unsigned short CSAASound::GetBytesPerSample (SAAPARAM dwParams_)
{
    return ((dwParams_ & 8) ? 2 : 1) * ((dwParams_ & 2) ? 2 : 1);
}

void CSAASound::GenerateMany(BYTE* pBuffer_, DWORD dwSamples_)
{
    // Generate as much silence as requested
    memset(pBuffer_, (*reinterpret_cast<DWORD*>(this) & 8) ? 0x00 : 0x80, dwSamples_ * GetCurrentBytesPerSample());
}

void CSAASound::ClickClick(int bValue_)
{
}

int CSAASound::SendCommand(SAACMD nCommandID, long nData)
{
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

#else

#ifdef _WINDOWS
#pragma comment(lib, "SAASound")
#endif

#endif  // DUMMY_SAASOUND
