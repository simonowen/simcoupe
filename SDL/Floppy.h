// Part of SimCoupe - A SAM Coupe emulator
//
// Floppy.h: SDL direct floppy access
//
//  Copyright (c) 1999-2012  Simon Owen
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

#ifndef FLOPPY_H
#define FLOPPY_H

#include "Stream.h"

typedef struct
{
    int sectors;
    BYTE cyl, head;     // physical track location
}
TRACK, *PTRACK;

typedef struct
{
    BYTE cyl, head, sector, size;
    BYTE status;
    BYTE *pbData;
}
SECTOR, *PSECTOR;


class CFloppyStream : public CStream
{
    public:
        CFloppyStream (const char* pcszStream_, bool fReadOnly_=false)
         : CStream(pcszStream_, fReadOnly_), m_nFloppy(-1), m_hThread(0) { }
        ~CFloppyStream () { Close(); }

    public:
        static bool IsRecognised (const char* pcszStream_);

    public:
        void Close ();
        void *ThreadProc ();

    public:
        bool IsOpen () const { return m_nFloppy != -1; }
        bool IsBusy (BYTE* pbStatus_, bool fWait_);

        // The normal stream functions are not used
        bool Rewind () { return false; }
        size_t Read (void*, size_t) { return 0; }
        size_t Write (void*, size_t) { return 0; }

        BYTE StartCommand (BYTE bCommand_, PTRACK pTrack_=NULL, UINT uSector_=0, BYTE *pbData_=NULL);

    protected:
        bool Open ();

    protected:
        int  m_nFloppy;                 // Floppy device handle
        UINT m_uSectors;                // Regular sector count, or zero for auto-detect (slower)

#ifdef __linux__
        pthread_t m_hThread;            // Thread handle
        bool m_fThreadDone;             // True when thread has completed
#else
        int m_hThread;                  // Dummy handle for non-Linux
#endif

        BYTE m_bCommand, m_bStatus;     // Current command and final status

        PTRACK  m_pTrack;               // Track for command
        UINT    m_uSector;              // Zero-based sector for write command
        BYTE   *m_pbData;               // Data to write (since track data is only updated when successful)
};

#endif  // FLOPPY_H
