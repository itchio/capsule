
#include <capsule.h>

#include <d3d9.h>

///////////////////////////////////////////////
// Direct3DCreate9
///////////////////////////////////////////////

typedef IDirect3D9 * (CAPSULE_STDCALL *Direct3DCreate9_t)(
  UINT SDKVersion
);
Direct3DCreate9_t Direct3DCreate9_real;
SIZE_T Direct3DCreate9_hookId = 0;

IDirect3D9 * CAPSULE_STDCALL Direct3DCreate9_hook (
  UINT SDKVersion
) {
  capsule_log("Direct3DCreate9 called with SDKVersion %u", (unsigned int) SDKVersion);
  IDirect3D9 * res = Direct3DCreate9_real(SDKVersion);
  if (res) {
    capsule_log("d3d9 device created!");
  } else {
    capsule_log("d3d9 device could not be created.");
  }

  return res;
}

void capsule_install_d3d9_hooks () {
  DWORD err;

  HMODULE d3d9 = LoadLibrary(L"d3d9.dll");
  if (!d3d9) {
    capsule_log("Could not load d3d9.dll, disabling D3D9 capture");
    return;
  }

  // Direct3DCreate9
  if (!Direct3DCreate9_hookId) {
    LPVOID Direct3DCreate9_addr = NktHookLibHelpers::GetProcedureAddress(d3d9, "Direct3DCreate9");
    if (!Direct3DCreate9_addr) {
      capsule_log("Could not find Direct3DCreate9");
      return;
    }

    err = cHookMgr.Hook(
      &Direct3DCreate9_hookId,
      (LPVOID *) &Direct3DCreate9_real,
      Direct3DCreate9_addr,
      Direct3DCreate9_hook,
      0
    );
    if (err != ERROR_SUCCESS) {
      capsule_log("Hooking Direct3DCreate9 derped with error %d (%x)", err, err);
      return;
    }
    capsule_log("Installed Direct3DCreate9 hook");
  }
}