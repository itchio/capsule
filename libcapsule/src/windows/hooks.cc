
#include <capsule.h>

#include <psapi.h>

CNktHookLib cHookMgr;
static int capsule_inited = 0;

void CapsuleInstallWindowsHooks () {
  if (capsule_inited) {
    return;
  }
  capsule_inited = 1;

  // Let Deviare-InProc be chatty
  cHookMgr.SetEnableDebugOutput(TRUE);

  CapsuleInstallProcessHooks();
  CapsuleInstallOpenglHooks();
  CapsuleInstallDxgiHooks();
  CapsuleInstallD3d9Hooks();
  CapsuleInstallDdrawHooks();
}

BOOL CAPSULE_STDCALL DllMain(void *hinstDLL, int reason, void *reserved) {
  // don't actually do anything here, since what we can do from DllMain
  // is limited (no LoadLibrary, etc.)

  return TRUE;
}

DWORD CAPSULE_DLL CapsuleWindowsInit() {
  wchar_t process_name[MAX_PATH];
  process_name[0] = '\0';
  GetModuleBaseName(GetCurrentProcess(), nullptr, process_name, MAX_PATH);
  DWORD pid = GetCurrentProcessId();

  CapsuleLog("capsule warming up for %S (pid %d)", process_name, pid);
  capsule::io::Init();
  CapsuleInstallWindowsHooks();

  return ERROR_SUCCESS;
}
