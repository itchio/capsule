
#include <capsule.h>

typedef bool (CAPSULE_STDCALL *wglSwapBuffers_t)(HANDLE hdc);
wglSwapBuffers_t wglSwapBuffers_real;
SIZE_T wglSwapBuffersHookId;

bool CAPSULE_STDCALL wglSwapBuffers_hook (HANDLE hdc) {
  opengl_capture(0, 0);
  return wglSwapBuffers_real(hdc);
}

void capsule_install_opengl_hooks () {
  DWORD err;

  HINSTANCE opengl = LoadLibrary(L"opengl32.dll");
  if (!opengl) {
    capsule_log("Could not load opengl32.dll, disabling OpenGL capture");
    return;
  }

  LPVOID wglSwapBuffers_addr = NktHookLibHelpers::GetProcedureAddress(opengl, "wglSwapBuffers");
  if (!wglSwapBuffers_addr) {
    capsule_log("Could not find wglSwapBuffers, disabling OpenGL capture");
    return;
  }

  err = cHookMgr.Hook(&wglSwapBuffersHookId, (LPVOID *) &wglSwapBuffers_real, wglSwapBuffers_addr, wglSwapBuffers_hook, 0);
  if (err != ERROR_SUCCESS) {
    capsule_log("Hooking wglSwapBuffers derped with error %d (%x)", err, err);
    return;
  }

  capsule_log("Installed wglSwapBuffers hook");
}
