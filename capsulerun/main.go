package main

import (
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
)

func must(err error) {
	if err != nil {
		log.Fatalf("%+v", err)
	}
}

func main() {
	// validate arguments
	if len(os.Args) < 2 {
		log.Fatalf("Usage: capsulerun %s", os.Args[0])
	}

	// locate libcapsule (next to capsulerun)
	ourPath, err := os.Executable()
	must(err)
	libCapsulePath := filepath.Join(
		filepath.Dir(ourPath),
		"libcapsule.so",
	)
	_, err = os.Stat(libCapsulePath)
	must(err)

	// make sure target executable exists
	execPath := os.Args[1]
	_, err = os.Stat(execPath)
	must(err)

	// launch executable with modified environment
	cmd := exec.Command(execPath)
	cmd.Env = os.Environ()
	cmd.Env = append(cmd.Env, fmt.Sprintf(
		"%s=%s",
		"LD_PRELOAD", libCapsulePath,
	))
	cmd.Stdin = os.Stdin
	cmd.Stderr = os.Stderr
	cmd.Stdout = os.Stdout
	must(cmd.Start())
	must(cmd.Wait())
}
