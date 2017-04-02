
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

#include <psapi.h>

#include "../io.h"
#include "../logging.h"
#include "win_capture.h"

static int capsule_inited = 0;

namespace capsule {

CNktHookLib cHookMgr;

void InstallHooks () {
  if (capsule_inited) {
    return;
  }
  capsule_inited = 1;

  // Let Deviare-InProc be chatty
  cHookMgr.SetEnableDebugOutput(TRUE);

  process::InstallHooks();
  gl::InstallHooks();
  dxgi::InstallHooks();
  d3d9::InstallHooks();
}

} // namespace capsule

extern "C" {

BOOL LAB_STDCALL DllMain(void *hinstDLL, int reason, void *reserved) {
  // don't actually do anything here, since what we can do from DllMain
  // is limited (no LoadLibrary, etc.)

  return TRUE;
}

DWORD __declspec(dllexport) CapsuleWindowsInit() {
  wchar_t process_name[MAX_PATH];
  process_name[0] = '\0';
  GetModuleBaseName(GetCurrentProcess(), nullptr, process_name, MAX_PATH);
  DWORD pid = GetCurrentProcessId();

  capsule::Log("capsule warming up for %S (pid %d)", process_name, pid);
  capsule::io::Init();
  capsule::InstallHooks();

  return ERROR_SUCCESS;
}

}
