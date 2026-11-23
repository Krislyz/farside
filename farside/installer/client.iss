; Farside Remote Setup (Inno Setup 6). Built by farside/package.sh:
;   ISCC /DVersion=<x.y.z> /DSource=<folder with FarsideRemote.exe and its DLLs> client.iss
; Until 0.1.5.2 the program was called Farside Client (FarsideClient.exe): same AppId, so this Setup
; updates such an install where it is, and removes the old program file and shortcuts.
; It installs the program, usbipd-win (which shares the controller) and shares the DualSense that is
; plugged in. Removing Farside Remote undoes all three.
#define Usbipd "5.3.0"
#define UsbipdUrl "https://github.com/dorssel/usbipd-win/releases/download/v5.3.0/usbipd-win_5.3.0_x64.msi"
; SHA-256 of that file. The download is refused when it differs.
#define UsbipdSha "1c984914aec944de19b64eff232421439629699f8138e3ddc29301175bc6d938"

[Setup]
AppId={{5A173446-28CA-4864-B17D-6E24F9FA6BC5}
AppName=Farside Remote
AppVersion={#Version}
AppPublisher=Moonthia
AppPublisherURL=https://github.com/Krislyz/farside
AppSupportURL=https://github.com/Krislyz/farside/issues
DefaultDirName={autopf}\Farside Remote
DisableProgramGroupPage=yes
LicenseFile={#Source}\COPYING
OutputBaseFilename=FarsideRemote-Setup
SetupIconFile=..\..\gui\farside.ico
UninstallDisplayIcon={app}\FarsideRemote.exe
UninstallDisplayName=Farside Remote
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; usbipd-win installs a driver and sharing a device needs administrator rights.
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
RestartApplications=no

[Tasks]
Name: usbipd; Description: "Install usbipd-win {#Usbipd} (free, from github.com/dorssel/usbipd-win; shares the controller)"; Check: not UsbipdFound
Name: share; Description: "Share the DualSense that is plugged in now (USB cable). Windows on this PC cannot use it while it is shared."
Name: desktopicon; Description: "Desktop shortcut"

[Files]
Source: "{#Source}\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion
; Kept so that removing Farside Remote can remove usbipd-win again.
Source: "{tmp}\usbipd-win.msi"; DestDir: "{app}"; Flags: external; Tasks: usbipd

[InstallDelete]
; The program file and the shortcuts under the name the program had until 0.1.5.2.
Type: files; Name: "{app}\FarsideClient.exe"
Type: files; Name: "{autoprograms}\Farside Client.lnk"
Type: files; Name: "{autodesktop}\Farside Client.lnk"

[Icons]
Name: "{autoprograms}\Farside Remote"; Filename: "{app}\FarsideRemote.exe"
Name: "{autodesktop}\Farside Remote"; Filename: "{app}\FarsideRemote.exe"; Tasks: desktopicon

[Registry]
; The program's own switches (log on/off). Nothing is written here; the key goes when the program goes.
Root: HKCU; Subkey: "Software\Chiaki\Chiaki\farside"; Flags: dontcreatekey uninsdeletekey

[Run]
Filename: "msiexec.exe"; Parameters: "/i ""{app}\usbipd-win.msi"" /qn /norestart"; StatusMsg: "Installing usbipd-win..."; Flags: runhidden; Tasks: usbipd
; DualSense, then DualSense Edge. The one that is not plugged in just fails. Not during an update.
Filename: "{code:UsbipdExe}"; Parameters: "bind --force --hardware-id 054c:0ce6"; StatusMsg: "Sharing the controller..."; Flags: runhidden; Tasks: share; Check: UsbipdFound and not WizardSilent
Filename: "{code:UsbipdExe}"; Parameters: "bind --force --hardware-id 054c:0df2"; Flags: runhidden; Tasks: share; Check: UsbipdFound and not WizardSilent
Filename: "{app}\FarsideRemote.exe"; Description: "Start Farside Remote"; Flags: nowait postinstall skipifsilent
; An update started from inside the program runs silently: start the program again, as the user.
Filename: "{app}\FarsideRemote.exe"; Flags: nowait runasoriginaluser; Check: WizardSilent

[UninstallRun]
; Give the controller back to Windows, then remove usbipd-win if this Setup installed it.
Filename: "{code:UsbipdExe}"; Parameters: "unbind --all"; Flags: runhidden; RunOnceId: "unbind"; Check: UsbipdFound
Filename: "msiexec.exe"; Parameters: "/x ""{app}\usbipd-win.msi"" /qn /norestart"; Flags: runhidden; RunOnceId: "usbipd"; Check: OurUsbipd

[UninstallDelete]
; The pairing with the Host and the log.
Type: filesandordirs; Name: "{userappdata}\Farside"

[Code]
function UsbipdExe(Param: String): String;
begin
  Result := ExpandConstant('{commonpf}\usbipd-win\usbipd.exe');
end;

function UsbipdFound: Boolean;
begin
  Result := FileExists(UsbipdExe(''));
end;

function OurUsbipd: Boolean;
begin
  Result := FileExists(ExpandConstant('{app}\usbipd-win.msi'));
end;

// True when the program has ended. An update started from inside the program finds it still ending.
function ProgramEnded(Question: String): Boolean;
var
  I: Integer;
begin
  for I := 1 to 75 do
  begin
    // The name of the mutex is from before the renaming; a version that is still ending holds it.
    if not CheckForMutexes('FarsideClientRunning') then
      Break;
    Sleep(200);
  end;
  Result := True;
  while CheckForMutexes('FarsideClientRunning') do
    if SuppressibleMsgBox(Question, mbError, MB_OKCANCEL, IDCANCEL) = IDCANCEL then
    begin
      Result := False;
      Exit;
    end;
end;

function InitializeSetup: Boolean;
begin
  Result := ProgramEnded('Farside Remote is running. Close it, then click OK.');
end;

function InitializeUninstall: Boolean;
begin
  Result := ProgramEnded('Farside Remote is running. Close it, then click OK.');
end;

var
  DownloadPage: TDownloadWizardPage;

procedure InitializeWizard;
begin
  DownloadPage := CreateDownloadPage('Fetching usbipd-win', 'From github.com/dorssel/usbipd-win. The file is checked before it is used.', nil);
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpReady) and WizardIsTaskSelected('usbipd') then
  begin
    DownloadPage.Clear;
    DownloadPage.Add('{#UsbipdUrl}', 'usbipd-win.msi', '{#UsbipdSha}');
    DownloadPage.Show;
    try
      try
        DownloadPage.Download;
      except
        SuppressibleMsgBox(AddPeriod(GetExceptionMessage), mbCriticalError, MB_OK, IDOK);
        Result := False;
      end;
    finally
      DownloadPage.Hide;
    end;
  end;
end;
