// Part of SimCoupe - A SAM Coupé emulator
//
// SimCoupe.h: Common SimCoupe header, included by all modules
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

#ifndef SIMCOUPE_H
#define SIMCOUPE_H

//#define NO_ZLIB           // Define this if Zlib is not available
//#define DUMMY_SAASOUND    // Define this if the real SAASound library is not available


// If it's not one of these we'll assume big endian (we have a run-time check to fall back on anyway)
#if !defined(__i386__) && !defined(WIN32) && !(defined(__alpha__) && !defined(__alpha)) && !defined(__arm__) && !(defined(__mips__) && !defined(__MIPSEL__))
#define __BIG_ENDIAN__
#endif

#ifdef DEBUG
#define _DEBUG
#endif

typedef unsigned long       DWORD;
typedef unsigned short      WORD;
typedef unsigned char       BYTE;
typedef unsigned int        UINT;


#include "OSD.h"            // OS-dependant stuff

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>

// Pull in just the STL namespace elements we need
#include <algorithm>
using std::pair;
#include <string>
using std::string;
using std::swap;
#include <vector>
using std::vector;
#include <list>
using std::list;
using std::sort;

// Windows CE lacks some headers
#ifndef _WIN32_WCE
#include <time.h>
#include <sys/stat.h>
#endif

#ifndef NO_ZLIB
#include "../Extern/unzip.h"    // for unzOpen, unzClose, etc.  Part of the contrib/minizip in the ZLib source package
#include "zlib.h"           // for gzopen, gzclose, etc.
#endif

#include "SAM.h"            // Various SAM constants
#include "Util.h"


#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX_PATH
#define MAX_PATH            260
#endif

#endif  // SIMCOUPE_H
