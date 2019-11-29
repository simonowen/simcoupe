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

#include "SimCoupe.h"
#include "Parallel.h"

#include "Frame.h"
#include "Options.h"
#include "Sound.h"


BYTE CPrintBuffer::In(WORD wPort_)
{
    BYTE bBusy = GetOption(printeronline) ? 0x00 : 0x01;
    return (wPort_ & 1) ? (m_bStatus | bBusy) : m_bData;
}

void CPrintBuffer::Out(WORD wPort_, BYTE bVal_)
{
    // Don't accept data if the printer is offline
    if (!GetOption(printeronline))
        return;

    // If the write is to the data port, save the byte for later
    if (!(wPort_ & 1))
        m_bData = bVal_;
    else
    {
        // If the printer is being strobed, write the data byte on rising edge
        if (((m_bControl ^ bVal_) & 0x01) && (bVal_ & 0x1))
        {
            // Add the new byte, and start the count-down to have it flushed
            m_abBuffer[m_uBuffer++] = m_bData;
            m_uFlushDelay = GetOption(flushdelay) * EMULATED_FRAMES_PER_SECOND;

            // Open the output stream if not already open
            if (!m_fOpen && Open())
            {
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

        // Update strobe state
        m_bControl = bVal_;
    }
}

void CPrintBuffer::Flush()
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

void CPrintBuffer::FrameEnd()
{
    // Flush the buffer when we've counted down to zero
    if (m_uFlushDelay && !--m_uFlushDelay)
        Flush();
}

///////////////////////////////////////////////////////////////////////////////

bool CPrinterFile::Open()
{
    m_pszFile = Util::GetUniqueFile("txt", m_szPath, sizeof(m_szPath));

    m_hFile = fopen(m_szPath, "wb");
    if (!m_hFile)
    {
        Frame::SetStatus("Failed to open %s", m_szPath);
        return false;
    }

    return true;
}

void CPrinterFile::Close()
{
    if (m_hFile)
    {
        fclose(m_hFile);
        m_hFile = nullptr;

        Frame::SetStatus("Saved %s", m_pszFile);
        m_pszFile = nullptr;
    }
}

void CPrinterFile::Write(BYTE* pb_, size_t uLen_)
{
    if (m_hFile)
        fwrite(pb_, uLen_, 1, m_hFile);
}

///////////////////////////////////////////////////////////////////////////////

void CMonoDACDevice::Out(WORD wPort_, BYTE bVal_)
{
    // If the write is to the data port, send it to the DAC
    if (!(wPort_ & 1))
        pDAC->Output(bVal_);
}

////////////////////////////////////////////////////////////////////////////////

void CStereoDACDevice::Out(WORD wPort_, BYTE bVal_)
{
    // Sample data?
    if (!(wPort_ & 1))
        m_bData = bVal_;
    else
    {
        // Port strobed?
        if ((m_bControl ^ bVal_) & 0x01)
        {
            // Output to left or right channel
            if (bVal_ & 0x01)
                pDAC->OutputLeft(m_bData);
            else
                pDAC->OutputRight(m_bData);
        }

        // Update strobe state
        m_bControl = bVal_;
    }
}
