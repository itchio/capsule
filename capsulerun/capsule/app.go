package capsule

import (
	"os"
	"os/exec"

	"github.com/pkg/errors"
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

func Run() error {
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

	// make sure target executable exists
	execPath := cli.exec
	execPath, err = exec.LookPath(execPath)
	if err != nil {
		return errors.WithStack(err)
	}
	_, err = os.Stat(execPath)
	if err != nil {
		return errors.WithStack(err)
	}

	// launch executable, injecting libcapsule
	launchParams := LaunchParams{
		ExecPath: execPath,
		Args: append(
			[]string{execPath},
			cli.args...,
		),
	}
	if err != nil {
		return errors.WithStack(err)
	}

	err = Launch(launchParams)
	if err != nil {
		return errors.WithStack(err)
	}

	return nil
}
