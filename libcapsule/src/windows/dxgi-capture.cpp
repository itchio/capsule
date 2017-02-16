
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

///////////////////////////////////////////////
// FIXME: forgive me father this is all temporary I SWEAR
///////////////////////////////////////////////

#include <chrono>

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
Present_t Present_real;
SIZE_T Present_hookId = 0;

HRESULT CAPSULE_STDCALL Present_hook (
  IDXGISwapChain *swap,
  UINT SyncInterval,
  UINT Flags
) {
  capsule_log("Before Present!");
  HRESULT hr;

  if (!data.swap) {
    setup_dxgi(swap);
  }

  // libobs has logic to capture *after* Present (if you want to capture
  // overlays). This isn't in capsule's scope, so we always capture before Present.

  bool capture = (swap == data.swap) && !!data.capture && capsule_capture_ready();
  if (capture) {
    IUnknown *backbuffer = get_dxgi_backbuffer(data.swap);
    if (!!backbuffer) {
      data.capture(swap, backbuffer);
      backbuffer->Release();
    }
  }

  return Present_real(swap, SyncInterval, Flags);
}

///////////////////////////////////////////////
// IDXGIFactory::CreateSwapChainForHwnd
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
CreateSwapChainForHwnd_t CreateSwapChainForHwnd_real;
SIZE_T CreateSwapChainForHwnd_hookId = 0;

HRESULT CAPSULE_STDCALL CreateSwapChainForHwnd_hook (
        IDXGIFactory2                   *factory,
        IUnknown                        *pDevice,
        HWND                            hWnd,
  const DXGI_SWAP_CHAIN_DESC1           *pDesc,
  const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
        IDXGIOutput                     *pRestrictToOutput,
        IDXGISwapChain1                 **ppSwapChain
  ) {
  DWORD err;

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

    IDXGISwapChain *swapChain = (IDXGISwapChain *) *ppSwapChain;

    // Present
    LPVOID Present_addr = capsule_get_Present_address((void *) swapChain);

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

  return res;
}

///////////////////////////////////////////////
// CreateDXGIFactory (DXGI 1.0)
///////////////////////////////////////////////

typedef HRESULT (CAPSULE_STDCALL *CreateDXGIFactory_t)(REFIID, void**);
CreateDXGIFactory_t CreateDXGIFactory_real;
SIZE_T CreateDXGIFactory_hookId = 0;

void install_swapchain_hooks (IDXGIFactory *factory) {
  DWORD err;

  // CreateSwapChainForHwnd
  if (!CreateSwapChainForHwnd_hookId) {
    LPVOID CreateSwapChainForHwnd_addr = capsule_get_CreateSwapChainForHwnd_address((void *) factory);

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
}

HRESULT CAPSULE_STDCALL CreateDXGIFactory_hook (REFIID riid, void** ppFactory) {
  DWORD err;

  capsule_log("CreateDXGIFactory called with riid: %s", NameFromIID(riid).c_str());
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
CreateDXGIFactory1_t CreateDXGIFactory1_real;
SIZE_T CreateDXGIFactory1_hookId;

HRESULT CAPSULE_STDCALL CreateDXGIFactory1_hook (REFIID riid, void** ppFactory) {
  DWORD err;

  capsule_log("Hooked_CreateDXGIFactory1 called with riid: %s", NameFromIID(riid).c_str());
  HRESULT res = CreateDXGIFactory1_real(riid, ppFactory);

  if (SUCCEEDED(res)) {
    install_swapchain_hooks((IDXGIFactory *) *ppFactory);
  } else {
    capsule_log("CreateDXGIFactory failed!");
  }

  return res;
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
      capsule_log("Could not find CreateDXGIFactory");
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
}
