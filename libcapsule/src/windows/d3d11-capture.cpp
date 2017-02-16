
#include <capsule.h>
#include "win-capture.h"
#include "dxgi-util.h"

#include <d3d11.h>
#include <dxgi.h>

// inspired by libobs
struct d3d11_data {
  ID3D11Device              *device; // do not release
  ID3D11DeviceContext       *context; // do not release
	uint32_t                  cx;
	uint32_t                  cy;
	DXGI_FORMAT               format;
  bool                      multisampled;

  // todo: async GPU download
  ID3D11Texture2D           *copy_surface; // staging, CPU_READ_ACCESS
  ID3D11Texture2D           *texture; // default (in-GPU), useful to resolve multisampled backbuffers
};

static struct d3d11_data data = {};

static bool d3d11_init_format (IDXGISwapChain *swap, HWND *window) {
  DXGI_SWAP_CHAIN_DESC desc;
  HRESULT hr;

  hr = swap->GetDesc(&desc);
  if (FAILED(hr)) {
    capsule_log("d3d11_init_format: swap->GetDesc failed");
    return false;
  }
  data.format = fix_dxgi_format(desc.BufferDesc.Format);
  data.multisampled = desc.SampleDesc.Count > 1;
  *window = desc.OutputWindow;
  data.cx = desc.BufferDesc.Width;
  data.cy = desc.BufferDesc.Height;

  return true;
}

static void d3d11_init (IDXGISwapChain *swap) {
  HRESULT hr;
  HWND window;

  hr = swap->GetDevice(__uuidof(ID3D11Device), (void**)&data.device);
	if (FAILED(hr)) {
		capsule_log("d3d11_init: failed to get device from swap", hr);
		return;
	}
  data.device->Release();

  data.device->GetImmediateContext(&data.context);
  data.context->Release();
}

void d3d11_capture (void *swap_ptr, void *backbuffer_ptr) {
  IDXGIResource *dxgi_backbuffer = (IDXGIResource*)backbuffer_ptr;
	IDXGISwapChain *swap = (IDXGISwapChain*)swap_ptr;

  static bool first_frame = true;
  auto timestamp = capsule_frame_timestamp();

  if (!data.device) {
    d3d11_init(swap);
  }

  HRESULT hr;

  // capsule_log("Casting to D3D11Texture");
  ID3D11Texture2D *pTexture = nullptr;
  hr = dxgi_backbuffer->QueryInterface(__uuidof(ID3D11Texture2D), (void**) &pTexture);
  if (FAILED(hr)) {
    capsule_log("DXGI: Casting backbuffer to texture failed")
    return;
  }

  D3D11_TEXTURE2D_DESC desc = {0};
  pTexture->GetDesc(&desc);
  capsule_log("Backbuffer size: %ux%u, format = %d", desc.Width, desc.Height, desc.Format);
  // capsule_log("MipLevels = %d, ArraySize = %d, SampleDesc.Count = %d", desc.MipLevels, desc.ArraySize, desc.SampleDesc.Count);

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

  ID3D11Texture2D *pResolveTexture = nullptr;
  ID3D11Texture2D *pStagingTexture = nullptr;

  hr = data.device->CreateTexture2D(&desc, nullptr, &pStagingTexture);
  if (FAILED(hr)) {
    capsule_log("Could not create staging texture (err %x)", hr)
    goto end;
  }

  if (multisampled) {
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;
    hr = data.device->CreateTexture2D(&desc, nullptr, &pResolveTexture);
    if (FAILED(hr)) {
      capsule_log("Could not create resolve texture (err %x)", hr)
      goto end;
    }

    data.context->ResolveSubresource(pResolveTexture, 0, pTexture, 0, desc.Format);
    data.context->CopyResource(pStagingTexture, pResolveTexture);
  } else {
    data.context->CopyResource(pStagingTexture, pTexture);
  }

  D3D11_MAPPED_SUBRESOURCE mapped = {0};
  // capsule_log("Mapping staging texture");
  hr = data.context->Map(pStagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
  if (FAILED(hr)) {
    capsule_log("Could not map staging texture (err %x)", hr)
    goto end;
  }

  // capsule_log("Sending frame");
  char *frameData = (char *) mapped.pData;
  int frameDataSize = desc.Width * desc.Height * 4;

  if (first_frame) {
    capsule_write_video_format(desc.Width, desc.Height, CAPSULE_PIX_FMT_RGBA, 0 /* no vflip */);
    first_frame = false;
  }
  capsule_write_video_frame(timestamp, frameData, frameDataSize);

  // capsule_log("Unmapping staging texture");
  data.context->Unmap(pStagingTexture, 0);

end:
  if (pResolveTexture) {
    pResolveTexture->Release();
  }
  if (pStagingTexture) {
    pStagingTexture->Release();
  }
}

void d3d11_free () {
  capsule_log("d3d11_free: stub");
}
