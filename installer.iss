; Inno Setup 脚本 — 行迹自录 全能版 GPS 实时发射软件
#define MyAppName "行迹自录-全能版"
#define GitVer GetEnv("GIT_VERSION")
#if GitVer == ""
  #define MyAppVersion "1.0.0"
#else
  #define MyAppVersion GitVer
#endif
#define MyAppPublisher "PosNavTech"
#define MyAppExeName "gps-ui-qt.exe"
#define PortableDir "gps-ui-qt\gps-ui-qt_portable"

[Setup]
AppId={{B8A3C5D1-2F4E-4A7B-9C6D-8E1F2A3B4C5D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\行迹自录-全能版
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputDir=.\installer_output
OutputBaseFilename=行迹自录_全能版_v{#MyAppVersion}
Compression=lzma2/normal
SolidCompression=yes
WizardStyle=modern
SetupIconFile={#PortableDir}\appicon.ico
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
; 引擎目录
Source: "{#PortableDir}\engine\*"; DestDir: "{app}\engine"; Flags: ignoreversion recursesubdirs; Components: main
; UHD FPGA 镜像
Source: "{#PortableDir}\uhd\*"; DestDir: "{app}\uhd"; Flags: ignoreversion recursesubdirs; Components: main
; 静态文件
Source: "{#PortableDir}\static\*"; DestDir: "{app}\static"; Flags: ignoreversion recursesubdirs; Components: main
; mapserver
Source: "{#PortableDir}\mapserver\*"; DestDir: "{app}\mapserver"; Flags: ignoreversion recursesubdirs; Components: main
; Qt 插件
Source: "{#PortableDir}\platforms\*.dll"; DestDir: "{app}\platforms"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\styles\*.dll"; DestDir: "{app}\styles"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\imageformats\*.dll"; DestDir: "{app}\imageformats"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\translations\*"; DestDir: "{app}\translations"; Flags: ignoreversion recursesubdirs; Components: main
; WebEngine 资源
Source: "{#PortableDir}\resources\*"; DestDir: "{app}\resources"; Flags: ignoreversion recursesubdirs; Components: main
; 其他文件
Source: "{#PortableDir}\appicon.ico"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\opengl32sw.dll"; DestDir: "{app}"; Flags: ignoreversion; Components: main
Source: "{#PortableDir}\plugins\tls\qschannelbackend.dll"; DestDir: "{app}\plugins\tls"; Flags: ignoreversion; Components: main

; 图标引擎
Source: "{#PortableDir}\iconengines\*.dll"; DestDir: "{app}\iconengines"; Flags: ignoreversion; Components: main
; 位置服务
Source: "{#PortableDir}\position\*.dll"; DestDir: "{app}\position"; Flags: ignoreversion; Components: main
; 网络信息
Source: "{#PortableDir}\networkinformation\*.dll"; DestDir: "{app}\networkinformation"; Flags: ignoreversion; Components: main
; Pluto 驱动（可选）
Source: "{#PortableDir}\PlutoSDR-M2k-USB-Drivers.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: plutodrv

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\appicon.ico"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\appicon.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\PlutoSDR-M2k-USB-Drivers.exe"; Description: "安装 PlutoSDR USB 驱动程序"; Components: plutodrv; Flags: nowait skipifsilent shellexec
Filename: "{app}\{#MyAppExeName}"; Description: "启动 {#MyAppName}"; Flags: postinstall nowait skipifsilent shellexec

[UninstallRun]
Filename: "{app}\gps-core.exe"; Parameters: "--stop"; Flags: runhidden skipifdoesntexist

[UninstallDelete]
Type: filesandordirs; Name: "{app}\engine"
Type: filesandordirs; Name: "{app}\static"
Type: filesandordirs; Name: "{app}\mapserver"
Type: filesandordirs; Name: "{app}\platforms"
Type: filesandordirs; Name: "{app}\styles"
Type: filesandordirs; Name: "{app}\imageformats"
Type: filesandordirs; Name: "{app}\translations"
Type: filesandordirs; Name: "{app}\resources"
