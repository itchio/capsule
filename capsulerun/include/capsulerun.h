
#pragma once

#include <capsule/constants.h>

#if defined(_WIN32)
#define CAPSULERUN_WINDOWS

#elif defined(__APPLE__)
#define CAPSULERUN_OSX

#elif defined(__linux__) || defined(__unix__)
#define CAPSULERUN_LINUX
#endif // linux stuff defined

#define CAPSULE_MAX_PATH_LENGTH 16384

// int64_t etc.
#include <stdint.h>

#if defined(CAPSULERUN_LINUX) || defined(CAPSULERUN_OSX)
#include <sys/types.h>
#endif // CAPSULERUN_LINUX || CAPSULERUN_OSX

int capsulerun_main (int argc, char **argv);

typedef struct video_format_s {
  int width;
  int height;
  capsule_pix_fmt_t format;
  int vflip;
} video_format_t;

typedef int (*receive_video_format_t)(void *private_data, video_format_t *vfmt);
typedef int (*receive_video_frame_t)(void *private_data, uint8_t *buffer, size_t buffer_size, int64_t *timestamp);

typedef struct audio_format_s {
  int channels;
  int samplerate;
  int samplewidth;
} audio_format_t;

typedef int (*receive_audio_format_t)(void *private_data, audio_format_t *afmt);
typedef void* (*receive_audio_frames_t)(void *private_data, int *num_frames);

typedef struct encoder_params_s {
  void *private_data;

  receive_video_format_t receive_video_format;
  receive_video_frame_t receive_video_frame;

  int has_audio;
  receive_audio_format_t receive_audio_format;
  receive_audio_frames_t receive_audio_frames;
} encoder_params_t;

void encoder_run(encoder_params_t *params);

#if defined(CAPSULERUN_WINDOWS)
#include "capsulerun_windows.h"
#elif defined(CAPSULERUN_LINUX)
#include "capsulerun_linux.h"
#elif defined(CAPSULERUN_OSX)
#include "capsulerun_macos.h"
#endif // CAPSULERUN_LINUX

// #define CAPSULERUN_PROFILE

#ifdef CAPSULERUN_PROFILE
#define eprintf(...) { fprintf(stdout, "[capsule-profile] "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); fflush(stdout); }
#else
#define eprintf(...)
#endif

#define CAPSULERUN_DEBUG

#ifdef CAPSULERUN_DEBUG
#define dprintf(...) { fprintf(stdout, "[capsule-debug] "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); fflush(stdout); }
#else
#define dprintf(...)
#endif

#define capsule_log(...) { fprintf(stdout, "[capsule] "); fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); fflush(stdout); }
