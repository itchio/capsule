
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

#include <Cocoa/Cocoa.h>
#include <OpenGL/OpenGL.h>

#include "interpose.h"
#include "../capture.h"
#include "../gl_capture_callback.h"

namespace capsule {
namespace gl {

bool LoadOpengl (const char *path) {
  handle = dlopen(path, (RTLD_NOW|RTLD_LOCAL));
  return !!handle;
}


// Must have platform-specific implementation
void *GetProcAddress(const char *symbol) {
  return dlsym(handle, symbol);
}

} // namespace gl
} // namespace capsule

CGLError CapsuleCglFlushDrawable (CGLContextObj ctx) {
  capsule::capture::SawBackend(capsule::capture::kBackendGL);

  int width = 0;
  int height = 0;

  NSOpenGLContext *nsCtx = [NSOpenGLContext currentContext];
  if (nsCtx) {
    NSView *view = [nsCtx view];
    NSSize size = [view convertSizeToBacking: [view bounds].size];

    if (size.width > 0 && size.height > 0) {
      width = size.width;
      height = size.height;
    }
  }

  capsule::gl::Capture(width, height);

  CGLError ret = CGLFlushDrawable(ctx);

  return ret;
}

DYLD_INTERPOSE(CapsuleCglFlushDrawable, CGLFlushDrawable)

