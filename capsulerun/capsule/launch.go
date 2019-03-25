package capsule

type LaunchParams struct {
	// ExecPath is the executable we're launching (and injecting libcapsule
	// into). When in this struct, it should always be an absolute path that
	// exists on disk.
	ExecPath string

	// Args are passed to the executable when launching it.
	Args []string
}
