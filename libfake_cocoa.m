
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

        /* int i=0; */
        /* unsigned int mc = 0; */
        /* Method * mlist = class_copyMethodList(self, &mc); */
        /* NSLog(@"%d methods", mc); */
        /* for (i=0; i < mc; i++) { */
        /*   NSLog(@"Method no #%d: %s", i, sel_getName(method_getName(mlist[i]))); */
        /* } */

        SEL originalSelector = @selector(flushBuffer);
        SEL swizzledSelector = @selector(xxx_flushBuffer);

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

- (void)xxx_flushBuffer {
  [self xxx_flushBuffer];
  NSLog(@"flushBuffer: %@", self);
}

@end
