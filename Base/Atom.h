// Part of SimCoupe - A SAM Coupe emulator
//
// Atom.h: ATOM hard disk interface
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

const uint8_t ATOM_ADDR_MASK = 0x1f;   // Chip select mask
const uint8_t ATOM_REG_MASK = 0x07;    // Device address mask
const uint8_t ATOM_NRESET = 0x20;      // Reset pin (negative logic)

class AtomDevice final : public AtaAdapter
{
public:
    uint8_t In(uint16_t wPort_) override;
    void Out(uint16_t wPort_, uint8_t bVal_) override;

public:
    bool Attach(std::unique_ptr<HardDisk> disk, int nDevice_) override;

protected:
    uint8_t m_bAddressLatch = 0;
    uint8_t m_bReadLatch = 0;
    uint8_t m_bWriteLatch = 0;
};
