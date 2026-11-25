package main

import (
	"crypto/tls"
	"crypto/x509"
	_ "embed"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"os"
	"regexp"
	"strings"
	"time"
	"unicode/utf8"
)

// Mozilla CA bundle, distributed by curl under MPL-2.0 (see roots.pem).
//
//go:embed roots.pem
var roots []byte
var buildDate = "2026-01-01"
var api = "https://api.github.com/repos/oliexe/Dashborne/releases?per_page=100"
var client *http.Client
var semver = regexp.MustCompile(`^v?([0-9]+\.[0-9]+\.[0-9]+)$`)
var target = regexp.MustCompile(`\(Stock OS ([0-9]+\.[0-9]+\.[0-9]+)\)`)

type asset struct {
	Name   string
	URL    string `json:"browser_download_url"`
	Size   int64
	Digest string
}
type release struct {
	ID                int64
	Tag               string `json:"tag_name"`
	Name, Body        string
	Draft, Prerelease bool
	Assets            []asset
}
type choice struct {
	ID                 int64
	Version, OS, Notes string
	Archive, Checksum  asset
	Supported          bool
}

func allowedURL(raw string) bool {
	u, e := url.Parse(raw)
	if e != nil || u.Scheme != "https" || u.User != nil || u.Port() != "" {
		return false
	}
	switch u.Hostname() {
	case "api.github.com", "github.com", "release-assets.githubusercontent.com", "objects.githubusercontent.com":
		return true
	}
	return false
}

// The DASH can start at 1970. Obtain a time estimate for TLS validation only;
// never change the OS clock, disable certificate checks or trust HTTP Date.
func tlsClock() (func() time.Time, error) {
	floor, e := time.Parse("2006-01-02", buildDate)
	if e != nil {
		return nil, e
	}
	now := time.Now()
	if now.After(floor) {
		return time.Now, nil
	}
	for _, host := range []string{"time.cloudflare.com:123", "time.google.com:123", "pool.ntp.org:123"} {
		c, e := net.DialTimeout("udp", host, 3*time.Second)
		if e != nil {
			continue
		}
		c.SetDeadline(time.Now().Add(3 * time.Second))
		p := make([]byte, 48)
		p[0] = 0x23
		nonce := uint64(time.Now().UnixNano())
		binary.BigEndian.PutUint64(p[40:], nonce)
		_, e = c.Write(p)
		reply := make([]byte, 48)
		n, err := c.Read(reply)
		c.Close()
		if e != nil || err != nil || n != 48 || reply[0]&7 != 4 || reply[0]>>6 == 3 || reply[1] == 0 || reply[1] > 15 || binary.BigEndian.Uint64(reply[24:]) != nonce {
			continue
		}
		t := time.Unix(int64(binary.BigEndian.Uint32(reply[40:]))-2208988800, 0)
		if t.Before(floor) || t.After(floor.AddDate(10, 0, 0)) {
			continue
		}
		start := time.Now()
		return func() time.Time { return t.Add(time.Since(start)) }, nil
	}
	return nil, fmt.Errorf("Cannot obtain time for HTTPS. Check internet access and retry")
}
func network() error {
	clock, e := tlsClock()
	if e != nil {
		return e
	}
	pool := x509.NewCertPool()
	if !pool.AppendCertsFromPEM(roots) {
		return fmt.Errorf("Missing HTTPS certificates")
	}
	client = &http.Client{Timeout: 10 * time.Minute, Transport: &http.Transport{TLSClientConfig: &tls.Config{RootCAs: pool, MinVersion: tls.VersionTLS12, Time: clock}, DialContext: (&net.Dialer{Timeout: 12 * time.Second}).DialContext, TLSHandshakeTimeout: 15 * time.Second, ResponseHeaderTimeout: 20 * time.Second}, CheckRedirect: func(r *http.Request, via []*http.Request) error {
		if len(via) > 6 || !allowedURL(r.URL.String()) {
			return fmt.Errorf("Unexpected download redirect")
		}
		return nil
	}}
	return nil
}
func get(raw string) (*http.Response, error) {
	if !allowedURL(raw) {
		return nil, fmt.Errorf("Unexpected release URL")
	}
	req, e := http.NewRequest("GET", raw, nil)
	if e != nil {
		return nil, e
	}
	req.Header.Set("User-Agent", "Dashborne-Updater")
	req.Header.Set("Accept", "application/vnd.github+json")
	r, e := client.Do(req)
	if e != nil {
		return nil, fmt.Errorf("HTTPS request failed: %w", e)
	}
	if r.StatusCode != 200 {
		r.Body.Close()
		return nil, fmt.Errorf("GitHub returned HTTP %d; retry later", r.StatusCode)
	}
	return r, nil
}
func bounded(raw string, limit int64) ([]byte, error) {
	r, e := get(raw)
	if e != nil {
		return nil, e
	}
	defer r.Body.Close()
	b, e := io.ReadAll(io.LimitReader(r.Body, limit+1))
	if int64(len(b)) > limit {
		return nil, fmt.Errorf("Response too large")
	}
	return b, e
}
func clean(s string, n int) string {
	s = strings.ToValidUTF8(s, "")
	s = strings.Map(func(r rune) rune {
		if r < 32 || r == 127 {
			return ' '
		}
		return r
	}, s)
	if len(s) > n {
		s = s[:n]
		for !utf8.ValidString(s) {
			s = s[:len(s)-1]
		}
	}
	return s
}

func choices() ([]choice, error) {
	b, e := bounded(api, 4<<20)
	if e != nil {
		return nil, e
	}
	var rs []release
	if e = json.Unmarshal(b, &rs); e != nil {
		return nil, e
	}
	out := []choice{}
	for _, r := range rs {
		v := semver.FindStringSubmatch(r.Tag)
		osv := target.FindStringSubmatch(r.Name)
		if r.Draft || r.Prerelease || len(v) != 2 || len(osv) != 2 {
			continue
		}
		c := choice{ID: r.ID, Version: v[1], OS: osv[1], Notes: clean(r.Body, 400)}
		for _, a := range r.Assets {
			if a.Name == "Dashborne-"+c.Version+"-microSD.zip" {
				c.Archive = a
			}
			if a.Name == "SHA256SUMS" {
				c.Checksum = a
			}
			if a.Name == "updater-v1.json" {
				c.Supported = true
			}
		}
		if c.Archive.URL != "" && c.Checksum.URL != "" && allowedURL(c.Archive.URL) && allowedURL(c.Checksum.URL) && c.Archive.Size > 0 && c.Archive.Size <= 256<<20 {
			out = append(out, c)
		}
	}
	return out, nil
}
func listReleases() error {
	status("Checking GitHub releases...")
	cs, e := choices()
	if e != nil {
		return e
	}
	data, e := json.Marshal(cs)
	if e != nil {
		return e
	}
	if e = atomic(cachePath, data, 0600); e != nil {
		return e
	}
	var b strings.Builder
	for _, c := range cs {
		fmt.Fprintf(&b, "%d\t%s\t%s\t%t\t%s\n", c.ID, c.Version, c.OS, c.Supported, c.Notes)
	}
	if e = atomic(listPath, []byte(b.String()), 0600); e != nil {
		return e
	}
	status("Select a release to view its target OS and notes")
	return nil
}
func lookup(id int64) (choice, error) {
	b, e := os.ReadFile(cachePath)
	if e != nil {
		return choice{}, e
	}
	var cs []choice
	if e = json.Unmarshal(b, &cs); e != nil {
		return choice{}, e
	}
	for _, c := range cs {
		if c.ID == id {
			return c, nil
		}
	}
	return choice{}, fmt.Errorf("Release list changed; refresh it")
}
