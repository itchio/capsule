
#include <capsulerun.h>

#include "argparse.h"

static const char *const usage[] = {
  "capsulerun -L libpath [options] -- executable [args]",
  NULL
};

#if defined(CAPSULE_WINDOWS)
#include "../windows/strings.h"
#endif // CAPSULE_WINDOWS

#if defined(CAPSULE_WINDOWS)
int main (int _argc, char **_argv) {
  LPWSTR in_command_line = GetCommandLineW();
  int argc;
  LPWSTR* argv_w = CommandLineToArgvW(in_command_line, &argc);

  // argv must be null-terminated, calloc zeroes so this works out.
  char **argv = (char **) calloc(argc + 1, sizeof(char *));
  for (int i = 0; i < argc; i++) {
    fromWideChar(argv_w[i], (char **) &argv[i]);
  }
#else // CAPSULE_WINDOWS

int main (int argc, char **argv) {

#endif // !CAPSULE_WINDOWS

  struct capsule_args_s args;
  memset(&args, 0, sizeof(args));
  args.dir = ".";

  struct argparse_option options[] = {
    OPT_HELP(),
    OPT_GROUP("Basic options"),
    OPT_STRING('L', "libpath", &args.libpath, "where libcapsule can be found"),
    OPT_STRING('d', "dir", &args.dir, "where to output .mp4 videos"),
    OPT_END(),
  };
  struct argparse argparse;
  argparse_init(&argparse, options, usage, 0);
  argparse_describe(
    &argparse,
    // header
    "\ncapsulerun is a wrapper to inject libcapsule into applications.",
    // footer
    ""
  );
  argc = argparse_parse(&argparse, argc, (const char **) argv);

  if (!args.libpath) {
    argparse_usage(&argparse);
    exit(1);
  }

  const int num_positional_args = 1;
  if (argc < num_positional_args) {
    argparse_usage(&argparse);
    exit(1);
  }

  args.exec = argv[0];
  args.exec_argc = argc - num_positional_args;
  args.exec_argv = argv + num_positional_args;

  capsule_log("thanks for flying capsule on %s", CAPSULE_PLATFORM);

  // different for each platform, CMake compiles the right one in.
  int ret = capsulerun_main(&args);

#if defined(CAPSULE_WINDOWS)
  free(argv);
#endif // CAPSULE_WINDOWS

  return ret;
}
