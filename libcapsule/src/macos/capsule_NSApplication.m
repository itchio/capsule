
#include "capsule.h"
#include "capsule_macos_utils.h"
#include <OpenGL/OpenGL.h>

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

@implementation NSApplication (Tracking)

+ (void)load {
  capsule_log("Loading NSApplication");

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    capsule_log("Swizzling sendEvent implementations");
    capsule_swizzle([self class], @selector(sendEvent:), @selector(capsule_sendEvent:));
  });
}

static CapsuleFixedRecorder *recorder;

- (void)capsule_sendEvent:(NSEvent*)event {
  if (([event type] == NSKeyDown && [event keyCode] == kVK_F9) || ([event type] == NSRightMouseDown)) {
    NSLog(@"F9 pressed! %@", event);
    if (capsule_capturing) {
      [[NSSound soundNamed:@"Pop"] play];
      capsule_capturing = 0;
      recorder = nil;
    } else {
      [[NSSound soundNamed:@"Ping"] play];
      if (capsule_had_opengl) {
        NSLog(@"Starting OpenGL capture");
        capsule_capturing = 1;
      } else {
        NSLog(@"Starting CGWindow capture");
        capsule_capturing = 2;
        recorder = [[CapsuleFixedRecorder alloc] init];
      }
    }
  } else {
    // NSLog(@"Another event: %@", event);
  }
  [self capsule_sendEvent:event];
}

@end
