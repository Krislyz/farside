; Farside Setup (Inno Setup 6). Until 0.1.5.2 the program was called Farside Host: same AppId, so this
; Setup updates such an install where it is, and removes its old shortcuts. Built by farside/package.sh:
;   ISCC /DVersion=<x.y.z> /DSource=<folder with Farside.exe and its DLLs> host.iss
; Installs for the current user, without administrator rights, so an update needs no question from Windows.
; usbip-win2 (which receives the controller) is not installed by this Setup: github.com/vadimgrn/usbip-win2.

[Setup]
AppId={{69B543C6-C277-43D1-9C0F-5306E20B8BEE}
AppName=Farside
AppVersion={#Version}
AppPublisher=Moonthia
AppPublisherURL=https://github.com/Krislyz/farside
AppSupportURL=https://github.com/Krislyz/farside/issues
DefaultDirName={autopf}\Farside
DisableProgramGroupPage=yes
LicenseFile={#Source}\COPYING
OutputBaseFilename=Farside-Setup
SetupIconFile=..\..\gui\farside.ico
UninstallDisplayIcon={app}\Farside.exe
UninstallDisplayName=Farside
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
RestartApplications=no

[Tasks]
Name: desktopicon; Description: "Desktop shortcut"

[Files]
Source: "{#Source}\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[InstallDelete]
; Shortcuts under the name the program had until 0.1.5.2.
Type: files; Name: "{autoprograms}\Farside Host.lnk"
Type: files; Name: "{autodesktop}\Farside Host.lnk"

[Icons]
Name: "{autoprograms}\Farside"; Filename: "{app}\Farside.exe"
Name: "{autodesktop}\Farside"; Filename: "{app}\Farside.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Farside.exe"; Description: "Start Farside"; Flags: nowait postinstall skipifsilent
; An update started from inside the program runs silently: start the program again.
Filename: "{app}\Farside.exe"; Flags: nowait; Check: WizardSilent

[Code]
// True when the program has ended. An update started from inside the program finds it still ending.
function ProgramEnded(Question: String): Boolean;
var
  I: Integer;
begin
  for I := 1 to 75 do
  begin
    // The name of the mutex is from before the renaming; a version that is still ending holds it.
    if not CheckForMutexes('FarsideHostRunning') then
      Break;
    Sleep(200);
  end;
  Result := True;
  while CheckForMutexes('FarsideHostRunning') do
    if SuppressibleMsgBox(Question, mbError, MB_OKCANCEL, IDCANCEL) = IDCANCEL then
    begin
      Result := False;
      Exit;
    end;
end;

function InitializeSetup: Boolean;
begin
  Result := ProgramEnded('Farside is running. Close it, then click OK.');
end;

function InitializeUninstall: Boolean;
begin
  Result := ProgramEnded('Farside is running. Close it, then click OK.');
end;
