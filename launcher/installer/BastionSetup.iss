; Установщик Bastion Online для Windows (Inno Setup 6).
; Собирается в CI (pc-launcher.yml): iscc /DAppVersion=… /DSrcDir=…\BastionLauncher BastionSetup.iss
; Ставит лаунчер всегда в %LocalAppData%\Bastion Online\Launcher (без прав администратора,
; папку не спрашивает), ярлыки на рабочем столе и в Пуске, сразу запускает лаунчер.
; Игру лаунчер ставит сам при первом запуске.

#ifndef AppVersion
  #define AppVersion "2.0.0.0"
#endif
#ifndef SrcDir
  #define SrcDir "..\..\dist\BastionLauncher"
#endif

[Setup]
AppId={{6C1E2B7A-3F0D-4B8E-9A51-B4571C0D2E11}
AppName=Bastion Online
AppVersion={#AppVersion}
AppVerName=Bastion Online
AppPublisher=Bastion Online
DefaultDirName={localappdata}\Bastion Online\Launcher
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableWelcomePage=yes
DisableReadyPage=yes
PrivilegesRequired=lowest
UsePreviousAppDir=no
OutputBaseFilename=BastionSetup
OutputDir=..\..\dist
SetupIconFile=..\source\bastion.ico
UninstallDisplayIcon={app}\BastionLauncher.exe
UninstallDisplayName=Bastion Online
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"

[Files]
; launcher.ini игрока при обновлении не трогаем
Source: "{#SrcDir}\launcher.ini"; DestDir: "{app}"; Flags: onlyifdoesntexist uninsneveruninstall
Source: "{#SrcDir}\*"; Excludes: "launcher.ini,source\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{userdesktop}\Bastion Online"; Filename: "{app}\BastionLauncher.exe"; WorkingDir: "{app}"
Name: "{userprograms}\Bastion Online"; Filename: "{app}\BastionLauncher.exe"; WorkingDir: "{app}"

[Run]
Filename: "{app}\BastionLauncher.exe"; WorkingDir: "{app}"; Description: "Запустить Bastion Online"; Flags: nowait postinstall
