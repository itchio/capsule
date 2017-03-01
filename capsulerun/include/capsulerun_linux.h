
// FILE, etc.
#include <stdio.h>

// malloc
#include <stdlib.h>

// memset
#include <string.h>

// pulseaudio
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

#include "../src/shared/io.h" // oh no.

typedef struct encoder_private_s {
  capsule_io_t *io;
  uint8_t *audio_buffer;
  size_t audio_buffer_size;
  pa_simple *pactx;
} encoder_private_t;

int receive_audio_format (encoder_private_t *p, audio_format_t *fmt);
void *receive_audio_frames (encoder_private_t *p, int *frames_received);
