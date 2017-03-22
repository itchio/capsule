
#pragma once

namespace capsule {

// all strings are UTF-8, even on windows
struct MainArgs {
  // positional arguments
  const char *libpath;
  char *exec;
  int exec_argc;
  char **exec_argv;

  // options
  const char *dir;
  const char *pix_fmt;
  int crf;
  int no_audio;
  int size_divider;
  int fps;
  bool gpu_color_conv;
  int threads;
  int debug_av;
  int gop_size;
  int max_b_frames;
  int buffered_frames;
  const char *priority;
  const char *x264_preset;

  const char *pipe;
};

}
