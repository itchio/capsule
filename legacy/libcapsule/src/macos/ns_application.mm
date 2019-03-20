
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

#import <OpenGL/OpenGL.h>
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#include "swizzle.h"
#include "../logging.h"
#include "../io.h"

#if defined(NSEventTypeKeyDown)
#define OurKeyDown NSEventTypeKeyDown
#else
#define OurKeyDown NSKeyDown
#endif

@implementation NSApplication (Tracking)

+ (void)load {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    capsule::Swizzle([self class], @selector(sendEvent:), @selector(capsule_sendEvent:));
  });
}

- (void)capsule_sendEvent:(NSEvent*)event {
  if (([event type] == OurKeyDown && [event keyCode] == kVK_F9)) {
    capsule::io::WriteHotkeyPressed();
  }
  [self capsule_sendEvent:event];
}

@end
