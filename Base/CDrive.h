// Part of SimCoupe - A SAM Coupé emulator
//
// CDrive.h: VL1772-02 floppy disk controller emulation
//
//  Copyright (c) 1996-2001  Allan Skillman
//  Copyright (c) 1999-2001  Simon Owen
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

#ifndef CDRIVE_H
#define CDRIVE_H

#include "IO.h"

#include "VL1772.h"
#include "CDisk.h"


enum { drvNone, drvFile, dskDevice, dskAtom };


class CDrive : public CDiskDevice
{
    public:
        CDrive () { ResetAll(); }
        virtual ~CDrive () { if (IsInserted()) Eject(); }

    public:
        const char* GetImage () const { return m_pDisk ? m_pDisk->GetName() : ""; }
        bool IsInserted () const { return m_pDisk != NULL; }
        bool IsWriteable () const { return m_pDisk && !m_pDisk->IsReadOnly(); }
        bool IsModified () const { return m_pDisk != NULL && m_pDisk->IsModified(); }

        int GetType () const { return 1; }
        bool IsLightOn () const { return IsMotorOn() && m_pDisk; }

        bool Insert (const char* pcszSource_, bool fReadOnly_/*=false*/);
        bool Eject ();
        bool Flush ();

        // Disk I/O calls used to drive us
        BYTE In (WORD wPort_);
        void Out (WORD wPort_, BYTE bVal_);
        void FrameEnd ();

        // Functions to deal with the contents of the disk
        bool FindSector  (int nSide_, int nTrack_, int nSector_, IDFIELD* pIdField_=NULL);
        BYTE ReadAddress (int nSide_, int nTrack_, IDFIELD* pIdField_);
        UINT ReadTrack (int nSide_, int nTrack_, BYTE* pbTrack_, UINT uSize_);
        BYTE VerifyTrack (int nSide_, int nTrack_);
        BYTE WriteTrack (int nSide_, int nTrack_, BYTE* pbTrack_, UINT uSize_);

    protected:
        CDisk*      m_pDisk;        // The disk currently inserted in the drive, if any
        VL1772Regs  m_sRegs;        // VL1772 controller registers
        int         m_nHeadPos;     // Physical track the drive head is above

        BYTE        m_abBuffer[MAX_TRACK_SIZE]; // Buffer big enough for anything we'll read or write
        BYTE*       m_pbBuffer;
        UINT        m_uBuffer;
        BYTE        m_bDataStatus;  // Status value for end of data, where the data CRC can be checked

        int         m_nMotorDelay;  // Delay before switching motor off

    protected:
        void ResetAll();
        void ModifyStatus (BYTE bEnable_, BYTE bReset_);
        void ModifyReadStatus ();

        bool IsMotorOn () const { return (m_sRegs.bStatus & MOTOR_ON) != 0; }
        void SetMotor (bool fOn_);
};

#endif
