
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

#include <lab/types.h>
#include <capsule/constants.h>

#include "args.h"

namespace capsule {
namespace encoder {

struct VideoFormat {
  int width;
  int height;
  capsule::PixFmt format;
  bool vflip;
  int64_t pitch;
};

struct AudioFormat {
  int channels;
  int samplerate;
  // FIXME: assumes F32LE, bad
  int samplewidth;
};

typedef int (*VideoFormatReceiver)(void *private_data, VideoFormat *vfmt);
typedef int (*VideoFrameReceiver)(void *private_data, uint8_t *buffer, size_t buffer_size, int64_t *timestamp);

typedef int (*AudioFormatReceiver)(void *private_data, AudioFormat *afmt);
typedef void* (*AudioFramesReceiver)(void *private_data, int *num_frames);

struct Params {
  void *private_data;

  VideoFormatReceiver receive_video_format;
  VideoFrameReceiver receive_video_frame;

  bool has_audio;
  AudioFormatReceiver receive_audio_format;
  AudioFramesReceiver receive_audio_frames;
};

void Run(MainArgs *args, Params *params);

} // namespace encoder
} // namespace capsule
