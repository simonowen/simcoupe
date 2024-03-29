// Part of SimCoupe - A SAM Coupe emulator
//
// Mouse.h: Mouse interface
//
//  Copyright (c) 1999-2012 Simon Owen
//  Copyright (c) 1996-2001 Allan Skillman
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

#include "SAMIO.h"

constexpr auto MOUSE_RESET_TIME = usecs_to_tstates(30);
constexpr auto MOUSE_ACTIVE_TIME = std::chrono::seconds(1);

// Mouse buffer format, as read
struct MOUSEBUFFER
{
    uint8_t bStrobe;
    uint8_t bDummy;
    uint8_t bButtons;
    uint8_t bY256, bY16, bY1;
    uint8_t bX256, bX16, bX1;
};


class MouseDevice : public IoDevice
{
public:
    MouseDevice();

public:
    void Reset() override;
    uint8_t In(uint16_t wPort_) override;

public:
    void Move(int nDeltaX_, int nDeltaY_);
    void SetButton(int nButton_, bool fPressed_ = true);
    bool IsActive() const;

protected:
    int m_nDeltaX = 0, m_nDeltaY = 0;   // System change in X and Y since last read
    int m_nReadX = 0, m_nReadY = 0;     // Read change in X and Y
    uint8_t m_bButtons = 0;             // Current button states
    std::optional<std::chrono::steady_clock::time_point> read_time;  // When the mouse was last read

    MOUSEBUFFER m_sMouse{};
    unsigned int m_uBuffer = 0;         // Read position in mouse data
};

extern std::unique_ptr<MouseDevice> pMouse;
