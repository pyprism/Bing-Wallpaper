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

var bingURL = "https://www.bing.com/HPImageArchive.aspx?format=js&idx=0&n=1"

//go:embed icon/bing.png
var iconData []byte

type BingResponse struct {
	Images []struct {
		URL  string `json:"url"`
		Date string `json:"startdate"`
	} `json:"images"`
}

var (
	execCommand      = exec.Command
	osExecutable     = os.Executable
	osExit           = os.Exit
	userCurrent      = user.Current
	httpGet          = http.Get
	setWallpaperFunc = setWallpaper
)

func main() {
	ensureInstall()

	bingApp := app.NewWithID("Bing Wallpaper")

	icon := fyne.NewStaticResource("bing.png", iconData)
	bingApp.SetIcon(icon)

	// System tray only works if supported
	if desk, ok := bingApp.(desktop.App); ok {
		menu := fyne.NewMenu("Bing Wallpaper",
			fyne.NewMenuItem("Set New Wallpaper Now", func() { go updateWallpaper() }),
			fyne.NewMenuItem("Browse Saved Images", func() { go openSavedImagesDir() }),
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
	resp, err := httpGet(bingURL)
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
	usr, _ := userCurrent()
	saveDir := getSaveDir(usr.HomeDir)
	os.MkdirAll(saveDir, 0755)

	savePath := filepath.Join(saveDir, data.Images[0].Date+".jpg")

	// Skip if already exists
	if _, err := os.Stat(savePath); err == nil {
		setWallpaperFunc(savePath)
		return
	}

	// Download image
	respImg, err := httpGet(imgURL)
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

	setWallpaperFunc(savePath)
}
