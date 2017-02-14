
#include <capsule.h>

CNktHookLib cHookMgr;
static int capsule_inited = 0;

void capsule_install_windows_hooks () {
  if (capsule_inited) {
    return;
  }
  capsule_inited = 1;

  // Let Deviare-InProc be chatty
  cHookMgr.SetEnableDebugOutput(TRUE);

  capsule_install_opengl_hooks();
  capsule_install_dxgi_hooks();
}

BOOL CAPSULE_STDCALL DllMain(void *hinstDLL, int reason, void *reserved) {
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      capsule_log("DllMain (PROCESS_ATTACH)", reason); break;
    case DLL_PROCESS_DETACH:
      capsule_log("DllMain (PROCESS_DETACH)", reason); break;
    case DLL_THREAD_ATTACH:
      capsule_log("DllMain (THREAD_ATTACH)", reason); break;
    case DLL_THREAD_DETACH:
      capsule_log("DllMain (THREAD_DETACH)", reason); break;
  }

  if (reason == DLL_PROCESS_ATTACH) {
    // we've just been attached to a process
	  capsule_install_windows_hooks();
  }
  return TRUE;
}