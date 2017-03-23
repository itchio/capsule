
#include "capsule_macos.h"

#import <Cocoa/Cocoa.h>

#include "../logging.h"
#include "../capture.h"
#include "../io.h"

@implementation NSWindow (Tracking)

+ (void)load {
  return;
  capsule::Log("Loading NSWindow");

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    capsule::Log("Swizzling sendEvent implementations");
    CapsuleSwizzle([self class], @selector(sendEvent:), @selector(capsule_sendEvent:));
  });
}
- (void)capsule_sendEvent:(NSEvent*)event {
  NSLog(@"Window event: %@", event);
  [self capsule_sendEvent:event];
}

@end

