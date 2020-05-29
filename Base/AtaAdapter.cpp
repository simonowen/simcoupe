// Part of SimCoupe - A SAM Coupe emulator
//
// AtaAdapter.cpp: ATA bus adapter
//
//  Copyright (c) 2012 Simon Owen
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
#include "AtaAdapter.h"


CAtaAdapter::~CAtaAdapter()
{
    delete m_pDisk0;
    delete m_pDisk1;
}

// 8-bit read
uint8_t CAtaAdapter::In(uint16_t wPort_)
{
    return InWord(wPort_) & 0xff;
}

// 16-bit read
uint16_t CAtaAdapter::InWord(uint16_t wPort_)
{
    uint16_t wRet = 0x0000;

    if (m_pDisk0) wRet |= m_pDisk0->In(wPort_);
    if (m_pDisk1) wRet |= m_pDisk1->In(wPort_);

    // Return the combined result
    return wRet;
}

void CAtaAdapter::Out(uint16_t wPort_, uint8_t bVal_)
{
    if (m_pDisk0) m_pDisk0->Out(wPort_, bVal_);
    if (m_pDisk1) m_pDisk1->Out(wPort_, bVal_);
}

void CAtaAdapter::OutWord(uint16_t wPort_, uint16_t wVal_)
{
    if (m_pDisk0) m_pDisk0->Out(wPort_, wVal_);
    if (m_pDisk1) m_pDisk1->Out(wPort_, wVal_);
}


void CAtaAdapter::Reset()
{
    if (m_pDisk0) m_pDisk0->Reset();
    if (m_pDisk1) m_pDisk1->Reset();
}


bool CAtaAdapter::Attach(const char* pcszDisk_, int nDevice_)
{
    // Return if successfully or path is empty
    return Attach(CHardDisk::OpenObject(pcszDisk_), nDevice_) || !*pcszDisk_;
}

bool CAtaAdapter::Attach(CHardDisk* pDisk_, int nDevice_)
{
    if (nDevice_ == 0)
    {
        delete m_pDisk0;
        m_pDisk0 = pDisk_;

        // Jumper the disk as device 0
        if (m_pDisk0) m_pDisk0->SetDeviceAddress(ATA_DEVICE_0);
    }
    else
    {
        delete m_pDisk1;
        m_pDisk1 = pDisk_;

        // Jumper the disk as device 1
        if (m_pDisk1) m_pDisk1->SetDeviceAddress(ATA_DEVICE_1);
    }

    return pDisk_ != nullptr;
}

void CAtaAdapter::Detach()
{
    delete m_pDisk0; m_pDisk0 = nullptr;
    delete m_pDisk1; m_pDisk1 = nullptr;
}
