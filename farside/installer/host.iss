; Farside Host Setup (Inno Setup 6). Built by farside/package.sh:
;   ISCC /DVersion=<x.y.z> /DSource=<folder with Farside.exe and its DLLs> host.iss
; Installs for the current user, without administrator rights, so an update needs no question from Windows.
; usbip-win2 (which receives the controller) is not installed by this Setup: github.com/vadimgrn/usbip-win2.

[Setup]
AppId={{69B543C6-C277-43D1-9C0F-5306E20B8BEE}
AppName=Farside Host
AppVersion={#Version}
AppPublisher=Moonthia
AppPublisherURL=https://github.com/Krislyz/farside
AppSupportURL=https://github.com/Krislyz/farside/issues
DefaultDirName={autopf}\Farside Host
DisableProgramGroupPage=yes
LicenseFile={#Source}\COPYING
OutputBaseFilename=FarsideHost-Setup
SetupIconFile=..\..\gui\farside.ico
UninstallDisplayIcon={app}\Farside.exe
UninstallDisplayName=Farside Host
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

[Icons]
Name: "{autoprograms}\Farside Host"; Filename: "{app}\Farside.exe"
Name: "{autodesktop}\Farside Host"; Filename: "{app}\Farside.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Farside.exe"; Description: "Start Farside Host"; Flags: nowait postinstall skipifsilent
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
  Result := ProgramEnded('Farside Host is running. Close it, then click OK.');
end;

function InitializeUninstall: Boolean;
begin
  Result := ProgramEnded('Farside Host is running. Close it, then click OK.');
end;
