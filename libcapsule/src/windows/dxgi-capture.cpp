
#include <capsule.h>

// #include <dxgi.h>
// #include <dxgi1_2.h>

#include "dxgi-vtable-helpers.h"
#include "dxgi-util.h"

IUnknown *g_device = NULL;
IUnknown *g_device_context = NULL;

///////////////////////////////////////////////
// FIXME: forgive me father this is all temporary I SWEAR
///////////////////////////////////////////////

#include <chrono>

using namespace std;

// oh boy that is not good
int dxgi_first_frame = 1;
int dxgi_frame_count = 0;
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
SIZE_T Present_hookId = 0;

HRESULT CAPSULE_STDCALL Present_hook (
  IDXGISwapChain *swapChain,
  UINT SyncInterval,
  UINT Flags
) {
  capsule_log("Before Present!");
  HRESULT hr;

  dxgi_frame_count++;

  if (dxgi_frame_count > 120) {
    // capsule_log("Getting buffer");
    IDXGIResource *res = nullptr;
    hr = swapChain->GetBuffer(0, __uuidof(IUnknown), (void**) &res);
    if (FAILED(hr)) {
      capsule_log("DXGI: GetBuffer failed");
    } else {
      // capsule_log("Casting to D3D11Texture");
      ID3D11Texture2D *pTexture = nullptr;
      hr = res->QueryInterface(__uuidof(ID3D11Texture2D), (void**) &pTexture);
      if (FAILED(hr)) {
        capsule_log("DXGI: Casting backbuffer to texture failed")
      } else {
        D3D11_TEXTURE2D_DESC desc = {0};
        pTexture->GetDesc(&desc);
        capsule_log("Backbuffer size: %ux%u, format = %d", desc.Width, desc.Height, desc.Format);
        capsule_log("MipLevels = %d, ArraySize = %d, SampleDesc.Count = %d", desc.MipLevels, desc.ArraySize, desc.SampleDesc.Count);

        if (desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM) {
          capsule_log("WARNING: backbuffer format isn't R8G8B8A8_UNORM, things *will* look and/or go wrong");
        }

        int multisampled = desc.SampleDesc.Count > 1;

        desc.BindFlags = 0;
        desc.MiscFlags = 0;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        ID3D11Device *device = (ID3D11Device *) g_device;
        ID3D11DeviceContext *ctx = (ID3D11DeviceContext *) g_device_context;

        ID3D11Texture2D *pResolveTexture = nullptr;

        // capsule_log("Creating Texture2D, device = %p", g_device);
        ID3D11Texture2D *pStagingTexture = nullptr;
        hr = device->CreateTexture2D(&desc, nullptr, &pStagingTexture);
        if (FAILED(hr)) {
          capsule_log("Could not create staging texture (err %x)", hr)
        }

        // capsule_log("Copying resource, ctx = %p", g_device_context);
        if (multisampled) {
          desc.Usage = D3D11_USAGE_DEFAULT;
          desc.CPUAccessFlags = 0;
          hr = device->CreateTexture2D(&desc, nullptr, &pResolveTexture);
          if (FAILED(hr)) {
            capsule_log("Could not create resolve texture (err %x)", hr)
          }

          ctx->ResolveSubresource(pResolveTexture, 0, pTexture, 0, desc.Format);
          ctx->CopyResource(pStagingTexture, pResolveTexture);
        } else {
          ctx->CopyResource(pStagingTexture, pTexture);
        }

        D3D11_MAPPED_SUBRESOURCE mapped = {0};
        // capsule_log("Mapping staging texture");
        hr = ctx->Map(pStagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(hr)) {
          capsule_log("Could not map staging texture (err %x)", hr)
        }

        // capsule_log("Sending frame");
        uint8_t *pixels = (uint8_t *) mapped.pData;

        if (dxgi_first_frame) {
          capsule_write_video_format(desc.Width, desc.Height, CAPSULE_PIX_FMT_RGBA, 0 /* no vflip */);
          capsule_write_video_timestamp((int64_t) 0);
          dxgi_first_frame = 0;
          dxgi_first_ts = chrono::steady_clock::now();
        } else {
          auto frame_timestamp = chrono::steady_clock::now() - dxgi_first_ts;
          capsule_write_video_timestamp((int64_t) chrono::duration_cast<chrono::microseconds>(frame_timestamp).count());
        }
        capsule_write_video_frame((char *) pixels, desc.Width * desc.Height * 4);

        // capsule_log("Unmapping staging texture");
        ctx->Unmap(pStagingTexture, 0);

        // capsule_log("Releasing staging texture");
        pStagingTexture->Release();

        if (pResolveTexture) {
          pResolveTexture->Release();
        }
      }

      res->Release();
    }
  }

  capsule_log("Calling present");
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

  if (!Present_hookId) {
    wchar_t new_title_buffer[title_buffer_size];
    new_title_buffer[0] = '\0';
    swprintf_s(new_title_buffer, title_buffer_size, L"[capsule] %s", title_buffer);
    SetWindowText(hWnd, new_title_buffer);

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
  }

  return res;
}

///////////////////////////////////////////////
// IDXGIFactory::CreateSwapChain
// Note: this seems deprecated/rarely used
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
  HRESULT res = CreateSwapChain_real(factory, pDevice, pDesc, ppSwapChain);
  capsule_log("After CreateSwapChain!");
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

  // CreateSwapChain
  if (!CreateSwapChain_hookId) {
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
    }
  }

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
