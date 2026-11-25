package main

import (
	"archive/zip"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"hash/crc32"
	"io"
	"os"
	"path/filepath"
	"strings"
)

type manifest struct {
	Format  int               `json:"format"`
	Version string            `json:"version"`
	OS      string            `json:"stock_os"`
	Files   map[string]string `json:"files"`
}
type journal struct {
	Phase   string
	Startup []byte
}

var card = "/mnt/extsd"
var statusPath = "/tmp/dashborne-update.status"
var listPath = "/tmp/dashborne-releases.tsv"
var cachePath = "/tmp/dashborne-releases.json"
var transactionStep = func(string) error { return nil } // fault injection in host tests
func work() string                                      { return filepath.Join(card, ".dashborne-update") }
func active() string                                    { return filepath.Join(card, "Dashborne") }
func syncDir(path string) error {
	f, e := os.Open(path)
	if e != nil {
		return e
	}
	defer f.Close()
	return f.Sync()
}
func atomic(path string, b []byte, mode os.FileMode) error {
	if e := os.MkdirAll(filepath.Dir(path), 0755); e != nil {
		return e
	}
	f, e := os.OpenFile(path+".next", os.O_CREATE|os.O_TRUNC|os.O_WRONLY, mode)
	if e != nil {
		return e
	}
	_, e = f.Write(b)
	if e == nil {
		e = f.Sync()
	}
	ce := f.Close()
	if e == nil {
		e = ce
	}
	if e != nil {
		return e
	}
	if e = os.Rename(path+".next", path); e != nil {
		return e
	}
	return syncDir(filepath.Dir(path))
}
func status(s string)      { _ = atomic(statusPath, []byte(clean(s, 240)), 0600) }
func exists(p string) bool { _, e := os.Lstat(p); return e == nil }
func copyFile(src, dst string) error {
	f, e := os.Open(src)
	if e != nil {
		return e
	}
	defer f.Close()
	info, e := f.Stat()
	if e != nil || !info.Mode().IsRegular() {
		return fmt.Errorf("Cannot copy non-regular file")
	}
	if e = os.MkdirAll(filepath.Dir(dst), 0755); e != nil {
		return e
	}
	o, e := os.OpenFile(dst, os.O_CREATE|os.O_EXCL|os.O_WRONLY, info.Mode().Perm())
	if e != nil {
		return e
	}
	_, e = io.Copy(o, f)
	if e == nil {
		e = o.Sync()
	}
	ce := o.Close()
	if e == nil {
		e = ce
	}
	return e
}
func copyTree(src, dst string, skip func(string) bool) error {
	copied := 0
	return filepath.WalkDir(src, func(path string, d os.DirEntry, e error) error {
		if e != nil {
			return e
		}
		r, e := filepath.Rel(src, path)
		if e != nil {
			return e
		}
		if r == "." {
			return os.MkdirAll(dst, 0755)
		}
		if skip != nil && skip(r) {
			if d.IsDir() {
				return filepath.SkipDir
			}
			return nil
		}
		if d.Type()&os.ModeSymlink != 0 {
			return fmt.Errorf("Symlink in installation")
		}
		to := filepath.Join(dst, r)
		if d.IsDir() {
			return os.MkdirAll(to, 0755)
		}
		if info, err := os.Lstat(to); err == nil {
			if !info.Mode().IsRegular() {
				return fmt.Errorf("Cannot replace non-regular staged file")
			}
			if err = os.Remove(to); err != nil {
				return err
			}
		}
		if e := copyFile(path, to); e != nil {
			return e
		}
		copied++
		if copied%32 == 0 {
			status(fmt.Sprintf("Preparing files: %d copied...", copied))
		}
		return nil
	})
}
func shaFile(path string) (string, error) {
	f, e := os.Open(path)
	if e != nil {
		return "", e
	}
	defer f.Close()
	h := sha256.New()
	_, e = io.Copy(h, f)
	return hex.EncodeToString(h.Sum(nil)), e
}
func installedOS() (string, error) {
	f, e := os.Open("/res/lib/libzkgui.so")
	if e != nil {
		return "", e
	}
	defer f.Close()
	h := crc32.NewIEEE()
	n, e := io.Copy(h, io.LimitReader(f, 32<<20))
	if e != nil {
		return "", e
	}
	if n == 3464120 {
		switch h.Sum32() {
		case 0xed0e1aa4:
			return "6.11.4", nil
		case 0x9de99542:
			return "6.11.2", nil
		}
	}
	return "", fmt.Errorf("Installed stock OS is not recognized; update compatibility must be checked first")
}
func validPath(p string) bool {
	if p == "" || strings.ContainsAny(p, "\\:\x00") || strings.HasPrefix(p, "/") || filepath.Clean(p) != p {
		return false
	}
	for _, s := range strings.Split(p, "/") {
		if s == ".." || strings.HasSuffix(s, ".") || strings.HasSuffix(s, " ") {
			return false
		}
	}
	return true
}
func readManifest(dir string) (manifest, error) {
	var m manifest
	b, e := os.ReadFile(filepath.Join(dir, "update-manifest.json"))
	if e != nil {
		return m, e
	}
	if len(b) > 2<<20 {
		return m, fmt.Errorf("Manifest too large")
	}
	e = json.Unmarshal(b, &m)
	if e != nil {
		return m, e
	}
	if m.Format != 1 || !semver.MatchString(m.Version) || !semver.MatchString(m.OS) || len(m.Files) == 0 || len(m.Files) > 4096 {
		return m, fmt.Errorf("Unsupported update manifest")
	}
	for p, h := range m.Files {
		if !validPath(p) || len(h) != 64 {
			return m, fmt.Errorf("Invalid manifest file")
		}
		if _, e = hex.DecodeString(h); e != nil {
			return m, e
		}
	}
	return m, nil
}
func extract(archive, dir, version, stock string) (manifest, error) {
	var empty manifest
	z, e := zip.OpenReader(archive)
	if e != nil {
		return empty, e
	}
	defer z.Close()
	if len(z.File) > 5000 {
		return empty, fmt.Errorf("Too many archive entries")
	}
	var total uint64
	seen := map[string]bool{}
	for _, f := range z.File {
		p := strings.TrimSuffix(f.Name, "/")
		key := strings.ToLower(p)
		if !validPath(p) || seen[key] || f.Mode()&os.ModeSymlink != 0 || (!f.FileInfo().IsDir() && !f.Mode().IsRegular()) {
			return empty, fmt.Errorf("Unsafe archive entry")
		}
		seen[key] = true
		total += f.UncompressedSize64
		if total > 512<<20 || f.UncompressedSize64 > 128<<20 {
			return empty, fmt.Errorf("Archive is too large")
		}
	}
	for index, f := range z.File {
		if index%32 == 0 {
			status(fmt.Sprintf("Extracting release: %d%%", index*100/len(z.File)))
		}
		if !strings.HasPrefix(f.Name, "card/Dashborne/") || f.FileInfo().IsDir() {
			continue
		}
		p := strings.TrimPrefix(f.Name, "card/Dashborne/")
		to := filepath.Join(dir, p)
		if e = os.MkdirAll(filepath.Dir(to), 0755); e != nil {
			return empty, e
		}
		in, e := f.Open()
		if e != nil {
			return empty, e
		}
		out, e := os.OpenFile(to, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0755)
		if e != nil {
			in.Close()
			return empty, e
		}
		n, err := io.Copy(out, io.LimitReader(in, int64(f.UncompressedSize64)+1))
		in.Close()
		if err == nil && n != int64(f.UncompressedSize64) {
			err = fmt.Errorf("Truncated archive")
		}
		if err == nil {
			err = out.Sync()
		}
		ce := out.Close()
		if err == nil {
			err = ce
		}
		if err != nil {
			return empty, err
		}
	}
	m, e := readManifest(dir)
	if e != nil {
		return empty, fmt.Errorf("This release requires manual installation: %w", e)
	}
	if m.Version != version || m.OS != stock {
		return empty, fmt.Errorf("Release version or stock OS target mismatch")
	}
	actual := map[string]bool{}
	e = filepath.WalkDir(dir, func(path string, d os.DirEntry, e error) error {
		if e != nil {
			return e
		}
		if d.IsDir() {
			return nil
		}
		r, _ := filepath.Rel(dir, path)
		if r == "update-manifest.json" {
			return nil
		}
		hash, ok := m.Files[r]
		if !ok {
			return fmt.Errorf("Unlisted release file")
		}
		got, e := shaFile(path)
		if e != nil {
			return e
		}
		if got != hash {
			return fmt.Errorf("Release file checksum mismatch")
		}
		actual[r] = true
		return nil
	})
	if e != nil {
		return empty, e
	}
	if len(actual) != len(m.Files) {
		return empty, fmt.Errorf("Incomplete release")
	}
	for _, p := range []string{"lib/libdashborne.so", "ui/loader.ftu", "bin/updater", "lib/update-recovery.so"} {
		if !actual[p] {
			return empty, fmt.Errorf("Missing updater component: %s", p)
		}
	}
	b, e := os.ReadFile(filepath.Join(dir, "lib/libdashborne.so"))
	if e != nil || len(b) < 52 || string(b[:6]) != "\x7fELF\x01\x01" || b[18] != 40 || b[19] != 0 {
		return empty, fmt.Errorf("Release is not an ARM Dashborne build")
	}
	return m, nil
}
func managed(m manifest) map[string]bool {
	out := map[string]bool{}
	for p := range m.Files {
		parts := strings.Split(p, "/")
		if len(parts) > 2 && parts[0] == "extensions" {
			out[parts[1]] = true
		}
	}
	return out
}
func preserve(stage string, new manifest) error {
	// The startup backup belongs to this card, not the release package.
	if exists(filepath.Join(stage, "previous.cfg")) {
		if e := os.Remove(filepath.Join(stage, "previous.cfg")); e != nil {
			return e
		}
	}
	old, _ := readManifest(active())
	owned := managed(old)
	for k := range managed(new) {
		owned[k] = true
	}
	e := copyTree(active(), stage, func(p string) bool {
		parts := strings.Split(p, "/")
		switch parts[0] {
		case "lib", "bin", "ui", "assets":
			return true
		case "extensions":
			return len(parts) > 1 && owned[parts[1]]
		case "patches":
			return p == "patches/catalog.tsv" || new.Files[p] != "" || old.Files[p] != ""
		case "update-manifest.json", "config.once", "stock.once", "settings.conf", "wifi.conf":
			return true
		}
		return exists(filepath.Join(stage, p)) && len(parts) == 1 && parts[0] != "extensions" && parts[0] != "patches"
	})
	if e != nil {
		return e
	}
	// Preserve custom catalog rows and ordering without duplicating shipped IDs.
	catalog := filepath.Join(stage, "patches/catalog.tsv")
	fresh, e := os.ReadFile(catalog)
	if e != nil {
		return e
	}
	previous, _ := os.ReadFile(filepath.Join(active(), "patches/catalog.tsv"))
	ids := map[string]bool{}
	for _, l := range strings.Split(string(fresh), "\n") {
		ids[strings.Split(l, "\t")[0]] = true
	}
	for _, l := range strings.Split(string(previous), "\n") {
		fields := strings.Split(l, "\t")
		if len(fields) < 3 || ids[fields[0]] || owned[fields[0]] {
			continue
		}
		fresh = append(fresh, []byte(l+"\n")...)
		ids[fields[0]] = true
	}
	return atomic(catalog, fresh, 0644)
}
func saveJournal(j journal) error {
	b, e := json.Marshal(j)
	if e != nil {
		return e
	}
	return atomic(filepath.Join(work(), "journal.json"), b, 0600)
}
func loadJournal() (journal, error) {
	var j journal
	b, e := os.ReadFile(filepath.Join(work(), "journal.json"))
	if e == nil {
		e = json.Unmarshal(b, &j)
	}
	return j, e
}
func apply(stage string) error {
	if exists(filepath.Join(work(), "journal.json")) {
		return fmt.Errorf("An unfinished update must be recovered first")
	}
	startup, e := os.ReadFile(filepath.Join(card, "EasyUI.cfg"))
	if e != nil {
		return e
	}
	var cfg map[string]any
	if e = json.Unmarshal(startup, &cfg); e != nil {
		return e
	}
	if cfg["startupLibPath"] != "/mnt/extsd/Dashborne/lib/libdashborne.so" {
		return fmt.Errorf("Dashborne is not the selected startup app")
	}
	// Recovery lives outside both installations, including its own executable.
	for _, p := range []struct{ src, dst string }{{"bin/updater", "updater"}, {"lib/update-recovery.so", "recovery.so"}} {
		b, e := os.ReadFile(filepath.Join(active(), p.src))
		if e != nil {
			return e
		}
		if e = atomic(filepath.Join(work(), p.dst), b, 0755); e != nil {
			return e
		}
	}
	if e = atomic(filepath.Join(stage, "config.once"), nil, 0644); e != nil {
		return e
	}
	if e = syncTree(stage); e != nil {
		return e
	}
	if e = os.RemoveAll(filepath.Join(work(), "previous")); e != nil {
		return e
	}
	j := journal{Phase: "armed", Startup: startup}
	if e = saveJournal(j); e != nil {
		return e
	}
	cfg["startupLibPath"] = "/mnt/extsd/.dashborne-update/recovery.so"
	b, e := json.Marshal(cfg)
	if e != nil {
		return e
	}
	if e = atomic(filepath.Join(card, "EasyUI.cfg"), b, 0644); e != nil {
		return e
	}
	if e = transactionStep("armed"); e != nil {
		return e
	}
	if e = os.Rename(active(), filepath.Join(work(), "previous")); e != nil {
		return e
	}
	if e = syncDir(card); e != nil {
		return e
	}
	if e = syncDir(work()); e != nil {
		return e
	}
	if e = transactionStep("old-moved"); e != nil {
		return e
	}
	if e = os.Rename(stage, active()); e != nil {
		return e
	}
	if e = syncDir(card); e != nil {
		return e
	}
	if e = syncDir(filepath.Dir(stage)); e != nil {
		return e
	}
	if e = transactionStep("new-moved"); e != nil {
		return e
	}
	j.Phase = "installed"
	if e = saveJournal(j); e != nil {
		return e
	}
	return transactionStep("installed")
}
func syncTree(path string) error {
	return filepath.WalkDir(path, func(p string, d os.DirEntry, e error) error {
		if e != nil {
			return e
		}
		if d.IsDir() {
			return syncDir(p)
		}
		return nil
	})
}
func recoverInstall() error {
	j, e := loadJournal()
	if os.IsNotExist(e) {
		return nil
	}
	if e != nil {
		return e
	}
	switch j.Phase {
	case "installed":
		j.Phase = "booting"
		return saveJournal(j)
	case "armed", "booting":
	default:
		return fmt.Errorf("Unknown recovery state")
	}
	previous := filepath.Join(work(), "previous")
	if exists(previous) {
		if e = os.RemoveAll(active()); e != nil {
			return e
		}
		if e = os.Rename(previous, active()); e != nil {
			return e
		}
		if e = syncDir(card); e != nil {
			return e
		}
		if e = syncDir(work()); e != nil {
			return e
		}
	}
	if !exists(filepath.Join(active(), "lib/libdashborne.so")) {
		return fmt.Errorf("Previous installation is unavailable")
	}
	if e = atomic(filepath.Join(card, "EasyUI.cfg"), j.Startup, 0644); e != nil {
		return e
	}
	if e = atomic(filepath.Join(active(), "config.once"), nil, 0644); e != nil {
		return e
	}
	if e = os.Remove(filepath.Join(work(), "journal.json")); e != nil {
		return e
	}
	return syncDir(work())
}
func confirm() error {
	j, e := loadJournal()
	if os.IsNotExist(e) {
		return nil
	}
	if e != nil {
		return e
	}
	if j.Phase != "booting" {
		return fmt.Errorf("Update has not booted")
	}
	if e = atomic(filepath.Join(card, "EasyUI.cfg"), j.Startup, 0644); e != nil {
		return e
	}
	if e = os.Remove(filepath.Join(work(), "journal.json")); e != nil {
		return e
	}
	return syncDir(work())
}
