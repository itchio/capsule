#pragma once

#include <pulse/simple.h>
#include <pulse/context.h>
#include <pulse/introspect.h>
#include <pulse/mainloop.h>

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
extern pa_simple_new_t SimpleNew;

typedef int (*pa_simple_read_t)(
  pa_simple *s,
  void *data,
  size_t bytes,
  int *error
);
extern pa_simple_read_t SimpleRead;

typedef int (*pa_simple_free_t)(
  pa_simple *s
);
extern pa_simple_free_t SimpleFree;

typedef pa_mainloop *(*pa_mainloop_new_t)();
extern pa_mainloop_new_t MainloopNew;

typedef pa_mainloop_api *(*pa_mainloop_get_api_t)(
  pa_mainloop *loop
);
extern pa_mainloop_get_api_t MainloopGetApi;

typedef void (*pa_mainloop_run_t)(
  pa_mainloop *loop,
  int *retval
);
extern pa_mainloop_run_t MainloopRun;

typedef void (*pa_mainloop_quit_t)(
  pa_mainloop *loop,
  int retval
);
extern pa_mainloop_quit_t MainloopQuit;

typedef void (*pa_mainloop_free_t)(
  pa_mainloop *loop
);
extern pa_mainloop_free_t MainloopFree;

typedef pa_context *(*pa_context_new_t)(
  pa_mainloop_api *api,
  const char *name
);
extern pa_context_new_t ContextNew;

typedef void (*pa_context_connect_t)(
  pa_context *ctx,
  const char *server,
  pa_context_flags_t flags,
  const pa_spawn_api* api
);
extern pa_context_connect_t ContextConnect;

typedef void (*pa_context_set_state_callback_t)(
  pa_context *ctx,
  pa_context_notify_cb_t cb,
  void *userdata
);
extern pa_context_set_state_callback_t ContextSetStateCallback;

typedef pa_context_state_t (*pa_context_get_state_t)(
  pa_context *ctx
);
extern pa_context_get_state_t ContextGetState;

typedef void (*pa_context_get_server_info_t)(
  pa_context *ctx,
  pa_server_info_cb_t cb,
  void *userdata
);
extern pa_context_get_server_info_t ContextGetServerInfo;

typedef void (*pa_context_get_sink_info_by_name_t)(
  pa_context *ctx,
  const char *name,
  pa_sink_info_cb_t cb,
  void *userdata
);
extern pa_context_get_sink_info_by_name_t ContextGetSinkInfoByName;

typedef void (*pa_context_disconnect_t)(
  pa_context *ctx
);
extern pa_context_disconnect_t ContextDisconnect;

bool Load();
char *GetDefaultSinkMonitor();

} // namespace pulse
} // namespace capsule
