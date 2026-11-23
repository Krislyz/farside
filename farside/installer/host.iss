; Farside Setup (Inno Setup 6). Until 0.1.5.2 the program was called Farside Host: same AppId, so this
; Setup updates such an install where it is, and removes its old shortcuts. Built by farside/package.sh:
;   ISCC /DVersion=<x.y.z> /DSource=<folder with Farside.exe and its DLLs> host.iss
; Installs for the current user, without administrator rights, so an update needs no question from Windows.
; usbip-win2 (which receives the controller) is not installed by this Setup: github.com/vadimgrn/usbip-win2.

; A test of the Setup program gives these three other values (ISCC /DName=... /DOldName=... /DId=...),
; so that it touches nothing of a real install.
#ifndef Name
#define Name "Farside"
#endif
#ifndef OldName
#define OldName "Farside Host"
#endif
#ifndef Id
#define Id "69B543C6-C277-43D1-9C0F-5306E20B8BEE"
#endif

[Setup]
AppId={{{#Id}}
AppName={#Name}
AppVersion={#Version}
AppPublisher=Moonthia
AppPublisherURL=https://github.com/Krislyz/farside
AppSupportURL=https://github.com/Krislyz/farside/issues
DefaultDirName={code:InstallDir}
; The folder comes from InstallDir, which knows an earlier install.
UsePreviousAppDir=no
DisableProgramGroupPage=yes
LicenseFile={#Source}\COPYING
OutputBaseFilename=Farside-Setup
SetupIconFile=..\..\gui\farside.ico
UninstallDisplayIcon={app}\Farside.exe
UninstallDisplayName={#Name}
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
Type: files; Name: "{autoprograms}\{#OldName}.lnk"
Type: files; Name: "{autodesktop}\{#OldName}.lnk"

[Icons]
Name: "{autoprograms}\{#Name}"; Filename: "{app}\Farside.exe"
Name: "{autodesktop}\{#Name}"; Filename: "{app}\Farside.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Farside.exe"; Description: "Start Farside"; Flags: nowait postinstall skipifsilent
; An update started from inside the program runs silently: start the program again.
Filename: "{app}\Farside.exe"; Flags: nowait; Check: WizardSilent

[Code]
// The folder of an install that is already there, or '' when there is none.
function EarlierDir: String;
begin
  if not RegQueryStringValue(HKA, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{{#Id}}_is1', 'Inno Setup: App Path', Result) then
    Result := '';
  Result := RemoveBackslash(Result);
end;

// True when that install sits in a folder with the name the program had until 0.1.5.2.
function EarlierDirHasOldName: Boolean;
begin
  Result := CompareText(ExtractFileName(EarlierDir), '{#OldName}') = 0;
end;

// Where to install. A new install: the default folder. An install in a folder of the old name moves
// to a folder of the new name beside it. Any other install stays where it is.
function InstallDir(Param: String): String;
begin
  if EarlierDir = '' then
    Result := ExpandConstant('{autopf}\{#Name}')
  else if EarlierDirHasOldName then
    Result := ExtractFilePath(EarlierDir) + '{#Name}'
  else
    Result := EarlierDir;
end;

// The move itself: the old folder is renamed before the files are written, so everything in it comes
// along. The program that started an update may still be letting go of the folder: tried for 5 seconds.
// It cannot be done when the new folder is there already, when a file stays in use, and when Setup
// was started from inside the old folder. That last one is what an update from the program does: the
// Setup program that Windows started (it only unpacks this one and waits) stands in that folder until
// the end. Then the files go into the new folder and the old folder is emptied; its empty shell is
// removed 10 seconds later by a command of its own, when Setup has ended.
// ponytail: a fixed 10 seconds. Someone who starts Setup by hand from inside the old folder and stays
// on its last page for longer keeps an empty folder.
var
  OldDirLeft: String;

procedure CurStepChanged(CurStep: TSetupStep);
var
  I, Code: Integer;
begin
  if (CurStep = ssInstall) and EarlierDirHasOldName and (CompareText(EarlierDir, WizardDirValue) <> 0) then
  begin
    OldDirLeft := EarlierDir;
    if not DirExists(WizardDirValue) then
      for I := 1 to 25 do
      begin
        if RenameFile(OldDirLeft, WizardDirValue) then
        begin
          Log('Moved ' + OldDirLeft + ' to ' + WizardDirValue);
          OldDirLeft := '';
          Break;
        end;
        Sleep(200);
      end;
  end;
  if (CurStep = ssPostInstall) and (OldDirLeft <> '') then
  begin
    Log('Could not move ' + OldDirLeft + '; removing it');
    if not DelTree(OldDirLeft, True, True, True) then
      // "rd" without /s takes an empty folder only.
      Exec(ExpandConstant('{cmd}'), '/c "ping -n 11 127.0.0.1 >nul & rd "' + OldDirLeft + '""', ExpandConstant('{sys}'), SW_HIDE, ewNoWait, Code);
  end;
end;

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
