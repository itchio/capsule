
#include "swizzle.h"

namespace capsule {

void Swizzle (Class clazz, SEL original_selector, SEL swizzled_selector) {
  Method original_method = class_getInstanceMethod(clazz, original_selector);
  Method swizzled_method = class_getInstanceMethod(clazz, swizzled_selector);

  BOOL didAddMethod = class_addMethod(clazz,
    original_selector,
    method_getImplementation(swizzled_method),
    method_getTypeEncoding(swizzled_method));

  if (didAddMethod) {
    class_replaceMethod(clazz, swizzled_selector,
      method_getImplementation(original_method),
      method_getTypeEncoding(original_method));
  } else {
    method_exchangeImplementations(original_method, swizzled_method);
  }
}

} // namespace capsule

