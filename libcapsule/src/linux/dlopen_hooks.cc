
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

#include "dlopen_hooks.h"

#include <lab/strings.h>

#include "../ensure.h"
#include "../logging.h"
#include "../gl_capture.h"

namespace capsule {
namespace dl {

typedef void* (*dlopen_type)(const char*, int);
dlopen_type dlopen; 

void *NakedOpen(const char* path, int flags) {
  if (!dl::dlopen) {
    // since we intercept dlopen, we need to get the address of
    // the real dlopen, so that we can, y'know, open libraries.
    dl::dlopen = (dlopen_type) dlsym(RTLD_NEXT, "dlopen");
    Ensure("found dlopen", !!dl::dlopen);
  }

  return dl::dlopen(path, flags);
}

}
}

extern "C" {

void* dlopen (const char *filename, int flag) {
  static bool faked_alsa = false;
  static bool faked_gl = false;

  if (filename != nullptr && lab::strings::CContains(filename, "libGL.so.1")) {
    capsule::gl::LoadOpengl(filename);

    if (lab::strings::CEquals(filename, "libGL.so.1")) {
      // load libGL symbols into our space
      capsule::dl::NakedOpen(filename, RTLD_NOW|RTLD_GLOBAL);
      if (!faked_gl) {
        faked_gl = true;
        capsule::Log("dlopen: faking libGL (for %s)", filename);
      }
      // then return our own space, so our intercepts still work
      return capsule::dl::NakedOpen(nullptr, RTLD_NOW|RTLD_LOCAL);
    } else {
      // vendor implementations of libGL will have an absolute path,
      // we need to load them regardless of our intercepts
      capsule::Log("dlopen: loading real libGL from %s", filename);
      return capsule::dl::NakedOpen(filename, flag);
    }
  } else if (filename != nullptr && lab::strings::CContains(filename, "libasound.so.2")) {
    // load libasound into our space
    capsule::dl::NakedOpen(filename, RTLD_NOW|RTLD_GLOBAL);
    if (!faked_alsa) {
      faked_alsa = true;
      capsule::Log("dlopen: faking libasound (for %s)", filename);
    }
    // then return our own space, so our intercepts still work
    return capsule::dl::NakedOpen(nullptr, RTLD_NOW|RTLD_LOCAL);
  } else {
    void *res = capsule::dl::NakedOpen(filename, flag);
    capsule::Log("dlopen %s with %0x: %p", filename, flag, res);
    return res;
  }
}

} // extern "C"
