// Replacement for the MFC afxres.h that Visual Studio forces even non-MFC projects to include

#define VS_VERSION_INFO     1

#ifndef WINVER
#define WINVER  0x0400
#endif

#include <winresrc.h>

#undef IDC_STATIC
#define IDC_STATIC          (-1)
