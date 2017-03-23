
#include "pulse_dynamic.h"

#include <dlfcn.h>

#include "../macros.h"

namespace capsule {
namespace pulse {

static bool loaded;
pa_simple_new_t SimpleNew;
pa_simple_read_t SimpleRead;
pa_simple_free_t SimpleFree;

pa_mainloop_new_t MainloopNew;
pa_mainloop_get_api_t MainloopGetApi;
pa_mainloop_run_t MainloopRun;
pa_mainloop_quit_t MainloopQuit;
pa_mainloop_free_t MainloopFree;

pa_context_new_t ContextNew;
pa_context_get_state_t ContextGetState;
pa_context_get_server_info_t ContextGetServerInfo;
pa_context_get_sink_info_by_name_t ContextGetSinkInfoByName;
pa_context_disconnect_t ContextDisconnect;

bool Load() {
  if (loaded) {
    return true;
  }

#define PULSESYM(var, sym) { \
  var = reinterpret_cast<sym ## _t>(dlsym(handle, #sym));\
  if (! var) { \
    CapsuleLog("Could not find PulseAudio function %s", #sym); \
    return false; \
  } \
}

  void *handle = dlopen("libpulse-simple.so", RTLD_NOW);
  if (!handle) {
    handle = dlopen("libpulse-simple.so.0", RTLD_NOW);
  }
  if (!handle) {
    CapsuleLog("Could not load libpulse-simple.so(.0)");
    return false;
  }

  PULSESYM(SimpleNew, pa_simple_new)
  PULSESYM(SimpleRead, pa_simple_read);
  PULSESYM(SimpleFree, pa_simple_free);

  handle = dlopen("libpulse.so", RTLD_NOW);
  if (!handle) {
    handle = dlopen("libpulse.so.0", RTLD_NOW);
  }
  if (!handle) {
    CapsuleLog("Could not load libpulse.so(.0)");
    return false;
  }

  PULSESYM(MainloopNew, pa_mainloop_new)
  PULSESYM(MainloopGetApi, pa_mainloop_get_api);
  PULSESYM(MainloopRun, pa_mainloop_run);
  PULSESYM(MainloopQuit, pa_mainloop_quit);
  PULSESYM(MainloopFree, pa_mainloop_free);
  PULSESYM(ContextNew, pa_context_new);
  PULSESYM(ContextGetState, pa_context_get_state);
  PULSESYM(ContextGetServerInfo, pa_context_get_server_info);
  PULSESYM(ContextGetSinkInfoByName, pa_context_get_sink_info_by_name);
  PULSESYM(ContextDisconnect, pa_context_disconnect);

  return true;
}

} // namespace pulse
} // namespace capsule
