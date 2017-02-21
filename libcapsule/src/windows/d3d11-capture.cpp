
#include <capsule.h>
#include "win-capture.h"
#include "dxgi-util.h"

#include <d3d11.h>
#include <dxgi.h>

// inspired by libobs
struct d3d11_data {
  ID3D11Device              *device; // do not release
  ID3D11DeviceContext       *context; // do not release
	uint32_t                  cx; // framebuffer width
	uint32_t                  cy; // framebuffer height
	DXGI_FORMAT               format; // pixel format
  bool                      multisampled; // if true, subresource needs to be resolved on GPU before downloading

  ID3D11Texture2D           *copy_surfaces[NUM_BUFFERS]; // staging, CPU_READ_ACCESS
  ID3D11Texture2D           *textures[NUM_BUFFERS]; // default (in-GPU), useful to resolve multisampled backbuffers
  int                       cur_tex;

  uint32_t                  pitch; // linesize, may not be (width * components) because of alignemnt
};

static struct d3d11_data data = {};

/**
 * Copy the format, size, and multisampling information of the
 * D3D11 backbuffer to our internal state
 */
static bool d3d11_init_format(IDXGISwapChain *swap, HWND *window) {
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

  capsule_log("Backbuffer: %ux%u (%s) format = %s",
    data.cx, data.cy,
    data.multisampled ? "multisampled" : "",
    name_from_dxgi_format(data.format).c_str()
  );

  return true;
}

/**
 * Release a COM resource, if it's non-null
 */
static inline void safe_release(IUnknown *p) {
  if (p) {
    p->Release();
  }
}

/**
 * Create a staging surface (used to download data from the GPU to the CPU)
 */
static bool create_d3d11_stage_surface(ID3D11Texture2D **tex) {
  HRESULT hr;

  D3D11_TEXTURE2D_DESC desc      = {};
  desc.Width                     = data.cx;
  desc.Height                    = data.cy;
  desc.Format                    = data.format;
  desc.MipLevels                 = 1;
  desc.ArraySize                 = 1;
  desc.SampleDesc.Count          = 1;
  desc.Usage                     = D3D11_USAGE_STAGING;
  desc.CPUAccessFlags            = D3D11_CPU_ACCESS_READ;

  hr = data.device->CreateTexture2D(&desc, nullptr, tex);
  if (FAILED(hr)) {
    capsule_log("create_d3d11_stage_surface: failed to create texture (%x)", hr);
    return false;
  }

  return true;
}

/**
 * Create an on-GPU texture (used to resolve multisampled backbuffers)
 */
static bool create_d3d11_tex(uint32_t cx, uint32_t cy, ID3D11Texture2D **tex) {
  HRESULT hr;

  D3D11_TEXTURE2D_DESC desc                 = {};
  desc.Width                                = cx;
  desc.Height                               = cy;
  desc.MipLevels                            = 1;
  desc.ArraySize                            = 1;
  // desc.BindFlags                            = 0;
  desc.BindFlags                            = D3D11_BIND_SHADER_RESOURCE;
  desc.Format                               = data.format;
  desc.SampleDesc.Count                     = 1;
  desc.Usage                                = D3D11_USAGE_DEFAULT;
  desc.MiscFlags                            = 0;

  hr = data.device->CreateTexture2D(&desc, nullptr, tex);
  if (FAILED(hr)) {
    capsule_log("create_d3d11_tex: failed to create texture (%x)", hr);
    return false;
  }

  return true;
}

/**
 * Initiate both on-GPU textures and copy surfaces (staging).
 */
static bool d3d11_shmem_init_buffers(size_t idx) {
  bool success;

  success = create_d3d11_stage_surface(&data.copy_surfaces[idx]);
  if (!success) {
    capsule_log("d3d11_shmem_init_buffers: failed to create copy surface");
    return false;
  }

  // for the first texture, retrieve pitch (linesize), too
  if (idx == 0) {
    D3D11_MAPPED_SUBRESOURCE map = {};
    HRESULT hr;

    hr = data.context->Map(data.copy_surfaces[idx], 0,
      D3D11_MAP_READ, 0, &map);

    if (FAILED(hr)) {
      capsule_log("d3d11_shmem_init_buffers: failed to get pitch (%x)", hr);
      return false;
    }

    data.pitch = map.RowPitch;
    data.context->Unmap(data.copy_surfaces[idx], 0);
  }

  success = create_d3d11_tex(data.cx, data.cy, &data.textures[idx]);

  if (!success) {
    capsule_log("d3d11_shmem_init_buffers: failed to create texture");
    return false;
  }

  return true;
}

static bool d3d11_shmem_init(HWND window) {
  for (size_t i = 0; i < NUM_BUFFERS; i++) {
    if (!d3d11_shmem_init_buffers(i)) {
      return false;
    }
  }

  capsule_log("d3d11 memory capture initialize successful");
  return true;
}

static void d3d11_init(IDXGISwapChain *swap) {
  capsule_log("d3d11_init");

  bool success = true;
  HWND window;
  HRESULT hr;

  hr = swap->GetDevice(__uuidof(ID3D11Device), (void**)&data.device);
	if (FAILED(hr)) {
		capsule_log("d3d11_init: failed to get device from swap (%x)", hr);
		return;
	}
  data.device->Release();

  data.device->GetImmediateContext(&data.context);
  data.context->Release();

  d3d11_init_format(swap, &window);

  success = d3d11_shmem_init(window);

  if (!success) {
    d3d11_free();
  }
}

static inline void d3d11_copy_texture (ID3D11Resource *dst, ID3D11Resource *src) {
  if (data.multisampled) {
    data.context->ResolveSubresource(dst, 0, src, 0, data.format);
  } else {
    data.context->CopyResource(dst, src);
  }
}

static inline void d3d11_shmem_capture (ID3D11Resource* backbuffer) {
  // TODO: buffering
  HRESULT hr;

  auto timestamp = capsule_frame_timestamp();

  d3d11_copy_texture(data.textures[data.cur_tex], backbuffer);
  data.context->CopyResource(data.copy_surfaces[data.cur_tex], data.textures[data.cur_tex]);
  // d3d11_copy_texture(data.copy_surfaces[data.cur_tex], data.textures[data.cur_tex]);

  D3D11_MAPPED_SUBRESOURCE map = {};
  hr = data.context->Map(data.copy_surfaces[data.cur_tex], 0, D3D11_MAP_READ, 0, &map);
  if (SUCCEEDED(hr)) {
    char *frameData = (char *) map.pData;
    int frameDataSize = data.cy * data.pitch;
    capsule_write_video_frame(timestamp, frameData, frameDataSize);
    data.context->Unmap(data.copy_surfaces[data.cur_tex], 0);
  } else {
    capsule_log("d3d11_shmem_capture: could not map staging surface");
  }
}

void d3d11_capture(void *swap_ptr, void *backbuffer_ptr) {
  static bool first_frame = true;

  if (!capsule_capture_ready()) {
    return;
  }

  IDXGIResource *dxgi_backbuffer = (IDXGIResource*)backbuffer_ptr;
	IDXGISwapChain *swap = (IDXGISwapChain*)swap_ptr;

  if (!data.device) {
    d3d11_init(swap);
  }

  HRESULT hr;

  ID3D11Resource *backbuffer;

  hr = dxgi_backbuffer->QueryInterface(__uuidof(ID3D11Resource), (void**) &backbuffer);
  if (FAILED(hr)) {
    capsule_log("d3d11_capture: failed to get backbuffer");
    return;
  }

  if (first_frame) {
    auto pix_fmt = dxgi_format_to_pix_fmt(data.format);
    capsule_write_video_format(
      data.cx,
      data.cy,
      pix_fmt,
      0 /* no vflip */,
      data.pitch
    );
    first_frame = false;
  }

  d3d11_shmem_capture(backbuffer);
  backbuffer->Release();
}

void d3d11_free() {
  capsule_log("d3d11_free: stub");
}
