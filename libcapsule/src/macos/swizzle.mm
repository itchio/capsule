
/*
 *  capsule - the game recording and overlay toolkit
 *  Copyright (C) 2017, Amos Wenger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 * https://github.com/itchio/capsule/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

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

