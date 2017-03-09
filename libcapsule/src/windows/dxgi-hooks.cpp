
#include <capsule.h>
#include "win-capture.h"

#include "dxgi-vtable-helpers.h"
#include "dxgi-util.h"

typedef void (* dxgi_capture_func_t)(void *swap_ptr, void *backbuffer_ptr);
typedef void (* dxgi_free_func_t)(void);

// inspired from libobs
struct dxgi_swap_data {
  IDXGISwapChain *swap;
  dxgi_capture_func_t capture;
  dxgi_free_func_t free;
};

static struct dxgi_swap_data data = {};

///////////////////////////////////////////////////
// Detect D3D version, set up capture & free.
// follows libobs's structure
///////////////////////////////////////////////////

static bool setup_dxgi(IDXGISwapChain *swap) {
  IUnknown *device;
  HRESULT hr;

  // libobs has some logic to ignore D3D10 for call of duty ghosts
  // and just cause 3, but I'm going to make a judgement call and
  // assume they're not ending up on itch.io anytime soon so we'll
  // just roll with the no-workaround version here

  hr = swap->GetDevice(__uuidof(ID3D10Device), (void**) &device);
  if (SUCCEEDED(hr)) {
    capsule_log("Found D3D10 Device!");
    data.swap = swap;
    data.capture = d3d10_capture;
    data.free = d3d10_free;
    device->Release();
    return true;
  }

  hr = swap->GetDevice(__uuidof(ID3D11Device), (void**) &device);
  if (SUCCEEDED(hr)) {
    capsule_log("Found D3D11 Device!");
    data.swap = swap;
    data.capture = d3d11_capture;
    data.free = d3d11_free;
    device->Release();
    return true;
  }

  return false;
}

static inline IUnknown *get_dxgi_backbuffer (IDXGISwapChain *swap) {
  IDXGIResource *res = nullptr;
  HRESULT hr;

  hr = swap->GetBuffer(0, __uuidof(IUnknown), (void**) &res);
  if (FAILED(hr)) {
    capsule_log("get_dxgi_backbuffer: GetBuffer failed");
  }

  return res;
}

///////////////////////////////////////////////
// IDXGISwapChain::Present
///////////////////////////////////////////////

typedef HRESULT (CAPSULE_STDCALL *Present_t)(
  IDXGISwapChain *swap,
  UINT SyncInterval,
  UINT Flags
);
static Present_t Present_real;
static SIZE_T Present_hookId = 0;

HRESULT CAPSULE_STDCALL Present_hook (
  IDXGISwapChain *swap,
  UINT SyncInterval,
  UINT Flags
) {
  capdata.saw_dxgi = true;

  if (!data.swap) {
    setup_dxgi(swap);
  }

  // libobs has logic to capture *after* Present (if you want to capture
  // overlays). This isn't in capsule's scope, so we always capture before Present.

  bool capture = (swap == data.swap) && !!data.capture;
  if (capture) {
    IUnknown *backbuffer = get_dxgi_backbuffer(data.swap);
    if (!!backbuffer) {
      data.capture(swap, backbuffer);
      backbuffer->Release();
    }
  }

  return Present_real(swap, SyncInterval, Flags);
}

void install_present_hook (IDXGISwapChain *swap) {
  DWORD err;

  // Present
  if (!Present_hookId) {
    LPVOID Present_addr = capsule_get_IDXGISwapChain_Present_address((void *) swap);

    err = cHookMgr.Hook(
      &Present_hookId,
      (LPVOID *) &Present_real,
      Present_addr,
      Present_hook,
      0
    );
    if (err != ERROR_SUCCESS) {
      capsule_log("Hooking Present derped with error %d (%x)", err, err);
    } else {
      capsule_log("Installed Present hook");
    }
  }
}

///////////////////////////////////////////////
// D3D11CreateDeviceAndSwapChain 
///////////////////////////////////////////////

typedef HRESULT (CAPSULE_STDCALL *D3D11CreateDeviceAndSwapChain_t)(
        IDXGIAdapter         *pAdapter,
        D3D_DRIVER_TYPE      DriverType,
        HMODULE              Software,
        UINT                 Flags,
  const D3D_FEATURE_LEVEL    *pFeatureLevels,
        UINT                 FeatureLevels,
        UINT                 SDKVersion,
  const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
        IDXGISwapChain       **ppSwapChain,
        ID3D11Device         **ppDevice,
        D3D_FEATURE_LEVEL    *pFeatureLevel,
        ID3D11DeviceContext  **ppImmediateContext 
);
static D3D11CreateDeviceAndSwapChain_t D3D11CreateDeviceAndSwapChain_real;
static SIZE_T D3D11CreateDeviceAndSwapChain_hookId = 0;

HRESULT CAPSULE_STDCALL D3D11CreateDeviceAndSwapChain_hook (
        IDXGIAdapter         *pAdapter,
        D3D_DRIVER_TYPE      DriverType,
        HMODULE              Software,
        UINT                 Flags,
  const D3D_FEATURE_LEVEL    *pFeatureLevels,
        UINT                 FeatureLevels,
        UINT                 SDKVersion,
  const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
        IDXGISwapChain       **ppSwapChain,
        ID3D11Device         **ppDevice,
        D3D_FEATURE_LEVEL    *pFeatureLevel,
        ID3D11DeviceContext  **ppImmediateContext 
) {
  capsule_log("Before D3D11CreateDeviceAndSwapChain!");

  HRESULT res = D3D11CreateDeviceAndSwapChain_real(
    pAdapter, DriverType, Software, Flags,
    pFeatureLevels, FeatureLevels, SDKVersion,
    pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel,
    ppImmediateContext
  );

  if (FAILED(res)) {
    capsule_log("Swap chain creation failed, bailing out");
    return res;
  }

  install_present_hook(*ppSwapChain);

  return res;
}

///////////////////////////////////////////////
// IDXGIFactory::CreateSwapChainForHwnd
// Example users: Unity 5.x
///////////////////////////////////////////////

typedef HRESULT (CAPSULE_STDCALL *CreateSwapChainForHwnd_t)(
        IDXGIFactory2                   *factory,
        IUnknown                        *pDevice,
        HWND                            hWnd,
  const DXGI_SWAP_CHAIN_DESC1           *pDesc,
  const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
        IDXGIOutput                     *pRestrictToOutput,
        IDXGISwapChain1                 **ppSwapChain
);
static CreateSwapChainForHwnd_t CreateSwapChainForHwnd_real;
static SIZE_T CreateSwapChainForHwnd_hookId = 0;

HRESULT CAPSULE_STDCALL CreateSwapChainForHwnd_hook (
        IDXGIFactory2                   *factory,
        IUnknown                        *pDevice,
        HWND                            hWnd,
  const DXGI_SWAP_CHAIN_DESC1           *pDesc,
  const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
        IDXGIOutput                     *pRestrictToOutput,
        IDXGISwapChain1                 **ppSwapChain
  ) {
  capsule_log("Before CreateSwapChainForHwnd!");

  // this part is just a mix of curiosity of hubris, aka
  // "we have an hWnd! we can do wonderful things with it!"
  const size_t title_buffer_size = 1024;
  wchar_t title_buffer[title_buffer_size];
  title_buffer[0] = '\0';
  GetWindowText(hWnd, title_buffer, title_buffer_size);
  capsule_log("Creating swapchain for window \"%S\"", title_buffer);

  HRESULT res = CreateSwapChainForHwnd_real(factory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
  capsule_log("After CreateSwapChainForHwnd!");

  if (FAILED(res)) {
    capsule_log("Swap chain creation failed, bailing out");
    return res;
  }

  if (!Present_hookId) {
    // just a little fluff for fun
    wchar_t new_title_buffer[title_buffer_size];
    new_title_buffer[0] = '\0';
    swprintf_s(new_title_buffer, title_buffer_size, L"[capsule] %s", title_buffer);
    SetWindowText(hWnd, new_title_buffer);
  }

  install_present_hook(*ppSwapChain);

  return res;
}

///////////////////////////////////////////////
// IDXGIFactory::CreateSwapChain (Deprecated)
// Example users: MonoGame
///////////////////////////////////////////////

typedef HRESULT (CAPSULE_STDCALL *CreateSwapChain_t)(
        IDXGIFactory2                   *factory,
        IUnknown                        *pDevice,
        DXGI_SWAP_CHAIN_DESC            *pDesc,
        IDXGISwapChain                  **ppSwapChain
);
static CreateSwapChain_t CreateSwapChain_real;
static SIZE_T CreateSwapChain_hookId = 0;

HRESULT CAPSULE_STDCALL CreateSwapChain_hook (
        IDXGIFactory2                   *factory,
        IUnknown                        *pDevice,
        DXGI_SWAP_CHAIN_DESC            *pDesc,
        IDXGISwapChain                  **ppSwapChain
  ) {
  capsule_log("Before CreateSwapChain!");

  HRESULT res = CreateSwapChain_real(factory, pDevice, pDesc, ppSwapChain);
  capsule_log("After CreateSwapChain!");

  if (FAILED(res)) {
    capsule_log("Swap chain creation failed, bailing out");
    return res;
  }

  install_present_hook(*ppSwapChain);

  return res;
}

static void install_swapchain_hooks (IDXGIFactory *factory) {
  DWORD err;

  // CreateSwapChainForHwnd
  if (!CreateSwapChainForHwnd_hookId) {
    LPVOID CreateSwapChainForHwnd_addr = capsule_get_IDXGIFactory_CreateSwapChainForHwnd_address((void *) factory);

    err = cHookMgr.Hook(
      &CreateSwapChainForHwnd_hookId,
      (LPVOID *) &CreateSwapChainForHwnd_real,
      CreateSwapChainForHwnd_addr,
      CreateSwapChainForHwnd_hook,
      0
    );
    if (err != ERROR_SUCCESS) {
      capsule_log("Hooking CreateSwapChainForHwnd derped with error %d (%x)", err, err);
    }
  }

  // CreateSwapChain
  if (!CreateSwapChain_hookId) {
    LPVOID CreateSwapChain_addr = capsule_get_IDXGIFactory_CreateSwapChain_address((void *) factory);

    err = cHookMgr.Hook(
      &CreateSwapChain_hookId,
      (LPVOID *) &CreateSwapChain_real,
      CreateSwapChain_addr,
      CreateSwapChain_hook,
      0
    );
    if (err != ERROR_SUCCESS) {
      capsule_log("Hooking CreateSwapChain derped with error %d (%x)", err, err);
    }
  }
}



///////////////////////////////////////////////
// CreateDXGIFactory (DXGI 1.0)
///////////////////////////////////////////////

typedef HRESULT (CAPSULE_STDCALL *CreateDXGIFactory_t)(REFIID, void**);
static CreateDXGIFactory_t CreateDXGIFactory_real;
static SIZE_T CreateDXGIFactory_hookId = 0;

HRESULT CAPSULE_STDCALL CreateDXGIFactory_hook (REFIID riid, void** ppFactory) {
  capsule_log("CreateDXGIFactory called with riid: %s", name_from_iid(riid).c_str());
  HRESULT res = CreateDXGIFactory_real(riid, ppFactory);

  if (SUCCEEDED(res)) {
    install_swapchain_hooks((IDXGIFactory *) *ppFactory);
  } else {
    capsule_log("CreateDXGIFactory failed!");
  }

  return res;
}

///////////////////////////////////////////////
// CreateDXGIFactory1 (DXGI 1.1)
///////////////////////////////////////////////

typedef HRESULT (CAPSULE_STDCALL *CreateDXGIFactory1_t)(REFIID, void**);
static CreateDXGIFactory1_t CreateDXGIFactory1_real;
static SIZE_T CreateDXGIFactory1_hookId;

HRESULT CAPSULE_STDCALL CreateDXGIFactory1_hook (REFIID riid, void** ppFactory) {
  capsule_log("Hooked_CreateDXGIFactory1 called with riid: %s", name_from_iid(riid).c_str());
  HRESULT res = CreateDXGIFactory1_real(riid, ppFactory);

  if (SUCCEEDED(res)) {
    install_swapchain_hooks((IDXGIFactory *) *ppFactory);
  } else {
    capsule_log("CreateDXGIFactory failed!");
  }

  return res;
}

static void capsule_install_d3d11_hooks () {
  DWORD err;

  // d3d11

  HMODULE d3d11 = LoadLibrary(L"d3d11.dll");
  if (!d3d11) {
    capsule_log("Could not load d3d11.dll, disabling D3D10/11 capture");
    return;
  }

  // D3D11CreateDeviceAndSwapChain
  {
    LPVOID D3D11CreateDeviceAndSwapChain_addr = NktHookLibHelpers::GetProcedureAddress(d3d11, "D3D11CreateDeviceAndSwapChain");
    if (!D3D11CreateDeviceAndSwapChain_addr) {
      capsule_log("Could not find D3D11CreateDeviceAndSwapChain");
      return;
    }

    err = cHookMgr.Hook(
      &D3D11CreateDeviceAndSwapChain_hookId,
      (LPVOID *) &D3D11CreateDeviceAndSwapChain_real,
      D3D11CreateDeviceAndSwapChain_addr,
      D3D11CreateDeviceAndSwapChain_hook,
      0
    );
    if (err != ERROR_SUCCESS) {
      capsule_log("Hooking D3D11CreateDeviceAndSwapChain derped with error %d (%x)", err, err);
      return;
    }
    capsule_log("Installed D3D11CreateDeviceAndSwapChain hook");
  }
}

void capsule_install_dxgi_hooks () {
  DWORD err;

  // dxgi

  HMODULE dxgi = LoadLibrary(L"dxgi.dll");
  if (!dxgi) {
    capsule_log("Could not load dxgi.dll, disabling D3D10/11 capture");
    return;
  }

  // CreateDXGIFactory
  {
    LPVOID CreateDXGIFactory_addr = NktHookLibHelpers::GetProcedureAddress(dxgi, "CreateDXGIFactory");
    if (!CreateDXGIFactory_addr) {
      capsule_log("Could not find CreateDXGIFactory");
      return;
    }

    err = cHookMgr.Hook(
      &CreateDXGIFactory_hookId,
      (LPVOID *) &CreateDXGIFactory_real,
      CreateDXGIFactory_addr,
      CreateDXGIFactory_hook,
      0
    );
    if (err != ERROR_SUCCESS) {
      capsule_log("Hooking CreateDXGIFactory derped with error %d (%x)", err, err);
      return;
    }
    capsule_log("Installed CreateDXGIFactory hook");
  }

  {
    // CreateDXGIFactory1
    LPVOID CreateDXGIFactory1_addr = NktHookLibHelpers::GetProcedureAddress(dxgi, "CreateDXGIFactory1");
    if (!CreateDXGIFactory1_addr) {
      capsule_log("Could not find CreateDXGIFactory1");
      return;
    }

    err = cHookMgr.Hook(
      &CreateDXGIFactory1_hookId,
      (LPVOID *) &CreateDXGIFactory1_real,
      CreateDXGIFactory1_addr,
      CreateDXGIFactory1_hook,
      0
    );
    if (err != ERROR_SUCCESS) {
      capsule_log("Hooking CreateDXGIFactory1 derped with error %d (%x)", err, err);
      return;
    }
    capsule_log("Installed CreateDXGIFactory1 hook");
  }

  capsule_install_d3d11_hooks();
}
