### Bing Wallpaper

A simple Go application that automatically sets your Linux desktop wallpaper to the daily Bing image. Supports multiple desktop environments and runs in the system tray.

## Features

- Fetches the latest Bing wallpaper daily
- Sets wallpaper for GNOME, KDE, XFCE, LXQt, sway, and more
- Runs in the system tray with quick actions
- Installs itself to `~/.local/bin` and autostarts on login

## Installation

1. **Build or dowmload the binary from the release:**
   ```sh
   go build -o bing-wallpaper
   ```

2. **Install the application:**
   ```sh
   ./bing-wallpaper
    ```
## Usage
- The app runs in the background and updates your wallpaper daily.
- Use the system tray icon to set a new wallpaper or quit the app.

## Requirements ( For building)
 - Go
 - Linux desktop environment (GNOME, KDE, XFCE, etc.)
 - System package 
  ```sh 
   sudo apt install -y libgl1-mesa-dev xorg-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev
  ```

## Icon Attribution

Image Comics icons created by [Design Circle](https://www.flaticon.com/free-icons/image-comics) - Flaticon

## License
 MIT License.