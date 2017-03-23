
#pragma once

#include <d3d9.h>

// Deviare-InProc, for hooking
#include <NktHookLib.h>

// LoadLibrary, etc.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

namespace capsule {

extern CNktHookLib cHookMgr;

namespace process {
void InstallHooks();
} // namespace process

namespace dc {
bool Init();
} // namespace gl

namespace gl {
void InstallHooks();
} // namespace gl

namespace dxgi {
void InstallHooks();
} // namespace dxgi

namespace d3d9 {
void InstallHooks();
void Capture(IDirect3DDevice9 *device, IDirect3DSurface9 *backbuffer);
void Free();
} // namespace d3d9

namespace d3d10 {
void Capture(void *swap_ptr, void *backbuffer_ptr);
void Free();
} // namespace d3d10

namespace d3d11 {
void DrawOverlay();
void Capture(void *swap_ptr, void *backbuffer_ptr);
void Free();
} // namespace d3d11

} // namespace capsule

