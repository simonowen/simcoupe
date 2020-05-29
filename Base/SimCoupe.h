/* Part of SimCoupe - A SAM Coupe emulator
//
// SimCoupe.h: Common SimCoupe header, included by all modules
//
//  Copyright (c) 1999-2015 Simon Owen
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
*/

#pragma once

#include "config.h"

#ifdef __cplusplus

/* If it's not one of these we'll assume big endian (we have a run-time check to fall back on anyway) */
#if (defined(__LITTLE_ENDIAN__) || defined(__i386__) || defined(__ia64__) || defined(__x86_64__) || \
    (defined(__alpha__) || defined(__alpha)) || (defined(__mips__) && defined(__MIPSEL__)) || \
     defined(__arm__) || defined(__SYMBIAN32__) || defined(_WIN32_WCE) || defined(_WIN32)) \
     && !defined(__BIG_ENDIAN__)
#ifndef __LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__
#endif
#else
#ifndef __BIG_ENDIAN__
#define __BIG_ENDIAN__
#endif
#endif

#if defined(DEBUG) && !defined(_DEBUG)
#define _DEBUG
#endif

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#define NOMINMAX    // no min/max macros from windef.h
#endif

typedef unsigned int        UINT;
typedef unsigned long       ULONG;

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>

#include <string>
#include <map>
#include <algorithm>
#include <queue>
#include <stack>
#include <sstream>

#ifdef HAVE_STD_FILESYSTEM
#include <filesystem>
namespace fs = std::filesystem;
#else
#include "filesystem.hpp"
namespace fs = ghc::filesystem;
#endif

#include "OSD.h"        /* OS-dependent stuff */
#include "SAM.h"        /* Various SAM constants */
#include "Util.h"       /* TRACE macro and other utility functions */

#ifdef HAVE_LIBSPECTRUM
#include "libspectrum.h"
#endif

#ifdef HAVE_LIBZ
#include "unzip.h"       /* for unzOpen, unzClose, etc.  Part of the contrib/minizip in the ZLib source package */
#include "zlib.h"        /* for gzopen, gzclose, etc. */
#endif

#endif  /* __cplusplus */
