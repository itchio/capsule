
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
