// Part of SimCoupe - A SAM Coupé emulator
//
// Util.h: Logging, tracing, and other utility tasks
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

#ifndef UTIL_H
#define UTIL_H

class Util
{
    public:
        static bool Init ();
        static void Exit ();
};


enum eMsgType { msgInfo, msgWarning, msgError, msgFatal };
void Message (eMsgType eType_, const char* pcszFormat_, ...);

bool OpenLog ();
void CloseLog ();

void TraceOutputString (const char *pcszFormat, ...);
void TraceOutputString (const BYTE *pcb_, size_t uLen_=0);

#ifdef _DEBUG
#define TRACE ::TraceOutputString
#else
void TraceOutputString (const char *, ...);
#define TRACE 1 ? (void)0 : ::TraceOutputString
#endif

#endif
