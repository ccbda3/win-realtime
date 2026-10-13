; Inno Setup 脚本 — 行迹自录 高端版 GPS 实时发射软件
#define MyAppName "行迹自录-高端版"
#define GitVer GetEnv("GIT_VERSION")
#if GitVer == ""
  #define MyAppVersion "1.0.0"
#else
  #define MyAppVersion GitVer
#endif
#define MyAppPublisher "PosNavTech"
#define MyAppExeName "gps-ui-qt.exe"
#define PortableDir "gaoduan\gps-ui-qt_portable"

[Setup]
AppId={{D9C4B6E2-3A5F-4C8B-0D7E-9F2A1B3C4D5E}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\行迹自录-高端版
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=.\installer_output
OutputBaseFilename=行迹自录_高端版_{#MyAppVersion}
Compression=lzma2/normal
SolidCompression=yes
WizardStyle=modern
SetupIconFile={#PortableDir}\favicon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
PrivilegesRequired=lowest

[Components]
Name: "main"; Description: "主程序（必需）"; Types: full compact custom; Flags: fixed
Name: "plutodrv"; Description: "PlutoSDR USB 驱动程序"; Types: full

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "快捷方式"; Flags: checkedonce

[Files]
; 主程序文件
Source: "{#PortableDir}\gps-ui-qt.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\gps-core.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\updater.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\切换GPU渲染.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\切换CPU渲染.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\version.txt"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\QtWebEngineProcess.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: main
; DLL 文件
Source: "{#PortableDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion; Components: main
; UHD FPGA 镜像
Source: "{#PortableDir}\uhd\*"; DestDir: "{app}\uhd"; Flags: ignoreversion recursesubdirs; Components: main
; 静态前端文件
Source: "{#PortableDir}\static\*"; DestDir: "{app}\static"; Flags: ignoreversion recursesubdirs; Components: main
; mapserver
Source: "{#PortableDir}\mapserver\*"; DestDir: "{app}\mapserver"; Flags: ignoreversion recursesubdirs; Components: main
; Qt 插件目录
Source: "{#PortableDir}\platforms\*.dll"; DestDir: "{app}\platforms"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\styles\*.dll"; DestDir: "{app}\styles"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\tls\*.dll"; DestDir: "{app}\tls"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\iconengines\*.dll"; DestDir: "{app}\iconengines"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\imageformats\*.dll"; DestDir: "{app}\imageformats"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\plugins\*"; DestDir: "{app}\plugins"; Flags: ignoreversion recursesubdirs; Components: main
Source: "{#PortableDir}\engine\*"; DestDir: "{app}\engine"; Flags: ignoreversion recursesubdirs; Components: main
Source: "{#PortableDir}\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs; Components: main
Source: "{#PortableDir}\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs; Components: main
Source: "{#PortableDir}\position\*"; DestDir: "{app}\position"; Flags: ignoreversion recursesubdirs; Components: main
Source: "{#PortableDir}\qmltooling\*"; DestDir: "{app}\qmltooling"; Flags: ignoreversion recursesubdirs; Components: main
Source: "{#PortableDir}\translations\*"; DestDir: "{app}\translations"; Flags: ignoreversion recursesubdirs; Components: main
; WebEngine 等资源
Source: "{#PortableDir}\resources\*"; DestDir: "{app}\resources"; Flags: ignoreversion recursesubdirs; Components: main
; 额外文件
Source: "{#PortableDir}\favicon.ico"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\卫星.png"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\opengl32sw.dll"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\D3Dcompiler_47.dll"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\dxcompiler.dll"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\dxil.dll"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\api_keys.json"; DestDir: "{app}"; Flags: ignoreversion; Components: main
; 手动星历目录（空目录占位）
Source: "{#PortableDir}\手动星历\*"; DestDir: "{app}\手动星历"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist; Components: main
; Pluto 驱动（可选）
Source: "{#PortableDir}\PlutoSDR-M2k-USB-Drivers.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: plutodrv

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\PlutoSDR-M2k-USB-Drivers.exe"; Description: "安装 PlutoSDR USB 驱动程序"; Components: plutodrv; Flags: nowait skipifsilent shellexec
Filename: "{app}\{#MyAppExeName}"; Description: "启动 {#MyAppName}"; Flags: postinstall nowait skipifsilent shellexec

[UninstallRun]
Filename: "{app}\gps-core.exe"; Parameters: "--stop"; Flags: runhidden skipifdoesntexist

[UninstallDelete]
Type: filesandordirs; Name: "{app}\static"
Type: filesandordirs; Name: "{app}\mapserver"
Type: filesandordirs; Name: "{app}\platforms"
Type: filesandordirs; Name: "{app}\styles"
Type: filesandordirs; Name: "{app}\imageformats"
Type: filesandordirs; Name: "{app}\iconengines"
Type: filesandordirs; Name: "{app}\tls"
Type: filesandordirs; Name: "{app}\translations"
Type: filesandordirs; Name: "{app}\resources"
Type: filesandordirs; Name: "{app}\uhd"
Type: filesandordirs; Name: "{app}\plugins"
Type: filesandordirs; Name: "{app}\engine"
Type: filesandordirs; Name: "{app}\generic"
Type: filesandordirs; Name: "{app}\networkinformation"
Type: filesandordirs; Name: "{app}\position"
Type: filesandordirs; Name: "{app}\qmltooling"
Type: filesandordirs; Name: "{app}\手动星历"
