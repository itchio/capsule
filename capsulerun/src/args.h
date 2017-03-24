
/*
 *  capsule - the game recording and overlay toolkit
 *  Copyright (C) 2017, Amos Wenger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 * https://github.com/itchio/capsule/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

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
  int headless;
};

}
