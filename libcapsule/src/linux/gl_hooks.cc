
#include <stdlib.h>

#include <lab/strings.h>

#include "../gl_capture.h"
#include "../ensure.h"
#include "../logging.h"

namespace capsule {
namespace gl {

typedef void* (*dlopen_type)(const char*, int);
dlopen_type dlopen; 

typedef int (*glXQueryExtension_t)(void*, void*, void*);
static glXQueryExtension_t _glXQueryExtension = nullptr;

typedef void (*glXSwapBuffers_t)(void*, void*);
static glXSwapBuffers_t _glXSwapBuffers = nullptr;

typedef void* (*glXGetProcAddressARB_t)(const char*);
static glXGetProcAddressARB_t _glXGetProcAddressARB = nullptr;

void EnsureDlopen() {
  if (!gl::dlopen) {
    // since we intercept dlopen, we need to get the address of
    // the real dlopen, so that we can, y'know, open libraries.
    gl::dlopen = (dlopen_type) dlsym(RTLD_NEXT, "dlopen");
    Ensure("found dlopen", !!gl::dlopen);
  }
}

bool LoadOpengl (const char *path) {
  EnsureDlopen();
  handle = gl::dlopen(path, (RTLD_NOW|RTLD_LOCAL));
  if (!handle) {
    return false;
  }

  GLSYM(glXQueryExtension)
  GLSYM(glXSwapBuffers)
  GLSYM(glXGetProcAddressARB)

  return true;
}

void *GetProcAddress (const char *symbol) {
  void *addr = nullptr;

  if (_glXGetProcAddressARB) {
    addr = _glXGetProcAddressARB(symbol);
  }
  if (!addr) {
    addr = dlsym(handle, symbol);
  }

  return addr;
}

} // namespace gl
} // namespace capsule

// intercepts
extern "C" {

void glXSwapBuffers (void *a, void *b) {
  capsule::gl::Capture(0, 0);
  return capsule::gl::_glXSwapBuffers(a, b);
}

int glXQueryExtension (void *a, void *b, void *c) {
  return capsule::gl::_glXQueryExtension(a, b, c);
}

void* glXGetProcAddressARB (const char *name) {
  if (lab::strings::CEquals(name, "glXSwapBuffers")) {
    capsule::Log("Hooking glXSwapBuffers");
    return (void*) &glXSwapBuffers;
  }

  if (!capsule::gl::EnsureOpengl()) {
    capsule::Log("Could not load opengl library, cannot get proc address for child");
    exit(124);
  }
  return capsule::gl::_glXGetProcAddressARB(name);
}

void* dlopen (const char *filename, int flag) {
  capsule::gl::EnsureDlopen();

  if (filename != NULL && lab::strings::CContains(filename, "libGL.so.1")) {
    capsule::gl::LoadOpengl(filename);

    if (lab::strings::CEquals(filename, "libGL.so.1")) {
      capsule::gl::dlopen(filename, flag);
      capsule::Log("dlopen: faking libGL");
      return capsule::gl::dlopen(NULL, RTLD_NOW|RTLD_LOCAL);
    } else {
      capsule::Log("dlopen real libGL from %s", filename);
      return capsule::gl::dlopen(filename, flag);
    }
  } else {
    void *res = capsule::gl::dlopen(filename, flag);
    capsule::Log("dlopen %s with %0x: %p", filename, flag, res);
    return res;
  }
}

} // extern "C"
