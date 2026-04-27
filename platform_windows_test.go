//go:build windows

package main

import (
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"strings"
	"testing"
)

func TestOpenSavedImagesDirWindows(t *testing.T) {
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
		if name != "explorer" {
			t.Errorf("Expected explorer, got %s", name)
		}
		if len(args) != 1 || !strings.Contains(args[0], "bing-wallpapers") {
			t.Errorf("Wrong directory: %v", args)
		}
		return exec.Command("cmd", "/c", "echo", "ok")
	}
	openSavedImagesDir()
	if !called {
		t.Error("explorer was not called")
	}
}

func TestSetWallpaperWindows(t *testing.T) {
	origExecCommand := execCommand
	defer func() { execCommand = origExecCommand }()
	calls := []string{}
	execCommand = func(name string, args ...string) *exec.Cmd {
		calls = append(calls, name+" "+strings.Join(args, " "))
		return exec.Command("cmd", "/c", "echo", "ok")
	}
	setWallpaper(`C:\Users\test\wallpaper.jpg`)
	if len(calls) == 0 {
		t.Error("No wallpaper command attempted")
	}
	if !strings.Contains(calls[0], "reg") {
		t.Errorf("First command should be reg, got: %s", calls[0])
	}
}

func TestEnsureInstallWindows(t *testing.T) {
	tempDir, _ := os.MkdirTemp("", "bing-test")
	defer os.RemoveAll(tempDir)
	// Override LOCALAPPDATA to use tempDir
	t.Setenv("LOCALAPPDATA", tempDir)
	origOsExecutable := osExecutable
	origOsExit := osExit
	origExecCommand := execCommand
	defer func() {
		osExecutable = origOsExecutable
		osExit = origOsExit
		execCommand = origExecCommand
	}()
	fakeExecPath := filepath.Join(tempDir, "fake-bing-wallpaper.exe")
	os.WriteFile(fakeExecPath, []byte("dummy binary"), 0755)
	osExecutable = func() (string, error) {
		return fakeExecPath, nil
	}
	exitCalled := false
	osExit = func(code int) { exitCalled = true }
	started := false
	execCommand = func(name string, args ...string) *exec.Cmd {
		started = true
		return exec.Command("cmd", "/c", "echo", "ok")
	}
	ensureInstall()
	target := filepath.Join(tempDir, "Programs", "bing-wallpaper", "bing-wallpaper.exe")
	if _, err := os.Stat(target); err != nil {
		t.Errorf("Binary not installed: %v", err)
	}
	if !started {
		t.Error("New process not started")
	}
	if !exitCalled {
		t.Error("os.Exit not called")
	}
}

func TestEnsureInstallAlreadyInstalledWindows(t *testing.T) {
	tempDir, _ := os.MkdirTemp("", "bing-test")
	defer os.RemoveAll(tempDir)
	t.Setenv("LOCALAPPDATA", tempDir)
	installDir := filepath.Join(tempDir, "Programs", "bing-wallpaper")
	os.MkdirAll(installDir, 0755)
	target := filepath.Join(installDir, "bing-wallpaper.exe")
	os.WriteFile(target, []byte("mock"), 0755)
	origOsExecutable := osExecutable
	origOsExit := osExit
	defer func() {
		osExecutable = origOsExecutable
		osExit = origOsExit
	}()
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

func TestGetSaveDirWindows(t *testing.T) {
	t.Setenv("APPDATA", `C:\Users\test\AppData\Roaming`)
	dir := getSaveDir(`C:\Users\test`)
	expected := filepath.Join(`C:\Users\test\AppData\Roaming`, "bing-wallpapers")
	if dir != expected {
		t.Errorf("Expected %s, got %s", expected, dir)
	}
}
