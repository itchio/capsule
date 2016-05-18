
#include "libfake.h"
#include <OpenGL/OpenGL.h>

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

@implementation NSOpenGLContext (Tracking)

  + (void)load {
    NSLog(@"Loading NSOpenGLContext");

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSLog(@"Doing the old reddit switcheroo");

        Class class = [self class];

        SEL originalSelector = @selector(flushBuffer);
        SEL swizzledSelector = @selector(libfake_flushBuffer);

        Method originalMethod = class_getInstanceMethod(class, originalSelector);
        Method swizzledMethod = class_getInstanceMethod(class, swizzledSelector);

        BOOL didAddMethod = class_addMethod(class,
            originalSelector,
            method_getImplementation(swizzledMethod),
            method_getTypeEncoding(swizzledMethod));

        if (didAddMethod) {
          NSLog(@"Did add method!");
          class_replaceMethod(class, swizzledSelector,
            method_getImplementation(originalMethod),
            method_getTypeEncoding(originalMethod));
        } else {
          NSLog(@"Did not add method, exchanging implementations");
          method_exchangeImplementations(originalMethod, swizzledMethod);
        }
    });
  }

- (void)libfake_flushBuffer {
  libfake_captureFrame();
  [self libfake_flushBuffer];
  NSLog(@"flushBuffer: %@", self);
}

@end