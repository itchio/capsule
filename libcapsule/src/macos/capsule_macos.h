
#import <Cocoa/Cocoa.h>

#pragma once

#define DYLD_INTERPOSE(_replacement,_replacee) \
  __attribute__((used)) static struct{ const void* replacement; const void* replacee; } _interpose_##_replacee \
__attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacement, (const void*)(unsigned long)&_replacee };

extern int capsule_capturing;
extern int capsule_had_opengl;
void capsule_swizzle (Class clazz, SEL originalSelector, SEL swizzledSelector);

@interface CapsuleFixedRecorder : NSObject

- (id)init;
- (void)handleTimer:(NSTimer *)timer;

@end
