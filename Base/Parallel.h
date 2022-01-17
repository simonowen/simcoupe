// Part of SimCoupe - A SAM Coupe emulator
//
// Parallel.cpp: Parallel interface
//
//  Copyright (c) 1999-2014 Simon Owen
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

class PrintBuffer : public IoDevice
{
public:
    ~PrintBuffer() { Flush(); }

    uint8_t In(uint16_t wPort_) override;
    void Out(uint16_t wPort_, uint8_t bVal_) override;
    void FrameEnd() override;

    bool IsFlushable() const { return !!m_uBuffer; }
    void Flush();

protected:
    bool m_fOpen = false;
    uint8_t m_bControl = 0, m_bData = 0, m_bStatus = 0;

    unsigned int m_uBuffer = 0, m_uFlushDelay = 0;
    uint8_t m_abBuffer[1024];

protected:
    bool IsOpen() const { return false; }

    virtual bool Open() = 0;
    virtual void Close() = 0;
    virtual void Write(uint8_t* pb_, size_t uLen_) = 0;
};


class PrinterFile final : public PrintBuffer
{
public:
    PrinterFile() = default;
    PrinterFile(const PrinterFile&) = delete;
    void operator= (const PrinterFile&) = delete;
    ~PrinterFile() { Close(); }

public:
    bool Open() override;
    void Close() override;
    void Write(uint8_t* pb_, size_t uLen_) override;

protected:
    unique_FILE m_file;
    std::string print_path;
};

class MonoDACDevice : public IoDevice
{
public:
    void Out(uint16_t wPort_, uint8_t bVal_) override;
};


class StereoDACDevice : public IoDevice
{
public:
    StereoDACDevice() : m_bControl(0x00), m_bData(0x80) { }

public:
    void Out(uint16_t wPort_, uint8_t bVal_) override;

protected:
    uint8_t m_bControl, m_bData;
};

extern std::unique_ptr<PrintBuffer> pPrinterFile;
