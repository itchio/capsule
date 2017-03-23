
#include "capsule_macos.h"

#import <OpenGL/OpenGL.h>
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#include "../logging.h"

@implementation NSApplication (Tracking)

+ (void)load {
  capsule::Log("Loading NSApplication");

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    capsule::Log("Swizzling sendEvent implementations");
    CapsuleSwizzle([self class], @selector(sendEvent:), @selector(capsule_sendEvent:));
  });
}

- (void)capsule_sendEvent:(NSEvent*)event {
  // NSLog(@"NSApplication event: %@", event);
  [self capsule_sendEvent:event];
}

@end
