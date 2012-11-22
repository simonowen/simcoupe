// Part of SimCoupe - A SAM Coupe emulator
//
// Tape.h: Tape handling
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

#ifndef TAPE_H
#define TAPE_H

class CTape;

class Tape
{
    public:
        static bool IsPlaying ();
        static bool IsInserted ();

        static const char* GetPath ();
#ifdef USE_LIBSPECTRUM
        static libspectrum_tape *GetTape ();
        static const char *GetBlockDetails (libspectrum_tape_block *block);
#endif

        static bool Insert (const char* pcsz_);
        static void Eject ();
        static void Play ();
        static void Stop ();

        static void NextEdge (DWORD dwTime_);
        static bool LoadTrap ();

        static bool EiHook ();
        static bool RetZHook ();
        static bool InFEHook ();
};

#endif
