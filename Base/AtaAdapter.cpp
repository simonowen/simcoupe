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
    : m_uActive(0), m_bActiveDevice(0), m_pDisk0(NULL), m_pDisk1(NULL)
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
    // Send the request to the appropriate device
    if (m_bActiveDevice == 0 && m_pDisk0)
        return m_pDisk0->In(wPort_);
    else if (m_bActiveDevice != 0 && m_pDisk1)
        return m_pDisk1->In(wPort_);

    return 0xffff;
}

// 8-bit write (16-bit handled by derived class)
void CAtaAdapter::Out (WORD wPort_, BYTE bVal_)
{
    // Drive/head and device control register writes must go to both devices
    bool fReg6 = (wPort_ & ATA_DA_MASK) == 6;

    // Track changes to active device
    if ((~wPort_ & ATA_CS_MASK) == ATA_CS0 && fReg6)
        m_bActiveDevice = (bVal_ & ATA_DEVICE_MASK);

    // Send the request to the appropriate device
    if ((m_bActiveDevice == 0 || fReg6) && m_pDisk0)
        m_pDisk0->Out(wPort_, bVal_);
    else if ((m_bActiveDevice != 0 || fReg6) && m_pDisk1)
        m_pDisk1->Out(wPort_, bVal_);
}


void CAtaAdapter::Reset ()
{
    if (m_pDisk0) m_pDisk0->Reset();
    if (m_pDisk1) m_pDisk1->Reset();
}


const char* CAtaAdapter::DiskPath (int nDevice_) const
{
    if (nDevice_ == 0)
        return m_pDisk0 ? m_pDisk0->GetPath() : "";
    else
        return m_pDisk1 ? m_pDisk1->GetPath() : "";
}


bool CAtaAdapter::Insert (const char *pcszDisk_, int nDevice_)
{
    return Insert(CHardDisk::OpenObject(pcszDisk_), nDevice_);
}

bool CAtaAdapter::Insert (CHardDisk *pDisk_, int nDevice_)
{
    if (nDevice_ == 0)
    {
        delete m_pDisk0;
        m_pDisk0 = pDisk_;
    }
    else
    {
        delete m_pDisk1;
        m_pDisk1 = pDisk_;
    }

    return pDisk_ != NULL;
}
