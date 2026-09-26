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
CloseApplications=yes
CloseApplicationsFilter=bing-wallpaper.exe
; No code signing — SmartScreen will warn on first run, documented in README.

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Bing Wallpaper"; Filename: "{app}\bing-wallpaper.exe"

[Registry]
; Autostart is written by the app itself (Installer::applyAutostart), but an
; uninstall must remove it too, rather than leaving a dangling Run entry
; pointing at a now-missing executable.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "BingWallpaper"; Flags: uninsdeletevalue

[Run]
Filename: "{app}\bing-wallpaper.exe"; Description: "Launch Bing Wallpaper"; Flags: nowait postinstall skipifsilent
