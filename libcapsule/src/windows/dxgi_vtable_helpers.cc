
#define CINTERFACE
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>
#undef CINTERFACE

#include "dxgi_vtable_helpers.h"

void *capsule_get_IDXGIFactory_CreateSwapChain_address (void *pFactory) {
  IDXGIFactory *factory = (IDXGIFactory *) pFactory;
  return factory->lpVtbl->CreateSwapChain;
}

void *capsule_get_IDXGIFactory_CreateSwapChainForHwnd_address (void *pFactory) {
  IDXGIFactory2 *factory = (IDXGIFactory2 *) pFactory;
  return factory->lpVtbl->CreateSwapChainForHwnd;
}

void *capsule_get_IDXGISwapChain_Present_address (void *pSwapChain) {
  IDXGISwapChain *swapChain = (IDXGISwapChain *) pSwapChain;
  return swapChain->lpVtbl->Present;
}