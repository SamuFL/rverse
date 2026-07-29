#ifndef BuildOutputDir
  #define BuildOutputDir "..\..\build\windows-vs2022\out"
#endif

#ifndef RepoRoot
  #define RepoRoot "..\.."
#endif

[Setup]
AppId={{57B31B56-CFE7-497C-B4F9-74A9CEB0E893}
AppName=RVRSE
AppContact=info@samufl.com
AppCopyright=Copyright (C) 2026 SamuFL
AppPublisher=SamuFL
AppPublisherURL=https://samufl.com
AppSupportURL=https://github.com/SamuFL/rverse/issues
AppVersion=1.0.0
VersionInfoVersion=1.0.0
DefaultDirName={autopf}\RVRSE
DefaultGroupName=RVRSE
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
OutputBaseFilename=RVRSE Installer
LicenseFile={#RepoRoot}\LICENSE
SetupLogging=yes
ShowComponentSizes=no
WizardStyle=modern
SetupIconFile={#RepoRoot}\RVRSE\resources\RVRSE.ico
UninstallDisplayIcon={app}\RVRSE.exe

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Messages]
WelcomeLabel1=Welcome to the RVRSE installer
SetupWindowTitle=RVRSE installer
SelectDirLabel3=The standalone application and supporting files will be installed in the following folder.
SelectDirBrowseLabel=To continue, click Next. If you would like to select a different folder, click Browse.

[Components]
Name: "app"; Description: "Standalone application"; Types: full custom
Name: "clap"; Description: "64-bit CLAP plugin"; Types: full custom
Name: "vst3"; Description: "64-bit VST3 plugin"; Types: full custom
Name: "manual"; Description: "User guide"; Types: full custom; Flags: fixed

[Files]
Source: "{#BuildOutputDir}\RVRSE.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "{#BuildOutputDir}\RVRSE.vst3\*"; DestDir: "{commoncf64}\VST3\RVRSE.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#BuildOutputDir}\RVRSE.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion
Source: "{#RepoRoot}\RVRSE\manual\RVRSE manual.pdf"; DestDir: "{app}"; Components: manual; Flags: ignoreversion
Source: "{#RepoRoot}\CHANGELOG.md"; DestDir: "{app}"; DestName: "CHANGELOG.md"; Flags: ignoreversion
Source: "{#RepoRoot}\RVRSE\installer\INSTALL-Windows.txt"; DestDir: "{app}"; DestName: "INSTALL.txt"; Flags: ignoreversion isreadme

[Icons]
Name: "{group}\RVRSE"; Filename: "{app}\RVRSE.exe"; Components: app
Name: "{group}\User guide"; Filename: "{app}\RVRSE manual.pdf"; Components: manual
Name: "{group}\Changelog"; Filename: "{app}\CHANGELOG.md"
Name: "{group}\Uninstall RVRSE"; Filename: "{uninstallexe}"

[UninstallDelete]
Type: files; Name: "{app}\InstallationLogFile.log"

[Code]
var
  OkToCopyLog: Boolean;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssDone then
    OkToCopyLog := True;
end;

procedure DeinitializeSetup();
begin
  if OkToCopyLog then
    FileCopy(ExpandConstant('{log}'), ExpandConstant('{app}\InstallationLogFile.log'), False);
  RestartReplace(ExpandConstant('{log}'), '');
end;
