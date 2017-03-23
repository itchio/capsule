
#import <OpenGL/OpenGL.h>
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#include "swizzle.h"
#include "../logging.h"
#include "../io.h"

#if defined(NSEventTypeKeyDown)
#define OurKeyDown NSEventTypeKeyDown
#else
#define OurKeyDown NSKeyDown
#endif

@implementation NSApplication (Tracking)

+ (void)load {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    capsule::Swizzle([self class], @selector(sendEvent:), @selector(capsule_sendEvent:));
  });
}

- (void)capsule_sendEvent:(NSEvent*)event {
  if (([event type] == OurKeyDown && [event keyCode] == kVK_F9)) {
    capsule::io::WriteHotkeyPressed();
  }
  [self capsule_sendEvent:event];
}

@end
