# Microsoft Developer Studio Project File - Name="SimCoupe" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=SimCoupe - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SimCoupe.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SimCoupe.mak" CFG="SimCoupe - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SimCoupe - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "SimCoupe - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SimCoupe - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /W3 /GX /Ot /Ow /Og /Oi /Oy /Ob2 /I "." /I ".." /I "..\Base" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "USE_SAASOUND" /D "USE_ZLIB" /FR /YX"SimCoupe.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "NDEBUG"
# ADD RSC /l 0x809 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 winmm.lib comctl32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ddraw.lib dsound.lib dinput.lib dxguid.lib /nologo /subsystem:windows /profile /map /machine:I386 /out:"Build/SimCoupe.exe"

!ELSEIF  "$(CFG)" == "SimCoupe - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /Gi /GX /Zi /Od /Oy /Gy /I "." /I ".." /I "..\Base" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "USE_SAASOUND" /D "USE_ZLIB" /FR /YX"SimCoupe.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x809 /d "_DEBUG"
# ADD RSC /l 0x809 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 winmm.lib comctl32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ddraw.lib dsound.lib dinput.lib dxguid.lib /nologo /subsystem:windows /map /debug /machine:I386 /out:"Build/SimCoupeD.exe"
# SUBTRACT LINK32 /profile /pdb:none /incremental:no

!ENDIF 

# Begin Target

# Name "SimCoupe - Win32 Release"
# Name "SimCoupe - Win32 Debug"
# Begin Group "Base Source Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Base\ATA.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\Atom.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\CDisk.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\CDrive.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\Clock.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\CPU.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\CScreen.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\CStream.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\Debug.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\Disassem.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\Expr.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\Font.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\Frame.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\GUI.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\GUIDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\GUIIcons.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\IO.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\Main.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\Memory.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\Mouse.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\Options.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\PNG.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\Profile.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\SDIDE.cpp
# End Source File
# Begin Source File

SOURCE=..\Base\Util.cpp
# End Source File
# End Group
# Begin Group "Base Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\Base\ATA.h
# End Source File
# Begin Source File

SOURCE=..\Base\Atom.h
# End Source File
# Begin Source File

SOURCE=..\Base\CBops.h
# End Source File
# Begin Source File

SOURCE=..\Base\CDisk.h
# End Source File
# Begin Source File

SOURCE=..\Base\CDrive.h
# End Source File
# Begin Source File

SOURCE=..\Base\Clock.h
# End Source File
# Begin Source File

SOURCE=..\Base\CPU.h
# End Source File
# Begin Source File

SOURCE=..\Base\CScreen.h
# End Source File
# Begin Source File

SOURCE=..\Base\CStream.h
# End Source File
# Begin Source File

SOURCE=..\Base\Debug.h
# End Source File
# Begin Source File

SOURCE=..\Base\Disassem.h
# End Source File
# Begin Source File

SOURCE=..\Base\EDops.h
# End Source File
# Begin Source File

SOURCE=..\Base\Expr.h
# End Source File
# Begin Source File

SOURCE=..\Base\Font.h
# End Source File
# Begin Source File

SOURCE=..\Base\Frame.h
# End Source File
# Begin Source File

SOURCE=..\Base\GUI.h
# End Source File
# Begin Source File

SOURCE=..\Base\GUIDlg.h
# End Source File
# Begin Source File

SOURCE=..\Base\GUIIcons.h
# End Source File
# Begin Source File

SOURCE=..\Base\IO.h
# End Source File
# Begin Source File

SOURCE=..\Base\Main.h
# End Source File
# Begin Source File

SOURCE=..\Base\Memory.h
# End Source File
# Begin Source File

SOURCE=..\Base\Mouse.h
# End Source File
# Begin Source File

SOURCE=..\Base\Options.h
# End Source File
# Begin Source File

SOURCE=..\Base\PNG.h
# End Source File
# Begin Source File

SOURCE=..\Base\Profile.h
# End Source File
# Begin Source File

SOURCE=..\Base\SAM.h
# End Source File
# Begin Source File

SOURCE=..\Base\SAMROM.h
# End Source File
# Begin Source File

SOURCE=..\Base\SDIDE.h
# End Source File
# Begin Source File

SOURCE=..\Base\SimCoupe.h
# End Source File
# Begin Source File

SOURCE=..\Base\Util.h
# End Source File
# Begin Source File

SOURCE=..\Base\VL1772.h
# End Source File
# Begin Source File

SOURCE=..\Base\Z80ops.h
# End Source File
# End Group
# Begin Group "Win32 Source Files"

# PROP Default_Filter ".cpp"
# Begin Source File

SOURCE=.\Display.cpp
# End Source File
# Begin Source File

SOURCE=.\Floppy.cpp
# End Source File
# Begin Source File

SOURCE=.\Input.cpp
# End Source File
# Begin Source File

SOURCE=.\MIDI.cpp
# End Source File
# Begin Source File

SOURCE=.\OSD.cpp
# End Source File
# Begin Source File

SOURCE=.\Parallel.cpp
# End Source File
# Begin Source File

SOURCE=.\Serial.cpp
# End Source File
# Begin Source File

SOURCE=.\Sound.cpp
# End Source File
# Begin Source File

SOURCE=.\UI.cpp
# End Source File
# Begin Source File

SOURCE=.\Video.cpp
# End Source File
# End Group
# Begin Group "Win32 Header Files"

# PROP Default_Filter ".h"
# Begin Source File

SOURCE=.\Display.h
# End Source File
# Begin Source File

SOURCE=.\Floppy.h
# End Source File
# Begin Source File

SOURCE=.\Input.h
# End Source File
# Begin Source File

SOURCE=.\MIDI.h
# End Source File
# Begin Source File

SOURCE=.\OSD.h
# End Source File
# Begin Source File

SOURCE=.\Parallel.h
# End Source File
# Begin Source File

SOURCE=.\Serial.h
# End Source File
# Begin Source File

SOURCE=.\Sound.h
# End Source File
# Begin Source File

SOURCE=.\UI.h
# End Source File
# Begin Source File

SOURCE=.\Video.h
# End Source File
# End Group
# Begin Group "Win32 Resources"

# PROP Default_Filter ""
# Begin Group "Icons"

# PROP Default_Filter "*.ico"
# Begin Source File

SOURCE=.\Icons\clock.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\dave.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\display.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\floppy.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\fnkeys.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\hardware.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\joystick.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\keyboard.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\main.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\memory.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\midi.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\misc.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\mouse.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\network.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\port.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\printer.ico
# End Source File
# Begin Source File

SOURCE=.\Icons\sound.ico
# End Source File
# End Group
# Begin Group "Cursors"

# PROP Default_Filter "*.cur"
# Begin Source File

SOURCE=.\Cursors\arrow.cur
# End Source File
# End Group
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\SimCoupe.exe.manifest
# End Source File
# Begin Source File

SOURCE=.\SimCoupe.rc
# End Source File
# End Group
# Begin Group "External Misc"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\Extern\SAASound.cpp
# End Source File
# Begin Source File

SOURCE=..\Extern\SAASound.h
# End Source File
# Begin Source File

SOURCE=..\Extern\unzip.c
# End Source File
# Begin Source File

SOURCE=..\Extern\unzip.h
# End Source File
# End Group
# End Target
# End Project
