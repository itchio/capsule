
#include <capsule.h>

#include <psapi.h>

CNktHookLib cHookMgr;
static int capsule_inited = 0;

void capsule_install_windows_hooks () {
  if (capsule_inited) {
    return;
  }
  capsule_inited = 1;

  // Let Deviare-InProc be chatty
  cHookMgr.SetEnableDebugOutput(TRUE);

  capsule_install_process_hooks();
  capsule_install_opengl_hooks();
  capsule_install_dxgi_hooks();
  capsule_install_d3d9_hooks();
  capsule_install_ddraw_hooks();
}

BOOL CAPSULE_STDCALL DllMain(void *hinstDLL, int reason, void *reserved) {
  // don't actually do anything here, since what we can do from DllMain
  // is limited (no LoadLibrary, etc.)

  return TRUE;
}

DWORD CAPSULE_DLL capsule_windows_init() {
  wchar_t process_name[MAX_PATH];
  process_name[0] = '\0';
  GetModuleBaseName(GetCurrentProcess(), nullptr, process_name, MAX_PATH);
  DWORD pid = GetCurrentProcessId();

  capsule_log("capsule warming up for %S (pid %d)", process_name, pid);
  capsule_io_init();
  capsule_install_windows_hooks();

  return ERROR_SUCCESS;
}
