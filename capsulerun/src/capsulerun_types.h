#pragma once

#include <stdint.h>

typedef struct video_format_s {
  int width;
  int height;
  capsule::PixFmt format;
  int vflip;
  int64_t pitch;
} video_format_t;

typedef struct audio_format_s {
  int channels;
  int samplerate;
  int samplewidth;
} audio_format_t;
