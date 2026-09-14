; WinKit installer script (Inno Setup 6).
;
; Builds WinKitSetup-<version>.exe from the self-contained publish output of
; WinKit.UI. Run installer\Publish-Installer.ps1 first (it publishes the
; files this script packages and then invokes ISCC on this script), or
; publish manually and run:
;   ISCC.exe /DMyAppVersion=1.2.0 installer\WinKit.iss
;
; Installs to Program Files, which is how WinKit.Infrastructure's
; UpdateService.IsRunningFromInstalledLocation() tells an installed copy
; apart from a portable one - an installed copy self-updates by downloading
; and silently re-running a newer WinKitSetup-<version>.exe (see
; UpdateService.ApplyViaInstallerAsync), which is why the /VERYSILENT
; /NORESTART /launchafterinstall=1 command line below is honored.

#define MyAppName "WinKit"
#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#define MyAppPublisher "WinKit"
#define MyAppURL "https://github.com/Im4Theone/WinKit"
#define MyAppExeName "WinKit.exe"
#define MyPublishDir "..\src\WinKit.UI\bin\Release\net9.0-windows\win-x64\publish"
#define MyIconFile "..\src\WinKit.UI\Assets\Icon\WinKit.ico"

[Setup]
; Fixed, stable GUID - do not change between versions. This is what lets
; Inno Setup recognize "install over an existing WinKit" as an upgrade
; (one Programs & Features entry, old files cleanly replaced) rather than a
; separate, parallel installation.
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
OutputDir=.\output
OutputBaseFilename=WinKitSetup-{#MyAppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupIconFile={#MyIconFile}
; WinKit itself always requests "asInvoker" (see app.manifest) and elevates
; individual operations on demand; only Setup, which writes to Program
; Files and HKLM, needs to run elevated.
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
; Self-updates relaunch a newer installer while the running WinKit.exe is
; still exiting; let Setup use Restart Manager to wait for/close it instead
; of failing to overwrite a locked file mid-update.
CloseApplications=yes
RestartApplications=no
UsePreviousAppDir=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#MyPublishDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"

[Run]
; Interactive installs: standard "Launch WinKit" checkbox, skipped entirely
; when Setup runs silently.
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
; Silent self-updates: WinKit passes /launchafterinstall=1 (see UpdateService.
; ApplyViaInstallerAsync) because there's no wizard for a checkbox to live on,
; but the user still expects WinKit to reopen once the update finishes.
Filename: "{app}\{#MyAppExeName}"; Flags: nowait; Check: ShouldLaunchAfterSilentInstall

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
function ShouldLaunchAfterSilentInstall(): Boolean;
begin
  Result := WizardSilent() and (ExpandConstant('{param:launchafterinstall|0}') = '1');
end;
