; Instalador do Nosfera (gerado pelo GitHub Actions)
[Setup]
AppName=Nosfera
AppVersion=0.4.0
AppPublisher=Nosfera Audio
DefaultDirName={autopf}\Nosfera
DefaultGroupName=Nosfera
OutputDir=..\dist
OutputBaseFilename=Nosfera-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName=Nosfera

[Languages]
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked

[Files]
; Plugin VST3 na pasta padrão que o FL Studio procura
Source: "..\stage\VST3\*"; DestDir: "{commoncf64}\VST3"; Flags: ignoreversion recursesubdirs createallsubdirs
; Versão independente (abre sem o FL)
Source: "..\stage\Standalone\Nosfera.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Nosfera"; Filename: "{app}\Nosfera.exe"
Name: "{autodesktop}\Nosfera"; Filename: "{app}\Nosfera.exe"; Tasks: desktopicon
