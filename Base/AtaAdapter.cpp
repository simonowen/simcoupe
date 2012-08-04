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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "SimCoupe.h"
#include "AtaAdapter.h"


CAtaAdapter::CAtaAdapter ()
    : m_uActive(0), m_pDisk0(NULL), m_pDisk1(NULL)
{
}

CAtaAdapter::~CAtaAdapter ()
{
    delete m_pDisk0;
    delete m_pDisk1;
}

// 8-bit read
BYTE CAtaAdapter::In (WORD wPort_)
{
    return InWord(wPort_) & 0xff;
}

// 16-bit read
WORD CAtaAdapter::InWord (WORD wPort_)
{
    WORD wRet = 0x0000;

    if (m_pDisk0) wRet |= m_pDisk0->In(wPort_);
    if (m_pDisk1) wRet |= m_pDisk1->In(wPort_);

    // Return the combined result
    return wRet;
}

// 8-bit write (16-bit handled by derived class)
void CAtaAdapter::Out (WORD wPort_, BYTE bVal_)
{
    if (m_pDisk0) m_pDisk0->Out(wPort_, bVal_);
    if (m_pDisk1) m_pDisk1->Out(wPort_, bVal_);
}


void CAtaAdapter::Reset ()
{
    if (m_pDisk0) m_pDisk0->Reset();
    if (m_pDisk1) m_pDisk1->Reset();
}


bool CAtaAdapter::Attach (const char *pcszDisk_, int nDevice_)
{
    // Return if successfully or path is empty
    return Attach(CHardDisk::OpenObject(pcszDisk_), nDevice_) || !*pcszDisk_;
}

bool CAtaAdapter::Attach (CHardDisk *pDisk_, int nDevice_)
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

    return pDisk_ != NULL;
}

void CAtaAdapter::Detach ()
{
    delete m_pDisk0, m_pDisk0 = NULL;
    delete m_pDisk1, m_pDisk1 = NULL;
}
