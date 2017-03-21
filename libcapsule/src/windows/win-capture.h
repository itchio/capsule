
#pragma once

#include <d3d9.h>

void D3d9Capture(IDirect3DDevice9 *device, IDirect3DSurface9 *backbuffer);
void D3d9Free();

void D3d10Capture(void *swap_ptr, void *backbuffer_ptr);
void D3d10Free();

void D3d11DrawOverlay();
void D3d11Capture(void *swap_ptr, void *backbuffer_ptr);
void D3d11Free();
