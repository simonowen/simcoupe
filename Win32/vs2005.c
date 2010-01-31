// This is a compatibility helper for Visual Studio 2005 targetting x86
// Unfortunately this won't work with VS2008 due to linker restrictions
//
// The startup code for binaries built with VS2005 rely kernel32 functions
// (including IsDebuggerPresent) that are not available on 95/NT4.
// This prevents the program running on those platforms, for no good reason.
//
// As a workaround, this file includes a couple of compiler CRT source modules
// directly, with a dummy implementation of the problem function(s).  These
// override the linked in versions from the standard CRT.
//
// If you don't require 95/NT4 support simply remove this file from the project

// Applies to VS2005 and x86 target only
#if _MSC_VER >= 1400 && _MSC_VER < 1500 && (defined(_M_X86) || defined(_M_IX86))

#include <ntstatus.h>
#define WIN32_NO_STATUS
#include <windows.h>

#define IsDebuggerPresent _IsDebuggerPresent
BOOL WINAPI IsDebuggerPresent (VOID) { return FALSE; }

#define _CRTBLD
#define _PHNDLR

// The files below are part of the CRT source code, and require you add the following
// to the compiler include path:  $(DevEnvDir)\..\..\VC\crt\src
// To add this use:  Tools -> Options -> Projects and Solutions -> VC++ Directories
// Add for the Win32 platform under the Include Files section

#include <gs_report.c>	// Error including this?  See instructions above!
#include <internal.h>

// Comment out the rogue  extern "C"  on the line it complains about in the file below
// You can then also comment out this warning message :-)
//#pragma message("*** Comment out  extern \"C\"  if you get a 'string' error above _initp_misc_invarg() in the file below ***")
#include <invarg.c>

#endif
