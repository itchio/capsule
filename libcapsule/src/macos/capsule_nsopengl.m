
#include "capsule.h"
#include <OpenGL/OpenGL.h>

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

@implementation NSOpenGLContext (Tracking)

+ (void)load {
  capsule_log("Loading NSOpenGLContext");

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    capsule_log("Swizzling flushBuffer implementations");

    Class class = [self class];

    SEL originalSelector = @selector(flushBuffer);
    SEL swizzledSelector = @selector(capsule_flushBuffer);

    Method originalMethod = class_getInstanceMethod(class, originalSelector);
    Method swizzledMethod = class_getInstanceMethod(class, swizzledSelector);

    BOOL didAddMethod = class_addMethod(class,
      originalSelector,
      method_getImplementation(swizzledMethod),
      method_getTypeEncoding(swizzledMethod));

      if (didAddMethod) {
        capsule_log("Added method");
        class_replaceMethod(class, swizzledSelector,
          method_getImplementation(originalMethod),
          method_getTypeEncoding(originalMethod));
      } else {
        capsule_log("Didn't add method, exchanging");
        method_exchangeImplementations(originalMethod, swizzledMethod);
      }
    });
  }

  - (void)capsule_flushBuffer {
    capsule_log("Enter capsule_flushBuffer");
    capsule_captureFrame();
    [self capsule_flushBuffer];
    capsule_log("Exit capsule_flushBuffer");
  }

  @end
