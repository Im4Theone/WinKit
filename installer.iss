; WinKit installer script (Inno Setup 6).  Builds WinKitSetup-<version>.exe from the single WinKit.exe.
; release.bat runs this for you:  ISCC.exe /DMyAppVersion=2.0.0 installer.iss
;
; Installs to Program Files. An installed copy self-updates by downloading and silently re-running a newer
; WinKitSetup-<version>.exe, which is why "/VERYSILENT /NORESTART /launchafterinstall=1" is honoured below.
; Same AppId as the .NET releases of WinKit, so installing 2.0 over 1.x is a clean upgrade (one entry in
; Programs & Features); the old .NET runtime files are removed by [InstallDelete].

#define MyAppName "WinKit"
#ifndef MyAppVersion
  #define MyAppVersion "2.0.0"
#endif
#define MyAppPublisher "WinKit"
#define MyAppURL "https://github.com/Im4Theone/WinKit"
#define MyAppExeName "WinKit.exe"

[Setup]
; Fixed, stable GUID - never change it between versions.
AppId={{9F2C6C0A-6D0B-4E9F-9E6C-2B7F6E5F9C41}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
VersionInfoVersion={#MyAppVersion}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
OutputDir=release
OutputBaseFilename=WinKitSetup-{#MyAppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupIconFile=winkit.ico
; WinKit itself runs as the normal user and elevates single actions on demand; only Setup (Program Files + HKLM) is elevated.
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
; Self-updates start a newer installer while the old WinKit.exe is still exiting: let Restart Manager close it.
CloseApplications=yes
RestartApplications=no
UsePreviousAppDir=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[InstallDelete]
; leftovers of the .NET (1.x) build: runtime DLLs, config files, the separate updater
Type: files; Name: "{app}\*.dll"
Type: files; Name: "{app}\*.deps.json"
Type: files; Name: "{app}\*.runtimeconfig.json"
Type: files; Name: "{app}\*.pdb"
Type: files; Name: "{app}\WinKit.Updater.exe"

[Files]
Source: "WinKit.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Run]
; interactive install: the usual "Launch WinKit" checkbox
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
; silent self-update: WinKit passes /launchafterinstall=1 and expects to be reopened
Filename: "{app}\{#MyAppExeName}"; Flags: nowait; Check: ShouldLaunchAfterSilentInstall

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
function ShouldLaunchAfterSilentInstall(): Boolean;
begin
  Result := WizardSilent() and (ExpandConstant('{param:launchafterinstall|0}') = '1');
end;
