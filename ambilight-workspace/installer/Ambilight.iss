#define AppName "MPC-HC Ambilight"
#define StageDir "..\work\installer-stage"

[Setup]
AppId={{75D3FAAD-E16D-49BC-9374-44A279A02E13}
AppName={#AppName}
AppVersion=2.8.4 Ambilight 4
AppVerName={#AppName} 2.8.4 (Ambilight 4)
AppPublisher=teguhsatria892 (community fork)
AppPublisherURL=https://github.com/teguhsatria892/mpc-hc
DefaultDirName={localappdata}\Programs\MPC-HC Ambilight
DefaultGroupName={#AppName}
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\mpc-hc64.exe
SetupIconFile=..\mpc-hc\src\mpc-hc\res\icon.ico
LicenseFile=..\mpc-hc\COPYING.txt
OutputDir=..\outputs
OutputBaseFilename=MPC-HC-Ambilight-Setup-x64-v4
VersionInfoVersion=2.8.4.4
VersionInfoDescription=MPC-HC Ambilight Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Shortcuts:"

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Excludes: "mpc-hc64.ini"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "mpc-hc64.ini"; DestDir: "{app}"; Flags: onlyifdoesntexist uninsneveruninstall

[Registry]
Root: HKCU; Subkey: "Software\MPC-BE Filters\MPC Video Renderer Ambilight"; ValueType: dword; ValueName: "UseD3D11"; ValueData: "1"; Flags: createvalueifdoesntexist

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\mpc-hc64.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\mpc-hc64.exe"; WorkingDir: "{app}"; Tasks: desktopicon
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\mpc-hc64.exe"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent
