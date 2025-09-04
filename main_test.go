package main

import (
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"strings"
	"testing"
)

// --- Mocks and helpers ---

func setupMockBingServer() *httptest.Server {
	return httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		mockResponse := `{
			"images": [
				{
					"url": "/th?id=OHR.SampleImage_EN-US1234567890_1920x1080.jpg",
					"startdate": "20240101"
				}
			]
		}`
		w.Write([]byte(mockResponse))
	}))
}

func setupMockImageServer() *httptest.Server {
	return httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "image/jpeg")
		w.Write([]byte{0xFF, 0xD8, 0xFF, 0xDB}) // JPEG header
	}))
}

// --- Tests ---

func TestBingResponseParsing(t *testing.T) {
	jsonData := `{
		"images": [
			{"url": "/th?id=OHR.TestImage_EN-US1234567890_1920x1080.jpg", "startdate": "20240301"}
		]
	}`
	var data BingResponse
	err := json.Unmarshal([]byte(jsonData), &data)
	if err != nil {
		t.Fatalf("Failed to parse JSON: %v", err)
	}
	if len(data.Images) != 1 {
		t.Fatalf("Expected 1 image, got %d", len(data.Images))
	}
	if data.Images[0].URL != "/th?id=OHR.TestImage_EN-US1234567890_1920x1080.jpg" {
		t.Errorf("Wrong URL parsed, got: %s", data.Images[0].URL)
	}
	if data.Images[0].Date != "20240301" {
		t.Errorf("Wrong date parsed, got: %s", data.Images[0].Date)
	}
}

func TestOpenSavedImagesDir(t *testing.T) {
	tempDir, _ := os.MkdirTemp("", "bing-test")
	defer os.RemoveAll(tempDir)
	origUserCurrent := userCurrent
	origExecCommand := execCommand
	defer func() {
		userCurrent = origUserCurrent
		execCommand = origExecCommand
	}()
	userCurrent = func() (*user.User, error) {
		return &user.User{HomeDir: tempDir}, nil
	}
	called := false
	execCommand = func(name string, args ...string) *exec.Cmd {
		called = true
		if name != "xdg-open" {
			t.Errorf("Expected xdg-open, got %s", name)
		}
		if len(args) != 1 || !strings.Contains(args[0], ".local/share/bing-wallpapers") {
			t.Errorf("Wrong directory: %v", args)
		}
		return exec.Command("true")
	}
	openSavedImagesDir()
	if !called {
		t.Error("xdg-open was not called")
	}
}

func TestUpdateWallpaper(t *testing.T) {
	tempDir, _ := os.MkdirTemp("", "bing-test")
	defer os.RemoveAll(tempDir)
	mockBing := setupMockBingServer()
	defer mockBing.Close()
	mockImg := setupMockImageServer()
	defer mockImg.Close()
	origBingURL := bingURL
	origHttpGet := httpGet
	origUserCurrent := userCurrent
	origSetWallpaperFunc := setWallpaperFunc
	defer func() {
		bingURL = origBingURL
		httpGet = origHttpGet
		userCurrent = origUserCurrent
		setWallpaperFunc = origSetWallpaperFunc
	}()
	bingURL = mockBing.URL
	httpGet = func(url string) (*http.Response, error) {
		if url == mockBing.URL {
			return http.Get(url)
		}
		return http.Get(mockImg.URL)
	}
	userCurrent = func() (*user.User, error) {
		return &user.User{HomeDir: tempDir}, nil
	}
	calledPath := ""
	setWallpaperFunc = func(path string) { calledPath = path }
	updateWallpaper()
	expected := filepath.Join(tempDir, ".local/share/bing-wallpapers/20240101.jpg")
	if _, err := os.Stat(expected); err != nil {
		t.Errorf("Image not saved: %v", err)
	}
	if calledPath != expected {
		t.Errorf("setWallpaper called with wrong path: %s", calledPath)
	}
}

func TestUpdateWallpaperExistingImage(t *testing.T) {
	tempDir, _ := os.MkdirTemp("", "bing-test")
	defer os.RemoveAll(tempDir)
	mockBing := setupMockBingServer()
	defer mockBing.Close()
	origBingURL := bingURL
	origHttpGet := httpGet
	origUserCurrent := userCurrent
	origSetWallpaperFunc := setWallpaperFunc
	defer func() {
		bingURL = origBingURL
		httpGet = origHttpGet
		userCurrent = origUserCurrent
		setWallpaperFunc = origSetWallpaperFunc
	}()
	bingURL = mockBing.URL
	downloaded := false
	httpGet = func(url string) (*http.Response, error) {
		if url == mockBing.URL {
			return http.Get(url)
		}
		downloaded = true
		return nil, fmt.Errorf("should not download")
	}
	userCurrent = func() (*user.User, error) {
		return &user.User{HomeDir: tempDir}, nil
	}
	saveDir := filepath.Join(tempDir, ".local/share/bing-wallpapers")
	os.MkdirAll(saveDir, 0755)
	existing := filepath.Join(saveDir, "20240101.jpg")
	os.WriteFile(existing, []byte{0xFF, 0xD8, 0xFF, 0xDB}, 0644)
	calledPath := ""
	setWallpaperFunc = func(path string) { calledPath = path }
	updateWallpaper()
	if downloaded {
		t.Error("Image was downloaded even though it exists")
	}
	if calledPath != existing {
		t.Errorf("setWallpaper not called with existing image: %s", calledPath)
	}
}

func TestSetWallpaper(t *testing.T) {
	origExecCommand := execCommand
	defer func() { execCommand = origExecCommand }()
	calls := []string{}
	execCommand = func(name string, args ...string) *exec.Cmd {
		calls = append(calls, name+" "+strings.Join(args, " "))
		return exec.Command("true")
	}
	setWallpaper("/tmp/test.jpg")
	if len(calls) == 0 {
		t.Error("No wallpaper command attempted")
	}
	if !strings.Contains(calls[0], "gsettings") {
		t.Errorf("First command should be gsettings, got: %s", calls[0])
	}
}

func TestEnsureInstall(t *testing.T) {
	tempDir, _ := os.MkdirTemp("", "bing-test")
	defer os.RemoveAll(tempDir)
	origUserCurrent := userCurrent
	origOsExecutable := osExecutable
	origOsExit := osExit
	origExecCommand := execCommand
	defer func() {
		userCurrent = origUserCurrent
		osExecutable = origOsExecutable
		osExit = origOsExit
		execCommand = origExecCommand
	}()
	userCurrent = func() (*user.User, error) {
		return &user.User{HomeDir: tempDir}, nil
	}
	// Create a dummy binary file to simulate the executable
	fakeExecPath := filepath.Join(tempDir, "fake-bing-wallpaper")
	os.WriteFile(fakeExecPath, []byte("dummy binary"), 0755)
	osExecutable = func() (string, error) {
		return fakeExecPath, nil
	}
	exitCalled := false
	osExit = func(code int) { exitCalled = true }
	started := false
	execCommand = func(name string, args ...string) *exec.Cmd {
		started = true
		return exec.Command("true")
	}
	ensureInstall()
	target := filepath.Join(tempDir, ".local/bin/bing-wallpaper")
	if _, err := os.Stat(target); err != nil {
		t.Errorf("Binary not installed: %v", err)
	}
	desktopFile := filepath.Join(tempDir, ".config/autostart/bing-wallpaper.desktop")
	if _, err := os.Stat(desktopFile); err != nil {
		t.Errorf("Desktop file not created: %v", err)
	}
	if !started {
		t.Error("New process not started")
	}
	if !exitCalled {
		t.Error("os.Exit not called")
	}
}

func TestEnsureInstallAlreadyInstalled(t *testing.T) {
	tempDir, _ := os.MkdirTemp("", "bing-test")
	defer os.RemoveAll(tempDir)
	localBin := filepath.Join(tempDir, ".local/bin")
	os.MkdirAll(localBin, 0755)
	target := filepath.Join(localBin, "bing-wallpaper")
	os.WriteFile(target, []byte("mock"), 0755)
	origUserCurrent := userCurrent
	origOsExecutable := osExecutable
	origOsExit := osExit
	defer func() {
		userCurrent = origUserCurrent
		osExecutable = origOsExecutable
		osExit = origOsExit
	}()
	userCurrent = func() (*user.User, error) {
		return &user.User{HomeDir: tempDir}, nil
	}
	osExecutable = func() (string, error) {
		return target, nil
	}
	exitCalled := false
	osExit = func(code int) { exitCalled = true }
	ensureInstall()
	if exitCalled {
		t.Error("os.Exit should not be called if already installed")
	}
}
