[Setup]
AppId=Hotakacchi.CameraGuard
AppName=CameraGuard
AppVersion={#AppVersion}
AppVerName=CameraGuard v{#AppVersion}
VersionInfoVersion={#AppVersion}
VersionInfoProductVersion={#AppVersion}
DisableWelcomePage=no
AppPublisher=Hotakacchi
AppPublisherURL=https://github.com/Hotakacchi/CameraGuard
UsePreviousAppDir=yes
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
CloseApplications=no
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

[CustomMessages]
english.CloseCameraGuard=CameraGuard is running or its files are in use. Choose Exit from the CameraGuard notification area menu, then run Setup again.
japanese.CloseCameraGuard=CameraGuardが起動中か、ファイルが使用されています。通知領域のCameraGuardメニューから「CameraGuardを終了」を選んでから、セットアップを再実行してください。

[Code]
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  AppPath: String;
  AppFile: TFileStream;
begin
  Result := '';
  AppPath := ExpandConstant('{app}\CameraGuard.exe');
  if FileExists(AppPath) then
  begin
    try
      AppFile := TFileStream.Create(AppPath, fmOpenReadWrite or fmShareExclusive);
      AppFile.Free;
    except
      Result := CustomMessage('CloseCameraGuard');
    end;
  end;
end;
