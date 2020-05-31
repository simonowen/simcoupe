// Part of SimCoupe - A SAM Coupe emulator
//
// AtomLite.h: Atom-Lite hard disk interface
//
//  Copyright (c) 1999-2012 Simon Owen
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

#include "AtaAdapter.h"
#include "Clock.h"

const uint8_t ATOM_LITE_ADDR_MASK = 0x1f;  // Chip select mask
const uint8_t ATOM_LITE_REG_MASK = 0x07;   // Device address mask

class CAtomLiteDevice final : public CAtaAdapter
{
public:
    uint8_t In(uint16_t wPort_) override;
    void Out(uint16_t wPort_, uint8_t bVal_) override;

public:
    bool Attach(std::unique_ptr<CHardDisk> disk, int nDevice_) override;

protected:
    CDallasClock m_Dallas{};
    uint8_t m_bAddressLatch = 0;
};
