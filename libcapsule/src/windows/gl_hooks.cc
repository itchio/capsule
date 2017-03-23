
#include "../gl_capture.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

namespace capsule {
namespace gl {

typedef void* (*wglGetProcAddress_t)(const char*);
static wglGetProcAddress_t _wglGetProcAddress = nullptr;

bool LoadOpengl (const char *path) {
  handle = dlopen(path, (RTLD_NOW|RTLD_LOCAL));
  if (!handle) {
    return false;
  }

  GLSYM(wglGetProcAddress)

  return true;
}

void *GetProcAddress (const char *symbol) {
  void *addr = nullptr;

  if (_wglGetProcAddress) {
    addr = _wglGetProcAddress(symbol);
  }
  if (!addr) {
    addr = ::GetProcAddress(handle, symbol);
  }

  return addr;
}

} // namespace gl
} // namespace capsule
