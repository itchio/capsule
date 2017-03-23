
#include <capsulerun.h>

#include <lab/platform.h>
#include <lab/paths.h>
#include <lab/strings.h>

#if defined(LAB_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <shellapi.h> // CommandLineToArgvW
#endif // LAB_WINDOWS

#include <string>

#include "argparse.h"
#include "runner.h"

#if defined(LAB_WINDOWS)
#include "windows/executor.h"
#elif defined(LAB_MACOS)
#include "macos/executor.h"
#elif defined(LAB_LINUX)
#include "linux/executor.h"
#endif

#include <microprofile.h>

MICROPROFILE_DEFINE(MAIN, "MAIN", "Main", 0xff0000);

static const char *const usage[] = {
  "capsulerun [options] -- executable [args]",
  NULL
};

#if defined(LAB_WINDOWS)
int main () {
  LPWSTR in_command_line = GetCommandLineW();
  int argc;
  LPWSTR* argv_w = CommandLineToArgvW(in_command_line, &argc);

  // argv must be null-terminated, calloc zeroes so this works out.
  char **argv = (char **) calloc(argc + 1, sizeof(char *));
  for (int i = 0; i < argc; i++) {
    lab::strings::FromWideChar(argv_w[i], (char **) &argv[i]);
  }
#else // LAB_WINDOWS

int main (int argc, char **argv) {

#endif // !LAB_WINDOWS

  MicroProfileOnThreadCreate("Main");

  MicroProfileSetEnableAllGroups(true);
	MicroProfileSetForceMetaCounters(true);

	MicroProfileStartContextSwitchTrace();

  capsule::MainArgs args;
  memset(&args, 0, sizeof(args));
  args.dir = ".";
  args.pix_fmt = "yuv420p";
  args.crf = -1;
  args.size_divider = 1;
  args.fps = 60;

  struct argparse_option options[] = {
    OPT_HELP(),
    OPT_GROUP("Basic options"),
    OPT_STRING('d', "dir", &args.dir, "where to output .mp4 videos (defaults to current directory)"),
    OPT_STRING(0, "pipe", &args.dir, "named pipe to listen on (defaults to unique name)"),
    OPT_GROUP("Video options"),
    OPT_INTEGER(0, "crf", &args.crf, "output quality. sane values range from 18 (~visually lossless) to 28 (fast but looks bad)"),
    OPT_INTEGER(0, "size_divider", &args.size_divider, "size divider: default 1, accepted values 2 or 4"),
    OPT_INTEGER('r', "fps", &args.fps, "maximum frames per second (default: 60)"),
    OPT_GROUP("Audio options"),
    OPT_BOOLEAN(0, "no-audio", &args.no_audio, "don't record audio"),
    OPT_GROUP("Advanced options"),
    OPT_STRING(0, "pix_fmt", &args.pix_fmt, "pixel format: yuv420p (default, compatible), or yuv444p"),
    OPT_INTEGER(0, "threads", &args.threads, "number of threads used to encode video"),
    OPT_BOOLEAN(0, "debug-av", &args.debug_av, "let video encoder be verbose"),
    OPT_INTEGER(0, "gop-size", &args.gop_size, "default: 120"),
    OPT_INTEGER(0, "max-b-frames", &args.max_b_frames, "default: 16"),
    OPT_INTEGER(0, "buffered-frames", &args.buffered_frames, "default: 60"),
    OPT_BOOLEAN(0, "gpu-color-conv", &args.gpu_color_conv, "do color conversion on the GPU (experimental)"),
    OPT_STRING(0, "priority", &args.priority, "above-normal or high (windows only)"),
    OPT_STRING(0, "x264-preset", &args.x264_preset, "slower, slow, medium, fast, faster, veryfast, ultrafast (default ultrafast)"),
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

  const int num_positional_args = 1;
  if (argc < num_positional_args) {
    argparse_usage(&argparse);
    exit(1);
  }

  args.exec = argv[0];
  args.exec_argc = argc - num_positional_args;
  args.exec_argv = argv + num_positional_args;

#if !defined(LAB_WINDOWS)
  // on non-windows platforms, exec_argv needs to include the executable
  int real_exec_argc = args.exec_argc + 1;
  char **real_exec_argv = (char **) calloc(real_exec_argc + 1, sizeof(char *));

  real_exec_argv[0] = args.exec;
  for (int i = 0; i < args.exec_argc; i++) {
    real_exec_argv[i + 1] = args.exec_argv[i];
  }

  args.exec_argc = real_exec_argc;
  args.exec_argv = real_exec_argv;
#endif // !LAB_WINDOWS

  CapsuleLog("thanks for flying capsule on %s", lab::kPlatform);

  if (args.priority) {
#if defined(LAB_WINDOWS)
    HANDLE hProcess = GetCurrentProcess();
    HRESULT hr = 0;

    if (0 == strcmp(args.priority, "above-normal")) {
      hr = SetPriorityClass(hProcess, ABOVE_NORMAL_PRIORITY_CLASS);
    } else if (0 == strcmp(args.priority, "high")) {
      hr = SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS);
    } else {
      CapsuleLog("Invalid priority parameter %s, expected above-normal or high", args.priority);
    }

    if (FAILED(hr)) {
      CapsuleLog("Failed to set process priority: %d (%x), continuing", hr, hr)
    }
#else // LAB_WINDOWS
    CapsuleLog("priority parameter not yet supported on %s", lab::kPlatform);
#endif // !LAB_WINDOWS
  }

  std::string self_path = lab::paths::SelfPath();
  CapsuleLog("Running from: %s", self_path.c_str());

  std::string lib_path = lab::paths::DirName(self_path);
  args.libpath = lib_path.c_str();

  CapsuleLog("Library path: %s", lib_path.c_str());

  if (!args.pipe) {
    args.pipe = "capsule";
  }

  {
    MICROPROFILE_SCOPE(MAIN);

    capsule::ExecutorInterface *executor;
#if defined(LAB_WINDOWS)
    executor = new capsule::windows::Executor();
#elif defined(LAB_MACOS)
    executor = new capsule::macos::Executor();
#elif defined(LAB_LINUX)
    executor = new capsule::linux::Executor();
#endif

    capsule::Runner runner{&args, executor};
#if defined(LAB_MACOS)
    std::thread main_thread(&capsule::Runner::Run, &runner);
    capsule::RunApp();
#else
    runner.Run();
#endif

#if defined(LAB_WINDOWS)
    free(argv);
#endif // LAB_WINDOWS

    return 0;
  }
}
