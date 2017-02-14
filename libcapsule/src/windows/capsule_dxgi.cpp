
#include <capsule.h>

///////////////////////////////////////////////
// CreateDXGIFactory
///////////////////////////////////////////////

typedef HRESULT (CAPSULE_STDCALL *CreateDXGIFactory_t)(REFIID, void**);
CreateDXGIFactory_t CreateDXGIFactory_real;
SIZE_T CreateDXGIFactory_hookId;

HRESULT CAPSULE_STDCALL CreateDXGIFactory_hook (REFIID riid, void** ppFactory) {
  capsule_log("Intercepted CreateDXGIFactory!");
  return CreateDXGIFactory_real(riid, ppFactory);
}

///////////////////////////////////////////////
// D3D10CreateDeviceAndSwapChain
///////////////////////////////////////////////

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

void capsule_install_dxgi_hooks () {
  DWORD err;

  // dxgi

  HINSTANCE dxgi = LoadLibrary(L"dxgi.dll");
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

  HINSTANCE d3d10 = LoadLibrary(L"d3d10.dll");
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
}
