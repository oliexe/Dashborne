package main

import (
	"fmt"
	"syscall"
)

func spaceCheck(path string, needed uint64) error {
	var s syscall.Statfs_t
	if e := syscall.Statfs(path, &s); e != nil {
		return e
	}
	if s.Bavail*uint64(s.Bsize) < needed {
		return fmt.Errorf("Not enough free space")
	}
	return nil
}
