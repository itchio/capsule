
#define CINTERFACE
#include <d3d9.h>
#undef CINTERFACE

#include "d3d9-vtable-helpers.h"

void *capsule_get_IDirect3D9_CreateDevice_address (void *pObj) {
  IDirect3D9 *obj = (IDirect3D9 *) pObj;
  return obj->lpVtbl->CreateDevice;
}