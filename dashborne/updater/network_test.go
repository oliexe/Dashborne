package main

import (
	"bytes"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"io"
	"net/http"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

type transportFunc func(*http.Request) (*http.Response, error)

func (f transportFunc) RoundTrip(r *http.Request) (*http.Response, error) { return f(r) }
func TestDownloadTransaction(t *testing.T) {
	for _, fault := range []string{"success", "checksum", "short", "HTTP", "OS"} {
		t.Run(fault, func(t *testing.T) {
			fixture(t)
			archive := makeZip(t, "", false)
			b, e := os.ReadFile(archive)
			if e != nil {
				t.Fatal(e)
			}
			h := sha256.Sum256(b)
			sum := hex.EncodeToString(h[:])
			if fault == "checksum" {
				sum = strings.Repeat("0", 64)
			}
			c := choice{Version: "0.2.0", OS: "6.11.4", Supported: true, Archive: asset{Name: "Dashborne-0.2.0-microSD.zip", URL: "https://github.com/archive", Size: int64(len(b))}, Checksum: asset{URL: "https://github.com/checksum"}}
			oldClient, oldOS := client, detectOS
			t.Cleanup(func() { client = oldClient; detectOS = oldOS; restartRequested = false })
			detectOS = func() (string, error) {
				if fault == "OS" {
					return "6.11.2", nil
				}
				return "6.11.4", nil
			}
			client = &http.Client{Transport: transportFunc(func(r *http.Request) (*http.Response, error) {
				data := b
				code := 200
				if r.URL.Path == "/checksum" {
					data = []byte(sum + "  " + c.Archive.Name + "\n")
				} else if fault == "short" {
					data = b[:len(b)/2]
				} else if fault == "HTTP" {
					code = 503
				}
				return &http.Response{StatusCode: code, Body: io.NopCloser(bytes.NewReader(data)), Header: http.Header{}}, nil
			})}
			e = download(c)
			if fault == "success" {
				if e != nil {
					t.Fatal(e)
				}
				if !restartRequested {
					t.Fatal("no restart")
				}
				if e = recoverInstall(); e != nil {
					t.Fatal(e)
				}
				if e = confirm(); e != nil {
					t.Fatal(e)
				}
				if !exists(filepath.Join(active(), "extensions/custom/main.so")) {
					t.Fatal("custom extension missing")
				}
			} else {
				if e == nil {
					t.Fatal("accepted fault", fault)
				}
				if content(t, filepath.Join(active(), "lib/libdashborne.so")) != "old" {
					t.Fatal("changed active installation on error")
				}
				if exists(filepath.Join(work(), "journal.json")) {
					t.Fatal("armed transaction before validation")
				}
			}
		})
	}
}
func TestReleaseFiltering(t *testing.T) {
	old := client
	defer func() { client = old }()
	client = &http.Client{Transport: transportFunc(func(r *http.Request) (*http.Response, error) {
		data := `[{"id":1,"tag_name":"v0.2.0","name":"Dashborne 0.2.0 (Stock OS 6.11.4)","body":"Release notes","assets":[{"name":"Dashborne-0.2.0-microSD.zip","browser_download_url":"https://github.com/archive","size":120},{"name":"SHA256SUMS","browser_download_url":"https://github.com/sum"},{"name":"updater-v1.json"}]},{"id":2,"draft":true,"tag_name":"v0.3.0","name":"Dashborne 0.3.0 (Stock OS 6.11.4)"}]`
		return &http.Response{StatusCode: 200, Body: io.NopCloser(strings.NewReader(data)), Header: http.Header{}}, nil
	})}
	cs, e := choices()
	if e != nil || len(cs) != 1 || !cs[0].Supported {
		t.Fatal(fmt.Sprint(cs), e)
	}
}
