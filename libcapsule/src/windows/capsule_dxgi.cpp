
#include <capsule.h>

HMODULE dxgi;
HMODULE d3d10;
HMODULE d3d11;

///////////////////////////////////////////////
// CreateDXGIFactory
///////////////////////////////////////////////

#define CINTERFACE
#include <dxgi.h>
#undef CINTERFACE

typedef HRESULT (CAPSULE_STDCALL *CreateDXGIFactory_t)(REFIID, void**);
CreateDXGIFactory_t CreateDXGIFactory_real;
SIZE_T CreateDXGIFactory_hookId;

IDXGIFactory *g_factory = NULL;

typedef HRESULT (CAPSULE_STDCALL *CreateSwapChain_t)(IDXGIFactory *, IUnknown *, DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **);
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

HRESULT CAPSULE_STDCALL CreateDXGIFactory_hook (REFIID riid, void** ppFactory) {
  capsule_log("Hooked_CreateDXGIFactory called with riid: %x", riid);
  capsule_log("Before CreateDXGIFactory!");
  HRESULT res = CreateDXGIFactory_real(riid, ppFactory);
  capsule_log("After CreateDXGIFactory!");

  if (SUCCEEDED(res) && ppFactory) {
    capsule_log("Installing CreateSwapChain hook");
    IDXGIFactory *factory = (IDXGIFactory *) *ppFactory;
    g_factory = factory;

    LPVOID CreateSwapChain_addr = factory->lpVtbl->CreateSwapChain;

    DWORD err = cHookMgr.Hook(
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
  } else {
    capsule_log("CreateDXGIFactory failed! Bailing out")
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
