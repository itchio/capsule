package capsule

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"

	"github.com/pkg/errors"
)

func Launch(params LaunchParams) error {
	libCapsulePath, err := getLibCapsulePath()
	if err != nil {
		return errors.WithStack(err)
	}

	cmd := exec.Command(params.ExecPath)
	cmd.Env = os.Environ()
	cmd.Env = append(cmd.Env, fmt.Sprintf(
		"%s=%s",
		"LD_PRELOAD", libCapsulePath,
	))
	cmd.Stdin = os.Stdin
	cmd.Stderr = os.Stderr
	cmd.Stdout = os.Stdout
	cmd.Args = params.Args

	err = cmd.Start()
	if err != nil {
		return errors.WithStack(err)
	}

	err = cmd.Wait()
	if err != nil {
		return errors.WithStack(err)
	}
	return err
}

func getLibCapsulePath() (string, error) {
	// locate libcapsule (next to capsulerun)
	ourPath, err := os.Executable()
	if err != nil {
		return "", errors.WithStack(err)
	}
	libCapsulePath := filepath.Join(
		filepath.Dir(ourPath),
		"libcapsule.so",
	)
	_, err = os.Stat(libCapsulePath)
	if err != nil {
		return "", errors.WithStack(err)
	}
	return libCapsulePath, nil
}
