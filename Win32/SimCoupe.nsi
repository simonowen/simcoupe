!define VER_MAJOR 0
!define VER_MINOR 90
!define VER_BETA 14
!define VER_DISPLAY ${VER_MAJOR}.${VER_MINOR}

Name "SimCoupe"

InstallDir "$PROGRAMFILES\SimCoupe"
InstallDirRegKey HKLM "Software\SimCoupe" ""

InstType "Full"
InstType "Minimal"

SetCompressor lzma

!ifndef VER_BETA
OutFile "SimCoupe-${VER_MAJOR}.${VER_MINOR}.exe"
!else
OutFile "SimCoupe-${VER_MAJOR}.${VER_MINOR}-beta${VER_BETA}.exe"
!endif


!include "MUI.nsh"
!include "Sections.nsh"

!define MUI_ABORTWARNING
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_UI_SMALLDESCRIPTION

!define MUI_WELCOMEPAGE_TITLE "Welcome to the SimCoupe ${VER_DISPLAY} Setup Wizard"
!define MUI_WELCOMEPAGE_TEXT "This wizard will guide you through the installation of SimCoupe, the SAM Coupe emulator.\r\n\r\n$_CLICK"

!define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKLM"
!define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\SimCoupe"
!define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
!define MUI_STARTMENUPAGE_DEFAULTFOLDER "SimCoupe"

!define MUI_FINISHPAGE_LINK "Visit the SimCoupe website for the updates and support."
!define MUI_FINISHPAGE_LINK_LOCATION "http://www.simcoupe.org/"
!define MUI_FINISHPAGE_RUN "$INSTDIR\SimCoupe.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Run SimCoupe now"
!define MUI_FINISHPAGE_SHOWREADME $INSTDIR\ReadMe.txt
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!define MUI_FINISHPAGE_NOREBOOTSUPPORT


Var STARTMENU_FOLDER

; Installer Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "../LICENCE"
!insertmacro MUI_PAGE_COMPONENTS

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_STARTMENU SimCoupe $STARTMENU_FOLDER
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE un.InstallCheck
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"


; Sections

Section "SimCoupe Core Files (required)" SecCore
	SectionIn RO
	SetOutPath $INSTDIR
	RMDir /r $SMPROGRAMS\$STARTMENU_FOLDER

	File "Build\SimCoupe.exe"
	File "Build\SAASound.dll"
	File "Build\zlib1.dll"
	File "..\ReadMe.txt"

	; Store the installation directory
	WriteRegStr HKLM "Software\SimCoupe" "" $INSTDIR

	; Write the uninstall keys for Windows
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SimCoupe" "DisplayName" "SimCoupe"
	WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SimCoupe" "UninstallString" '"$INSTDIR\uninstall.exe"'
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SimCoupe" "NoModify" 1
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SimCoupe" "NoRepair" 1
	WriteUninstaller "uninstall.exe"

!insertmacro MUI_STARTMENU_WRITE_BEGIN SimCoupe
	CreateDirectory "$SMPROGRAMS\$STARTMENU_FOLDER"
	CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Uninstall SimCoupe.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
	CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\SimCoupe.lnk" "$INSTDIR\SimCoupe.exe" "" "$INSTDIR\SimCoupe.exe" 0
	WriteINIStr "$SMPROGRAMS\$STARTMENU_FOLDER\SimCoupe Homepage.url" "InternetShortcut" "URL" "http://www.simcoupe.org/"
!insertmacro MUI_STARTMENU_WRITE_END

SectionEnd


SubSection "Sample game disks" SecGames

Section "Manic Miner (demo)" SecManicMiner
	SectionIn 1
	SetOutPath $INSTDIR
	File "..\manicm.dsk"

!insertmacro MUI_STARTMENU_WRITE_BEGIN SimCoupe
	CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Play Manic Miner.lnk" "$INSTDIR\SimCoupe.exe" '-autoboot 1 -drive1 1 -disk1 "$INSTDIR\manicm.dsk"' "$INSTDIR\SimCoupe.exe" 1
!insertmacro MUI_STARTMENU_WRITE_END

SectionEnd

Section "Defender" SecDefender
	SectionIn 1
	SetOutPath $INSTDIR
	File "..\defender.dsk"

!insertmacro MUI_STARTMENU_WRITE_BEGIN SimCoupe
	CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Play Defender.lnk" "$INSTDIR\SimCoupe.exe" '-autoboot 1 -drive1 1 -disk1 "$INSTDIR\defender.dsk"' "$INSTDIR\SimCoupe.exe" 1
!insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

Section "Tetris" SecTetris
	SectionIn 1
	SetOutPath $INSTDIR
	File "..\tetris.dsk"

!insertmacro MUI_STARTMENU_WRITE_BEGIN SimCoupe
	CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Play Tetris.lnk" "$INSTDIR\SimCoupe.exe" '-autoboot 1 -drive1 1 -disk1 "$INSTDIR\tetris.dsk"' "$INSTDIR\SimCoupe.exe" 1
!insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

SubSectionEnd


SubSection "Sample demo disks" SecDemos

Section "MNEMOdemo 1" SecMneDemo1
	SectionIn 1
	SetOutPath $INSTDIR
	File "..\mnedemo1.dsk"

!insertmacro MUI_STARTMENU_WRITE_BEGIN SimCoupe
	CreateShortCut "$SMPROGRAMS\$STARTMENU_FOLDER\Run MNEMOdemo 1.lnk" "$INSTDIR\SimCoupe.exe" '-autoboot 1 -drive1 1 -disk1 "$INSTDIR\mnedemo1.dsk"' "$INSTDIR\SimCoupe.exe" 1
!insertmacro MUI_STARTMENU_WRITE_END
SectionEnd

SubSectionEnd


SubSection "File associations" SecAssocs

Section "Raw disks (.dsk)" SecDSK
	SectionIn 1
	Push ".dsk"
	Call InstallFileAssoc
SectionEnd

Section "SAM disks (.sad)" SecSAD
	SectionIn 1
	Push ".sad"
	Call InstallFileAssoc
SectionEnd

Section "Teledisk images (.td0)" SecTD0
	SectionIn 1
	Push ".td0"
	Call InstallFileAssoc
SectionEnd

SubSectionEnd


Section "Add desktop shortcut" SecDesktop
	SectionIn 1
	CreateShortCut "$DESKTOP\SimCoupe.lnk" "$INSTDIR\SimCoupe.exe" "" "$INSTDIR\SimCoupe.exe" 0
SectionEnd


!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${SecCore} "Core emulator program files"
	!insertmacro MUI_DESCRIPTION_TEXT ${SecGames} "Sample games to get you started"
	!insertmacro MUI_DESCRIPTION_TEXT ${SecManicMiner} "Manic Miner, written by Matthew Holt"
	!insertmacro MUI_DESCRIPTION_TEXT ${SecDefender} "Defender, written by Chris Pile"
	!insertmacro MUI_DESCRIPTION_TEXT ${SecTetris} "Tetris, written by David Gommeren"
	!insertmacro MUI_DESCRIPTION_TEXT ${SecDemos} "Sample demos to get you started"
	!insertmacro MUI_DESCRIPTION_TEXT ${SecMneDemo1} "MNEMOdemo 1, written by MNEMOtech"
	!insertmacro MUI_DESCRIPTION_TEXT ${SecAssocs} "Double-click support for disk image types"
	!insertmacro MUI_DESCRIPTION_TEXT ${SecDSK} "File association for .dsk images"
	!insertmacro MUI_DESCRIPTION_TEXT ${SecSAD} "File association for .sad images"
	!insertmacro MUI_DESCRIPTION_TEXT ${SecTD0} "File association for .td0 images"
	!insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "Adds a shortcut to start SimCoupe to the desktop"
!insertmacro MUI_FUNCTION_DESCRIPTION_END


Section "Uninstall"

	; Delete the main program files and the directory
	Delete $INSTDIR\SimCoupe.exe
	Delete $INSTDIR\SimCoupe.cfg
	Delete $INSTDIR\SAASound.dll
	Delete $INSTDIR\zlib1.dll
	Delete $INSTDIR\uninstall.exe
	Delete $INSTDIR\manicm.dsk
	Delete $INSTDIR\defender.dsk
	Delete $INSTDIR\tetris.dsk
	Delete $INSTDIR\mnedemo1.dsk
	Delete $INSTDIR\ReadMe.txt
	RMDir $INSTDIR

	; Delete start menu items and group
	!insertmacro MUI_STARTMENU_GETFOLDER SimCoupe $0
	Delete "$SMPROGRAMS\$0\*.*"
	Delete "$DESKTOP\SimCoupe.lnk"
	RMDir "$SMPROGRAMS\$0"

	; Delete the product keys and uninstall entry from Add/Remove Programs
	DeleteRegKey HKLM "Software\SimCoupe"
	DeleteRegKey /ifempty HKLM "Software\SimCoupe"
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SimCoupe"

	push ".dsk"
	Call un.InstallFileAssoc
	push ".sad"
	Call un.InstallFileAssoc
	push ".td0"
	Call un.InstallFileAssoc

SectionEnd


Function InstallFileAssoc
	Pop $0
	ReadRegStr $1 HKCR $0 ""
	StrCmp $1 "" +2
		WriteRegStr HKCR $0 "simcoupe_backup" $1
	WriteRegStr HKCR $0 "" "SimCoupe.Disk"

	WriteRegStr HKCR "SimCoupe.Disk" "" "SimCoupe Disk Image"
	WriteRegStr HKCR "SimCoupe.Disk\shell" "" "open"
	WriteRegStr HKCR "SimCoupe.Disk\DefaultIcon" "" "$INSTDIR\SimCoupe.exe,1"
	WriteRegStr HKCR "SimCoupe.Disk\shell\open\command" "" '"$INSTDIR\SimCoupe.exe" -autoboot 1 -drive1 1 -disk1 "%1"'

FunctionEnd


Function un.InstallFileAssoc
	Pop $0
	ReadRegStr $1 HKCR $0 "simcoupe_backup"
	DeleteRegValue HKCR $0 "simcoupe_backup"
	StrCmp $1 "" +2
		WriteRegStr HKCR $0 "" $1

	StrCmp $1 "" 0 +2
		DeleteRegKey HKCR $0

	DeleteRegKey HKCR "SimCoupe.Disk"

FunctionEnd


Function un.InstallCheck

	FindWindow $5 "SimCoupeClass"
	IntCmp $5 0 +3
		MessageBox "MB_OK" "Please close SimCoupe before uninstalling!"
		Abort

FunctionEnd
