
#include <capsulerun.h>

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
    fromWideChar(argv_w[i], &argv[i]);
  }
#else // CAPSULE_WINDOWS

int main (int argc, char **argv) {

#endif // !CAPSULE_WINDOWS

  // different for each platform, CMake compiles the right one in.
  int ret = capsulerun_main(argc, argv);

#if defined(CAPSULE_WINDOWS)
  free(argv);
#endif // CAPSULE_WINDOWS

  return ret;
}
