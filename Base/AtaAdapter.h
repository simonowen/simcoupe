// Part of SimCoupe - A SAM Coupe emulator
//
// AtaAdapter.h: ATA bus adapter
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

#pragma once

#include "HardDisk.h"

class CAtaAdapter : public CIoDevice
{
public:
    uint8_t In(uint16_t wPort_) override;
    void Out(uint16_t wPort_, uint8_t bVal_) override;

    void Reset() override;
    void FrameEnd() override { if (m_uActive) m_uActive--; }

public:
    bool IsActive() const { return m_uActive != 0; }

public:
    bool Attach(const char* pcszDisk_, int nDevice_);
    virtual bool Attach(std::unique_ptr<CHardDisk> disk, int nDevice_);
    virtual void Detach();

protected:
    uint16_t InWord(uint16_t wPort_);
    void OutWord(uint16_t wPort_, uint16_t wVal_);

protected:
    unsigned int m_uActive = 0; // active when non-zero, decremented by FrameEnd()

private:
    std::unique_ptr<CHardDisk> m_pDisk0;
    std::unique_ptr<CHardDisk> m_pDisk1;
};

extern std::unique_ptr<CAtaAdapter> pAtom, pAtomLite, pSDIDE;
