; installer.iss
; last updated: 19/06/2026

[Setup]
AppName=MAGE
AppVersion=0.1a
AppVerName=MAGE - Make actually good encryption v0.1a
AppPublisher=Commonwealthrocks
AppCopyright=Copyright (C) 2026 Commonwealthrocks
DefaultDirName={autopf}\MAGE
DisableDirPage=no
OutputBaseFilename=setup_mage_0.1a_win64
Compression=lzma2/ultra64
DefaultGroupName=MAGE
SolidCompression=yes
SetupIconFile=assets\imgs\s_icons\b_mage.ico
UninstallDisplayIcon={app}\assets\imgs\s_icons\b_mage.ico
InfoBeforeFile=..\license.txt
WizardStyle=modern

[Files]
Source: "build\bin\mage.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\bin\*"; Excludes: "mage.exe"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\MAGE"; Filename: "{app}\mage.exe"; IconFilename: "{app}\assets\imgs\s_icons\mage.ico"; Tasks: startmenuicon
Name: "{autodesktop}\MAGE"; Filename: "{app}\mage.exe"; IconFilename: "{app}\assets\imgs\s_icons\mage.ico"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked
Name: "startmenuicon"; Description: "Create a &Start Menu shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked
