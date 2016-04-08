// Part of SimCoupe - A SAM Coupe emulator
//
// Floppy.h: Real floppy access (Linux-only)
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

#ifndef FLOPPY_H
#define FLOPPY_H

#include "Stream.h"

typedef struct
{
    BYTE sectors = 0;
    BYTE cyl = 0, head = 0;     // physical track location
} TRACK, *PTRACK;

typedef struct
{
    BYTE cyl = 0, head = 0, sector = 0, size = 0;
    BYTE status = 0;
    BYTE *pbData = nullptr;
} SECTOR, *PSECTOR;


class CFloppyStream final : public CStream
{
    public:
        CFloppyStream (const char* pcszStream_, bool fReadOnly_=false);
        CFloppyStream (const CFloppyStream &) = delete;
        void operator= (const CFloppyStream &) = delete;
        ~CFloppyStream () { Close(); }

    public:
        static bool IsRecognised (const char* pcszStream_);

    public:
        void Close () override;
        void *ThreadProc ();

    public:
        bool IsOpen () const override { return m_hFloppy != -1; }
        bool IsBusy (BYTE* pbStatus_, bool fWait_);

        // The normal stream functions are not used
        bool Rewind () override { return false; }
        size_t Read (void*, size_t) override { return 0; }
        size_t Write (void*, size_t) override { return 0; }

        BYTE StartCommand (BYTE bCommand_, PTRACK pTrack_=nullptr, UINT uSectorIndex_=0);

    protected:
        bool Open ();

    protected:
        int m_hFloppy = -1;             // Floppy device handle
        UINT m_uSectors = 0;            // Regular sector count, or zero for auto-detect (slower)

#ifdef __linux__
        pthread_t m_hThread = 0;        // Thread handle
        bool m_fThreadDone = false;     // True when thread has completed
#else
        int m_hThread = -1;             // Dummy handle for non-Linux
#endif

        BYTE m_bCommand = 0;            // Current command
        BYTE m_bStatus = 0;             // Final status

        PTRACK m_pTrack = nullptr;      // Track for command
        UINT m_uSectorIndex = 0;        // Zero-based sector for write command
};

#endif  // FLOPPY_H
