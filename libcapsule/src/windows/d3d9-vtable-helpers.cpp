
#define CINTERFACE
#include <d3d9.h>
#undef CINTERFACE

#include "d3d9-vtable-helpers.h"

void *capsule_get_IDirect3D9_CreateDevice_address (void *pObj) {
  IDirect3D9 *obj = (IDirect3D9 *) pObj;
  return obj->lpVtbl->CreateDevice;
}

void* capsule_get_IDirect3DDevice9_Present_address(void *pDevice) {
  IDirect3DDevice9 *device = (IDirect3DDevice9 *) pDevice;
  return device->lpVtbl->Present;
}

void* capsule_get_IDirect3DDevice9Ex_PresentEx_address(void *pDevice) {
  IDirect3DDevice9Ex *device = (IDirect3DDevice9Ex *) pDevice;
  return device->lpVtbl->PresentEx;
}

void* capsule_get_IDirect3DSwapChain9_Present_address(void *pSwap) {
  IDirect3DSwapChain9 *swap = (IDirect3DSwapChain9 *) pSwap;
  return swap->lpVtbl->Present;
}