//go:build linux

package main

import (
	"fmt"
	"os"
	"path/filepath"
)

func getSaveDir(homeDir string) string {
	return filepath.Join(homeDir, ".local", "share", "bing-wallpapers")
}
func openSavedImagesDir() {
	usr, _ := userCurrent()
	saveDir := getSaveDir(usr.HomeDir)
	execCommand("xdg-open", saveDir).Start()
}
func setWallpaper(path string) {
	candidates := [][]string{
		// GNOME / Cinnamon / MATE
		{"gsettings", "set", "org.gnome.desktop.background", "picture-uri", "file://" + path},
		{"gsettings", "set", "org.gnome.desktop.background", "picture-uri-dark", "file://" + path},
		{"gsettings", "set", "org.cinnamon.desktop.background", "picture-uri", "file://" + path},
		{"gsettings", "set", "org.mate.background", "picture-filename", path},
		// KDE Plasma
		{"plasma-apply-wallpaperimage", path},
		// XFCE
		{"xfconf-query", "-c", "xfce4-desktop", "-p", "/backdrop/screen0/monitor0/image-path", "-s", path},
		// LXQt / Openbox (feh)
		{"feh", "--bg-fill", path},
		// sway / wayland
		{"swaymsg", "output", "*", "bg", path, "fill"},
	}
	for _, cmd := range candidates {
		if err := execCommand(cmd[0], cmd[1:]...).Run(); err == nil {
			fmt.Println("Wallpaper set using:", cmd[0])
			return
		}
	}
	fmt.Println("No suitable wallpaper command found.")
}
func ensureInstall() {
	usr, _ := userCurrent()
	localBin := filepath.Join(usr.HomeDir, ".local", "bin")
	os.MkdirAll(localBin, 0755)
	execPath, _ := osExecutable()
	targetPath := filepath.Join(localBin, "bing-wallpaper")
	if execPath != targetPath {
		input, err := os.ReadFile(execPath)
		if err == nil {
			os.WriteFile(targetPath, input, 0755)
			fmt.Println("Installed to", targetPath)
		}
		autoDir := filepath.Join(usr.HomeDir, ".config", "autostart")
		os.MkdirAll(autoDir, 0755)
		desktopFile := filepath.Join(autoDir, "bing-wallpaper.desktop")
		content := "[Desktop Entry]\n" +
			"Type=Application\n" +
			"Exec=" + targetPath + "\n" +
			"Hidden=false\n" +
			"NoDisplay=false\n" +
			"X-GNOME-Autostart-enabled=true\n" +
			"Name=Bing Wallpaper\n" +
			"Comment=Daily Bing Wallpaper\n"
		os.WriteFile(desktopFile, []byte(content), 0644)
		execCommand(targetPath).Start()
		osExit(0)
	}
}
