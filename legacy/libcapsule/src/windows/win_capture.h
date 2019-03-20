
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

#pragma once

#include <d3d9.h>

// Deviare-InProc, for hooking
#include <NktHookLib.h>

// LoadLibrary, etc.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#include "../capture.h"

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

namespace wasapi {
void InstallHooks();
}

} // namespace capsule

