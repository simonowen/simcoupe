// Part of SimCoupe - A SAM Coupe emulator
//
// Parallel.cpp: Parallel interface
//
//  Copyright (c) 1999-2005  Simon Owen
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
#include "Parallel.h"


CPrinterDevice::CPrinterDevice ()
    : m_bPrint(0), m_bStatus(0), m_fFailed(false), m_nPrinter(0)
{
}

CPrinterDevice::~CPrinterDevice ()
{
    Close();
}


BYTE CPrinterDevice::In (WORD wPort_)
{
    return (wPort_ & 1) ? m_bStatus : 0;
}

void CPrinterDevice::Out (WORD wPort_, BYTE bVal_)
{
    // If the write is to the data port, save the byte for later
    if (!(wPort_ & 1))
        m_bPrint = bVal_;

    else
    {
        // If the printer is being strobed, write the data byte
        if ((m_bStatus & 0x01) && !(bVal_ & 0x01))
            Write(m_bPrint);

        // Update the status with the strobe bit
        m_bStatus &= ~0x01;
        m_bStatus |= (bVal_ & 0x01);
    }
}


bool CPrinterDevice::Open ()
{
    Close();

    // Start print job here

    // Signal the printer is ready to receive data
    m_bStatus |= 0x81;
    return true;
}

void CPrinterDevice::Close ()
{
    if (IsOpen())
    {
        Flush();

        // Finish print job here

        m_bStatus = 0;
    }
}

void CPrinterDevice::Write (BYTE bPrint_)
{
    // Start a new job if there isn't an existing one
    if (IsOpen() || Open())
    {
        if (m_nPrinter < (int)sizeof m_abPrinter)
        {
            m_abPrinter[m_nPrinter++] = bPrint_;

            if (m_nPrinter >= (int)sizeof m_abPrinter)
                Flush();
        }
    }
}

void CPrinterDevice::Flush ()
{
    if (m_nPrinter && IsOpen())
    {
        // Write data to printer here
    }

    m_nPrinter = 0;
}

////////////////////////////////////////////////////////////////////////////////

void CMonoDACDevice::Out (WORD wPort_, BYTE bVal_)
{
    // If the write is to the data port, send it to the DAC
    if (!(wPort_ & 1))
        Sound::OutputDAC(bVal_);
}

////////////////////////////////////////////////////////////////////////////////

void CStereoDACDevice::Out (WORD wPort_, BYTE bVal_)
{
    wPort_ &= 3;

    // Sample value?
    if (!(wPort_ & 1))
        m_bVal = bVal_;

    // Output to left or right channel
    else if (bVal_ & 1)
        Sound::OutputDACLeft(m_bVal);
    else
        Sound::OutputDACRight(m_bVal);
}
