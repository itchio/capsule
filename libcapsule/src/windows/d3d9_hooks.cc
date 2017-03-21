
#include <capsule.h>
#include "win_capture.h"

#include <d3d9.h>
#include "d3d9_vtable_helpers.h"

/* this is used just in case Present calls PresentEx or vice versa. */
static int present_recurse = 0;

static inline HRESULT get_backbuffer(IDirect3DDevice9 *device, IDirect3DSurface9 **surface) {
  // libobs includes a workaround for The Typing of The Dead: Overkill
  // and, to be completely honest: lol no.
  return device->GetRenderTarget(0, surface);
}

static void present_begin(IDirect3DDevice9 *device, IDirect3DSurface9 **backbuffer) {
  HRESULT hr;

  if (!present_recurse) {
    hr = get_backbuffer(device, backbuffer);
    if (FAILED(hr)) {
      CapsuleLog("d3d9 present_begin: failed to get backbuffer %d (%x)", hr, hr);
    }

    d3d9_capture(device, *backbuffer);
  }

  present_recurse++;
}

static void present_end(IDirect3DDevice9 *device, IDirect3DSurface9 *backbuffer) {
  if (backbuffer) {
    backbuffer->Release();
  }

  present_recurse--;
}

///////////////////////////////////////////////
// IDirect3DDevice9::Present
///////////////////////////////////////////////
typedef HRESULT (CAPSULE_STDCALL *Present_t)(
  IDirect3DDevice9   *device,
  const RECT         *pSourceRect,
  const RECT         *pDestRect,
        HWND         hDestWindowOverride,
  const RGNDATA      *pDirtyRegion
);
static Present_t Present_real;
static SIZE_T Present_hookId = 0;

HRESULT CAPSULE_STDCALL Present_hook (
  IDirect3DDevice9   *device,
  const RECT         *pSourceRect,
  const RECT         *pDestRect,
        HWND         hDestWindowOverride,
  const RGNDATA      *pDirtyRegion
) {
  // CapsuleLog("IDirect3DDevice9::Present called");
  capdata.saw_d3d9 = true;

  IDirect3DSurface9 *backbuffer = nullptr;
  present_begin(device, &backbuffer);

  HRESULT res = Present_real(
    device,
    pSourceRect,
    pDestRect,
    hDestWindowOverride,
    pDirtyRegion
  );

  present_end(device, backbuffer);

  return res;
}

///////////////////////////////////////////////
// IDirect3DDevice9Ex::PresentEx
///////////////////////////////////////////////
typedef HRESULT (CAPSULE_STDCALL *PresentEx_t)(
  IDirect3DDevice9Ex *device,
  const RECT         *pSourceRect,
  const RECT         *pDestRect,
        HWND         hDestWindowOverride,
  const RGNDATA      *pDirtyRegion,
        DWORD        dwFlags
);
static PresentEx_t PresentEx_real;
static SIZE_T PresentEx_hookId = 0;

HRESULT CAPSULE_STDCALL PresentEx_hook(
  IDirect3DDevice9Ex *device,
  const RECT         *pSourceRect,
  const RECT         *pDestRect,
        HWND         hDestWindowOverride,
  const RGNDATA      *pDirtyRegion,
        DWORD        dwFlags
) {
  // CapsuleLog("IDirect3DDevice9Ex::PresentEx called");
  capdata.saw_d3d9 = true;

  IDirect3DSurface9 *backbuffer = nullptr;
  present_begin(device, &backbuffer);

  HRESULT res = PresentEx_real(
    device,
    pSourceRect,
    pDestRect,
    hDestWindowOverride,
    pDirtyRegion,
    dwFlags
  );

  present_end(device, backbuffer);

  return res;
}

///////////////////////////////////////////////
// IDirect3DSwapChain9::Present
///////////////////////////////////////////////
typedef HRESULT (CAPSULE_STDCALL *PresentSwap_t)(
  IDirect3DSwapChain9   *swap,
  const RECT            *pSourceRect,
  const RECT            *pDestRect,
        HWND            hDestWindowOverride,
  const RGNDATA         *pDirtyRegion,
        DWORD           dwFlags
);
static PresentSwap_t PresentSwap_real;
static SIZE_T PresentSwap_hookId = 0;

HRESULT CAPSULE_STDCALL PresentSwap_hook(
  IDirect3DSwapChain9   *swap,
  const RECT            *pSourceRect,
  const RECT            *pDestRect,
        HWND            hDestWindowOverride,
  const RGNDATA         *pDirtyRegion,
        DWORD           dwFlags
) {
  // CapsuleLog("IDirect3DSwapChain9::Present called");
  capdata.saw_d3d9 = true;

  IDirect3DSurface9 *backbuffer = nullptr;
  IDirect3DDevice9 *device = nullptr;
  HRESULT hr;

  if (!present_recurse) {
    hr = swap->GetDevice(&device);
    if (SUCCEEDED(hr)) {
      device->Release();
    }
  }

  if (device) {
    // TODO: hook reset

    present_begin(device, &backbuffer);
  }

  HRESULT res = PresentSwap_real(
    swap,
    pSourceRect,
    pDestRect,
    hDestWindowOverride,
    pDirtyRegion,
    dwFlags
  );

  if (device) {
    present_end(device, backbuffer);
  }

  return res;
}

static void install_present_hooks(IDirect3DDevice9 *device) {
  DWORD err;

  if (!Present_hookId) {
    LPVOID Present_addr = capsule_get_IDirect3DDevice9_Present_address((void*) device);
    if (!Present_addr) {
      CapsuleLog("Could not find IDirect3DDevice9::Present");
      return;
    }

    err = cHookMgr.Hook(
      &Present_hookId,
      (LPVOID *) &Present_real,
      Present_addr,
      Present_hook,
      0
    );
    if (err != ERROR_SUCCESS) {
      CapsuleLog("Hooking IDirect3DDevice9::Present derped with error %d (%x)", err, err);
    } else {
      CapsuleLog("Installed IDirect3DDevice9::Present hook");
    }
  }

  if (!PresentEx_hookId) {
    IDirect3DDevice9Ex *deviceEx;
    HRESULT castRes = device->QueryInterface(__uuidof(IDirect3DDevice9Ex), (void**) &deviceEx);
    if (SUCCEEDED(castRes)) {
      LPVOID PresentEx_addr = capsule_get_IDirect3DDevice9Ex_PresentEx_address((void*) deviceEx);
      if (!PresentEx_addr) {
        CapsuleLog("Could not find IDirect3DDevice9Ex::PresentEx");
        return;
      }

      err = cHookMgr.Hook(
        &PresentEx_hookId,
        (LPVOID *) &PresentEx_real,
        PresentEx_addr,
        PresentEx_hook,
        0
      );
      if (err != ERROR_SUCCESS) {
        CapsuleLog("Hooking IDirect3DDevice9Ex::PresentEx derped with error %d (%x)", err, err);
      } else {
        CapsuleLog("Installed IDirect3DDevice9Ex::PresentEx hook");
      }
    }
  }

  if (!PresentSwap_hookId) {
    IDirect3DSwapChain9 *swap;
    HRESULT getRes = device->GetSwapChain(0, &swap);
    if (SUCCEEDED(getRes)) {
      LPVOID PresentSwap_addr = capsule_get_IDirect3DSwapChain9_Present_address((void*) swap);
      if (!PresentSwap_addr) {
        CapsuleLog("Could not find IDirect3DSwapChain9::Present");
        return;
      }

      err = cHookMgr.Hook(
        &PresentSwap_hookId,
        (LPVOID *) &PresentSwap_real,
        PresentSwap_addr,
        PresentSwap_hook,
        0
      );
      if (err != ERROR_SUCCESS) {
        CapsuleLog("Hooking IDirect3DSwapChain9::Present derped with error %d (%x)", err, err);
      } else {
        CapsuleLog("Installed IDirect3DSwapChain9::Present hook");
      }
    }
  }
}

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
static CreateDevice_t CreateDevice_real;
static SIZE_T CreateDevice_hookId = 0;

HRESULT CAPSULE_STDCALL CreateDevice_hook(
  IDirect3D9            *obj,
  UINT                  Adapter,
  D3DDEVTYPE            DeviceType,
  HWND                  hFocusWindow,
  DWORD                 BehaviorFlags,
  D3DPRESENT_PARAMETERS *pPresentationParameters,
  IDirect3DDevice9      **ppReturnedDeviceInterface
) {
  CapsuleLog("IDirect3D9::CreateDevice called with adapter %u", (unsigned int) Adapter);
  HRESULT res = CreateDevice_real(
    obj,
    Adapter,
    DeviceType,
    hFocusWindow,
    BehaviorFlags,
    pPresentationParameters,
    ppReturnedDeviceInterface
  );

  if (SUCCEEDED(res)) {
    install_present_hooks(*ppReturnedDeviceInterface);
  }

  return res;
}

static void install_device_hooks(IDirect3D9 *obj) {
  DWORD err;

  // CreateDevice
  if (!CreateDevice_hookId) {
    LPVOID CreateDevice_addr = capsule_get_IDirect3D9_CreateDevice_address((void *) obj);

    CapsuleLog("Address of CreateDevice: %p", CreateDevice_addr);

    err = cHookMgr.Hook(
      &CreateDevice_hookId,
      (LPVOID *) &CreateDevice_real,
      CreateDevice_addr,
      CreateDevice_hook,
      0
    );
    if (err != ERROR_SUCCESS) {
      CapsuleLog("Hooking CreateDevice derped with error %d (%x)", err, err);
    } else {
      CapsuleLog("Installed CreateDevice hook");
    }
  }
}

///////////////////////////////////////////////
// IDirect3D9Ex::CreateDeviceEx
///////////////////////////////////////////////
typedef HRESULT (CAPSULE_STDCALL *CreateDeviceEx_t)(
  IDirect3D9Ex          *obj,
  UINT                  Adapter,
  D3DDEVTYPE            DeviceType,
  HWND                  hFocusWindow,
  DWORD                 BehaviorFlags,
  D3DPRESENT_PARAMETERS *pPresentationParameters,
  D3DDISPLAYMODEEX      *pFullscreenDisplayMode,
  IDirect3DDevice9      **ppReturnedDeviceInterface
);
static CreateDeviceEx_t CreateDeviceEx_real;
static SIZE_T CreateDeviceEx_hookId = 0;

HRESULT CAPSULE_STDCALL CreateDeviceEx_hook (
  IDirect3D9Ex          *obj,
  UINT                  Adapter,
  D3DDEVTYPE            DeviceType,
  HWND                  hFocusWindow,
  DWORD                 BehaviorFlags,
  D3DPRESENT_PARAMETERS *pPresentationParameters,
  D3DDISPLAYMODEEX      *pFullscreenDisplayMode,
  IDirect3DDevice9      **ppReturnedDeviceInterface
) {
  CapsuleLog("IDirect3D9Ex::CreateDeviceEx called with adapter %u", (unsigned int) Adapter);
  HRESULT res = CreateDeviceEx_real(
    obj,
    Adapter,
    DeviceType,
    hFocusWindow,
    BehaviorFlags,
    pPresentationParameters,
    pFullscreenDisplayMode,
    ppReturnedDeviceInterface
  );

  if (SUCCEEDED(res)) {
    install_present_hooks(*ppReturnedDeviceInterface);
  }

  return res;
}

static void install_device_ex_hooks (IDirect3D9Ex *obj) {
  DWORD err;

  install_device_hooks((IDirect3D9 *) obj);

  // CreateDeviceEx
  if (!CreateDeviceEx_hookId) {
    LPVOID CreateDeviceEx_addr = capsule_get_IDirect3D9Ex_CreateDeviceEx_address((void *) obj);

    CapsuleLog("Address of CreateDeviceEx: %p", CreateDeviceEx_addr);

    err = cHookMgr.Hook(
      &CreateDeviceEx_hookId,
      (LPVOID *) &CreateDeviceEx_real,
      CreateDeviceEx_addr,
      CreateDeviceEx_hook,
      0
    );
    if (err != ERROR_SUCCESS) {
      CapsuleLog("Hooking CreateDeviceEx derped with error %d (%x)", err, err);
    } else {
      CapsuleLog("Installed CreateDeviceEx hook");
    }
  }
}

///////////////////////////////////////////////
// Direct3DCreate9
///////////////////////////////////////////////

typedef IDirect3D9* (CAPSULE_STDCALL *Direct3DCreate9_t)(
  UINT SDKVersion
);
static Direct3DCreate9_t Direct3DCreate9_real;
static SIZE_T Direct3DCreate9_hookId = 0;

IDirect3D9 * CAPSULE_STDCALL Direct3DCreate9_hook(
  UINT SDKVersion
) {
  CapsuleLog("Direct3DCreate9 called with SDKVersion %u", (unsigned int) SDKVersion);
  IDirect3D9 * res = Direct3DCreate9_real(SDKVersion);
  if (res) {
    CapsuleLog("d3d9 created!");
    install_device_hooks(res);
  } else {
    CapsuleLog("d3d9 could not be created.");
  }

  return res;
}

///////////////////////////////////////////////
// Direct3DCreate9Ex
///////////////////////////////////////////////

typedef HRESULT (CAPSULE_STDCALL *Direct3DCreate9Ex_t)(
  UINT SDKVersion,
  IDirect3D9Ex **ppD3D
);
static Direct3DCreate9Ex_t Direct3DCreate9Ex_real;
static SIZE_T Direct3DCreate9Ex_hookId = 0;

HRESULT CAPSULE_STDCALL Direct3DCreate9Ex_hook(
  UINT SDKVersion,
  IDirect3D9Ex **ppD3D
) {
  CapsuleLog("Direct3DCreate9Ex called with SDKVersion %u", (unsigned int) SDKVersion);
  HRESULT res = Direct3DCreate9Ex_real(SDKVersion, ppD3D);
  if (SUCCEEDED(res)) {
    CapsuleLog("d3d9ex created!, address = %p", *ppD3D);
    void *d3d9ex;
    HRESULT hr = (*ppD3D)->QueryInterface(__uuidof(IDirect3D9Ex), (void**) &d3d9ex);
    if (SUCCEEDED(hr)) {
      CapsuleLog("found IDirect3D9Ex interface");
    } else {
      CapsuleLog("could not find IDirect3D9Ex interface");
    }
    install_device_ex_hooks(*ppD3D);
  } else {
    CapsuleLog("d3d9ex could not be created.");
  }

  return res;
}

void capsule_install_d3d9_hooks () {
  DWORD err;

  HMODULE d3d9 = LoadLibrary(L"d3d9.dll");
  if (!d3d9) {
    CapsuleLog("Could not load d3d9.dll, disabling D3D9 capture");
    return;
  }

  // Direct3DCreate9
  if (!Direct3DCreate9_hookId) {
    LPVOID Direct3DCreate9_addr = NktHookLibHelpers::GetProcedureAddress(d3d9, "Direct3DCreate9");
    if (!Direct3DCreate9_addr) {
      CapsuleLog("Could not find Direct3DCreate9");
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
      CapsuleLog("Hooking Direct3DCreate9 derped with error %d (%x)", err, err);
      return;
    }
    CapsuleLog("Installed Direct3DCreate9 hook");
  }

  // Direct3DCreate9Ex
  if (!Direct3DCreate9Ex_hookId) {
    LPVOID Direct3DCreate9Ex_addr = NktHookLibHelpers::GetProcedureAddress(d3d9, "Direct3DCreate9Ex");
    if (!Direct3DCreate9Ex_addr) {
      CapsuleLog("Could not find Direct3DCreate9Ex");
      return;
    }

    err = cHookMgr.Hook(
      &Direct3DCreate9Ex_hookId,
      (LPVOID *) &Direct3DCreate9Ex_real,
      Direct3DCreate9Ex_addr,
      Direct3DCreate9Ex_hook,
      0
    );
    if (err != ERROR_SUCCESS) {
      CapsuleLog("Hooking Direct3DCreate9Ex derped with error %d (%x)", err, err);
      return;
    }
    CapsuleLog("Installed Direct3DCreate9Ex hook");
  }
}