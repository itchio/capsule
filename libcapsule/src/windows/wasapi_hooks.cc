
/*
 *
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

// IMMDeviceEnumerator, IMMDevice
#include <mmDeviceapi.h>

namespace capsule {
namespace wasapi {

const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

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
  auto guid = riid;
  Log("Wasapi: CoCreate instance called with riid {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}", 
    guid.Data1, guid.Data2, guid.Data3, 
    guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
    guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

  if (riid == IID_IMMDeviceEnumerator) {
    Log("Found the IMMDeviceEnumerator!");
  }

  return CoCreateInstance_real(
    rclsid,
    pUnkOuter,
    dwClsContext,
    riid,
    ppv
  );
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
