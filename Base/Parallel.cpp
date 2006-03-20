// Part of SimCoupe - A SAM Coupe emulator
//
// Parallel.cpp: Parallel interface
//
//  Copyright (c) 1999-2006  Simon Owen
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

#include "Frame.h"
#include "Options.h"


BYTE CPrintBuffer::In (WORD wPort_)
{
    BYTE bBusy = GetOption(printeronline) ? 0x00 : 0x01;
    return (wPort_ & 1) ? (m_bStatus|bBusy) : 0x00;
}

void CPrintBuffer::Out (WORD wPort_, BYTE bVal_)
{
    // Don't accept data if the printer is offline
    if (!GetOption(printeronline))
        return;

    // If the write is to the data port, save the byte for later
    if (!(wPort_ & 1))
        m_bPrint = bVal_;
    else
    {
        // If the printer is being strobed, write the data byte
        if ((m_bStatus & 0x01) && !(bVal_ & 0x01))
        {
            // Add the new byte, and start the count-down to have it flushed
            m_abBuffer[m_uBuffer++] = m_bPrint;
            m_uFlushDelay = GetOption(flushdelay)*EMULATED_FRAMES_PER_SECOND;

            // Open the output stream if not already open
            if (!m_fOpen)
            {
                if (Open())
                    Frame::SetStatus("Print job started");

                m_fOpen = true;
            }

            // If we've filled the buffer, write it to the stream
            if (m_uBuffer == sizeof(m_abBuffer))
            {
                Write(m_abBuffer, m_uBuffer);
                m_uBuffer = 0;
            }
        }

        // Update the status with the strobe bit
        m_bStatus &= ~0x01;
        m_bStatus |= (bVal_ & 0x01);
    }
}

void CPrintBuffer::Flush ()
{
    // Do we have any unflushed data?
    if (m_uBuffer)
    {
        // Write the remaining data
        Write(m_abBuffer, m_uBuffer);
        m_uBuffer = 0;

        // Close the device to finish the job
        Close();
        m_fOpen = false;
    }
}

void CPrintBuffer::FrameEnd ()
{
    // Flush the buffer when we've counted down to zero
    if (m_uFlushDelay && !--m_uFlushDelay)
        Flush();
}

///////////////////////////////////////////////////////////////////////////////

bool CPrinterFile::Open ()
{
    static int nNext = 0;
    char szTemplate[MAX_PATH], szOutput[MAX_PATH];

    sprintf(szTemplate, "%sprnt%%04d.txt", OSD::GetDirPath(GetOption(datapath)));
    nNext = Util::GetUniqueFile(szTemplate, m_nPrint=nNext, szOutput, sizeof(szOutput));

    m_hFile = fopen(szOutput, "wb");

    if (!m_hFile)
    {
        Frame::SetStatus("Failed to open %s", szOutput);
        return false;
    }

    return true;
}

void CPrinterFile::Close ()
{
    if (m_hFile)
    {
        fclose(m_hFile);
        m_hFile = NULL;

        Frame::SetStatus("Print complete:  prnt%04d.txt", m_nPrint);
    }
}

void CPrinterFile::Write (BYTE *pb_, size_t uLen_)
{
    if (m_hFile)
        fwrite(pb_, uLen_, 1, m_hFile);
}

///////////////////////////////////////////////////////////////////////////////

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
