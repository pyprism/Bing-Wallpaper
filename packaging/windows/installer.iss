; Inno Setup script for Bing Wallpaper. Unsigned installer.
; Build with: ISCC packaging\windows\installer.iss /DAppVersion=1.0.0 /DSourceDir=build\windeploy

#ifndef AppVersion
#define AppVersion "dev"
#endif
#ifndef SourceDir
#define SourceDir "build\windeploy"
#endif

[Setup]
AppName=Bing Wallpaper
AppVersion={#AppVersion}
DefaultDirName={autopf}\bing-wallpaper
DefaultGroupName=Bing Wallpaper
OutputBaseFilename=bing-wallpaper-{#AppVersion}-windows-setup
OutputDir=..\..\
Compression=lzma
SolidCompression=yes
DisableProgramGroupPage=yes
ArchitecturesInstallIn64BitMode=x64compatible
; No code signing — SmartScreen will warn on first run, documented in README.

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Bing Wallpaper"; Filename: "{app}\bing-wallpaper.exe"

[Run]
Filename: "{app}\bing-wallpaper.exe"; Description: "Launch Bing Wallpaper"; Flags: nowait postinstall skipifsilent
