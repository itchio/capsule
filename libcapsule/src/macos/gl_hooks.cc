
#include "../gl_capture.h"

namespace capsule {
namespace gl {

bool LoadOpengl (const char *path) {
  handle = dlopen(path, (RTLD_NOW|RTLD_LOCAL));
  return !!handle;
}


// Must have platform-specific implementation
void *GetProcAddress(const char *symbol) {
  return dlsym(handle, symbol);
}

} // namespace gl
} // namespace capsule

