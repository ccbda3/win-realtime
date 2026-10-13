; Inno Setup 脚本 — 行迹自录 GPS 授权码生成器
#define MyAppName "行迹自录-授权生成器"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "PosNavTech"
#define MyAppExeName "license_gen_gui.exe"

[Setup]
AppId={{E8A4D5C2-3F5E-4B7A-8C6D-9E1F2A3B4C5E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\PosNavTech\license-gen
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=.\installer_output
OutputBaseFilename=授权生成器_v{#MyAppVersion}
Compression=lzma2/normal
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest

[Files]
Source: "gps-ui-qt\gps-ui-qt_portable\license_gen_gui.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "gps-ui-qt\gps-ui-qt_portable\license_gen_icon.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "gps-ui-qt\gps-ui-qt_portable\Qt6Core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "gps-ui-qt\gps-ui-qt_portable\Qt6Gui.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "gps-ui-qt\gps-ui-qt_portable\Qt6Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "gps-ui-qt\gps-ui-qt_portable\platforms\qwindows.dll"; DestDir: "{app}\platforms"; Flags: ignoreversion
Source: "gps-ui-qt\gps-ui-qt_portable\platforms\qdirect2d.dll"; DestDir: "{app}\platforms"; Flags: ignoreversion
Source: "gps-ui-qt\gps-ui-qt_portable\styles\qmodernwindowsstyle.dll"; DestDir: "{app}\styles"; Flags: ignoreversion
Source: "gps-ui-qt\gps-ui-qt_portable\VCRUNTIME140.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "gps-ui-qt\gps-ui-qt_portable\VCRUNTIME140_1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "gps-ui-qt\gps-ui-qt_portable\MSVCP140.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "gps-ui-qt\gps-ui-qt_portable\msvcp140_1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "gps-ui-qt\gps-ui-qt_portable\msvcp140_2.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "gps-ui-qt\gps-ui-qt_portable\concrt140.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "gps-ui-qt\gps-ui-qt_portable\appicon.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "启动 {#MyAppName}"; Flags: postinstall nowait skipifsilent shellexec
