
#include <capsulerun.h>

#include "argparse.h"

#if defined(CAPSULE_LINUX)
#include <unistd.h> // readlink
#include <limits.h> // realpath
#endif

#if defined(CAPSULE_MACOS)
#include <mach-o/dyld.h> // _NSGetExecutablePath
#include <sys/param.h> // realpath
#endif

#if defined(CAPSULE_WINDOWS)
#include "../windows/strings.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#include <shellapi.h> // CommandLineToArgvW
#endif // CAPSULE_WINDOWS

#include <string>

#include <microprofile.h>

MICROPROFILE_DEFINE(MAIN, "MAIN", "Main", 0xff0000);

static const char *const usage[] = {
  "capsulerun [options] -- executable [args]",
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
  args.pix_fmt = "yuv420p";
  args.crf = -1;
  args.size_divider = 1;
  args.fps = 60;

  struct argparse_option options[] = {
    OPT_HELP(),
    OPT_GROUP("Basic options"),
    OPT_STRING('d', "dir", &args.dir, "where to output .mp4 videos (defaults to current directory)"),
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

  if (args.priority) {
#if defined(CAPSULE_WINDOWS)
    HANDLE hProcess = GetCurrentProcess();
    HRESULT hr = 0;

    if (0 == strcmp(args.priority, "above-normal")) {
      hr = SetPriorityClass(hProcess, ABOVE_NORMAL_PRIORITY_CLASS);
    } else if (0 == strcmp(args.priority, "high")) {
      hr = SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS);
    } else {
      capsule_log("Invalid priority parameter %s, expected above-normal or high", args.priority);
    }

    if (FAILED(hr)) {
      capsule_log("Failed to set process priority: %d (%x), continuing", hr, hr)
    }
#else
    capsule_log("priority parameter not yet supported");
#endif
  }

  // get our own path
  std::string exe_path;

  const size_t file_name_characters = 16 * 1024;
#if defined(CAPSULE_WINDOWS)
  wchar_t *file_name = reinterpret_cast<wchar_t*>(calloc(file_name_characters, sizeof(wchar_t)));
  DWORD file_name_actual_characters = GetModuleFileNameW(NULL, file_name, static_cast<DWORD>(file_name_characters));
  char *utf8_file_name = nullptr;
  fromWideChar(file_name, &utf8_file_name);
  free(file_name);
  exe_path = std::string(utf8_file_name);
  free(utf8_file_name);
#elif defined(CAPSULE_MACOS)
  uint32_t mac_file_name_characters = file_name_characters;
  char *utf8_file_name = reinterpret_cast<char *>(calloc(file_name_characters, sizeof(char)));
  if (_NSGetExecutablePath(utf8_file_name, &mac_file_name_characters) != 0) {
    capsule_log("Could not get our own executable path, bailing out");
    exit(1);
  }
  char *utf8_absolute_file_name = realpath(utf8_file_name, nullptr);
  free(utf8_file_name);
  exe_path = std::string(utf8_absolute_file_name);
  free(utf8_absolute_file_name);
#elif defined(CAPSULE_LINUX)
  char *utf8_file_name = reinterpret_cast<char *>(calloc(file_name_characters, sizeof(char)));
  ssize_t dest_num_characters = readlink("/proc/self/exe", utf8_file_name, file_name_characters);
  // readlink does not append a null character
  utf8_file_name[dest_num_characters] = '\0';
  char *utf8_absolute_file_name = realpath(utf8_file_name, nullptr);
  free(utf8_file_name);
  exe_path = std::string(utf8_absolute_file_name);
  free(utf8_absolute_file_name);
#endif

  capsule_log("Running from: %s", exe_path.c_str());

  size_t slash_index = exe_path.find_last_of("/\\");
  std::string lib_path = exe_path.substr(0, slash_index);
  args.libpath = lib_path.c_str();

  capsule_log("Library path: %s", lib_path.c_str());

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
