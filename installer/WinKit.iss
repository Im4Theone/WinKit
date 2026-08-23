; WinKit installer script (Inno Setup 6).
;
; Builds a Windows installer from the self-contained win-x64 Release
; publish output. Use build-installer.ps1 to publish and compile in one
; step, or run: dotnet publish ..\src\WinKit.UI\WinKit.UI.csproj -c Release
; -r win-x64 --self-contained true -o ..\publish\win-x64
; then compile this script with ISCC.exe.

#define MyAppName "WinKit"
#ifndef MyAppVersion
  #define MyAppVersion "1.0.1"
#endif
#define MyAppPublisher "WinKit"
#define MyAppExeName "WinKit.exe"
#define PublishDir "..\publish\win-x64"

[Setup]
AppId={{611A2A8E-E24D-4CA5-812A-B8D864D53C7D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=output
OutputBaseFilename=WinKitSetup-{#MyAppVersion}
SetupIconFile=..\src\WinKit.UI\Assets\Icon\WinKit.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "{#PublishDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall; Check: ShouldLaunchAfterInstall

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
// Interactive installs keep launching WinKit as before. A plain silent install
// (e.g. scripted testing) does not auto-launch, same as before. The auto-updater
// (WinKit.Infrastructure.UpdateService) passes /launchafterinstall=1 specifically
// so a silent update still relaunches WinKit afterward.
function ShouldLaunchAfterInstall: Boolean;
begin
  if WizardSilent then
    Result := ExpandConstant('{param:launchafterinstall|0}') = '1'
  else
    Result := True;
end;
