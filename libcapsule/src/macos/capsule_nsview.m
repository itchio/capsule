
#include "capsule.h"

// for getpid
#include <unistd.h>

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#import <CoreGraphics/CoreGraphics.h>

#define LogRect(RECT) NSLog(@"%s: (%0.0f, %0.0f) %0.0f x %0.0f", #RECT, RECT.origin.x, RECT.origin.y, RECT.size.width, RECT.size.height)

@interface NSImage(saveAsJpegWithName)
- (void) saveAsJpegWithName:(NSString*) fileName;
@end

@implementation NSImage(saveAsJpegWithName)

- (void) saveAsJpegWithName:(NSString*) fileName
{
    // Cache the reduced image
    NSData *imageData = [self TIFFRepresentation];
    NSBitmapImageRep *imageRep = [NSBitmapImageRep imageRepWithData:imageData];
    NSDictionary *imageProps = [NSDictionary dictionaryWithObject:[NSNumber numberWithFloat:1.0] forKey:NSImageCompressionFactor];
    imageData = [imageRep representationUsingType:NSJPEGFileType properties:imageProps];
    [imageData writeToFile:fileName atomically:NO];
}

@end

NSBitmapImageRep *ImageRepFromImage (NSImage *image) {
  NSSize size = [image size];
  int width = size.width;
  int height = size.height;

  if (width < 1 || height < 1) {
    return nil;
  }

  NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
    initWithBitmapDataPlanes: NULL
    pixelsWide: width
    pixelsHigh: height
    bitsPerSample: 8
    samplesPerPixel: 4
    hasAlpha: YES
    isPlanar: NO
    colorSpaceName: NSCalibratedRGBColorSpace
    bytesPerRow: width * 4
    bitsPerPixel: 32];

  NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep: rep];
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext: ctx];
  [image drawAtPoint: NSZeroPoint fromRect: NSZeroRect operation: NSCompositeCopy fraction: 1.0];
  [ctx flushGraphics];
  [NSGraphicsContext restoreGraphicsState];

  return rep;
}


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
    capsule_log("Enter capsule_displayIfNeeded");
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
          NSLog(@"PID %d, name %@", ownerPID, windowName);

          NSDictionary *bounds = [window objectForKey:(NSString *)kCGWindowBounds];
          int width = [[bounds objectForKey:@"Width"] intValue];
          int height = [[bounds objectForKey:@"Height"] intValue];
          NSLog(@"bounds: %dx%d", width, height);

          if (width >= bestWidth || height >= bestHeight) {
            NSLog(@"new best size: %dx%d", width, height);
            bestWidth = width;
            bestHeight = height;
            windowId = [[window objectForKey:(NSString *)kCGWindowNumber] unsignedIntValue];
          }
        }
      }
    }

    NSLog(@"Our window id = %d", windowId);
    CGImageRef image = CGWindowListCreateImage(
      CGRectNull, // indicate the minimum rectangle that encloses the specified windows
      kCGWindowListOptionIncludingWindow,
      windowId,
      kCGWindowImageBoundsIgnoreFraming|kCGWindowImageBestResolution
    );

    int width = CGImageGetWidth(image);
    int height = CGImageGetHeight(image);

    NSLog(@"Got image: %@, %dx%d", image, width, height);
    NSLog(@"Bits per component %zu", CGImageGetBitsPerComponent(image));
    NSLog(@"Bits per pixel %zu", CGImageGetBitsPerPixel(image));
    NSLog(@"Alpha info %u", CGImageGetAlphaInfo(image));
    CFDataRef dataRef = CGDataProviderCopyData(CGImageGetDataProvider(image));
    char *frameData = (char*) CFDataGetBytePtr(dataRef);
    size_t frameDataSize = CFDataGetLength(dataRef);
    capsule_writeFrame(frameData, frameDataSize);

    // NSRect bounds = [[self contentView] bounds];
    // LogRect(bounds);
    //
    // NSImage *image = [[NSImage alloc] initWithData:[self dataWithPDFInsideRect:bounds]];
    // [image saveAsJpegWithName: @"frame.jpg"];
    // NSSize size;
    //
    // size = [image size];
    // capsule_log("Got %.0fx%.0f image", size.width, size.height);
    //
    // for (NSImageRep *rep in [image representations]) {
    //   NSLog(@"Got image representation %@", rep);
    // }
    //
    // NSBitmapImageRep *rep = ImageRepFromImage(image);
    //
    // size = [rep size];
    // capsule_log("Got %.0fx%.0f rep", size.width, size.height);
    //
    // int width = size.width;
    // int height = size.height;
    // char *frameData = (char*) [rep bitmapData];
    // size_t frameDataSize = width * 4 * height;
    // capsule_writeFrame(frameData, frameDataSize);
    //
    // NSColor *color = [rep colorAtX: width/2 y: height/2];
    // capsule_log("Color at center: %.2f, %.2f, %.2f, %.2f", [color redComponent], [color greenComponent], [color blueComponent], [color alphaComponent]);
    capsule_log("Exit capsule_displayIfNeeded");
  }

  @end
