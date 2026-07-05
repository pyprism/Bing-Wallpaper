### Bing Wallpaper [![CI](https://github.com/pyprism/Bing-Wallpaper/actions/workflows/run-tests.yaml/badge.svg)](https://github.com/pyprism/Bing-Wallpaper/actions/workflows/run-tests.yaml)

A native Qt 6 / C++ application that automatically sets your desktop wallpaper to the daily
Bing image. Supports **Linux**, **macOS**, and **Windows**, and runs entirely from the
system tray — no dock icon, no taskbar entry, no window.

## Features

- Fetches the latest Bing wallpaper (optionally in UHD resolution) and caches it locally
- **Linux**: sets wallpaper for GNOME, KDE Plasma, XFCE, MATE, Cinnamon, LXQt/Openbox (`feh`), sway
- **macOS**: sets wallpaper via AppleScript (System Events / Finder fallback)
- **Windows**: sets wallpaper via `SystemParametersInfoW` (native WinAPI)
- Tray menu: Set New Wallpaper Now, Browse Saved Images, Copy Description, Previous
  Wallpapers (last 8 days), Market, Refresh Interval, Start at Login, Quit
- Desktop notification when the wallpaper changes
- Installs itself and can autostart on login for all platforms (toggle from the tray menu)
- No Dock icon (macOS `LSUIElement`), no taskbar button (Windows/Linux), tray-only everywhere

## Installation

Grab the package for your platform from the
[releases page](https://github.com/pyprism/Bing-Wallpaper/releases). All builds are
**unsigned** (no code signing / notarization) — each OS's first-run warning is expected,
not a sign of a broken download.

### macOS

1. Download `bing-wallpaper-*-macos.dmg` and open it.
2. Drag `Bing Wallpaper.app` into `/Applications` (or `~/Applications`).
3. Gatekeeper will block the first launch since the app is unsigned. **Right-click the
   app → Open → Open** in the confirmation dialog. (Only needed once — subsequent launches
   are normal double-clicks.)
4. Look for the icon in the menu bar — there's no Dock icon or window by design.

### Windows

1. Download `bing-wallpaper-*-windows-setup.exe` and run it.
2. SmartScreen will warn since the installer is unsigned. Click **More info → Run anyway**.
3. Follow the installer; it adds a Start Menu entry and launches the app.
4. Look for the icon in the system tray (may be under the "hidden icons" chevron).

### Linux

1. Download `bing-wallpaper-*-linux-x86_64.AppImage` (or `-linux-arm64...`).
2. Make it executable and run it:
   ```sh
   chmod +x bing-wallpaper-*.AppImage
   ./bing-wallpaper-*.AppImage
   ```
   (Or extract the `.tar.gz` instead and run the `bing-wallpaper` binary inside.)
3. A tray icon requires a `StatusNotifierItem`/AppIndicator host — most desktops (GNOME
   with an extension, KDE, XFCE, Cinnamon, MATE) provide one out of the box.

On first launch the app installs itself to a standard per-OS location and registers
autostart (see table below) — this can be turned off later from the tray menu's
**Start at Login** checkbox.

## Platform Install Locations

| Platform | Binary | Autostart |
|----------|--------|-----------|
| Linux    | `~/.local/bin/bing-wallpaper` | `~/.config/autostart/bing-wallpaper.desktop` |
| macOS    | wherever the `.app` is placed (e.g. `/Applications`) | `~/Library/LaunchAgents/com.bing-wallpaper.plist` |
| Windows  | `%LOCALAPPDATA%\Programs\bing-wallpaper\bing-wallpaper.exe` | `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` |

## Uninstalling

Before removing anything, uncheck **Start at Login** in the tray menu and **Quit** — this
cleanly deregisters autostart through the app itself instead of hand-editing files. Then:

### macOS
```sh
rm -rf "/Applications/Bing Wallpaper.app"          # or ~/Applications
rm -f ~/Library/LaunchAgents/com.bing-wallpaper.plist   # only if still present
rm -rf ~/Library/Application\ Support/bing-wallpapers   # cached wallpapers, optional
rm -f ~/Library/Preferences/com.bing-wallpaper.BingWallpaper.plist  # settings, optional
```

### Windows
```powershell
Remove-Item -Recurse -Force "$env:LOCALAPPDATA\Programs\bing-wallpaper"
Remove-ItemProperty "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name BingWallpaper -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "$env:APPDATA\bing-wallpapers"                          # cached wallpapers, optional
Remove-Item -Recurse -Force "HKCU:\Software\bing-wallpaper" -ErrorAction SilentlyContinue  # settings, optional
```

### Linux
```sh
rm -f ~/.local/bin/bing-wallpaper
rm -f ~/.config/autostart/bing-wallpaper.desktop        # only if still present
rm -rf ~/.local/share/bing-wallpapers                   # cached wallpapers, optional
rm -rf ~/.config/bing-wallpaper                         # settings, optional
```

(If installed via AppImage instead of the self-installed copy, just delete the
`.AppImage` file itself in addition to the above.)

## Saved Wallpapers

| Platform | Location |
|----------|----------|
| Linux    | `~/.local/share/bing-wallpapers/` |
| macOS    | `~/Library/Application Support/bing-wallpapers/` |
| Windows  | `%APPDATA%\bing-wallpapers\` |

## Usage

The app runs in the background with no visible window — everything is driven from the
system tray icon's menu:

- **Set New Wallpaper Now** — fetch and apply today's image immediately
- **Browse Saved Images** — open the cache folder in your file manager
- **Copy Description** — copy the current image's caption/photo credit to the clipboard
- **Previous Wallpapers** — pick from the last 8 days
- **Market** — which Bing regional edition to fetch from (`en-US`, `ja-JP`, `de-DE`, …)
- **Refresh Interval** — how often to check for a new wallpaper (1h–24h, default 5h)
- **Start at Login** — toggle autostart
- **Quit**

## Building Requirements

Requires **Qt 6.5+**, **CMake ≥ 3.21**, and a C++17 compiler.

### Linux
```sh
sudo apt install -y qt6-base-dev libqt6svg6-dev qt6-tools-dev cmake build-essential \
    libgl1-mesa-dev libxkbcommon-x11-0
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### macOS
```sh
xcode-select --install   # Xcode Command Line Tools
brew install qt cmake
cmake -S . -B build -DCMAKE_PREFIX_PATH=$(brew --prefix qt) -DCMAKE_BUILD_TYPE=Release
cmake --build build
open build/bing-wallpaper.app
```

### Windows
```powershell
# Qt 6 (MSVC kit) and CMake installed, then from a "Qt command prompt" or with
# CMAKE_PREFIX_PATH pointing at the Qt install:
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.7.2\msvc2019_64" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Packaging

Scripts in `packaging/` turn a Release build into a distributable package:

```sh
packaging/macos/package.sh   build <version>   # -> bing-wallpaper-<version>-macos.dmg
packaging/linux/package.sh   build <version>   # -> AppImage + .tar.gz (needs linuxdeploy + linuxdeploy-plugin-qt on PATH)
# Windows: windeployqt into build\windeploy, then run ISCC on packaging\windows\installer.iss
```

None of these are code-signed — see the Gatekeeper/SmartScreen notes above.

## Running Tests

```sh
cmake --build build
ctest --test-dir build --output-on-failure
```

Tests use QtTest and run headless (`QT_QPA_PLATFORM=offscreen`, no network access).
`BingClient`'s API parsing / URL building / save-path logic is tested directly as pure
functions; `WallpaperSetter` tests are guarded per-OS (`#ifdef Q_OS_MAC/WIN/...`) and only
compile the tests relevant to the platform being built.

## Icon Attribution

Image Comics icons created by [Design Circle](https://www.flaticon.com/free-icons/image-comics) - Flaticon

## License
 MIT License.
