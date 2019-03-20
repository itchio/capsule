
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

#include "d3d9_vtable_helpers.h"

#define CINTERFACE
#include <d3d9.h>
#undef CINTERFACE

void *capsule_get_IDirect3D9_CreateDevice_address (void *pObj) {
  IDirect3D9 *obj = (IDirect3D9 *) pObj;
  return obj->lpVtbl->CreateDevice;
}

void *capsule_get_IDirect3D9Ex_CreateDeviceEx_address (void *pObj) {
  IDirect3D9Ex *obj = (IDirect3D9Ex *) pObj;
  return obj->lpVtbl->CreateDeviceEx;
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