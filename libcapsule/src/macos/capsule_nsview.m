
#include "capsule.h"

// for getpid
#include <unistd.h>

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#import <CoreGraphics/CoreGraphics.h>

@implementation NSWindow (Tracking)

+ (void)load {
  capsule_log("Loading NSWindow");

  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    capsule_log("Swizzling displayIfNeeded implementations");

    Class class = [self class];

    // TODO: DRY with NSOpenGL stuff
    SEL originalSelector = @selector(displayIfNeeded);
    SEL swizzledSelector = @selector(capsule_displayIfNeeded);

    Method originalMethod = class_getInstanceMethod(class, originalSelector);
    Method swizzledMethod = class_getInstanceMethod(class, swizzledSelector);

    BOOL didAddMethod = class_addMethod(class,
      originalSelector,
      method_getImplementation(swizzledMethod),
      method_getTypeEncoding(swizzledMethod));

      if (didAddMethod) {
        capsule_log("Added method");
        class_replaceMethod(class, swizzledSelector,
          method_getImplementation(originalMethod),
          method_getTypeEncoding(originalMethod));
      } else {
        capsule_log("Didn't add method, exchanging");
        method_exchangeImplementations(originalMethod, swizzledMethod);
      }
    });
  }

  static bool windowsDone = false;
  static CGWindowID windowId = kCGNullWindowID;
  static int bestWidth = -1;
  static int bestHeight = -1;

  - (void)capsule_displayIfNeeded {
    [self capsule_displayIfNeeded];

    if (!windowsDone) {
      windowsDone = true;
      pid_t ourPID = getpid();

      NSArray *windows = (NSArray*) CGWindowListCopyWindowInfo(kCGWindowListExcludeDesktopElements, kCGNullWindowID);
      capsule_log("Got %lu windows", (unsigned long) [windows count])

      for (NSDictionary *window in windows) {
        pid_t ownerPID = [[window objectForKey:(NSString *)kCGWindowOwnerPID] intValue];
        if (ownerPID == ourPID) {
          NSString *windowName = [window objectForKey:(NSString *)kCGWindowName];
          capsule_log("PID %d, name %s", ownerPID, [windowName UTF8String]);

          NSDictionary *bounds = [window objectForKey:(NSString *)kCGWindowBounds];
          int width = [[bounds objectForKey:@"Width"] intValue];
          int height = [[bounds objectForKey:@"Height"] intValue];
          capsule_log("bounds: %dx%d", width, height);

          if (width >= bestWidth || height >= bestHeight) {
            capsule_log("new best size: %dx%d", width, height);
            bestWidth = width;
            bestHeight = height;
            windowId = [[window objectForKey:(NSString *)kCGWindowNumber] unsignedIntValue];
          }
        }
      }
      capsule_log("Our window id = %d", (int) windowId);
    }

    CGImageRef image = CGWindowListCreateImage(
      CGRectNull, // indicate the minimum rectangle that encloses the specified windows
      kCGWindowListOptionIncludingWindow,
      windowId,
      kCGWindowImageBoundsIgnoreFraming|kCGWindowImageBestResolution
    );

    int width = CGImageGetWidth(image);
    int height = CGImageGetHeight(image);

    CFDataRef dataRef = CGDataProviderCopyData(CGImageGetDataProvider(image));
    char *frameData = (char*) CFDataGetBytePtr(dataRef);
    size_t frameDataSize = CFDataGetLength(dataRef);
    capsule_writeFrame(frameData, frameDataSize);

    CFRelease(dataRef);
    CGImageRelease(image);
  }

  @end
