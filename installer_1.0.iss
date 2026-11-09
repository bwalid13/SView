; ---------------------------------------------------------------------------
; Sview VFX 1.0 — installeur Inno Setup (première release publique)
;
; Produit deploy_output\Sview_Setup_v1.0.exe
;
; Compilation :
;   "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" installer_1.0.iss
;
; Le contenu installé vient de build\package_1.0 (staging produit à partir de
; build\Release, sans les exécutables de test, avec le runtime VC++ et
; LICENSES.txt).
; ---------------------------------------------------------------------------

[Setup]
AppName=Sview VFX
AppVersion=1.0.0
AppVerName=Sview VFX 1.0.0
AppPublisher=Sview
AppPublisherURL=https://github.com/bwalid13
AppSupportURL=https://github.com/bwalid13
AppComments=SView — a fast, focused image viewer for visual effects
DefaultDirName={autopf}\Sview
DefaultGroupName=Sview
DisableProgramGroupPage=yes
OutputDir=deploy_output
OutputBaseFilename=Sview_Setup_v1.0
Compression=lzma2
SolidCompression=yes
SetupIconFile=res\sview.ico
UninstallDisplayIcon={app}\sview.exe
UninstallDisplayName=Sview VFX 1.0
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
LicenseFile=LICENSES.txt
VersionInfoVersion=1.0.0.0
VersionInfoProductName=Sview VFX
VersionInfoProductVersion=1.0.0.0
VersionInfoDescription=Sview VFX 1.0.0 installer
VersionInfoCompany=Sview

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "build\package_1.0\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "LICENSES.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Sview VFX"; Filename: "{app}\sview.exe"
Name: "{autodesktop}\Sview VFX"; Filename: "{app}\sview.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"

[Run]
Filename: "{app}\sview.exe"; Description: "Launch Sview VFX"; Flags: nowait postinstall skipifsilent
