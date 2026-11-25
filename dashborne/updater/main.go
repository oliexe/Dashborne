package main

import (
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"
	"runtime/debug"
	"strconv"
	"strings"
	"syscall"
	"time"
)

var restartRequested bool
var detectOS = installedOS

type downloadProgress struct {
	written, total int64
	last           time.Time
}

func (p *downloadProgress) Write(b []byte) (int, error) {
	p.written += int64(len(b))
	if time.Since(p.last) > time.Second {
		status(fmt.Sprintf("Downloading release: %d%%", p.written*100/p.total))
		p.last = time.Now()
	}
	return len(b), nil
}
func download(c choice) error {
	if c.Archive.Size <= 0 || c.Archive.Size > 256<<20 {
		return fmt.Errorf("Invalid release size; refresh the release list")
	}
	if !c.Supported {
		return fmt.Errorf("This older release requires manual installation")
	}
	stock, e := detectOS()
	if e != nil {
		return e
	}
	if stock != c.OS {
		return fmt.Errorf("Release targets stock OS %s; installed OS is %s", c.OS, stock)
	}
	sum, e := bounded(c.Checksum.URL, 65536)
	if e != nil {
		return e
	}
	expected := ""
	for _, line := range strings.Split(string(sum), "\n") {
		fields := strings.Fields(line)
		if len(fields) == 2 && fields[1] == c.Archive.Name {
			if expected != "" {
				return fmt.Errorf("Duplicate archive checksum")
			}
			expected = fields[0]
		}
	}
	if len(expected) != 64 {
		return fmt.Errorf("Missing archive SHA256")
	}
	if _, e = hex.DecodeString(expected); e != nil {
		return e
	}
	if c.Archive.Digest != "" && c.Archive.Digest != "sha256:"+expected {
		return fmt.Errorf("GitHub and release checksums disagree")
	}
	if e = os.MkdirAll(work(), 0755); e != nil {
		return e
	}
	if e = spaceCheck(card, uint64(c.Archive.Size)+(768<<20)); e != nil {
		return e
	}
	status("Downloading Dashborne " + c.Version + "...")
	r, e := get(c.Archive.URL)
	if e != nil {
		return e
	}
	defer r.Body.Close()
	archive := filepath.Join(work(), "download.zip")
	out, e := os.Create(archive)
	if e != nil {
		return e
	}
	h := sha256.New()
	n, e := io.Copy(io.MultiWriter(out, h, &downloadProgress{total: c.Archive.Size}), io.LimitReader(r.Body, c.Archive.Size+1))
	if e == nil {
		e = out.Sync()
	}
	ce := out.Close()
	if e == nil {
		e = ce
	}
	if e != nil {
		return e
	}
	if n != c.Archive.Size || hex.EncodeToString(h.Sum(nil)) != expected {
		os.Remove(archive)
		return fmt.Errorf("Download checksum failed; installation unchanged")
	}
	status("Verifying and staging release...")
	stage := filepath.Join(work(), "stage")
	if e = os.RemoveAll(stage); e != nil {
		return e
	}
	m, e := extract(archive, stage, c.Version, stock)
	if e != nil {
		return e
	}
	if e = preserve(stage, m); e != nil {
		return e
	}
	if e = applyChecked(stage); e != nil {
		return e
	}
	os.Remove(archive)
	restartRequested = true
	return nil
}
func applyChecked(stage string) error {
	status("Installing on microSD...")
	if e := apply(stage); e != nil {
		j, err := loadJournal()
		if err == nil {
			j.Phase = "armed"
			if err = saveJournal(j); err == nil {
				err = recoverInstall()
			}
		}
		if err != nil {
			return fmt.Errorf("Installation interrupted; restart to recover: %w", e)
		}
		return e
	}
	return nil
}
func rollback() error {
	previous := filepath.Join(work(), "previous")
	m, e := readManifest(previous)
	if e != nil {
		return fmt.Errorf("No previous updater-compatible installation")
	}
	stock, e := detectOS()
	if e != nil {
		return e
	}
	if stock != m.OS {
		return fmt.Errorf("Previous installation targets another stock OS")
	}
	if e = spaceCheck(card, 768<<20); e != nil {
		return e
	}
	stage := filepath.Join(work(), "stage")
	if e = os.RemoveAll(stage); e != nil {
		return e
	}
	status("Preparing previous installation...")
	if e = copyTree(previous, stage, nil); e != nil {
		return e
	}
	if e = preserve(stage, m); e != nil {
		return e
	}
	if e = applyChecked(stage); e != nil {
		return e
	}
	restartRequested = true
	return nil
}
func restart() error {
	status("Installed. Restarting into the boot menu...")
	syscall.Sync()
	if e := exec.Command("/bin/setprop", "ctl.restart", "zkswe").Run(); e != nil {
		return fmt.Errorf("Installed. Restart the DASH to finish")
	}
	return nil
}
func main() {
	debug.SetMemoryLimit(48 << 20)
	debug.SetGCPercent(50)
	if len(os.Args) < 2 {
		fmt.Fprintln(os.Stderr, "Usage: updater list | install RELEASE_ID | rollback | recover | confirm")
		os.Exit(2)
	}
	if e := os.MkdirAll(work(), 0755); e != nil {
		status("Cannot write to microSD")
		os.Exit(1)
	}
	lock, e := os.OpenFile(filepath.Join(work(), "lock"), os.O_CREATE|os.O_RDWR, 0600)
	if e != nil {
		status("Cannot lock updater")
		os.Exit(1)
	}
	defer lock.Close()
	if e = syscall.Flock(int(lock.Fd()), syscall.LOCK_EX|syscall.LOCK_NB); e != nil {
		status("An update is already running")
		os.Exit(1)
	}
	switch os.Args[1] {
	case "list":
		if e = network(); e == nil {
			e = listReleases()
		}
	case "install":
		if len(os.Args) != 3 {
			e = fmt.Errorf("Select a release")
			break
		}
		var id int64
		id, e = strconv.ParseInt(os.Args[2], 10, 64)
		if e == nil {
			var c choice
			c, e = lookup(id)
			if e == nil {
				if e = network(); e == nil {
					e = download(c)
				}
			}
		}
	case "rollback":
		e = rollback()
	case "recover":
		e = recoverInstall()
	case "confirm":
		e = confirm()
	default:
		e = fmt.Errorf("Unknown updater command")
	}
	if e == nil && restartRequested {
		_ = syscall.Flock(int(lock.Fd()), syscall.LOCK_UN)
		_ = lock.Close()
		e = restart()
	}

	if e != nil {
		status("Update: " + e.Error())
		fmt.Fprintln(os.Stderr, e)
		os.Exit(1)
	}
}
