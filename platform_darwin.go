//go:build darwin

package main

import (
	"fmt"
	"os"
	"path/filepath"
)

func getSaveDir(homeDir string) string {
	return filepath.Join(homeDir, "Library", "Application Support", "bing-wallpapers")
}

func openSavedImagesDir() {
	usr, _ := userCurrent()
	saveDir := getSaveDir(usr.HomeDir)
	execCommand("open", saveDir).Start()
}

func setWallpaper(path string) {
	// Try System Events (works on all modern macOS versions)
	script := `tell application "System Events" to tell every desktop to set picture to "` + path + `"`
	if err := execCommand("osascript", "-e", script).Run(); err == nil {
		fmt.Println("Wallpaper set using osascript (System Events)")
		return
	}
	// Fallback: Finder approach
	script = `tell application "Finder" to set desktop picture to POSIX file "` + path + `"`
	if err := execCommand("osascript", "-e", script).Run(); err == nil {
		fmt.Println("Wallpaper set using osascript (Finder)")
		return
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

		// Create LaunchAgent for autostart
		launchAgentsDir := filepath.Join(usr.HomeDir, "Library", "LaunchAgents")
		os.MkdirAll(launchAgentsDir, 0755)
		plistPath := filepath.Join(launchAgentsDir, "com.bing-wallpaper.plist")
		plistContent := "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
			"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n" +
			"<plist version=\"1.0\">\n<dict>\n" +
			"\t<key>Label</key>\n\t<string>com.bing-wallpaper</string>\n" +
			"\t<key>ProgramArguments</key>\n\t<array>\n\t\t<string>" + targetPath + "</string>\n\t</array>\n" +
			"\t<key>RunAtLoad</key>\n\t<true/>\n" +
			"\t<key>KeepAlive</key>\n\t<false/>\n" +
			"</dict>\n</plist>\n"
		os.WriteFile(plistPath, []byte(plistContent), 0644)
		execCommand("launchctl", "load", plistPath).Run()

		execCommand(targetPath).Start()
		osExit(0)
	}
}
