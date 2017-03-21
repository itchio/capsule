
#include <capsule.h>

#include <ddraw.h>

///////////////////////////////////////////////
// DirectDrawCreate
///////////////////////////////////////////////

typedef HRESULT (CAPSULE_STDCALL *DirectDrawCreate_t)(
  GUID FAR         *lpGUID,
  LPDIRECTDRAW FAR *lplpDD,
  IUnknown FAR     *pUnkOuter
);
static DirectDrawCreate_t DirectDrawCreate_real;
static SIZE_T DirectDrawCreate_hookId;

HRESULT CAPSULE_STDCALL DirectDrawCreate_hook (
  GUID FAR         *lpGUID,
  LPDIRECTDRAW FAR *lplpDD,
  IUnknown FAR     *pUnkOuter
) {
    capsule_log("DirectDrawCreate called!")
    HRESULT res = DirectDrawCreate_real(lpGUID, lplpDD, pUnkOuter);
    if (SUCCEEDED(res)) {
        capsule_log("DirectDrawCreate succeeded!")
        IDirectDraw7 *dd = (IDirectDraw7 *) lplpDD;
        DDCAPS caps;
        ZeroMemory(&caps, sizeof(DDCAPS));
        caps.dwSize = sizeof(DDCAPS);
        HRESULT hr = dd->GetCaps(&caps, NULL);
        if (SUCCEEDED(hr)) {
            capsule_log("Retrieved caps. Mem: %d bytes free / %d bytes total",
                caps.dwVidMemFree, caps.dwVidMemTotal);
        }
    } else {
        capsule_log("DirectDrawCreate failed!")
    }

    return res;
}

///////////////////////////////////////////////
// DirectDrawCreateEx
///////////////////////////////////////////////

typedef HRESULT (CAPSULE_STDCALL *DirectDrawCreateEx_t)(
  GUID FAR         *lpGUID,
  LPDIRECTDRAW FAR *lplpDD,
  REFIID           iid,
  IUnknown FAR     *pUnkOuter
);
static DirectDrawCreateEx_t DirectDrawCreateEx_real;
static SIZE_T DirectDrawCreateEx_hookId;

HRESULT CAPSULE_STDCALL DirectDrawCreateEx_hook (
  GUID FAR         *lpGUID,
  LPDIRECTDRAW FAR *lplpDD,
  REFIID           iid,
  IUnknown FAR     *pUnkOuter
) {
    capsule_log("DirectDrawCreateEx called!")
    HRESULT res = DirectDrawCreateEx_real(lpGUID, lplpDD, iid, pUnkOuter);
    if (SUCCEEDED(res)) {
        capsule_log("DirectDrawCreateEx succeeded!")
        IDirectDraw7 *dd = (IDirectDraw7 *) lplpDD;
        DDCAPS caps;
        ZeroMemory(&caps, sizeof(DDCAPS));
        caps.dwSize = sizeof(DDCAPS);
        HRESULT hr = dd->GetCaps(&caps, NULL);
        if (SUCCEEDED(hr)) {
            capsule_log("Retrieved caps. Mem: %d bytes free / %d bytes total",
                caps.dwVidMemFree, caps.dwVidMemTotal);
        }
    } else {
        capsule_log("DirectDrawCreateEx failed!")
    }

    return res;
}

void capsule_install_ddraw_hooks() {
    DWORD err;

    // ddraw

    HMODULE ddraw = LoadLibrary(L"ddraw.dll");
    if (!ddraw) {
        capsule_log("Could not load ddraw.dll, disabling DirectDraw capture");
        return;
    }

    if (!DirectDrawCreate_hookId) {
        LPVOID DirectDrawCreate_addr = NktHookLibHelpers::GetProcedureAddress(ddraw, "DirectDrawCreate");
        if (!DirectDrawCreate_addr) {
            capsule_log("Could not find DirectDrawCreate");
            return;
        }

        err = cHookMgr.Hook(
            &DirectDrawCreate_hookId,
            (LPVOID *) &DirectDrawCreate_real,
            DirectDrawCreate_addr,
            DirectDrawCreate_hook,
            0
        );
        if (err != ERROR_SUCCESS) {
            capsule_log("Hooking DirectDrawCreate derped with error %d (%x)", err, err);
            return;
        }
        capsule_log("Installed DirectDrawCreate hook");
    }

    if (!DirectDrawCreateEx_hookId) {
        LPVOID DirectDrawCreateEx_addr = NktHookLibHelpers::GetProcedureAddress(ddraw, "DirectDrawCreateEx");
        if (!DirectDrawCreateEx_addr) {
            capsule_log("Could not find DirectDrawCreateEx");
            return;
        }

        err = cHookMgr.Hook(
            &DirectDrawCreateEx_hookId,
            (LPVOID *) &DirectDrawCreateEx_real,
            DirectDrawCreateEx_addr,
            DirectDrawCreateEx_hook,
            0
        );
        if (err != ERROR_SUCCESS) {
            capsule_log("Hooking DirectDrawCreateEx derped with error %d (%x)", err, err);
            return;
        }
        capsule_log("Installed DirectDrawCreateEx hook");
    }
}