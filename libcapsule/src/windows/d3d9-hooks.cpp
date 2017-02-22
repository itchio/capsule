
#include <capsule.h>

#include <d3d9.h>
#include "d3d9-vtable-helpers.h"

///////////////////////////////////////////////
// IDirect3D9::CreateDevice
///////////////////////////////////////////////
typedef HRESULT (CAPSULE_STDCALL *CreateDevice_t)(
  IDirect3D9            *obj,
  UINT                  Adapter,
  D3DDEVTYPE            DeviceType,
  HWND                  hFocusWindow,
  DWORD                 BehaviorFlags,
  D3DPRESENT_PARAMETERS *pPresentationParameters,
  IDirect3DDevice9      **ppReturnedDeviceInterface
);
CreateDevice_t CreateDevice_real;
SIZE_T CreateDevice_hookId = 0;

HRESULT CAPSULE_STDCALL CreateDevice_hook (
  IDirect3D9            *obj,
  UINT                  Adapter,
  D3DDEVTYPE            DeviceType,
  HWND                  hFocusWindow,
  DWORD                 BehaviorFlags,
  D3DPRESENT_PARAMETERS *pPresentationParameters,
  IDirect3DDevice9      **ppReturnedDeviceInterface
) {
  capsule_log("IDirect3D9::CreateDevice called with adapter %u", (unsigned int) Adapter);
  HRESULT res = CreateDevice_real(
    obj,
    Adapter,
    DeviceType,
    hFocusWindow,
    BehaviorFlags,
    pPresentationParameters,
    ppReturnedDeviceInterface
  );
  return res;
}

static void install_device_hooks (IDirect3D9 *obj) {
  DWORD err;

  // CreateDevice
  if (!CreateDevice_hookId) {
    LPVOID CreateDevice_addr = capsule_get_IDirect3D9_CreateDevice_address((void *) obj);

    err = cHookMgr.Hook(
      &CreateDevice_hookId,
      (LPVOID *) &CreateDevice_real,
      CreateDevice_addr,
      CreateDevice_hook,
      0
    );
    if (err != ERROR_SUCCESS) {
      capsule_log("Hooking CreateDevice derped with error %d (%x)", err, err);
    } else {
      capsule_log("Installed CreateDevice hook");
    }
  }
}

///////////////////////////////////////////////
// Direct3DCreate9
///////////////////////////////////////////////

typedef IDirect3D9* (CAPSULE_STDCALL *Direct3DCreate9_t)(
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
    install_device_hooks(res);
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