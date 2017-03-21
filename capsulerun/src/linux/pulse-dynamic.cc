
#include "pulse-dynamic.h"

#include <dlfcn.h>

static bool loaded = false;
pa_simple_new_t _pa_simple_new = nullptr;
pa_simple_read_t _pa_simple_read = nullptr;
pa_simple_free_t _pa_simple_free = nullptr;

bool capsule_pulse_init() {
  if (loaded) {
    return true;
  }

  void *handle = dlopen("libpulse-simple.so", RTLD_NOW);
  if (!handle) {
    handle = dlopen("libpulse-simple.so.0", RTLD_NOW);
  }
  if (!handle) {
    return false;
  }

  _pa_simple_new = reinterpret_cast<pa_simple_new_t>(dlsym(handle, "pa_simple_new"));
  _pa_simple_read = reinterpret_cast<pa_simple_read_t>(dlsym(handle, "pa_simple_read"));
  _pa_simple_free = reinterpret_cast<pa_simple_free_t>(dlsym(handle, "pa_simple_free"));

  dlclose(handle);

  loaded = (_pa_simple_new && _pa_simple_read && _pa_simple_free);
  return loaded;
}
