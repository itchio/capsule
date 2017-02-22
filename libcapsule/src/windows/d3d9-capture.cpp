
#include <capsule.h>
#include "win-capture.h"

#include <d3d9.h>
#include <dxgi.h>

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

void d3d9_capture (void *device_ptr, void *backbuffer_ptr) {

}

bool d3d9_init_format_swapchain(HWND *window) {
  capsule_log("d3d9_init_format_swapchain: stub");
  return false;
}

bool d3d9_init_format_backbuffer(HWND *window) {
  capsule_log("d3d9_init_format_backbuffer: stub");
  return false;
}

bool d3d9_shmem_init() {
  capsule_log("d3d9_shmem_init: stub");
  return false;
}

void d3d9_init(IDirect3DDevice9 *device) {
  bool success;

  HWND window = nullptr;
  HRESULT hr;

  data.device = device;

  capsule_log("Determining d3d9 format");

  if (!d3d9_init_format_backbuffer(&window)) {
    if (!d3d9_init_format_swapchain(&window)) {
      return;
    }
  }

  capsule_log("Initializing shmem d3d9 capture");

  success = d3d9_shmem_init();

  if (!success) {
    return;
  }
}
