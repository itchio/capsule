
#pragma once

// all strings are UTF-8, even on windows
typedef struct capsule_args_s {
  // positional arguments
  char *libpath;
  char *exec;
  int exec_argc;
  char **exec_argv;

  // options
  char *dir;
} capsule_args_t;
