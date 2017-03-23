
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#include <psapi.h>

#include "../capsule.h"
#include "../io.h"
#include "win_capture.h"

static int capsule_inited = 0;

namespace capsule {

CNktHookLib cHookMgr;

void InstallHooks () {
  if (capsule_inited) {
    return;
  }
  capsule_inited = 1;

  // Let Deviare-InProc be chatty
  cHookMgr.SetEnableDebugOutput(TRUE);

  process::InstallHooks();
  gl::InstallHooks();
  dxgi::InstallHooks();
  d3d9::InstallHooks();
}

} // namespace capsule

extern "C" {

BOOL LAB_STDCALL DllMain(void *hinstDLL, int reason, void *reserved) {
  // don't actually do anything here, since what we can do from DllMain
  // is limited (no LoadLibrary, etc.)

  return TRUE;
}

DWORD __declspec(dllexport) CapsuleWindowsInit() {
  wchar_t process_name[MAX_PATH];
  process_name[0] = '\0';
  GetModuleBaseName(GetCurrentProcess(), nullptr, process_name, MAX_PATH);
  DWORD pid = GetCurrentProcessId();

  capsule::Log("capsule warming up for %S (pid %d)", process_name, pid);
  capsule::io::Init();
  capsule::InstallHooks();

  return ERROR_SUCCESS;
}

}
