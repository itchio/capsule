
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

#include "wasapi_vtable_helpers.h"

#define CINTERFACE
#include <audioclient.h>
#undef CINTERFACE

void* capsule_get_IAudioClient_Initialize(void *pObj) {
  auto obj = reinterpret_cast<IAudioClient *>(pObj);
  return obj->lpVtbl->Initialize;
}

void* capsule_get_IAudioClient_GetCurrentPadding(void *pObj) {
  auto obj = reinterpret_cast<IAudioClient *>(pObj);
  return obj->lpVtbl->GetCurrentPadding;
}

void* capsule_get_IAudioRenderClient_GetBuffer(void *pObj) {
  auto obj = reinterpret_cast<IAudioRenderClient *>(pObj);
  return obj->lpVtbl->GetBuffer;
}

void* capsule_get_IAudioRenderClient_ReleaseBuffer(void *pObj) {
  auto obj = reinterpret_cast<IAudioRenderClient *>(pObj);
  return obj->lpVtbl->ReleaseBuffer;
}
