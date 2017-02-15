
#include <capsule.h>
#include "dxgi_util.h"

HMODULE dxgi;
HMODULE d3d10;
HMODULE d3d11;

#include <dxgi.h>
#include <dxgi1_2.h>

IDXGIFactory *g_factory = NULL;
IUnknown *g_device = NULL;
IUnknown *g_device_context = NULL;

///////////////////////////////////////////////
// FIXME: forgive me father this is all temporary I SWEAR
///////////////////////////////////////////////

#include <chrono>

using namespace std;

// oh boy that is not good
int dxgi_first_frame = 1;
chrono::time_point<chrono::steady_clock> dxgi_first_ts;

///////////////////////////////////////////////
// IDXGISwapChain::Present
///////////////////////////////////////////////

typedef HRESULT (CAPSULE_STDCALL *Present_t)(
  IDXGISwapChain *swapChain,
  UINT SyncInterval,
  UINT Flags
);
Present_t Present_real;
SIZE_T Present_hookId;

HRESULT CAPSULE_STDCALL Present_hook (
  IDXGISwapChain *swapChain,
  UINT SyncInterval,
  UINT Flags
) {
  capsule_log("Before Present!");
  HRESULT hr;

  IDXGIResource *res = nullptr;
  hr = swapChain->GetBuffer(0, __uuidof(IUnknown), (void**) &res);
  if (FAILED(hr)) {
    capsule_log("DXGI: GetBuffer failed");
  } else {
    ID3D11Texture2D *pTexture = nullptr;
    hr = res->QueryInterface(__uuidof(ID3D11Texture2D), (void**) &pTexture);
    if (FAILED(hr)) {
      capsule_log("DXGI: Casting backbuffer to texture failed")
    } else {
      D3D11_TEXTURE2D_DESC desc = {0};
      pTexture->GetDesc(&desc);
      // capsule_log("Backbuffer size: %ux%u, format = %d", desc.Width, desc.Height, desc.Format);

      if (desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM) {
        capsule_log("WARNING: backbuffer format isn't R8G8B8A8_UNORM, things *will* look and/or go wrong");
      }

      desc.Usage = D3D11_USAGE_STAGING;
      desc.BindFlags = 0;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
      desc.MiscFlags = 0;

      ID3D11Device *device = (ID3D11Device *) g_device;
      ID3D11DeviceContext *ctx = (ID3D11DeviceContext *) g_device_context;

      ID3D11Texture2D *pStagingTexture = nullptr;
      device->CreateTexture2D(&desc, nullptr, &pStagingTexture);

      ctx->CopyResource(pStagingTexture, pTexture);

      D3D11_MAPPED_SUBRESOURCE mapped = {0};
      ctx->Map(pStagingTexture, 0, D3D11_MAP_READ, 0, &mapped);

      uint8_t *pixels = (uint8_t *) mapped.pData;

      if (dxgi_first_frame) {
        capsule_write_resolution(desc.Width, desc.Height);
        capsule_write_timestamp((int64_t) 0);
        dxgi_first_frame = 0;
        dxgi_first_ts = chrono::steady_clock::now();
      } else {
        auto frame_timestamp = chrono::steady_clock::now() - dxgi_first_ts;
        capsule_write_timestamp((int64_t) chrono::duration_cast<chrono::microseconds>(frame_timestamp).count());
      }
      capsule_write_frame((char *) pixels, desc.Width * desc.Height * 4);
      // capsule_log("Color of first pixel: %u, %u, %u, %u",
      //   pixels[0],
      //   pixels[1],
      //   pixels[2],
      //   pixels[3]
      // );

      ctx->Unmap(pStagingTexture, 0);
      pStagingTexture->Release();
    }

    res->Release();
  }

  HRESULT phr = Present_real(swapChain, SyncInterval, Flags);
  capsule_log("After Present! succeeded? %d", SUCCEEDED(phr));
  return phr;
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
SIZE_T CreateSwapChainForHwnd_hookId;

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

  capsule_log("Before CreateSwapChainForHwnd! same factory? %d", g_factory == factory);
  const int title_buffer_size = 1024;
  wchar_t title_buffer[title_buffer_size];
  title_buffer[0] = '\0';
  GetWindowText(hWnd, title_buffer, title_buffer_size);
  capsule_log("Hwnd title = %S", title_buffer);
  HRESULT res = CreateSwapChainForHwnd_real(factory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
  capsule_log("After CreateSwapChainForHwnd!")

  g_device = pDevice;
  ((ID3D11Device *) pDevice)->GetImmediateContext((ID3D11DeviceContext **) &g_device_context);

  if (FAILED(res)) {
    capsule_log("Swap chain creation failed, bailing out");
    return res;
  }

  capsule_log("Swap chain creation succeeded, swapChain = %p", *ppSwapChain);
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

  return res;
}

///////////////////////////////////////////////
// IDXGIFactory::CreateSwapChain
///////////////////////////////////////////////

typedef HRESULT (CAPSULE_STDCALL *CreateSwapChain_t)(
    IDXGIFactory *factory,
    IUnknown *pDevice,
    DXGI_SWAP_CHAIN_DESC *pDesc,
    IDXGISwapChain **ppSwapChain
);
CreateSwapChain_t CreateSwapChain_real;
SIZE_T CreateSwapChain_hookId;

HRESULT CAPSULE_STDCALL CreateSwapChain_hook (
    IDXGIFactory *factory,
    IUnknown *pDevice,
    DXGI_SWAP_CHAIN_DESC *pDesc,
    IDXGISwapChain **ppSwapChain
  ) {
  capsule_log("Before CreateSwapChain! same factory? %d", g_factory == factory);
  HRESULT res = CreateSwapChain_real(factory, pDevice, pDesc, ppSwapChain);
  capsule_log("After CreateSwapChain!");
  return res;
}

///////////////////////////////////////////////
// CreateDXGIFactory
///////////////////////////////////////////////

typedef HRESULT (CAPSULE_STDCALL *CreateDXGIFactory_t)(REFIID, void**);
CreateDXGIFactory_t CreateDXGIFactory_real;
SIZE_T CreateDXGIFactory_hookId;

HRESULT CAPSULE_STDCALL CreateDXGIFactory_hook (REFIID riid, void** ppFactory) {
  DWORD err;

  capsule_log("Hooked_CreateDXGIFactory called with riid: %s", NameFromIID(riid).c_str());
  capsule_log("Before CreateDXGIFactory!");
  HRESULT res = CreateDXGIFactory_real(riid, ppFactory);
  capsule_log("After CreateDXGIFactory!");

  if (FAILED(res)) {
    capsule_log("CreateDXGIFactory failed! Bailing out")
    return res;
  }

  capsule_log("Installing swapchain hooks");
  IDXGIFactory *factory = (IDXGIFactory *) *ppFactory;
  g_factory = factory;

  // CreateSwapChain
  LPVOID CreateSwapChain_addr = capsule_get_CreateSwapChain_address((void *) factory);

  err = cHookMgr.Hook(
    &CreateSwapChain_hookId,
    (LPVOID *) &CreateSwapChain_real,
    CreateSwapChain_addr,
    CreateSwapChain_hook,
    0
  );
  if (err != ERROR_SUCCESS) {
    capsule_log("Hooking CreateSwapChain derped with error %d (%x)", err, err);
  } else {
    capsule_log("Installed CreateSwapChain hook");
  }

  // CreateSwapChainForHwnd
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
  } else {
    capsule_log("Installed CreateSwapChainForHwnd hook");
  }

  return res;
}

///////////////////////////////////////////////
// D3D10CreateDeviceAndSwapChain
///////////////////////////////////////////////

#include <D3D10_1.h>
#include <D3D10Misc.h>

typedef HRESULT (CAPSULE_STDCALL *D3D10CreateDeviceAndSwapChain_t)(
  IDXGIAdapter         *pAdapter,
  D3D10_DRIVER_TYPE    DriverType,
  HMODULE              Software,
  UINT                 Flags,
  UINT                 SDKVersion,
  DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
  IDXGISwapChain       **ppSwapChain,
  ID3D10Device         **ppDevice
);
D3D10CreateDeviceAndSwapChain_t D3D10CreateDeviceAndSwapChain_real;
SIZE_T D3D10CreateDeviceAndSwapChain_hookId;

HRESULT D3D10CreateDeviceAndSwapChain_hook (
  IDXGIAdapter         *pAdapter,
  D3D10_DRIVER_TYPE    DriverType,
  HMODULE              Software,
  UINT                 Flags,
  UINT                 SDKVersion,
  DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
  IDXGISwapChain       **ppSwapChain,
  ID3D10Device         **ppDevice
) {
  capsule_log("Intercepted D3D10CreateDeviceAndSwapChain!");
  return D3D10CreateDeviceAndSwapChain_real(pAdapter, DriverType, Software, Flags, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice);
}

///////////////////////////////////////////////
// D3D11CreateDeviceAndSwapChain
///////////////////////////////////////////////

#include <D3D11.h>

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
D3D11CreateDeviceAndSwapChain_t D3D11CreateDeviceAndSwapChain_real;
SIZE_T D3D11CreateDeviceAndSwapChain_hookId;

HRESULT D3D11CreateDeviceAndSwapChain_hook (
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
  capsule_log("Intercepted D3D11CreateDeviceAndSwapChain!");
  return D3D11CreateDeviceAndSwapChain_real(pAdapter, DriverType, Software,
    Flags, pFeatureLevels, FeatureLevels, SDKVersion,
    pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext
  );
}

void capsule_install_dxgi_hooks () {
  DWORD err;

  // dxgi

  dxgi = LoadLibrary(L"dxgi.dll");
  if (!dxgi) {
    capsule_log("Could not load dxgi.dll, disabling D3D10/11 capture");
    return;
  }

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

  // d3d10

  d3d10 = LoadLibrary(L"d3d10.dll");
  if (!d3d10) {
    capsule_log("Could not load d3d10.dll, disabling D3D10/11 capture");
    return;
  }

  LPVOID D3D10CreateDeviceAndSwapChain_addr = NktHookLibHelpers::GetProcedureAddress(d3d10, "D3D10CreateDeviceAndSwapChain");
  if (!D3D10CreateDeviceAndSwapChain_addr) {
    capsule_log("Could not find D3D10CreateDeviceAndSwapChain");
    return;
  }

  err = cHookMgr.Hook(
    &D3D10CreateDeviceAndSwapChain_hookId,
    (LPVOID *) &D3D10CreateDeviceAndSwapChain_real,
    D3D10CreateDeviceAndSwapChain_addr,
    D3D10CreateDeviceAndSwapChain_hook,
    0
  );
  if (err != ERROR_SUCCESS) {
    capsule_log("Hooking D3D10CreateDeviceAndSwapChain derped with error %d (%x)", err, err);
    return;
  }
  capsule_log("Installed D3D10CreateDeviceAndSwapChain hook");

  // d3d11

  d3d11 = LoadLibrary(L"d3d11.dll");
  if (!d3d11) {
    capsule_log("Could not load d3d11.dll, disabling D3D10/11 capture");
    return;
  }

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
