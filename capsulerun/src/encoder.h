
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
