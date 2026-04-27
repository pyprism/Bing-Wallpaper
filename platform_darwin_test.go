//go:build darwin

package main

import (
	"os"
	"os/exec"
	"os/user"
	"path/filepath"
	"strings"
	"testing"
)

func TestOpenSavedImagesDirDarwin(t *testing.T) {
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
		if name != "open" {
			t.Errorf("Expected open, got %s", name)
		}
		if len(args) != 1 || !strings.Contains(args[0], "bing-wallpapers") {
			t.Errorf("Wrong directory: %v", args)
		}
		return exec.Command("true")
	}
	openSavedImagesDir()
	if !called {
		t.Error("open was not called")
	}
}

func TestSetWallpaperDarwin(t *testing.T) {
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
	if !strings.Contains(calls[0], "osascript") {
		t.Errorf("First command should be osascript, got: %s", calls[0])
	}
}

func TestEnsureInstallDarwin(t *testing.T) {
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
	target := filepath.Join(tempDir, ".local", "bin", "bing-wallpaper")
	if _, err := os.Stat(target); err != nil {
		t.Errorf("Binary not installed: %v", err)
	}
	plistFile := filepath.Join(tempDir, "Library", "LaunchAgents", "com.bing-wallpaper.plist")
	if _, err := os.Stat(plistFile); err != nil {
		t.Errorf("LaunchAgent plist not created: %v", err)
	}
	if !started {
		t.Error("New process not started")
	}
	if !exitCalled {
		t.Error("os.Exit not called")
	}
}

func TestEnsureInstallAlreadyInstalledDarwin(t *testing.T) {
	tempDir, _ := os.MkdirTemp("", "bing-test")
	defer os.RemoveAll(tempDir)
	localBin := filepath.Join(tempDir, ".local", "bin")
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

func TestGetSaveDirDarwin(t *testing.T) {
	dir := getSaveDir("/Users/testuser")
	expected := filepath.Join("/Users/testuser", "Library", "Application Support", "bing-wallpapers")
	if dir != expected {
		t.Errorf("Expected %s, got %s", expected, dir)
	}
}
