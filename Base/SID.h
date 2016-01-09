// Part of SimCoupe - A SAM Coupe emulator
//
// SID.h: SID interface implementation using reSID library
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
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef SID_H
#define SID_H

#include "Sound.h"

#ifdef USE_RESID
#undef F // TODO: limit scope of Z80 registers!

#ifndef RESID_NAMESPACE
#define RESID_NAMESPACE     // default to root namespace
#endif

#include <resid/sid.h>
#define SID_CLOCK_PAL   985248
#endif

class CSID final : public CSoundDevice
{
    public:
        CSID ();
        CSID (const CSID &) = delete;
        void operator= (const CSID &) = delete;
        ~CSID ();

    public:
        void Reset () override;
        void Update (bool fFrameEnd_);
        void FrameEnd () override;

        void Out (WORD wPort_, BYTE bVal_) override;

    protected:
#ifdef USE_RESID
        RESID_NAMESPACE::SID *m_pSID = nullptr;
#endif
        int m_nChipType = 0;
};

extern CSID *pSID;

#endif // SID_H
