; Compile only from build.bat --installer after windeployqt succeeds.
#ifndef DistDir
  #error DistDir is required
#endif
#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif
#ifndef NeedsVCRuntime
  #error NeedsVCRuntime is required
#endif
[Setup]
AppId={{E2711809-4A86-4F2C-B612-2BE5C46E057A}
AppName=PonchoWriter
AppVersion={#AppVersion}
AppPublisher=Alfonso Alvarez
AppPublisherURL=https://github.com/ponchoalv/PonchoWriter
DefaultDirName={localappdata}\Programs\PonchoWriter
DefaultGroupName=PonchoWriter
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir={#OutputDir}
OutputBaseFilename=PonchoWriter-{#AppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\ponchowriter.exe
CloseApplications=yes

[Code]
function InitializeSetup(): Boolean;
var
  Installed: Cardinal;
begin
  Result := True;
#if NeedsVCRuntime == 1
  Result := RegQueryDWordValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Installed', Installed) and (Installed = 1);
  if not Result then
    MsgBox('Install the latest Microsoft Visual C++ 2015-2022 x64 Redistributable, then run this installer again:' + #13#10 + 'https://aka.ms/vs/17/release/vc_redist.x64.exe', mbError, MB_OK);
#endif
end;

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\PonchoWriter"; Filename: "{app}\ponchowriter.exe"
Name: "{autodesktop}\PonchoWriter"; Filename: "{app}\ponchowriter.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\ponchowriter.exe"; Description: "Launch PonchoWriter"; Flags: nowait postinstall skipifsilent

[Messages]
WelcomeLabel2=This installs PonchoWriter for your account.%n%nMSVC builds require the Microsoft Visual C++ 2015-2022 x64 Redistributable. Mermaid additionally requires separately installed Node.js, Mermaid CLI and its browser. This unsigned installer does not install those prerequisites.%n%nSee the installed notices folder for third-party licenses. Packaging does not establish redistribution clearance.
