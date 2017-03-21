
#include <capsule.h>
#include "win-capture.h"

#include <d3d9.h>
#include <dxgi.h>
#include "dxgi_util.h"

// inspired by libobs
struct d3d9_data {
  HMODULE                 d3d9;
  IDirect3DDevice9        *device; // do not release
  int                     cx;
  int                     cy;
  D3DFORMAT               format;

  IDirect3DSurface9       *d3d9_copytex;

  IDirect3DSurface9       *copy_surfaces[NUM_BUFFERS];
  IDirect3DSurface9       *render_targets[NUM_BUFFERS];
  IDirect3DQuery9         *queries[NUM_BUFFERS];
 	bool                    texture_mapped[NUM_BUFFERS];
	volatile bool           issued_queries[NUM_BUFFERS];
  intptr_t                pitch;
};

static struct d3d9_data data = {};

void D3d9Free() {
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

  CapsuleLog("----------------------- d3d9 capture freed ----------------------");
}

static DXGI_FORMAT D3d9ToDxgiFormat (D3DFORMAT format) {
  switch ((unsigned long) format) {
    case D3DFMT_A2B10G10R10: return DXGI_FORMAT_R10G10B10A2_UNORM;
    case D3DFMT_A8R8G8B8:    return DXGI_FORMAT_B8G8R8A8_UNORM;
    case D3DFMT_X8R8G8B8:    return DXGI_FORMAT_B8G8R8X8_UNORM;
    default:                 return DXGI_FORMAT_UNKNOWN;
  }
}

static bool D3d9GetSwapDesc (D3DPRESENT_PARAMETERS *pp) {
  IDirect3DSwapChain9 *swap = nullptr;
  HRESULT hr;

  hr = data.device->GetSwapChain(0, &swap);
  if (FAILED(hr)) {
    CapsuleLog("D3d9GetSwapDesc: Failed to get swap chain %d (%x)", hr, hr);
    return false;
  }

  hr = swap->GetPresentParameters(pp);
  swap->Release();

  if (FAILED(hr)) {
    CapsuleLog("D3d9GetSwapDesc: Failed to get presentation parameters %d (%x)", hr, hr);
    return false;
  }

  return true;
}

static bool D3d9InitFormatBackbuffer(HWND *window) {
  IDirect3DSurface9 *back_buffer = nullptr;
  D3DPRESENT_PARAMETERS pp;
  D3DSURFACE_DESC desc;
  HRESULT hr;
  
  if (!D3d9GetSwapDesc(&pp)) {
    return false;
  }

  hr = data.device->GetRenderTarget(0, &back_buffer);
  if (FAILED(hr)) {
    return false;
  }

  hr = back_buffer->GetDesc(&desc);
  back_buffer->Release();

  if (FAILED(hr)) {
    CapsuleLog("D3d9InitFormatBackbuffer: Failed to get backbuffer descriptor %d (%x)", hr, hr);
  }

  data.format = desc.Format;
  *window = pp.hDeviceWindow;
  data.cx = desc.Width;
  data.cy = desc.Height;

  return true;
}

static bool D3d9InitFormatSwapchain(HWND *window) {
  D3DPRESENT_PARAMETERS pp;

  if (!D3d9GetSwapDesc(&pp)) {
    return false;
  }

  data.format = pp.BackBufferFormat;
  data.cx = pp.BackBufferWidth;
  data.cy = pp.BackBufferHeight;

  return false;
}

static bool D3d9ShmemInitBuffers(size_t buffer) {
  HRESULT hr;

  hr = data.device->CreateOffscreenPlainSurface(
    data.cx,
    data.cy,
    data.format,
    D3DPOOL_SYSTEMMEM,
    &data.copy_surfaces[buffer],
    nullptr
  );

  if (FAILED(hr)) {
    CapsuleLog("D3d9ShmemInitBuffers: Failed to create surface %d (%x)", hr, hr);
    return false;
  }

  if (buffer == 0) {
    D3DLOCKED_RECT rect;
    hr = data.copy_surfaces[buffer]->LockRect(&rect, nullptr, D3DLOCK_READONLY);
    if (FAILED(hr)) {
      CapsuleLog("D3d9ShmemInitBuffers: Failed to lock buffer %d (%x)", hr, hr);
      return false;
    }

    data.pitch = rect.Pitch;
    data.copy_surfaces[buffer]->UnlockRect();
  }

  hr = data.device->CreateRenderTarget(
    data.cx,
    data.cy,
    data.format,
    D3DMULTISAMPLE_NONE,
    0,
    false,
    &data.render_targets[buffer],
    nullptr
  );
  if (FAILED(hr)) {
    CapsuleLog("D3d9ShmemInitBuffers: Failed to create render target %d (%x)", hr, hr);
    return false;
  }

  hr = data.device->CreateQuery(D3DQUERYTYPE_EVENT, &data.queries[buffer]);
  if (FAILED(hr)) {
    CapsuleLog("D3d9ShmemInitBuffers: Failed to create query %d (%x)", hr, hr);
    return false;
  }

  return true;
}

static bool D3d9ShmemInit() {
  for (size_t i = 0; i < NUM_BUFFERS; i++) {
    if (!D3d9ShmemInitBuffers(i)) {
      return false;
    }
  }

  CapsuleLog("d3d9 memory capture successful");
  return true;
}

static void D3d9Init(IDirect3DDevice9 *device) {
  CapsuleLog("Initializing D3D9 capture");

  bool success;

  HWND window = nullptr;

  data.device = device;

  CapsuleLog("Determining D3D9 format");

  if (!D3d9InitFormatBackbuffer(&window)) {
    if (!D3d9InitFormatSwapchain(&window)) {
      return;
    }
  }

  CapsuleLog("Initializing shmem D3D9 capture");
  CapsuleLog("Backbuffer format: %ux%u %s",
    data.cx, data.cy,
    NameFromDxgiFormat(D3d9ToDxgiFormat(data.format)).c_str());

  success = D3d9ShmemInit();

  if (!success) {
    return;
  }
}

void D3d9ShmemCapture (IDirect3DSurface9 *backbuffer) {
  // TODO: buffering (using queries)
  HRESULT hr;

  auto timestamp = capsule::FrameTimestamp();

  hr = data.device->StretchRect(
    backbuffer,
    nullptr,
    data.render_targets[0],
    nullptr,
    D3DTEXF_NONE /* or D3DTEXF_LINEAR */
  );

  if (FAILED(hr)) {
    CapsuleLog("D3d9ShmemCapture: StretchRect failed %d (%x)", hr, hr);
    return;
  }

  hr = data.device->GetRenderTargetData(
    data.render_targets[0],
    data.copy_surfaces[0]
  );

  if (FAILED(hr)) {
    CapsuleLog("D3d9ShmemCapture: GetRenderTargetData failed %d (%x)", hr, hr);
  }

  D3DLOCKED_RECT rect;

  hr = data.copy_surfaces[0]->LockRect(&rect, nullptr, D3DLOCK_READONLY);
  if (SUCCEEDED(hr)) {
    char *frame_data = (char *) rect.pBits;
    int frame_data_size = data.cy * data.pitch;
    capsule::io::WriteVideoFrame(timestamp, frame_data, frame_data_size);
    data.copy_surfaces[0]->UnlockRect();
  }
}

void D3d9Capture (IDirect3DDevice9 *device, IDirect3DSurface9 *backbuffer) {
  static bool first_frame = true;

  if (!capsule::CaptureReady()) {
    if (!capsule::CaptureActive()) {
      first_frame = true;
    }

    return;
  }

  if (!data.device) {
    D3d9Init(device);
  }

  if (data.device != device) {
    D3d9Free();
    return;
  }

  if (first_frame) {
    auto pix_fmt = DxgiFormatToPixFmt(D3d9ToDxgiFormat(data.format));
    capsule::io::WriteVideoFormat(
      data.cx,
      data.cy,
      pix_fmt,
      0 /* no vflip */,
      data.pitch
    );
    first_frame = false;
  }

  D3d9ShmemCapture(backbuffer);
}
