package main

import (
	_ "embed"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"time"

	"fyne.io/fyne/v2"
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/driver/desktop"
)

const bingURL = "https://www.bing.com/HPImageArchive.aspx?format=js&idx=0&n=1"

//go:embed icon/bing.png
var iconData []byte

type BingResponse struct {
	Images []struct {
		URL  string `json:"url"`
		Date string `json:"startdate"`
	} `json:"images"`
}

func main() {
	ensureInstall()

	bingApp := app.NewWithID("Bing Wallpaper")

	icon := fyne.NewStaticResource("bing.png", iconData)
	bingApp.SetIcon(icon)

	// System tray only works if supported
	if desk, ok := bingApp.(desktop.App); ok {
		menu := fyne.NewMenu("Bing Wallpaper",
			fyne.NewMenuItem("Set New Wallpaper Now", func() { go updateWallpaper() }),
			fyne.NewMenuItem("Quit", func() { bingApp.Quit() }),
		)
		desk.SetSystemTrayMenu(menu)
		desk.SetSystemTrayIcon(icon)
	}

	// Run once immediately
	go updateWallpaper()

	// Schedule daily refresh
	go func() {
		for {
			time.Sleep(24 * time.Hour)
			updateWallpaper()
		}
	}()

	bingApp.Run()
}

func updateWallpaper() {
	resp, err := http.Get(bingURL)
	if err != nil {
		fmt.Println("Error fetching Bing API:", err)
		return
	}
	defer resp.Body.Close()

	var data BingResponse
	if err := json.NewDecoder(resp.Body).Decode(&data); err != nil {
		fmt.Println("Error decoding JSON:", err)
		return
	}

	if len(data.Images) == 0 {
		fmt.Println("No images found")
		return
	}

	imgURL := "https://www.bing.com" + data.Images[0].URL
	usr, _ := user.Current()
	saveDir := filepath.Join(usr.HomeDir, ".local/share/bing-wallpapers")
	os.MkdirAll(saveDir, 0755)

	savePath := filepath.Join(saveDir, data.Images[0].Date+".jpg")

	// Skip if already exists
	if _, err := os.Stat(savePath); err == nil {
		setWallpaper(savePath)
		return
	}

	// Download image
	respImg, err := http.Get(imgURL)
	if err != nil {
		fmt.Println("Error downloading image:", err)
		return
	}
	defer respImg.Body.Close()

	file, err := os.Create(savePath)
	if err != nil {
		fmt.Println("Error creating file:", err)
		return
	}
	defer file.Close()

	_, err = io.Copy(file, respImg.Body)
	if err != nil {
		fmt.Println("Error saving image:", err)
		return
	}

	setWallpaper(savePath)
}

func setWallpaper(path string) {
	candidates := [][]string{
		// GNOME / Cinnamon / MATE
		{"gsettings", "set", "org.gnome.desktop.background", "picture-uri", "file://" + path},
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
		if err := exec.Command(cmd[0], cmd[1:]...).Run(); err == nil {
			fmt.Println("Wallpaper set using:", cmd[0])
			return
		}
	}
	fmt.Println("No suitable wallpaper command found.")
}

func ensureInstall() {
	usr, _ := user.Current()
	localBin := filepath.Join(usr.HomeDir, ".local/bin")
	os.MkdirAll(localBin, 0755)

	execPath, _ := os.Executable()
	targetPath := filepath.Join(localBin, "bing-wallpaper")

	if execPath != targetPath {
		// Copy binary to ~/.local/bin
		input, err := os.ReadFile(execPath)
		if err == nil {
			os.WriteFile(targetPath, input, 0755)
			fmt.Println("Installed to", targetPath)
		}

		// Create autostart entry
		autoDir := filepath.Join(usr.HomeDir, ".config/autostart")
		os.MkdirAll(autoDir, 0755)
		desktopFile := filepath.Join(autoDir, "bing-wallpaper.desktop")
		content := `[Desktop Entry]
					Type=Application
					Exec=` + targetPath + `
					Hidden=false
					NoDisplay=false
					X-GNOME-Autostart-enabled=true
					Name=Bing Wallpaper
					Comment=Daily Bing Wallpaper
					`
		os.WriteFile(desktopFile, []byte(content), 0644)

		// Relaunch from installed location
		exec.Command(targetPath).Start()
		os.Exit(0)
	}
}
