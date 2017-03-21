#pragma once

// These help get addresses of interesting methods.
// They're implemented in a separate file because we're using
// D3D9's CINTERFACE to access the vtable cleanly.

void* capsule_get_IDirect3D9_CreateDevice_address(void *);
void* capsule_get_IDirect3D9Ex_CreateDeviceEx_address(void *);

void* capsule_get_IDirect3DDevice9_Present_address(void *);
void* capsule_get_IDirect3DDevice9Ex_PresentEx_address(void *);
void* capsule_get_IDirect3DSwapChain9_Present_address(void *);
