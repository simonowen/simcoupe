; Inno Setup script for SimCoupe installer

#ifdef OFFICIAL_BUILD
#define BASE_PATH "..\build_x86"
#else
#define BASE_PATH "..\out\build\x86-Release"
#endif

#define MyAppName "SimCoupe"
#define MyAppExeName MyAppName + ".exe"
#define VerMajor
#define VerMinor
#define VerRev
#define VerBuild
#define FullVersion=GetVersionComponents(BASE_PATH + "\" + MyAppExeName, VerMajor, VerMinor, VerRev, VerBuild)
#define MyAppVersion Str(VerMajor) + "." + Str(VerMinor) + "." + Str(VerRev)
#define MyAppPublisher "Simon Owen"
#define MyAppURL "https://github.com/simonowen/simcoupe"
#define DateString GetDateTimeString('yyyymmdd', '', '')

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{A478D40A-E17B-4273-941E-76E15B6D4731}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={commonpf}\{#MyAppName}
DefaultGroupName={#MyAppName}
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
DisableProgramGroupPage=auto
ChangesAssociations=yes
OutputDir=..
OutputBaseFilename={#MyAppName}-{#MyAppVersion}-win
Compression=lzma
SolidCompression=yes
VersionInfoVersion={#MyAppVersion}
MinVersion=6.1sp1
#ifdef OFFICIAL_BUILD
SignTool=signtool $f
SignedUninstaller=yes
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
SetupAppTitle=Setup {#MyAppName}
SetupWindowTitle=Setup - {#MyAppName} v{#MyAppVersion}

[Files]
Source: "{#BASE_PATH}\SimCoupe.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BASE_PATH}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Resource\*.rom"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Resource\*.bin"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Resource\*.map"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\ReadMe.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Manual.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{commonprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{commonprograms}\{#MyAppName} Website"; Filename: "{#MyAppURL}"

[Registry]
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\{#MyAppExeName}"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName}"; Flags: uninsdeletekey

[Run]
Filename: "{app}\{#MyAppExeName}"; Flags: postinstall nowait
