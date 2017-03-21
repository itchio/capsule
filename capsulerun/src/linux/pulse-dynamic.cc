
#include "pulse-dynamic.h"

#include <dlfcn.h>

namespace capsule {
namespace pulse {

static bool loaded;
pa_simple_new_t New;
pa_simple_read_t Read;
pa_simple_free_t Free;

bool Load() {
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

  New = reinterpret_cast<pa_simple_new_t>(dlsym(handle, "pa_simple_new"));
  Read = reinterpret_cast<pa_simple_read_t>(dlsym(handle, "pa_simple_read"));
  Free = reinterpret_cast<pa_simple_free_t>(dlsym(handle, "pa_simple_free"));

  dlclose(handle);

  loaded = (New && Read && Free);
  return loaded;
}

} // namespace pulse
} // namespace capsule
