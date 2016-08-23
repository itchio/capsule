
#include "capsule.h"
#include "capsule_macos_utils.h"
#include <OpenGL/OpenGL.h>

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <objc/runtime.h>

@implementation NSApplication (Tracking)

+ (void)load {
  capsule_log("Loading NSApplication");

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    capsule_log("Swizzling sendEvent implementations");
    capsule_swizzle([self class], @selector(sendEvent:), @selector(capsule_sendEvent:));
  });
}

- (void)capsule_sendEvent:(NSEvent*)event {
  if ([event type] == NSKeyDown && [event keyCode] == kVK_F9) {
    NSLog(@"F9 pressed! %@", event);
    if (capsule_capturing) {
      [[NSSound soundNamed:@"Pop"] play];
      capsule_capturing = 0;
    } else {
      [[NSSound soundNamed:@"Ping"] play];
      capsule_capturing = 1;
    }
  }
  [self capsule_sendEvent:event];
}

@end

