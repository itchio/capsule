#pragma once

typedef struct video_format_s {
  int width;
  int height;
  capsule_pix_fmt_t format;
  int vflip;
  int pitch;
} video_format_t;

typedef struct audio_format_s {
  int channels;
  int samplerate;
  int samplewidth;
} audio_format_t;
