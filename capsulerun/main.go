package main

import (
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"

	"gopkg.in/alecthomas/kingpin.v2"
)

var (
	app     = kingpin.New("capsulerun", "Your very own libcapsule injector")
	version = "master"
)

var cli = struct {
	exec string
	args []string
}{}

func init() {
	app.Arg("exec", "The executable to run").Required().StringVar(&cli.exec)
	app.Arg("args", "Arguments to pass to the executable").StringsVar(&cli.args)
}

func must(err error) {
	if err != nil {
		log.Fatalf("%+v", err)
	}
}

func main() {
	app.Version(version)
	app.VersionFlag.Short('V')
	_, err := app.Parse(os.Args[1:])
	if err != nil {
		ctx, _ := app.ParseContext(os.Args[1:])
		if ctx != nil {
			app.FatalUsageContext(ctx, "%s\n", err.Error())
		} else {
			app.FatalUsage("%s\n", err.Error())
		}
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
	execPath := cli.exec
	execPath, err = exec.LookPath(execPath)
	must(err)
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
	cmd.Args = append(
		[]string{execPath},
		cli.args...,
	)
	must(cmd.Start())
	must(cmd.Wait())
}
