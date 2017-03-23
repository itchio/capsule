
#pragma once

#import <objc/runtime.h>
#include <Foundation/NSObjCRuntime.h>

namespace capsule {

void Swizzle (Class clazz, SEL originalSelector, SEL swizzledSelector);

} // namespace capsule

