#pragma once

// These help get addresses of interesting methods.
// They're implemented in a separate file because we're using
// DXGI's CINTERFACE to access the vtable cleanly.

void* capsule_get_IDXGIFactory_CreateSwapChain_address(void *);
void* capsule_get_IDXGIFactory_CreateSwapChainForHwnd_address(void *);
void* capsule_get_IDXGISwapChain_Present_address(void *);
