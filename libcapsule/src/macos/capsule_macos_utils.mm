
#include "capsule.h"

#import <objc/runtime.h>
#include <Foundation/NSObjCRuntime.h>

void CapsuleSwizzle (Class clazz, SEL originalSelector, SEL swizzledSelector) {
  NSLog(@"Getting original");
  Method originalMethod = class_getInstanceMethod(clazz, originalSelector);
  NSLog(@"Getting swizzled");
  Method swizzledMethod = class_getInstanceMethod(clazz, swizzledSelector);

  NSLog(@"Adding method, class = %@", clazz);
  BOOL didAddMethod = class_addMethod(clazz,
    originalSelector,
    method_getImplementation(swizzledMethod),
    method_getTypeEncoding(swizzledMethod));

  if (didAddMethod) {
    NSLog(@"Added method, replacing");
    class_replaceMethod(clazz, swizzledSelector,
      method_getImplementation(originalMethod),
      method_getTypeEncoding(originalMethod));
  } else {
    NSLog(@"Did not add method, exchanging impls");
    method_exchangeImplementations(originalMethod, swizzledMethod);
  }
}
