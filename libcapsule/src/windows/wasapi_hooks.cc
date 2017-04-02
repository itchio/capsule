
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

#include "win_capture.h"

#include "../logging.h"
#include "wasapi_vtable_helpers.h"

// IMMDeviceEnumerator, IMMDevice
#include <mmDeviceapi.h>
// IAudioClient, IAudioCaptureClient
#include <audioclient.h>

namespace capsule {
namespace wasapi {

/**
 * Release a COM resource, if it's non-null
 */
static inline void SafeRelease(IUnknown *p) {
  if (p) {
    p->Release();
  }
}

typedef HRESULT (LAB_STDCALL *CoCreateInstance_t) (
  REFCLSID  rclsid,
  LPUNKNOWN pUnkOuter,
  DWORD     dwClsContext,
  REFIID    riid,
  LPVOID    *ppv
);
static CoCreateInstance_t CoCreateInstance_real;
static SIZE_T CoCreateInstance_hookId = 0;

HRESULT LAB_STDCALL CoCreateInstance_hook (
  REFCLSID  rclsid,
  LPUNKNOWN pUnkOuter,
  DWORD     dwClsContext,
  REFIID    riid,
  LPVOID    *ppv
) {
  static bool hooking = false;

  auto guid = riid;
  Log("Wasapi: CoCreate instance called with riid {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}", 
    guid.Data1, guid.Data2, guid.Data3, 
    guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
    guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

  HRESULT ret = CoCreateInstance_real(
    rclsid,
    pUnkOuter,
    dwClsContext,
    riid,
    ppv
  );

  if (hooking) {
    return ret;
  }

  if (riid == __uuidof(IMMDeviceEnumerator) && !GetBuffer_hookId) {
    hooking = true;

    Log("Found the IMMDeviceEnumerator!");

    IMMDevice *pDevice = nullptr;
    IAudioClient *pAudioClient = nullptr;
    IAudioRenderClient *pRenderClient = nullptr;
    WAVEFORMATEX *pwfx = nullptr;

    do {
      HRESULT hr;
      auto pEnumerator = reinterpret_cast<IMMDeviceEnumerator *>(*ppv);
      hr = pEnumerator->GetDefaultAudioEndpoint(
        eRender, eConsole, &pDevice
      );
      if (FAILED(hr)) {
        Log("Wasapi: GetDefaultAudioEndpoint failed");
        break;
      }

      hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**) &pAudioClient);
      if (FAILED(hr)) {
        Log("Wasapi: Activate failed");
        break;
      }

      hr = pAudioClient->GetMixFormat(&pwfx);
      if (FAILED(hr)) {
        Log("Wasapi: GetMixFormat failed");
        break;
      }

      int hnsRequestedDuration = 10000000;
      hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        0,
        hnsRequestedDuration,
        0,
        pwfx,
        nullptr
      );
      if (FAILED(hr)) {
        Log("Wasapi: Initialize failed");
        break;
      }

      pAudioClient->GetService(
        __uuidof(IAudioRenderClient),
        (void**) &pRenderClient
      );
      if (FAILED(hr)) {
        Log("Wasapi: GetService failed");
        break;
      }

      Log("Wasapi: hey we got a render client! Poking vtable...");
      void *GetBuffer_addr = capsule_get_IAudioRenderClient_GetBuffer(pAudioClient);
      void *ReleaseBuffer_addr = capsule_get_IAudioRenderClient_ReleaseBuffer(pAudioClient);

      Log("Wasapi: GetBuffer address: %p", GetBuffer_addr);
      Log("Wasapi: ReleaseBuffer address: %p", ReleaseBuffer_addr);
    } while(false);

    SafeRelease(pRenderClient);
    SafeRelease(pAudioClient);
    SafeRelease(pDevice);
    if (pwfx) {
      CoTaskMemFree(pwfx);
    }

    Log("Wasapi: done doing the stuff");
    hooking = false;
  }

  return ret;
}

void InstallHooks() {
  DWORD err;

  HMODULE ole = LoadLibrary(L"ole32.dll");
  if (!ole) {
    Log("Could not load ole32.dll, disabling Wasapi hooking");
    return;
  }

  // CoCreateInstance
  if (!CoCreateInstance_hookId) {
    LPVOID CoCreateInstance_addr = GetProcAddress(ole, "CoCreateInstance");
    if (!CoCreateInstance_addr) {
      Log("Could not find CoCreateInstance");
      return;
    }

    err = cHookMgr.Hook(
      &CoCreateInstance_hookId,
      (LPVOID *) &CoCreateInstance_real,
      CoCreateInstance_addr,
      CoCreateInstance_hook,
      0
    );
    if (err != ERROR_SUCCESS) {
      Log("Hooking CoCreateInstance derped with error %d (%x)", err, err);
      return;
    }
    Log("Installed CoCreateInstance hook");
  }
}

}
} // namespace capsule
