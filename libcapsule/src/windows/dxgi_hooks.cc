
/*
 *  capsule - the game recording and overlay toolkit
 *  Copyright (C) 2017, Amos Wenger
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details:
 * https://github.com/itchio/capsule/blob/master/LICENSE
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "../capture.h"
#include "win_capture.h"
#include "dxgi_vtable_helpers.h"
#include "dxgi_util.h"

namespace capsule {
namespace dxgi {

typedef void (* dxgi_draw_overlay_func_t)(void);
typedef void (* dxgi_capture_func_t)(void *swap_ptr, void *backbuffer_ptr);
typedef void (* dxgi_free_func_t)(void);

// inspired from libobs
struct SwapData {
  IDXGISwapChain *swap;
  dxgi_draw_overlay_func_t draw_overlay;
  dxgi_capture_func_t capture;
  dxgi_free_func_t free;
};

static SwapData data = {};

///////////////////////////////////////////////////
// Detect D3D version, set up capture & free.
// follows libobs's structure
///////////////////////////////////////////////////

static bool SetupDxgi(IDXGISwapChain *swap) {
  IUnknown *device;
  HRESULT hr;

  // libobs has some logic to ignore D3D10 for call of duty ghosts
  // and just cause 3, but I'm going to make a judgement call and
  // assume they're not ending up on itch.io anytime soon so we'll
  // just roll with the no-workaround version here

  hr = swap->GetDevice(__uuidof(ID3D10Device), (void**) &device);
  if (SUCCEEDED(hr)) {
    Log("Found D3D10 Device!");
    data.swap = swap;
    data.capture = d3d10::Capture;
    data.free = d3d10::Free;
    device->Release();
    return true;
  }

  hr = swap->GetDevice(__uuidof(ID3D11Device), (void**) &device);
  if (SUCCEEDED(hr)) {
    Log("Found D3D11 Device!");
    data.swap = swap;
    data.draw_overlay = d3d11::DrawOverlay;
    data.capture = d3d11::Capture;
    data.free = d3d11::Free;
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
    Log("get_dxgi_backbuffer: GetBuffer failed");
  }

  return res;
}

///////////////////////////////////////////////
// IDXGISwapChain::Present
///////////////////////////////////////////////

typedef HRESULT (LAB_STDCALL *Present_t)(
  IDXGISwapChain *swap,
  UINT SyncInterval,
  UINT Flags
);
static Present_t Present_real;
static SIZE_T Present_hookId = 0;

HRESULT LAB_STDCALL Present_hook (
  IDXGISwapChain *swap,
  UINT SyncInterval,
  UINT Flags
) {
  capture::SawBackend(capture::kBackendDXGI);

  if (!data.swap) {
    SetupDxgi(swap);
  }

  // libobs has logic to capture *after* Present (if you want to capture
  // overlays). This isn't in capsule's scope, so we always capture before Present.

  if (swap == data.swap) {
    if (data.capture) {
      IUnknown *backbuffer = get_dxgi_backbuffer(data.swap);
      if (!!backbuffer) {
        data.capture(swap, backbuffer);
        backbuffer->Release();
      }
    }

    if (data.draw_overlay) {
      data.draw_overlay();
    }
  }

  return Present_real(swap, SyncInterval, Flags);
}

void InstallPresentHook (IDXGISwapChain *swap) {
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
      Log("Hooking Present derped with error %d (%x)", err, err);
    } else {
      Log("Installed Present hook");
    }
  }
}

///////////////////////////////////////////////
// IDXGIFactory::CreateSwapChainForHwnd
// Example users: Unity 5.x
///////////////////////////////////////////////

typedef HRESULT (LAB_STDCALL *CreateSwapChainForHwnd_t)(
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

HRESULT LAB_STDCALL CreateSwapChainForHwnd_hook (
        IDXGIFactory2                   *factory,
        IUnknown                        *pDevice,
        HWND                            hWnd,
  const DXGI_SWAP_CHAIN_DESC1           *pDesc,
  const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
        IDXGIOutput                     *pRestrictToOutput,
        IDXGISwapChain1                 **ppSwapChain
  ) {
  Log("Before CreateSwapChainForHwnd!");

  // this part is just a mix of curiosity of hubris, aka
  // "we have an hWnd! we can do wonderful things with it!"
  const size_t title_buffer_size = 1024;
  wchar_t title_buffer[title_buffer_size];
  title_buffer[0] = '\0';
  GetWindowText(hWnd, title_buffer, title_buffer_size);
  Log("Creating swapchain for window \"%S\"", title_buffer);

  HRESULT res = CreateSwapChainForHwnd_real(factory, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
  Log("After CreateSwapChainForHwnd!");

  if (FAILED(res)) {
    Log("Swap chain creation failed, bailing out");
    return res;
  }

  if (!Present_hookId) {
    // just a little fluff for fun
    wchar_t new_title_buffer[title_buffer_size];
    new_title_buffer[0] = '\0';
    swprintf_s(new_title_buffer, title_buffer_size, L"[capsule] %s", title_buffer);
    SetWindowText(hWnd, new_title_buffer);
  }

  InstallPresentHook(*ppSwapChain);

  return res;
}

///////////////////////////////////////////////
// IDXGIFactory::CreateSwapChain (Deprecated)
// Example users: MonoGame
///////////////////////////////////////////////

typedef HRESULT (LAB_STDCALL *CreateSwapChain_t)(
        IDXGIFactory2                   *factory,
        IUnknown                        *pDevice,
        DXGI_SWAP_CHAIN_DESC            *pDesc,
        IDXGISwapChain                  **ppSwapChain
);
static CreateSwapChain_t CreateSwapChain_real;
static SIZE_T CreateSwapChain_hookId = 0;

HRESULT LAB_STDCALL CreateSwapChain_hook (
        IDXGIFactory2                   *factory,
        IUnknown                        *pDevice,
        DXGI_SWAP_CHAIN_DESC            *pDesc,
        IDXGISwapChain                  **ppSwapChain
  ) {
  Log("Before CreateSwapChain!");

  HRESULT res = CreateSwapChain_real(factory, pDevice, pDesc, ppSwapChain);
  Log("After CreateSwapChain!");

  if (FAILED(res)) {
    Log("Swap chain creation failed, bailing out");
    return res;
  }

  InstallPresentHook(*ppSwapChain);

  return res;
}

static void InstallSwapchainHooks (IDXGIFactory *factory) {
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
      Log("Hooking CreateSwapChainForHwnd derped with error %d (%x)", err, err);
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
      Log("Hooking CreateSwapChain derped with error %d (%x)", err, err);
    }
  }
}



///////////////////////////////////////////////
// CreateDXGIFactory (DXGI 1.0)
///////////////////////////////////////////////

typedef HRESULT (LAB_STDCALL *CreateDXGIFactory_t)(REFIID, void**);
static CreateDXGIFactory_t CreateDXGIFactory_real;
static SIZE_T CreateDXGIFactory_hookId = 0;

HRESULT LAB_STDCALL CreateDXGIFactory_hook (REFIID riid, void** ppFactory) {
  Log("CreateDXGIFactory called with riid: %s", NameFromIid(riid).c_str());
  HRESULT res = CreateDXGIFactory_real(riid, ppFactory);

  if (SUCCEEDED(res)) {
    InstallSwapchainHooks((IDXGIFactory *) *ppFactory);
  } else {
    Log("CreateDXGIFactory failed!");
  }

  return res;
}

///////////////////////////////////////////////
// CreateDXGIFactory1 (DXGI 1.1)
///////////////////////////////////////////////

typedef HRESULT (LAB_STDCALL *CreateDXGIFactory1_t)(REFIID, void**);
static CreateDXGIFactory1_t CreateDXGIFactory1_real;
static SIZE_T CreateDXGIFactory1_hookId;

HRESULT LAB_STDCALL CreateDXGIFactory1_hook (REFIID riid, void** ppFactory) {
  Log("Hooked_CreateDXGIFactory1 called with riid: %s", NameFromIid(riid).c_str());
  HRESULT res = CreateDXGIFactory1_real(riid, ppFactory);

  if (SUCCEEDED(res)) {
    InstallSwapchainHooks((IDXGIFactory *) *ppFactory);
  } else {
    Log("CreateDXGIFactory failed!");
  }

  return res;
}

///////////////////////////////////////////////
// D3D11CreateDeviceAndSwapChain 
///////////////////////////////////////////////

typedef HRESULT (LAB_STDCALL *D3D11CreateDeviceAndSwapChain_t)(
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

HRESULT LAB_STDCALL D3D11CreateDeviceAndSwapChain_hook (
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
  Log("Before D3D11CreateDeviceAndSwapChain!");

  HRESULT res = D3D11CreateDeviceAndSwapChain_real(
    pAdapter, DriverType, Software, Flags,
    pFeatureLevels, FeatureLevels, SDKVersion,
    pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel,
    ppImmediateContext
  );

  if (FAILED(res)) {
    Log("Swap chain creation failed, bailing out");
    return res;
  }

  // don't be fooled by the fact that this function is called D3D11CreateDeviceAndSwapChain.
  // it is actually called (sometimes) by D3D11CreateDevice, where ppSwapChain is null.
  if (ppSwapChain && *ppSwapChain) {
    InstallPresentHook(*ppSwapChain);
  }

  return res;
}

void InstallD3d11Hooks () {
  DWORD err;

  // d3d11

  HMODULE d3d11 = LoadLibrary(L"d3d11.dll");
  if (!d3d11) {
    Log("Could not load d3d11.dll, disabling D3D10/11 capture");
    return;
  }

  // D3D11CreateDeviceAndSwapChain
  {
    LPVOID D3D11CreateDeviceAndSwapChain_addr = NktHookLibHelpers::GetProcedureAddress(d3d11, "D3D11CreateDeviceAndSwapChain");
    if (!D3D11CreateDeviceAndSwapChain_addr) {
      Log("Could not find D3D11CreateDeviceAndSwapChain");
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
      Log("Hooking D3D11CreateDeviceAndSwapChain derped with error %d (%x)", err, err);
      return;
    }
    Log("Installed D3D11CreateDeviceAndSwapChain hook");
  }
}


void InstallHooks () {
  DWORD err;

  // dxgi

  HMODULE dxgi = LoadLibrary(L"dxgi.dll");
  if (!dxgi) {
    Log("Could not load dxgi.dll, disabling D3D10/11 capture");
    return;
  }

  // CreateDXGIFactory
  {
    LPVOID CreateDXGIFactory_addr = NktHookLibHelpers::GetProcedureAddress(dxgi, "CreateDXGIFactory");
    if (!CreateDXGIFactory_addr) {
      Log("Could not find CreateDXGIFactory");
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
      Log("Hooking CreateDXGIFactory derped with error %d (%x)", err, err);
      return;
    }
    Log("Installed CreateDXGIFactory hook");
  }

  {
    // CreateDXGIFactory1
    LPVOID CreateDXGIFactory1_addr = NktHookLibHelpers::GetProcedureAddress(dxgi, "CreateDXGIFactory1");
    if (!CreateDXGIFactory1_addr) {
      Log("Could not find CreateDXGIFactory1");
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
      Log("Hooking CreateDXGIFactory1 derped with error %d (%x)", err, err);
      return;
    }
    Log("Installed CreateDXGIFactory1 hook");
  }

  InstallD3d11Hooks();
}

} // namespace dxgi

} // namespace capsule
