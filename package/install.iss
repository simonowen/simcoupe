; Inno Setup install script for SimCoupe
;
; Note: ensure both x86 and x64 Release versions are built before running!

#define X86_AND_X64

#ifdef CUSTOM_BUILD
#define BASE_PATH_X86 "..\build-x86"
#define BASE_PATH_X64 "..\build-x64"
#else
#define BASE_PATH_X86 "..\out\build\x86-Release"
#define BASE_PATH_X64 "..\out\build\x64-Release"
#endif

#ifexist "..\Demos\nul"
#define INSTALL_DEMOS
#endif

#define MyAppName "SimCoupe"
#define MyAppExeName MyAppName + ".exe"
#define VerMajor
#define VerMinor
#define VerRev
#define VerBuild
#define FullVersion=GetVersionComponents(BASE_PATH_X86 + "\" + MyAppExeName, VerMajor, VerMinor, VerRev, VerBuild)
#define MyAppVersion Str(VerMajor) + "." + Str(VerMinor) + "." + Str(VerRev)
#define MyAppPublisher "Simon Owen"
#define MyAppURL "https://simonowen.com/simcoupe/"
#define MyAppProjectURL "https://github.com/simonowen/simcoupe"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{A478D40A-E17B-4273-941E-76E15B6D4731}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppProjectURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={commonpf}\{#MyAppName}
DefaultGroupName={#MyAppName}
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
DisableProgramGroupPage=auto
UsedUserAreasWarning=no
ShowComponentSizes=no
ChangesAssociations=yes
OutputDir=..
OutputBaseFilename={#MyAppName}-{#MyAppVersion}-win
Compression=lzma
SolidCompression=yes
VersionInfoVersion={#MyAppVersion}
MinVersion=6.1sp1

#ifdef X86_AND_X64
ArchitecturesInstallIn64BitMode=x64
#endif

#ifdef SIGN_BUILD
SignTool=signtool $f
SignedUninstaller=yes
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
SetupAppTitle=Setup {#MyAppName}
SetupWindowTitle=Setup - {#MyAppName} v{#MyAppVersion}

[Types]
Name: "full"; Description: "Full installation"
Name: "compact"; Description: "Basic installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Tasks]
Name: startmenu; Description: Add shortcuts to Start menu
Name: desktop; Description: Add shortcut to desktop
Name: assoc; Description: Associate file types:
Name: assoc/mgt; Description: .mgt
Name: assoc/dsk; Description: .dsk
Name: assoc/sad; Description: .sad

[Components]
Name: main; Description: Core emulator files; Types: full compact custom; Flags: fixed
#ifdef INSTALL_DEMOS
Name: demos; Description: Demo disks; Types: full
#endif

[Files]
Source: "{#BASE_PATH_X86}\SimCoupe.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: main; Check: not Is64BitInstallMode
Source: "{#BASE_PATH_X86}\*.dll"; DestDir: "{app}"; Flags: ignoreversion; Components: main; Check: not Is64BitInstallMode
Source: "{#BASE_PATH_X64}\SimCoupe.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: main; Check: Is64BitInstallMode
Source: "{#BASE_PATH_X64}\*.dll"; DestDir: "{app}"; Flags: ignoreversion; Components: main; Check: Is64BitInstallMode
Source: "..\Resource\*.rom"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "..\Resource\*.bin"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "..\Resource\*.map"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "..\Resource\*.sbt"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "..\ReadMe.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Manual.md"; DestDir: "{app}"; Flags: ignoreversion

#ifdef INSTALL_DEMOS
Source: "..\Demos\*.mgt"; DestDir: "{app}\Demos"; Flags: ignoreversion; Components: demos
#endif

[Icons]
Name: "{commonprograms}\{#MyAppName}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Components: main; Tasks: startmenu
Name: "{userdesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Components: main; Tasks: desktop
Name: "{commonprograms}\{#MyAppName}\{#MyAppName} Website"; Filename: "{#MyAppURL}"; Tasks: startmenu
Name: "{commonprograms}\{#MyAppName}\{#MyAppName} Project"; Filename: "{#MyAppProjectURL}"; Tasks: startmenu

#ifexist "..\Demos\manicminer.mgt"
Name: "{commonprograms}\{#MyAppName}\Manic Miner"; Filename: "{app}\{#MyAppExeName}"; IconIndex: 1; Parameters: """{app}\Demos\manicminer.mgt"""; Components: demos; Tasks: startmenu
#endif
#ifexist "..\Demos\mnedemo1.mgt"
Name: "{commonprograms}\{#MyAppName}\MNEMOdemo"; Filename: "{app}\{#MyAppExeName}"; IconIndex: 1; Parameters: """{app}\Demos\mnedemo1.mgt"""; Components: demos; Tasks: startmenu
#endif

[Registry]
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\{#MyAppExeName}"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName}"; Flags: uninsdeletekey

Root: HKCR; Subkey: "{#MyAppName}.Disk"; ValueType: string; ValueName: ""; ValueData: "{#MyAppName} Disk Image"; Flags: uninsdeletekey; Tasks: assoc
Root: HKCR; Subkey: "{#MyAppName}.Disk\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"",1"; Flags: uninsdeletevalue; Tasks: assoc
Root: HKCR; Subkey: "{#MyAppName}.Disk\shell"; ValueType: string; ValueName: ""; ValueData: "boot"; Flags: uninsdeletevalue; Tasks: assoc
Root: HKCR; Subkey: "{#MyAppName}.Disk\shell\boot"; ValueType: string; ValueName: ""; ValueData: "Boot with {#MyAppName}"; Flags: uninsdeletevalue; Tasks: assoc
Root: HKCR; Subkey: "{#MyAppName}.Disk\shell\boot\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletevalue; Tasks: assoc
Root: HKCR; Subkey: "{#MyAppName}.Disk\shell\open"; ValueType: string; ValueName: ""; ValueData: "Open with {#MyAppName}"; Flags: uninsdeletevalue; Tasks: assoc
Root: HKCR; Subkey: "{#MyAppName}.Disk\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"" -autoboot 0"; Flags: uninsdeletevalue; Tasks: assoc

Root: HKCR; Subkey: ".mgt"; ValueType: string; ValueName: ""; ValueData: "{#MyAppName}.Disk"; Flags: uninsdeletevalue; Tasks: assoc/mgt
Root: HKCR; Subkey: ".dsk"; ValueType: string; ValueName: ""; ValueData: "{#MyAppName}.Disk"; Flags: uninsdeletevalue; Tasks: assoc/dsk
Root: HKCR; Subkey: ".sad"; ValueType: string; ValueName: ""; ValueData: "{#MyAppName}.Disk"; Flags: uninsdeletevalue; Tasks: assoc/sad

[Run]
Filename: "{app}\{#MyAppExeName}"; Flags: postinstall nowait
