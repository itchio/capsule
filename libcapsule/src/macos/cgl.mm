
#include <Cocoa/Cocoa.h>
#include <OpenGL/OpenGL.h>

#include "interpose.h"
#include "../capture.h"
#include "../gl_capture.h"

CGLError CapsuleCglFlushDrawable (CGLContextObj ctx) {
  capsule::capture::SawBackend(capsule::capture::kBackendGL);

  CGLError ret = CGLFlushDrawable(ctx);

  int width = 0;
  int height = 0;

  NSOpenGLContext *nsCtx = [NSOpenGLContext currentContext];
  if (nsCtx) {
    NSView *view = [nsCtx view];
    NSSize size = [view convertSizeToBacking: [view bounds].size];

    if (size.width > 0 && size.height > 0) {
      width = size.width;
      height = size.height;
    }
  }

  capsule::gl::Capture(width, height);

  return ret;
}

DYLD_INTERPOSE(CapsuleCglFlushDrawable, CGLFlushDrawable)

