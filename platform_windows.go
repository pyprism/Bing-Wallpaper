//go:build windows

package main

import (
	"fmt"
	"os"
	"path/filepath"
)

func getSaveDir(homeDir string) string {
	appData := os.Getenv("APPDATA")
	if appData == "" {
		appData = filepath.Join(homeDir, "AppData", "Roaming")
	}
	return filepath.Join(appData, "bing-wallpapers")
}

func openSavedImagesDir() {
	usr, _ := userCurrent()
	saveDir := getSaveDir(usr.HomeDir)
	execCommand("explorer", saveDir).Start()
}

func setWallpaper(path string) {
	// Set wallpaper via registry + rundll32
	if err := execCommand("reg", "add", `HKCU\Control Panel\Desktop`, "/v", "Wallpaper", "/t", "REG_SZ", "/d", path, "/f").Run(); err != nil {
		fmt.Println("Error setting wallpaper in registry:", err)
		return
	}
	if err := execCommand("rundll32.exe", "user32.dll,UpdatePerUserSystemParameters").Run(); err != nil {
		fmt.Println("Error refreshing wallpaper:", err)
		return
	}
	fmt.Println("Wallpaper set using registry + rundll32")
}

func ensureInstall() {
	localAppData := os.Getenv("LOCALAPPDATA")
	if localAppData == "" {
		usr, _ := userCurrent()
		localAppData = filepath.Join(usr.HomeDir, "AppData", "Local")
	}
	installDir := filepath.Join(localAppData, "Programs", "bing-wallpaper")
	os.MkdirAll(installDir, 0755)
	targetPath := filepath.Join(installDir, "bing-wallpaper.exe")

	execPath, _ := osExecutable()
	if execPath != targetPath {
		input, err := os.ReadFile(execPath)
		if err == nil {
			os.WriteFile(targetPath, input, 0755)
			fmt.Println("Installed to", targetPath)
		}

		// Add to registry Run key for autostart
		execCommand("reg", "add",
			`HKCU\Software\Microsoft\Windows\CurrentVersion\Run`,
			"/v", "BingWallpaper", "/t", "REG_SZ", "/d", targetPath, "/f",
		).Run()

		execCommand(targetPath).Start()
		osExit(0)
	}
}
