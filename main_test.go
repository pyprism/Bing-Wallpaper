package main

import (
	"encoding/json"
	"fmt"
	"net/http"
	"net/http/httptest"
	"os"
	"os/user"
	"path/filepath"
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

func TestBingResponseEmpty(t *testing.T) {
	jsonData := `{"images": []}`
	var data BingResponse
	if err := json.Unmarshal([]byte(jsonData), &data); err != nil {
		t.Fatalf("Failed to parse JSON: %v", err)
	}
	if len(data.Images) != 0 {
		t.Errorf("Expected 0 images, got %d", len(data.Images))
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
	expected := filepath.Join(getSaveDir(tempDir), "20240101.jpg")
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
	saveDir := getSaveDir(tempDir)
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

func TestUpdateWallpaperAPIError(t *testing.T) {
	origBingURL := bingURL
	origHttpGet := httpGet
	defer func() {
		bingURL = origBingURL
		httpGet = origHttpGet
	}()
	bingURL = "http://localhost:1"
	httpGet = func(url string) (*http.Response, error) {
		return nil, fmt.Errorf("connection refused")
	}
	// Should not panic
	updateWallpaper()
}

func TestUpdateWallpaperInvalidJSON(t *testing.T) {
	origBingURL := bingURL
	origHttpGet := httpGet
	defer func() {
		bingURL = origBingURL
		httpGet = origHttpGet
	}()
	server := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte("not json"))
	}))
	defer server.Close()
	bingURL = server.URL
	httpGet = http.Get
	// Should not panic
	updateWallpaper()
}
