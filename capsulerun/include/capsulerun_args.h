
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
  char *pix_fmt;
  int crf;
  int no_audio;
  int divider;
  int threads;
  int debug_av;
  int gop_size;
  int max_b_frames;
  int buffered_frames;
  char *priority;
} capsule_args_t;
