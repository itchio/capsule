
#include "capsule.h"
#include "capsule_macos.h"

#include <OpenGL/OpenGL.h>

CGLError capsule_CGLFlushDrawable (CGLContextObj ctx) {
  capdata.saw_opengl = true;

  CGLError ret = CGLFlushDrawable(ctx);
  int width = 0;
  int height = 0;

  NSOpenGLContext *nsCtx = [NSOpenGLContext currentContext];
  if (nsCtx) {
    NSView *view = [nsCtx view];
    NSSize size = [view convertSizeToBacking: [view bounds].size];
    // capsule_log("nsCtx view size: %dx%d", (int) size.width, (int) size.height)

    if (size.width > 0 && size.height > 0) {
      width = size.width;
      height = size.height;
    }
  } else {
    // capsule_log("no nsCtx");
  }

  gl_capture(width, height);

  return ret;
}

DYLD_INTERPOSE(capsule_CGLFlushDrawable, CGLFlushDrawable)
