
#pragma once

#include <d3d9.h>

void d3d9_capture(IDirect3DDevice9 *device, IDirect3DSurface9 *backbuffer);
void d3d9_free();

void d3d10_capture(void *swap_ptr, void *backbuffer_ptr);
void d3d10_free();

void d3d11_capture(void *swap_ptr, void *backbuffer_ptr);
void d3d11_free();
