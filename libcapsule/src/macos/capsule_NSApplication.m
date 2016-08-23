
#include "capsule.h"
#include "capsule_macos_utils.h"
#include <OpenGL/OpenGL.h>

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <objc/runtime.h>

@implementation NSApplication (Tracking)

CGEventRef eventCallback (CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
  NSLog(@"Tapped event: %@", event);
  return event;
}

OSStatus KeyboardEventHandler(EventHandlerCallRef myHandlerChain, 
        EventRef event, void* userData) {
  NSLog(@"CG keyboard event");
  return noErr;
}

+ (void)load {
  capsule_log("Loading NSApplication");

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    capsule_log("Swizzling sendEvent implementations");
    capsule_swizzle([self class], @selector(sendEvent:), @selector(capsule_sendEvent:));
    capsule_log("Swizzling run implementations");
    capsule_swizzle([self class], @selector(run), @selector(capsule_run));
  });
}

static CapsuleFixedRecorder *recorder;

- (void)capsule_run {
  [NSEvent addLocalMonitorForEventsMatchingMask:NSKeyDownMask 
					handler:^(NSEvent *event) {
					  NSLog( @"keyDown event! %@", event);
					  return event;
					}];

  EventTypeSpec keyboardHandlerEvents = 
  { kEventClassKeyboard, kEventRawKeyDown }; 

  InstallApplicationEventHandler(NewEventHandlerUPP(KeyboardEventHandler), 
      1, &keyboardHandlerEvents, self, NULL); 

  CFMachPortRef      eventTap;
  CGEventMask        eventMask;
  CFRunLoopSourceRef runLoopSource;

  // Create an event tap. We are interested in key presses.
  capsule_log("Creating event tap...");
  eventMask = ((1 << kCGEventKeyDown) | (1 << kCGEventKeyUp));
  ProcessSerialNumber *psn;
  GetCurrentProcess(&psn);
  eventTap = CGEventTapCreateForPSN(&psn, kCGHeadInsertEventTap, 0,
			      eventMask, eventCallback, NULL);
  if (!eventTap) {
    fprintf(stderr, "failed to create event tap\n");
  } else {
    // Create a run loop source.
    capsule_log("Creating run loop source");
    runLoopSource = CFMachPortCreateRunLoopSource(
			kCFAllocatorDefault, eventTap, 0);

    // Add to the current run loop.
    capsule_log("Adding to current run loop");
    CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource,
			kCFRunLoopCommonModes);

    // Enable the event tap.
    capsule_log("Enabling event tap");
    CGEventTapEnable(eventTap, true);

    capsule_log("Done adding event tap!");
  }

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

