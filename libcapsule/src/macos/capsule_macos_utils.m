
#include "capsule.h"

#import <objc/runtime.h>
#include <Foundation/NSObjCRuntime.h>

void capsule_swizzle (Class class, SEL originalSelector, SEL swizzledSelector) {
  NSLog(@"Getting original");
  Method originalMethod = class_getInstanceMethod(class, originalSelector);
  NSLog(@"Getting swizzled");
  Method swizzledMethod = class_getInstanceMethod(class, swizzledSelector);

  NSLog(@"Adding method, class = %@", class);
  BOOL didAddMethod = class_addMethod(class,
    originalSelector,
    method_getImplementation(swizzledMethod),
    method_getTypeEncoding(swizzledMethod));

  if (didAddMethod) {
    NSLog(@"Added method, replacing");
    class_replaceMethod(class, swizzledSelector,
      method_getImplementation(originalMethod),
      method_getTypeEncoding(originalMethod));
  } else {
    NSLog(@"Did not add method, exchanging impls");
    method_exchangeImplementations(originalMethod, swizzledMethod);
  }
}
