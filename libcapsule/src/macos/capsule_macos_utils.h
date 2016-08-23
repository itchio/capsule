
#import <Cocoa/Cocoa.h>

#pragma once

extern int capsule_capturing;
extern int capsule_had_opengl;
void capsule_swizzle (Class class, SEL originalSelector, SEL swizzledSelector);

@interface CapsuleFixedRecorder : NSObject

- (id)init;
- (void)handleTimer:(NSTimer *)timer;

@end
