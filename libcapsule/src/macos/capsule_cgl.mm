
#include "capsule.h"
#include "capsule_macos.h"

#include <OpenGL/OpenGL.h>

CGLError CapsuleCglFlushDrawable (CGLContextObj ctx) {
  capdata.saw_opengl = true;

  CGLError ret = CGLFlushDrawable(ctx);
  int width = 0;
  int height = 0;

  NSOpenGLContext *nsCtx = [NSOpenGLContext currentContext];
  if (nsCtx) {
    NSView *view = [nsCtx view];
    NSSize size = [view convertSizeToBacking: [view bounds].size];
    // CapsuleLog("nsCtx view size: %dx%d", (int) size.width, (int) size.height)

    if (size.width > 0 && size.height > 0) {
      width = size.width;
      height = size.height;
    }
  } else {
    // CapsuleLog("no nsCtx");
  }

  GlCapture(width, height);

  return ret;
}

DYLD_INTERPOSE(CapsuleCglFlushDrawable, CGLFlushDrawable)
