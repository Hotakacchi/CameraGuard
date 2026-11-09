[Setup]
AppId=Hotakacchi.CameraGuard
AppName=CameraGuard
AppVersion={#AppVersion}
AppPublisher=Hotakacchi
AppPublisherURL=https://github.com/Hotakacchi/CameraGuard
DefaultDirName={localappdata}\Programs\CameraGuard
DefaultGroupName=CameraGuard
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\CameraGuard.exe
OutputDir=..\dist
OutputBaseFilename=CameraGuard-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\package\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\CameraGuard"; Filename: "{app}\CameraGuard.exe"; WorkingDir: "{app}"
Name: "{userdesktop}\CameraGuard"; Filename: "{app}\CameraGuard.exe"; WorkingDir: "{app}"; Tasks: desktopicon
