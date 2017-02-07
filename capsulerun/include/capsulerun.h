
#pragma once

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

#if defined(CAPSULERUN_LINUX)
#include <sys/types.h>
#endif // CAPSULERUN_LINUX

int capsulerun_main (int argc, char **argv);

#if defined(CAPSULERUN_LINUX) || defined(CAPSULERUN_WINDOWS)

typedef int (*receive_resolution_t)(void *private_data, int64_t *width, int64_t *height);
typedef int (*receive_frame_t)(void *private_data, uint8_t *buffer, size_t buffer_size);

typedef struct encoder_params_s {
  void *private_data;

  receive_resolution_t receive_resolution;
  receive_frame_t receive_frame;
} encoder_params_t;

void encoder_run(encoder_params_t *params);

#endif
