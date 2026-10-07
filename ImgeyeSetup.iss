; Imgeye Inno Setup script
; Registers all supported image formats as default-opened by Imgeye, and
; provides a full uninstaller (Inno generates unins000.exe automatically).

[Setup]
AppId={{8C24C33A-484D-4384-BB75-542B149BA988}
AppName=Imgeye
AppVersion=1.1.0
AppVerName=Imgeye 1.1.0
AppPublisher=Imgeye
DefaultDirName={localappdata}\Programs\Imgeye
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=installer
OutputBaseFilename=ImgeyeSetup
Compression=lzma2
SolidCompression=yes
SetupIconFile=res\imgeye.ico
UninstallDisplayIcon={app}\imgeye.exe
UninstallDisplayName=Imgeye
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "assoc_bmp"; Description: "BMP 图像 (*.bmp)"; GroupDescription: "文件关联:"
Name: "assoc_png"; Description: "PNG 图像 (*.png)"; GroupDescription: "文件关联:"
Name: "assoc_jpg"; Description: "JPEG 图像 (*.jpg)"; GroupDescription: "文件关联:"
Name: "assoc_jpeg"; Description: "JPEG 图像 (*.jpeg)"; GroupDescription: "文件关联:"
Name: "assoc_gif"; Description: "GIF 图像 (*.gif)"; GroupDescription: "文件关联:"
Name: "assoc_tiff"; Description: "TIFF 图像 (*.tiff)"; GroupDescription: "文件关联:"
Name: "assoc_tif"; Description: "TIFF 图像 (*.tif)"; GroupDescription: "文件关联:"
Name: "assoc_ico"; Description: "ICO 图像 (*.ico)"; GroupDescription: "文件关联:"
Name: "assoc_webp"; Description: "WebP 图像 (*.webp)"; GroupDescription: "文件关联:"
Name: "assoc_svg"; Description: "SVG 矢量图像 (*.svg)"; GroupDescription: "文件关联:"
Name: "assoc_hig"; Description: "HIG 灰度图像 (*.hig)"; GroupDescription: "文件关联:"

[Files]
Source: "build\imgeye.exe"; DestDir: "{app}"; Flags: ignoreversion

[Registry]
; ProgID
Root: HKCU; Subkey: "Software\Classes\Imgeye.Image"; ValueType: string; ValueName: ""; ValueData: "Imgeye Image"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Imgeye.Image\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\imgeye.exe,0"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\Imgeye.Image\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\imgeye.exe"" ""%1"""; Flags: uninsdeletevalue

; Per-format associations (default open with Imgeye)
Root: HKCU; Subkey: "Software\Classes\.bmp"; ValueType: string; ValueName: ""; ValueData: "Imgeye.Image"; Flags: uninsdeletevalue; Tasks: assoc_bmp
Root: HKCU; Subkey: "Software\Classes\.png"; ValueType: string; ValueName: ""; ValueData: "Imgeye.Image"; Flags: uninsdeletevalue; Tasks: assoc_png
Root: HKCU; Subkey: "Software\Classes\.jpg"; ValueType: string; ValueName: ""; ValueData: "Imgeye.Image"; Flags: uninsdeletevalue; Tasks: assoc_jpg
Root: HKCU; Subkey: "Software\Classes\.jpeg"; ValueType: string; ValueName: ""; ValueData: "Imgeye.Image"; Flags: uninsdeletevalue; Tasks: assoc_jpeg
Root: HKCU; Subkey: "Software\Classes\.gif"; ValueType: string; ValueName: ""; ValueData: "Imgeye.Image"; Flags: uninsdeletevalue; Tasks: assoc_gif
Root: HKCU; Subkey: "Software\Classes\.tiff"; ValueType: string; ValueName: ""; ValueData: "Imgeye.Image"; Flags: uninsdeletevalue; Tasks: assoc_tiff
Root: HKCU; Subkey: "Software\Classes\.tif"; ValueType: string; ValueName: ""; ValueData: "Imgeye.Image"; Flags: uninsdeletevalue; Tasks: assoc_tif
Root: HKCU; Subkey: "Software\Classes\.ico"; ValueType: string; ValueName: ""; ValueData: "Imgeye.Image"; Flags: uninsdeletevalue; Tasks: assoc_ico
Root: HKCU; Subkey: "Software\Classes\.webp"; ValueType: string; ValueName: ""; ValueData: "Imgeye.Image"; Flags: uninsdeletevalue; Tasks: assoc_webp
Root: HKCU; Subkey: "Software\Classes\.svg"; ValueType: string; ValueName: ""; ValueData: "Imgeye.Image"; Flags: uninsdeletevalue; Tasks: assoc_svg
Root: HKCU; Subkey: "Software\Classes\.hig"; ValueType: string; ValueName: ""; ValueData: "Imgeye.Image"; Flags: uninsdeletevalue; Tasks: assoc_hig