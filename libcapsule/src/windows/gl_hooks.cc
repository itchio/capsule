
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

#include "../gl_capture.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

namespace capsule {
namespace gl {

typedef void* (WINAPI *wglGetProcAddress_t)(const char*);
static wglGetProcAddress_t _wglGetProcAddress = nullptr;

bool LoadOpengl (const char *path) {
  handle = dlopen(path, 0);
  if (!handle) {
    return false;
  }

  GLSYM(wglGetProcAddress)

  return true;
}

void *GetProcAddress (const char *symbol) {
  void *addr = nullptr;

  if (_wglGetProcAddress) {
    addr = _wglGetProcAddress(symbol);
  }
  if (!addr) {
    addr = ::GetProcAddress(handle, symbol);
  }

  return addr;
}

} // namespace gl
} // namespace capsule
