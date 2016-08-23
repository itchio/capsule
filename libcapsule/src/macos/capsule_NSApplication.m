
#include "capsule.h"
#include "capsule_macos_utils.h"
#include <OpenGL/OpenGL.h>

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <objc/runtime.h>

@implementation NSApplication (Tracking)

OSStatus capsule_hotKeyHandler(EventHandlerCallRef nextHandler, EventRef theEvent, void *userData) {
  @autoreleasepool {
    EventHotKeyID hotKeyID;
    GetEventParameter(theEvent, kEventParamDirectObject, typeEventHotKeyID, NULL, sizeof(hotKeyID), NULL, &hotKeyID);

    UInt32 keyID = hotKeyID.id;
    NSLog(@"keyID: %d", (unsigned int) keyID);
  }

  return noErr;
}

CGEventRef eventCallback (CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
  NSLog(@"Tapped event: %@", event);
  return event;
}

+ (void)load {
  capsule_log("Loading NSApplication");

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    // capsule_log("Swizzling sendEvent implementations");
    // capsule_swizzle([self class], @selector(sendEvent:), @selector(capsule_sendEvent:));
    capsule_log("Swizzling run implementations");
    capsule_swizzle([self class], @selector(run), @selector(capsule_run));
  });
}

static CapsuleFixedRecorder *recorder;

- (void)capsule_run {
  OSStatus err;

  // [self capsule_run];
  capsule_log("Installing hotKey handler");
  EventTypeSpec eventSpec;
  eventSpec.eventClass = kEventClassKeyboard;
  eventSpec.eventKind = kEventHotKeyReleased;
  err = InstallApplicationEventHandler(&capsule_hotKeyHandler, 1, &eventSpec, NULL, NULL);
  if (err != 0) {
    capsule_log("While installing hotkey handler: %d", err);
  }
  
  capsule_log("Adding hotkey");
  EventHotKeyID keyID;
  keyID.signature = 'cap1';
  keyID.id = 1;

  EventHotKeyRef carbonHotKey;
  UInt32 flags = 0;
  err = RegisterEventHotKey(kVK_F9, flags, keyID, GetEventDispatcherTarget(), 0, &carbonHotKey);

  if (err != 0) {
    capsule_log("While adding hotkey: err %d", err);
    return;
  }
  capsule_log("Installed hotkey.");

  [self capsule_run];
}

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
    NSLog(@"Another event: %@", event);
  }
  [self capsule_sendEvent:event];
}

@end

