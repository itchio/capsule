
#include "capsule.h"
#include "capsule_macos_utils.h"
#include <OpenGL/OpenGL.h>

#define DYLD_INTERPOSE(_replacement,_replacee) \
  __attribute__((used)) static struct{ const void* replacement; const void* replacee; } _interpose_##_replacee \
  __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacement, (const void*)(unsigned long)&_replacee };

int capsule_capturing = 0;

CGLError capsule_CGLFlushDrawable (CGLContextObj ctx) {
  if (capsule_capturing) {
    // capsule_log("capturing at CGLFLushDrawable")
    capsule_captureFrame();
  }
  return CGLFlushDrawable(ctx);
}

DYLD_INTERPOSE(capsule_CGLFlushDrawable, CGLFlushDrawable)
