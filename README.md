### Bing Wallpaper [![CI](https://github.com/pyprism/Bing-Wallpaper/actions/workflows/run-tests.yaml/badge.svg)](https://github.com/pyprism/Bing-Wallpaper/actions/workflows/run-tests.yaml) [![codecov](https://codecov.io/gh/pyprism/Bing-Wallpaper/graph/badge.svg?token=VRN8AA0BVX)](https://codecov.io/gh/pyprism/Bing-Wallpaper)

A simple Go application that automatically sets your desktop wallpaper to the daily Bing image. Supports **Linux**, **macOS**, and **Windows**, and runs in the system tray.

## Features

- Fetches the latest Bing wallpaper daily
- **Linux**: Sets wallpaper for GNOME, KDE Plasma, XFCE, MATE, Cinnamon, LXQt, sway, and more
- **macOS**: Sets wallpaper via AppleScript (System Events / Finder)
- **Windows**: Sets wallpaper via registry + `rundll32`
- Runs in the system tray with quick actions (Set Now, Browse Saved Images, Quit)
- Installs itself and autostarts on login for all platforms

## Installation

1. **Build or download the binary from the [release](https://github.com/pyprism/Bing-Wallpaper/releases):**
   ```sh
   go build -o bing-wallpaper      # Linux / macOS
   go build -o bing-wallpaper.exe  # Windows
   ```

2. **Run the application — it self-installs and sets up autostart:**
   ```sh
   ./bing-wallpaper
   ```

## Platform Install Locations

| Platform | Binary | Autostart |
|----------|--------|-----------|
| Linux    | `~/.local/bin/bing-wallpaper` | `~/.config/autostart/bing-wallpaper.desktop` |
| macOS    | `~/.local/bin/bing-wallpaper` | `~/Library/LaunchAgents/com.bing-wallpaper.plist` |
| Windows  | `%LOCALAPPDATA%\Programs\bing-wallpaper\bing-wallpaper.exe` | `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` |

## Saved Wallpapers

| Platform | Location |
|----------|----------|
| Linux    | `~/.local/share/bing-wallpapers/` |
| macOS    | `~/Library/Application Support/bing-wallpapers/` |
| Windows  | `%APPDATA%\bing-wallpapers\` |

## Usage
- The app runs in the background and updates your wallpaper daily.
- Use the system tray icon to set a new wallpaper, browse saved images, or quit the app.

## Building Requirements

### Linux
```sh
sudo apt install -y libgl1-mesa-dev xorg-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev
go build -o bing-wallpaper .
```

### macOS
```sh
# Xcode Command Line Tools required
xcode-select --install
go build -o bing-wallpaper .
```

### Windows
```sh
go build -o bing-wallpaper.exe .
```

## Running Tests

```sh
go test ./...
```

Tests are platform-aware — common tests run on all platforms, and platform-specific tests (wallpaper commands, install paths, autostart mechanism) run only on their respective OS.

Coverage reports can be generated with:
```sh
go test -coverprofile=coverage.out ./...
go tool cover -html=coverage.out
```

Files listed in `.coverageignore` (platform integration and GUI entry points) are excluded from coverage requirements.

## Icon Attribution

Image Comics icons created by [Design Circle](https://www.flaticon.com/free-icons/image-comics) - Flaticon

## License
 MIT License.
