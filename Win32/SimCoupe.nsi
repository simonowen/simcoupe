; Part of SimCoupe - A SAM Coupé emulator
;
; SimCoupe.nsi: GUI installer for SimCoupe Win32
;
;  Copyright (c) 2001-2002  Dave Laundon
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

; ToDo:
;  - Re-work file associations so SimCoupe is added to, not obliterates, any
;    existing associations.

; Define in NSIS command line as required
!ifdef NOCOMPRESS
  SetCompress off
  SetDatablockOptimize off
!else
  SetCompress auto
  SetDatablockOptimize on
  !ifdef UPX
    !packhdr tmp.dat '"${UPX}" --best -q --compress-resources=0 tmp.dat'
  !endif
!endif

CRCCheck on
SetDateSave on
SilentInstall normal

; Header configuration commands
Name 'SimCoupé'
Icon 'Icons\inst.ico'
Caption 'SimCoupé 0.90 beta 5 for Win32'
OutFile 'Build\SimCoupe-0.90-beta-5-Win32.exe'

; License page configuration commands
LicenseText 'Please read and agree to this licence before continuing.'
LicenseData '..\license.txt'
SubCaption 0 ": Licence Agreement"

; Directory selection page configuration commands
InstallDir '$PROGRAMFILES\SimCoupe'
InstallDirRegKey HKLM 'Software\SimCoupe' 'InstallPath-Win32'
DirShow show
DirText 'Please select the folder you would like SimCoupé installed to, then click Next.'

; Component page configuration commands
ComponentText 'This will install or update SimCoupé for Win32.  Please select the type of installation or the components to install.'
EnabledBitmap 'Icons\tickyes.bmp'
DisabledBitmap 'Icons\tickno.bmp'
InstType 'Full'    ; 1
InstType 'Minimal' ; 2

; Installation commands
AutoCloseWindow false
ShowInstDetails show
InstallColors /windows

; Called before the installation proper begins
Function .onInit
  MessageBox MB_ICONQUESTION|MB_YESNO \
             'This will install SimCoupé for Win32.$\n$\nDo you wish to continue?' \
             IDYES InitContinue
  Abort
InitContinue:
FunctionEnd

; Main program
Section 'SimCoupé for Win32 (required)'
  SectionIn 1 2
  SetOutPath $INSTDIR
  SetOverwrite on           ; Mmm, possible one might want to install a previous version?
  File 'Build\SimCoupe.exe'
  File '..\ChangeLog.txt'
  File '..\ReadMe.txt'
  SetOverwrite ifnewer
  File 'Build\ZLib.dll'
  File 'Build\SAASound.dll'
  File '..\SAASound.txt'
  File '..\License.txt'
  SetOverwrite off
  File '..\SamDemo1.dsk'    ; :)
  File '..\SamDemo2.dsk'    ; :)
  IfFileExists '$INSTDIR\SimCoupe Home Page.url' +2
  WriteIniStr '$INSTDIR\SimCoupe Home Page.url' \
              "InternetShortcut" "URL" 'http://www.simcoupe.org/'
  WriteUninstaller "uninst-SimCoupe.exe"
SectionEnd

; Start Menu
Section 'Start Menu shortcuts'
  SectionIn 1
  SetOutPath $INSTDIR
  CreateDirectory '$SMPROGRAMS\SimCoupé'
  CreateShortCut '$SMPROGRAMS\SimCoupé\SimCoupé.lnk' \
                 '$INSTDIR\SimCoupe.exe'
  CreateShortCut '$SMPROGRAMS\SimCoupé\SimCoupé Help.lnk' \
                 '$INSTDIR\ReadMe.txt'
  CreateShortCut '$SMPROGRAMS\SimCoupé\SimCoupé Home Page.lnk' \
                 '$INSTDIR\SimCoupe Home Page.url'
  CreateShortCut '$SMPROGRAMS\SimCoupé\Original SAM Coupé Demo Disk.lnk' \
                 '$INSTDIR\SimCoupe.exe' '-disk1 "$INSTDIR\SamDemo1.dsk" -autoboot 1'
  CreateShortCut '$SMPROGRAMS\SimCoupé\Second SAM Coupé Demo Disk.lnk' \
                 '$INSTDIR\SimCoupe.exe' '-disk1 "$INSTDIR\SamDemo2.dsk" -autoboot 1'
SectionEnd

; Desktop
Section 'Desktop shortcuts'
  SectionIn 1
  SetOutPath $INSTDIR
  CreateShortCut '$DESKTOP\SimCoupé.lnk' \
                 '$INSTDIR\SimCoupe.exe'
SectionEnd

; File associations
Section 'Associate SimCoupé with Disk Image files'
  SectionIn 1
  ; Do the processing for this outside of this section
SectionEnd

; Helper macro
!macro IfSectionChecked sec yes no
  SectionGetFlags "${sec}" $0
  IntCmp $0 0 "${no}" "${yes}" "${no}"
!macroend

Section
  ; Save installation path
  WriteRegStr HKLM 'Software\SimCoupe' 'InstallPath-Win32' $INSTDIR
  ; Setup file associations?
  !insertmacro IfSectionChecked 3 InstallAssoc ""
  ; "Uninstall" associations
  StrCpy $0 '.dsk'
  Call UnsetAssoc
  StrCpy $0 '.sad'
  Call UnsetAssoc
  StrCpy $0 '.sdf'
  Call UnsetAssoc
  Goto DoneAssoc
InstallAssoc:
  StrCpy $0 '.dsk'
  Call SetAssoc
  StrCpy $0 '.sad'
  Call SetAssoc
  StrCpy $0 '.sdf'
  Call SetAssoc
  WriteRegStr HKCR 'SimCoupe.DiskImage' '' 'SimCoupé Disk Image'
  WriteRegStr HKCR 'SimCoupe.DiskImage\DefaultIcon' '' $INSTDIR\SimCoupe.exe,1
  WriteRegStr HKCR 'SimCoupe.DiskImage\shell' '' 'open'
  WriteRegStr HKCR 'SimCoupe.DiskImage\shell\open' '' 'Open with SimCoupé'
  WriteRegStr HKCR 'SimCoupe.DiskImage\shell\open\command' '' '"$INSTDIR\SimCoupe.exe" -disk1 "%1" -autoboot 1'
  WriteRegStr HKCR 'SimCoupe.DiskImage\shell\open.no.boot' '' 'Open with SimCoupé (no boot)'
  WriteRegStr HKCR 'SimCoupe.DiskImage\shell\open.no.boot\command' '' '"$INSTDIR\SimCoupe.exe" -disk1 "%1" -autoboot 0'
DoneAssoc:
  ; Uninstaller registry details
  WriteRegStr HKLM 'Software\Microsoft\Windows\CurrentVersion\Uninstall\SimCoupe' \
              'DisplayName' 'SimCoupé for Win32'
  WriteRegStr HKLM 'Software\Microsoft\Windows\CurrentVersion\Uninstall\SimCoupe' \
              'HelpLink' 'http://www.simcoupe.org/'
  WriteRegStr HKLM 'Software\Microsoft\Windows\CurrentVersion\Uninstall\SimCoupe' \
              'UninstallString' '"$INSTDIR\uninst-SimCoupe.exe"'
SectionEnd

; Called after a successful installation
Function .onInstSuccess
  ExecShell open '$INSTDIR\ChangeLog.txt'
FunctionEnd

; Add our associations to file extension $0
Function SetAssoc
  ; Backup old value
  ReadRegStr $1 HKCR $0 ''
  StrCmp $1 '' NoBackup
  StrCmp $1 'SimCoupe.DiskImage' NoBackup
  WriteRegStr HKCR $0 'Pre_SimCoupe' $1
NoBackup:
  WriteRegStr HKCR $0 '' 'SimCoupe.DiskImage'
FunctionEnd

; Remove our associations from file extension $0
Function UnsetAssoc
  ReadRegStr $1 HKCR $0 ''
  StrCmp $1 'SimCoupe.DiskImage' 0 NoOwn ; only do this if we own it
  ReadRegStr $1 HKCR $0 'Pre_SimCoupe'
  StrCmp $1 '' 0 RestoreBackup ; if backup == '' then delete the whole key
  DeleteRegKey HKCR $0
  Goto NoOwn
RestoreBackup:
  WriteRegStr HKCR $0 '' $1
  DeleteRegValue HKCR $0 'Pre_SimCoupe'
NoOwn:
FunctionEnd

; Carbon copy for use by uninstaller (Installer can't see Uninstaller functions and vice-versa)
Function un.UnsetAssoc
  ReadRegStr $1 HKCR $0 ''
  StrCmp $1 'SimCoupe.DiskImage' 0 unNoOwn ; only do this if we own it
  ReadRegStr $1 HKCR $0 'Pre_SimCoupe'
  StrCmp $1 '' 0 unRestoreBackup ; if backup == '' then delete the whole key
  DeleteRegKey HKCR $0
  Goto unNoOwn
unRestoreBackup:
  WriteRegStr HKCR $0 '' $1
  DeleteRegValue HKCR $0 'Pre_SimCoupe'
unNoOwn:
FunctionEnd

; Uninstaller
UninstallText 'This will uninstall SimCoupé from your system:'
UninstallIcon 'Icons\uninst.ico'

Section Uninstall
  ; "Uninstall" associations
  StrCpy $0 '.dsk'
  Call un.UnsetAssoc
  StrCpy $0 '.sad'
  Call un.UnsetAssoc
  StrCpy $0 '.sdf'
  Call un.UnsetAssoc
  DeleteRegKey HKCR 'SimCoupe.DiskImage'
  ; Remove other registry details
  DeleteRegKey HKLM 'Software\Microsoft\Windows\CurrentVersion\Uninstall\SimCoupe'
  DeleteRegKey HKLM 'Software\SimCoupe'
  ; Remove Start Menu shortcuts
  Delete '$SMPROGRAMS\SimCoupé\SimCoupé.lnk'
  Delete '$SMPROGRAMS\SimCoupé\SimCoupé Help.lnk'
  Delete '$SMPROGRAMS\SimCoupé\SimCoupé Home Page.lnk'
  Delete '$SMPROGRAMS\SimCoupé\Original SAM Coupé Demo Disk.lnk'
  Delete '$SMPROGRAMS\SimCoupé\Second SAM Coupé Demo Disk.lnk'
  RmDir '$SMPROGRAMS\SimCoupé'
  ; Remove Desktop shortcuts
  Delete '$DESKTOP\SimCoupé.lnk'
  ; Remove installed files
  Delete '$INSTDIR\SimCoupe.exe'
  Delete '$INSTDIR\ChangeLog.txt'
  Delete '$INSTDIR\SimCoupe.txt'  ; Existed in earlier version of the installer
  Delete '$INSTDIR\ReadMe.txt'
  Delete '$INSTDIR\ZLib.dll'
  Delete '$INSTDIR\SAASound.dll'
  Delete '$INSTDIR\SAASound.txt'
  Delete '$INSTDIR\License.txt'
  Delete '$INSTDIR\SAM_ROM0.ROM'
  Delete '$INSTDIR\SAM_ROM1.ROM'  ; Existed in earlier version of the installer
  Delete '$INSTDIR\SamDemo1.dsk'
  Delete '$INSTDIR\SamDemo2.dsk'
  Delete '$INSTDIR\SimCoupe Home Page.url'
  Delete '$INSTDIR\uninst-SimCoupe.exe'
  ; Remove expected extra files
  Delete '$INSTDIR\SimCoupe.cfg'
  RmDir $INSTDIR
  ; If $INSTDIR still exists then other files must be present
  IfFileExists $INSTDIR 0 Removed
  MessageBox MB_YESNO|MB_ICONQUESTION "\
Your SimCoupé folder contains files that were not part of the original installation.$\r$\n\
Do you wish to delete these files and remove the folder from your system?" IDNO Removed
  RmDir /r $INSTDIR
  IfFileExists $INSTDIR '' Removed
  MessageBox MB_OK|MB_ICONEXCLAMATION \
             'Note: $INSTDIR could not be removed.'
Removed:
SectionEnd
