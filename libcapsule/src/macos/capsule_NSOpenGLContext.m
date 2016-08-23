
#include "capsule.h"
#include "capsule_macos_utils.h"
#include <OpenGL/OpenGL.h>

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

/*
@implementation NSOpenGLContext (Tracking)

+ (void)load {
  capsule_log("Loading NSOpenGLContext");

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    capsule_log("Swizzling flushBuffer implementations");
    capsule_swizzle([self class], @selector(flushBuffer), @selector(capsule_flushBuffer));
  });
}

static bool glDone = false;

- (void)capsule_flushBuffer {
  if (!glDone) {
    glDone = true;
    capsule_log("First NSOpenGL frame capture");
  }

  capsule_captureFrame();
  [self capsule_flushBuffer];
}

@end
*/
