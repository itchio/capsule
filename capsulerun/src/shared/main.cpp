
#include <capsulerun.h>

#include "argparse.h"

#if defined(CAPSULE_WINDOWS)
#include "../windows/strings.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <shellapi.h> // CommandLineToArgvW
#endif // CAPSULE_WINDOWS

#include <microprofile.h>

MICROPROFILE_DEFINE(MAIN, "MAIN", "Main", 0xff0000);

static const char *const usage[] = {
  "capsulerun -L libpath [options] -- executable [args]",
  NULL
};

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

  MicroProfileOnThreadCreate("Main");

  MicroProfileSetEnableAllGroups(true);
	MicroProfileSetForceMetaCounters(true);

	MicroProfileStartContextSwitchTrace();

  struct capsule_args_s args;
  memset(&args, 0, sizeof(args));
  args.dir = ".";
  args.yuv444 = 0;

  struct argparse_option options[] = {
    OPT_HELP(),
    OPT_GROUP("Required options"),
    OPT_STRING('L', "libpath", &args.libpath, "where libcapsule can be found"),
    OPT_GROUP("Basic options"),
    OPT_STRING('d', "dir", &args.dir, "where to output .mp4 videos (defaults to current directory)"),
    OPT_GROUP("Video options"),
    OPT_BOOLEAN(0, "yuv444", &args.yuv444, "use yuv444 colorspace ?"),
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

#if !defined(CAPSULE_WINDOWS)
  // on non-windows platforms, exec_argv needs to include the executable
  int real_exec_argc = args.exec_argc + 1;
  char **real_exec_argv = (char **) calloc(real_exec_argc + 1, sizeof(char *));

  real_exec_argv[0] = args.exec;
  for (int i = 0; i < args.exec_argc; i++) {
    real_exec_argv[i + 1] = args.exec_argv[i];
  }

  args.exec_argc = real_exec_argc;
  args.exec_argv = real_exec_argv;
#endif // !CAPSULE_WINDOWS

  capsule_log("thanks for flying capsule on %s", CAPSULE_PLATFORM);

  {
    MICROPROFILE_SCOPE(MAIN);

    // different for each platform, CMake compiles the right one in.
    int ret = capsulerun_main(&args);

#if defined(CAPSULE_WINDOWS)
    free(argv);
#endif // CAPSULE_WINDOWS

    return ret;
  }
}
