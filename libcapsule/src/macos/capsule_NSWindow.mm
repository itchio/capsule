
#include "capsule.h"
#include "capsule_macos.h"

// for getpid
#include <unistd.h>

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>

static bool windows_found = false;
static CGWindowID windowId = kCGNullWindowID;

@implementation CapsuleFixedRecorder

- (id)init {
  self = [super init];
  float interval = 1.0f / 20.0f;
  [NSTimer scheduledTimerWithTimeInterval:interval target: self selector:@selector(handleTimer:) userInfo:nil repeats:YES];
  return self;
}

- (void)handleTimer:(NSTimer *)timer {
  if (!capdata.active) {
    [timer invalidate];
    return;
  }

  auto timestamp = capsule::FrameTimestamp();

  if (!windows_found) {
    windows_found = true;
    CapsuleLog("First CGWindow frame capture");
    pid_t ourPID = getpid();

    NSArray *windows = (NSArray*) CGWindowListCopyWindowInfo(kCGWindowListExcludeDesktopElements, kCGNullWindowID);
    CapsuleLog("Got %lu windows", (unsigned long) [windows count])

    int bestWidth = -1;
    int bestHeight = -1;

    for (NSDictionary *window in windows) {
      pid_t ownerPID = [[window objectForKey:(NSString *)kCGWindowOwnerPID] intValue];
      if (ownerPID == ourPID) {
        NSString *windowName = [window objectForKey:(NSString *)kCGWindowName];
        CapsuleLog("PID %d, name %s", ownerPID, [windowName UTF8String]);

        NSDictionary *bounds = [window objectForKey:(NSString *)kCGWindowBounds];
        int width = [[bounds objectForKey:@"Width"] intValue];
        int height = [[bounds objectForKey:@"Height"] intValue];
        CapsuleLog("bounds: %dx%d", width, height);

        if (width >= bestWidth || height >= bestHeight) {
          CapsuleLog("new best size: %dx%d", width, height);
          bestWidth = width;
          bestHeight = height;
          windowId = [[window objectForKey:(NSString *)kCGWindowNumber] unsignedIntValue];
        }
      }
    }
    CapsuleLog("Our window id = %d", (int) windowId);
  }

  CGImageRef image = CGWindowListCreateImage(
      CGRectNull, // indicate the minimum rectangle that encloses the specified windows
      kCGWindowListOptionIncludingWindow,
      windowId,
      // kCGWindowImageBoundsIgnoreFraming|kCGWindowImageBestResolution
      kCGWindowImageBoundsIgnoreFraming
      );

  int width = CGImageGetWidth(image);
  int height = CGImageGetHeight(image);
  CapsuleLog("Captured %dx%d CFImage", width, height);

  CFDataRef dataRef = CGDataProviderCopyData(CGImageGetDataProvider(image));
  char *frameData = (char*) CFDataGetBytePtr(dataRef);
  size_t frameDataSize = CFDataGetLength(dataRef);
  capsule::io::WriteVideoFrame(timestamp, frameData, frameDataSize);

  CFRelease(dataRef);
  CGImageRelease(image);
}

@end

@implementation NSWindow (Tracking)

+ (void)load {
  return;
  CapsuleLog("Loading NSWindow");

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    CapsuleLog("Swizzling sendEvent implementations");
    CapsuleSwizzle([self class], @selector(sendEvent:), @selector(capsule_sendEvent:));
  });
}
- (void)capsule_sendEvent:(NSEvent*)event {
  NSLog(@"Window event: %@", event);
  [self capsule_sendEvent:event];
}

@end
