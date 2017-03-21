#pragma once

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>

namespace capsule {
namespace pulse {

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
extern pa_simple_new_t New;

typedef int (*pa_simple_read_t)(
  pa_simple *s,
  void *data,
  size_t bytes,
  int *error
);
extern pa_simple_read_t Read;

typedef int (*pa_simple_free_t)(
  pa_simple *s
);
extern pa_simple_free_t Free;

bool Load();

} // namespace pulse
} // namespace capsule
