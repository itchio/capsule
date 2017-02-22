
#include <capsule.h>
#include "win-capture.h"

#include <d3d9.h>
#include <dxgi.h>
#include "dxgi-util.h"

// inspired by libobs
struct d3d9_data {
  HMODULE                 d3d9;
  IDirect3DDevice9        *device; // do not release
  uint32_t                cx;
  uint32_t                cy;
  D3DFORMAT               format;

  IDirect3DSurface9       *d3d9_copytex;

  IDirect3DSurface9       *copy_surfaces[NUM_BUFFERS];
  IDirect3DSurface9       *render_targets[NUM_BUFFERS];
  IDirect3DQuery9         *queries[NUM_BUFFERS];
 	bool                    texture_mapped[NUM_BUFFERS];
	volatile bool           issued_queries[NUM_BUFFERS];
  uint32_t                pitch;
};

static struct d3d9_data data = {};

void d3d9_free() {
  for (size_t i = 0; i < NUM_BUFFERS; i++) {
    if (data.copy_surfaces[i]) {
      if (data.texture_mapped[i]) {
        data.copy_surfaces[i]->UnlockRect();
      }
      data.copy_surfaces[i]->Release();
    }
    if (data.render_targets[i]) {
      data.render_targets[i]->Release();
    }
    if (data.queries[i]) {
      data.queries[i]->Release();
    }
  }

  memset(&data, 0, sizeof(data));

  capsule_log("----------------------- d3d9 capture freed ----------------------");
}

static DXGI_FORMAT d3d9_to_dxgi_format (D3DFORMAT format) {
  switch ((unsigned long) format) {
    case D3DFMT_A2B10G10R10: return DXGI_FORMAT_R10G10B10A2_UNORM;
    case D3DFMT_A8R8G8B8:    return DXGI_FORMAT_B8G8R8A8_UNORM;
    case D3DFMT_X8R8G8B8:    return DXGI_FORMAT_B8G8R8X8_UNORM;
  }
}

static bool d3d9_get_swap_desc (D3DPRESENT_PARAMETERS *pp) {
  IDirect3DSwapChain9 *swap = nullptr;
  HRESULT hr;

  hr = data.device->GetSwapChain(0, &swap);
  if (FAILED(hr)) {
    capsule_log("d3d9_get_swap_desc: Failed to get swap chain %d (%x)", hr, hr);
    return false;
  }

  hr = swap->GetPresentParameters(pp);
  swap->Release();

  if (FAILED(hr)) {
    capsule_log("d3d9_get_swap_desc: Failed to get presentation parameters %d (%x)", hr, hr);
    return false;
  }

  return true;
}

static bool d3d9_init_format_backbuffer(HWND *window) {
  IDirect3DSurface9 *back_buffer = nullptr;
  D3DPRESENT_PARAMETERS pp;
  D3DSURFACE_DESC desc;
  HRESULT hr;
  
  if (!d3d9_get_swap_desc(&pp)) {
    return false;
  }

  hr = data.device->GetRenderTarget(0, &back_buffer);
  if (FAILED(hr)) {
    return false;
  }

  hr = back_buffer->GetDesc(&desc);
  back_buffer->Release();

  if (FAILED(hr)) {
    capsule_log("d3d9_init_format_backbuffer: Failed to get backbuffer descriptor %d (%x)", hr, hr);
  }

  data.format = desc.Format;
  *window = pp.hDeviceWindow;
  data.cx = desc.Width;
  data.cy = desc.Height;

  return true;
}

static bool d3d9_init_format_swapchain(HWND *window) {
  D3DPRESENT_PARAMETERS pp;

  if (!d3d9_get_swap_desc(&pp)) {
    return false;
  }

  data.format = pp.BackBufferFormat;
  data.cx = pp.BackBufferWidth;
  data.cy = pp.BackBufferHeight;

  return false;
}

static bool d3d9_shmem_init() {
  capsule_log("d3d9_shmem_init: stub");
  return false;
}

static void d3d9_init(IDirect3DDevice9 *device) {
  capsule_log("Initializing D3D9 capture");

  bool success;

  HWND window = nullptr;
  HRESULT hr;

  data.device = device;

  capsule_log("Determining D3D9 format");

  if (!d3d9_init_format_backbuffer(&window)) {
    if (!d3d9_init_format_swapchain(&window)) {
      return;
    }
  }

  capsule_log("Initializing shmem D3D9 capture");
  capsule_log("Backbuffer format: %ux%u %s",
    data.cx, data.cy,
    name_from_dxgi_format(d3d9_to_dxgi_format(data.format)).c_str());

  success = d3d9_shmem_init();

  if (!success) {
    return;
  }
}

void d3d9_capture (IDirect3DDevice9 *device, IDirect3DSurface9 *backbuffer) {
  static bool first_frame = true;

  if (!capsule_capture_ready()) {
    return;
  }

  if (!data.device) {
    d3d9_init(device);
  }

  capsule_log("d3d9_capture: stub");
}
