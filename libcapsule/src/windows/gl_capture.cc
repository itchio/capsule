
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

#include "win_capture.h"
#include "../capsule.h"
#include "../capture.h"
#include "../gl_capture.h"

namespace capsule {
namespace gl {

typedef bool (LAB_STDCALL *wglSwapBuffers_t)(HANDLE hdc);
wglSwapBuffers_t wglSwapBuffers_real;
SIZE_T wglSwapBuffersHookId;

bool LAB_STDCALL wglSwapBuffers_hook (HANDLE hdc) {
  capture::SawBackend(capture::kBackendGL);
  Capture(0, 0);
  return wglSwapBuffers_real(hdc);
}

void InstallHooks () {
  DWORD err;

  HINSTANCE opengl = LoadLibrary(L"opengl32.dll");
  if (!opengl) {
    Log("Could not load opengl32.dll, disabling OpenGL capture");
    return;
  }

  LPVOID wglSwapBuffers_addr = NktHookLibHelpers::GetProcedureAddress(opengl, "wglSwapBuffers");
  if (!wglSwapBuffers_addr) {
    Log("Could not find wglSwapBuffers, disabling OpenGL capture");
    return;
  }

  err = cHookMgr.Hook(&wglSwapBuffersHookId, (LPVOID *) &wglSwapBuffers_real, wglSwapBuffers_addr, wglSwapBuffers_hook, 0);
  if (err != ERROR_SUCCESS) {
    Log("Hooking wglSwapBuffers derped with error %d (%x)", err, err);
    return;
  }

  Log("Installed wglSwapBuffers hook");
}

} // namespace gl
} // namespace capsule
