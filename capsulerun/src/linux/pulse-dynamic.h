#pragma once

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

typedef pa_simple* (*pa_simple_new_t)(
  const char *server,
  const char *name,
  pa_stream_direction_t dir,
  const char *dev,
  const char *stream_name,
  const pa_sample_spec *ss,
  const pa_channel_map *map,
  const pa_buffer_attr *attr,
  int *error
);
extern pa_simple_new_t _pa_simple_new;

typedef int (*pa_simple_read_t)(
  pa_simple *s,
  void *data,
  size_t bytes,
  int *error
);
extern pa_simple_read_t _pa_simple_read;

typedef int (*pa_simple_free_t)(
  pa_simple *s
);
extern pa_simple_free_t _pa_simple_free;

bool capsule_pulse_init();
