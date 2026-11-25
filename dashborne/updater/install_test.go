package main

import (
	"archive/zip"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"strings"
	"testing"
)

func put(t *testing.T, path string, b []byte) {
	t.Helper()
	if e := atomic(path, b, 0755); e != nil {
		t.Fatal(e)
	}
}
func fixture(t *testing.T) string {
	t.Helper()
	old := card
	oldStatus := statusPath
	card = t.TempDir()
	statusPath = filepath.Join(card, "test.status")
	t.Cleanup(func() { statusPath = oldStatus })
	t.Cleanup(func() { card = old; transactionStep = func(string) error { return nil } })
	for p, b := range map[string]string{"lib/libdashborne.so": "old", "bin/updater": "helper", "lib/update-recovery.so": "shim", "previous.cfg": "original startup", "extensions/custom/main.so": "community", "patches/catalog.tsv": "custom\tCustom\tDescription\tnative\n"} {
		put(t, filepath.Join(active(), p), []byte(b))
	}
	put(t, filepath.Join(card, "EasyUI.cfg"), []byte(`{"startupLibPath":"/mnt/extsd/Dashborne/lib/libdashborne.so","touchDev":"/dev/input/event0"}`))
	put(t, filepath.Join(card, "dashborne.conf"), []byte("wifi.password=private\nloader.autolaunch=1\n"))
	stage := filepath.Join(work(), "stage")
	put(t, filepath.Join(stage, "lib/libdashborne.so"), []byte("new"))
	put(t, filepath.Join(stage, "bin/updater"), []byte("new helper"))
	put(t, filepath.Join(stage, "lib/update-recovery.so"), []byte("new shim"))
	return stage
}
func content(t *testing.T, p string) string {
	t.Helper()
	b, e := os.ReadFile(p)
	if e != nil {
		t.Fatal(e)
	}
	return string(b)
}
func TestPowerLossRecovery(t *testing.T) {
	for _, step := range []string{"armed", "old-moved", "new-moved", "installed"} {
		t.Run(step, func(t *testing.T) {
			stage := fixture(t)
			transactionStep = func(s string) error {
				if s == step {
					return errors.New("power loss")
				}
				return nil
			}
			if apply(stage) == nil {
				t.Fatal("fault not reached")
			}
			if e := recoverInstall(); e != nil {
				t.Fatal(e)
			}
			if step == "installed" {
				if content(t, filepath.Join(active(), "lib/libdashborne.so")) != "new" {
					t.Fatal("new build not selected")
				} // unconfirmed startup rolls back on next boot
				if e := recoverInstall(); e != nil {
					t.Fatal(e)
				}
			}
			if content(t, filepath.Join(active(), "lib/libdashborne.so")) != "old" {
				t.Fatal("old build not restored")
			}
			if strings.Contains(content(t, filepath.Join(card, "EasyUI.cfg")), "recovery.so") {
				t.Fatal("startup not restored")
			}
			if content(t, filepath.Join(card, "dashborne.conf")) != "wifi.password=private\nloader.autolaunch=1\n" {
				t.Fatal("settings changed")
			}
			if e := recoverInstall(); e != nil {
				t.Fatal("recovery not idempotent", e)
			}
		})
	}
}
func TestConfirmedUpdate(t *testing.T) {
	stage := fixture(t)
	if e := apply(stage); e != nil {
		t.Fatal(e)
	}
	if e := confirm(); e == nil {
		t.Fatal("accepted confirmation before startup")
	}
	if e := recoverInstall(); e != nil {
		t.Fatal(e)
	}
	if e := confirm(); e != nil {
		t.Fatal(e)
	}
	if e := recoverInstall(); e != nil {
		t.Fatal(e)
	}
	if content(t, filepath.Join(active(), "lib/libdashborne.so")) != "new" {
		t.Fatal("lost confirmed update")
	}
	if content(t, filepath.Join(work(), "previous/lib/libdashborne.so")) != "old" {
		t.Fatal("lost backup")
	}
}
func TestPreserveCommunity(t *testing.T) {
	stage := fixture(t)
	put(t, filepath.Join(active(), "extensions/retired/main.so"), []byte("old"))
	put(t, filepath.Join(active(), "update-manifest.json"), []byte(`{"format":1,"version":"0.1.0","stock_os":"6.11.4","files":{"extensions/retired/main.so":"`+strings.Repeat("0", 64)+`"}}`))
	put(t, filepath.Join(stage, "patches/catalog.tsv"), []byte("bundled\tBundled\tNew\tnative\n"))
	put(t, filepath.Join(stage, "previous.cfg"), []byte("default"))
	m := manifest{Files: map[string]string{"extensions/bundled/main.so": "hash"}}
	if e := preserve(stage, m); e != nil {
		t.Fatal(e)
	}
	if !exists(filepath.Join(stage, "extensions/custom/main.so")) {
		t.Fatal("custom extension lost")
	}
	if exists(filepath.Join(stage, "extensions/retired")) {
		t.Fatal("retired extension retained")
	}
	if !strings.Contains(content(t, filepath.Join(stage, "patches/catalog.tsv")), "custom\t") {
		t.Fatal("custom catalog lost")
	}
	if content(t, filepath.Join(stage, "previous.cfg")) != "original startup" {
		t.Fatal("startup backup replaced")
	}
}
func makeZip(t *testing.T, extra string, corrupt bool) string {
	t.Helper()
	files := map[string][]byte{"lib/libdashborne.so": make([]byte, 52), "ui/loader.ftu": []byte("layout"), "bin/updater": []byte("helper"), "lib/update-recovery.so": []byte("shim"), "patches/catalog.tsv": []byte("bundled\tBundled\tDescription\tnative\n")}
	copy(files["lib/libdashborne.so"], "\x7fELF\x01\x01")
	files["lib/libdashborne.so"][18] = 40
	m := manifest{Format: 1, Version: "0.2.0", OS: "6.11.4", Files: map[string]string{}}
	for p, b := range files {
		h := sha256.Sum256(b)
		m.Files[p] = hex.EncodeToString(h[:])
	}
	if corrupt {
		files["ui/loader.ftu"] = []byte("wrong")
	}
	files["update-manifest.json"], _ = json.Marshal(m)
	path := filepath.Join(t.TempDir(), "release.zip")
	f, e := os.Create(path)
	if e != nil {
		t.Fatal(e)
	}
	z := zip.NewWriter(f)
	for p, b := range files {
		w, e := z.Create("card/Dashborne/" + p)
		if e != nil {
			t.Fatal(e)
		}
		w.Write(b)
	}
	if extra != "" {
		w, e := z.Create(extra)
		if e != nil {
			t.Fatal(e)
		}
		w.Write([]byte("bad"))
	}
	z.Close()
	f.Close()
	return path
}
func TestArchiveValidation(t *testing.T) {
	for _, extra := range []string{"card/Dashborne/../escape", "/absolute", "card/Dashborne/ui/LOADER.ftu", "card/Dashborne/unlisted", "card/Dashborne/bad\\path", "card/Dashborne/trailing."} {
		t.Run(extra, func(t *testing.T) {
			if _, e := extract(makeZip(t, extra, false), t.TempDir(), "0.2.0", "6.11.4"); e == nil {
				t.Fatal("accepted unsafe archive")
			}
		})
	}
	if _, e := extract(makeZip(t, "", true), t.TempDir(), "0.2.0", "6.11.4"); e == nil {
		t.Fatal("accepted wrong digest")
	}
	if _, e := extract(makeZip(t, "", false), t.TempDir(), "0.2.0", "6.11.2"); e == nil {
		t.Fatal("accepted wrong OS")
	}
	if _, e := extract(makeZip(t, "", false), t.TempDir(), "0.2.0", "6.11.4"); e != nil {
		t.Fatal(e)
	}
}
func TestURLAndText(t *testing.T) {
	for _, s := range []string{"http://github.com/a", "https://github.com.evil/a", "https://user@github.com/a", "https://github.com:443/a", "file:///etc/passwd"} {
		if allowedURL(s) {
			t.Fatal(s)
		}
	}
	if !allowedURL("https://release-assets.githubusercontent.com/a") {
		t.Fatal("asset blocked")
	}
	if clean("αβ\nword", 3) != "α" {
		t.Fatal("UTF8 truncation")
	}
}
