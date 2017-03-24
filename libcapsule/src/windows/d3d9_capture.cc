
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

#include <d3d9.h>
#include <dxgi.h>
#include "dxgi_util.h"

#include "../capsule.h"
#include "../io.h"
#include "../logging.h"
#include "../capture.h"
#include "win_capture.h"

namespace capsule {
namespace d3d9 {

// inspired by libobs
struct State {
  HMODULE                 d3d9;
  IDirect3DDevice9        *device; // do not release
  int                     cx;
  int                     cy;
  D3DFORMAT               format;

  IDirect3DSurface9       *d3d9_copytex;

  IDirect3DSurface9       *copy_surfaces[kNumBuffers];
  IDirect3DSurface9       *render_targets[kNumBuffers];
  IDirect3DQuery9         *queries[kNumBuffers];
 	bool                    texture_mapped[kNumBuffers];
	volatile bool           issued_queries[kNumBuffers];
  intptr_t                pitch;
};

static struct State state = {};

void Free() {
  for (size_t i = 0; i < kNumBuffers; i++) {
    if (state.copy_surfaces[i]) {
      if (state.texture_mapped[i]) {
        state.copy_surfaces[i]->UnlockRect();
      }
      state.copy_surfaces[i]->Release();
    }
    if (state.render_targets[i]) {
      state.render_targets[i]->Release();
    }
    if (state.queries[i]) {
      state.queries[i]->Release();
    }
  }

  memset(&state, 0, sizeof(state));

  Log("----------------------- d3d9 capture freed ----------------------");
}

static DXGI_FORMAT ToDxgiFormat (D3DFORMAT format) {
  switch ((unsigned long) format) {
    case D3DFMT_A2B10G10R10: return DXGI_FORMAT_R10G10B10A2_UNORM;
    case D3DFMT_A8R8G8B8:    return DXGI_FORMAT_B8G8R8A8_UNORM;
    case D3DFMT_X8R8G8B8:    return DXGI_FORMAT_B8G8R8X8_UNORM;
    default:                 return DXGI_FORMAT_UNKNOWN;
  }
}

static bool GetSwapDesc (D3DPRESENT_PARAMETERS *pp) {
  IDirect3DSwapChain9 *swap = nullptr;
  HRESULT hr;

  hr = state.device->GetSwapChain(0, &swap);
  if (FAILED(hr)) {
    Log("GetSwapDesc: Failed to get swap chain %d (%x)", hr, hr);
    return false;
  }

  hr = swap->GetPresentParameters(pp);
  swap->Release();

  if (FAILED(hr)) {
    Log("GetSwapDesc: Failed to get presentation parameters %d (%x)", hr, hr);
    return false;
  }

  return true;
}

static bool InitFormatBackbuffer(HWND *window) {
  IDirect3DSurface9 *back_buffer = nullptr;
  D3DPRESENT_PARAMETERS pp;
  D3DSURFACE_DESC desc;
  HRESULT hr;
  
  if (!GetSwapDesc(&pp)) {
    return false;
  }

  hr = state.device->GetRenderTarget(0, &back_buffer);
  if (FAILED(hr)) {
    return false;
  }

  hr = back_buffer->GetDesc(&desc);
  back_buffer->Release();

  if (FAILED(hr)) {
    Log("InitFormatBackbuffer: Failed to get backbuffer descriptor %d (%x)", hr, hr);
  }

  state.format = desc.Format;
  *window = pp.hDeviceWindow;
  state.cx = desc.Width;
  state.cy = desc.Height;

  return true;
}

static bool InitFormatSwapchain(HWND *window) {
  D3DPRESENT_PARAMETERS pp;

  if (!GetSwapDesc(&pp)) {
    return false;
  }

  state.format = pp.BackBufferFormat;
  state.cx = pp.BackBufferWidth;
  state.cy = pp.BackBufferHeight;

  return false;
}

static bool ShmemInitBuffers(size_t buffer) {
  HRESULT hr;

  hr = state.device->CreateOffscreenPlainSurface(
    state.cx,
    state.cy,
    state.format,
    D3DPOOL_SYSTEMMEM,
    &state.copy_surfaces[buffer],
    nullptr
  );

  if (FAILED(hr)) {
    Log("ShmemInitBuffers: Failed to create surface %d (%x)", hr, hr);
    return false;
  }

  if (buffer == 0) {
    D3DLOCKED_RECT rect;
    hr = state.copy_surfaces[buffer]->LockRect(&rect, nullptr, D3DLOCK_READONLY);
    if (FAILED(hr)) {
      Log("ShmemInitBuffers: Failed to lock buffer %d (%x)", hr, hr);
      return false;
    }

    state.pitch = rect.Pitch;
    state.copy_surfaces[buffer]->UnlockRect();
  }

  hr = state.device->CreateRenderTarget(
    state.cx,
    state.cy,
    state.format,
    D3DMULTISAMPLE_NONE,
    0,
    false,
    &state.render_targets[buffer],
    nullptr
  );
  if (FAILED(hr)) {
    Log("ShmemInitBuffers: Failed to create render target %d (%x)", hr, hr);
    return false;
  }

  hr = state.device->CreateQuery(D3DQUERYTYPE_EVENT, &state.queries[buffer]);
  if (FAILED(hr)) {
    Log("ShmemInitBuffers: Failed to create query %d (%x)", hr, hr);
    return false;
  }

  return true;
}

static bool ShmemInit() {
  for (size_t i = 0; i < kNumBuffers; i++) {
    if (!ShmemInitBuffers(i)) {
      return false;
    }
  }

  Log("d3d9 memory capture successful");
  return true;
}

static void Init(IDirect3DDevice9 *device) {
  Log("Initializing D3D9 capture");

  bool success;

  HWND window = nullptr;

  state.device = device;

  Log("Determining D3D9 format");

  if (!InitFormatBackbuffer(&window)) {
    if (!InitFormatSwapchain(&window)) {
      return;
    }
  }

  Log("Initializing shmem D3D9 capture");
  Log("Backbuffer format: %ux%u %s",
    state.cx, state.cy,
    dxgi::NameFromFormat(ToDxgiFormat(state.format)).c_str());

  success = ShmemInit();

  if (!success) {
    return;
  }
}

void ShmemCapture (IDirect3DSurface9 *backbuffer) {
  // TODO: buffering (using queries)
  HRESULT hr;

  auto timestamp = capture::FrameTimestamp();

  hr = state.device->StretchRect(
    backbuffer,
    nullptr,
    state.render_targets[0],
    nullptr,
    D3DTEXF_NONE /* or D3DTEXF_LINEAR */
  );

  if (FAILED(hr)) {
    Log("ShmemCapture: StretchRect failed %d (%x)", hr, hr);
    return;
  }

  hr = state.device->GetRenderTargetData(
    state.render_targets[0],
    state.copy_surfaces[0]
  );

  if (FAILED(hr)) {
    Log("ShmemCapture: GetRenderTargetData failed %d (%x)", hr, hr);
  }

  D3DLOCKED_RECT rect;

  hr = state.copy_surfaces[0]->LockRect(&rect, nullptr, D3DLOCK_READONLY);
  if (SUCCEEDED(hr)) {
    char *frame_data = (char *) rect.pBits;
    int frame_data_size = state.cy * state.pitch;
    io::WriteVideoFrame(timestamp, frame_data, frame_data_size);
    state.copy_surfaces[0]->UnlockRect();
  }
}

void Capture (IDirect3DDevice9 *device, IDirect3DSurface9 *backbuffer) {
  static bool first_frame = true;

  if (!capture::Ready()) {
    if (!capture::Active()) {
      first_frame = true;
    }

    return;
  }

  if (!state.device) {
    Init(device);
  }

  if (state.device != device) {
    Free();
    return;
  }

  if (first_frame) {
    auto pix_fmt = dxgi::FormatToPixFmt(ToDxgiFormat(state.format));
    io::WriteVideoFormat(state.cx, state.cy, pix_fmt, false /* no vflip */,
                         state.pitch);
    first_frame = false;
  }

  ShmemCapture(backbuffer);
}

} // namespace d3d9
} // namespace capsule
